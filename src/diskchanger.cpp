/* diskchanger.cpp
 *
 *  This file is part of vchanger by Josh Fisher.
 *
 *  vchanger copyright (C) 2008-2009 Josh Fisher
 *
 *  vchanger is free software.
 *  You may redistribute it and/or modify it under the terms of the
 *  GNU General Public License version 2, as published by the Free
 *  Software Foundation.
 *
 *  vchanger is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with vchanger.  See the file "COPYING".  If not,
 *  write to:  The Free Software Foundation, Inc.,
 *             59 Temple Place - Suite 330,
 *             Boston,  MA  02111-1307, USA.
 *
 *  The bulk of the work gets done by the DiskChanger class, which does
 *  the actual "loading" and "unloading" of volumes.
 */

#include "config.h"
#ifdef HAVE_WINDOWS_H
#include "junctions.h"
#endif
#include "vchanger.h"
#include "util.h"
#include "changerstate.h"
#include "diskchanger.h"
#include "vchanger_common.h"
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

char junct_msg[4096];
unsigned int junct_err;

/*-------------------------------------------------
 *  Static function to get pid of process that owns
 *  the lockfile. Returns zero if lockfile doesn't exist or
 *  if not owned. If owner process does not exist,
 *  then the lockfile is deleted and zero is returned.
 *------------------------------------------------*/
static pid_t GetLockfileOwner(const char *lockfile)
{
   char tmp[PATH_MAX];
   pid_t result = 0;
   FILE *fs = fopen(lockfile, "r");
   if (!fs) {
      return 0;
   }
   fgets(tmp, sizeof(tmp), fs);
   fclose(fs);
   result = (pid_t)strtol(tmp, NULL, 10);
   if (!result) {
      return 0;
   }
   if (!unlink(lockfile)) {
      return 0;
   }
   return result;
}

/*
 *  Delete symlink 'path'
 */
static int DeleteSymlink(const char *path)
{
#ifdef HAVE_WINDOWS_H
   return unlink_symlink(path);
#else
   return unlink(path);
#endif
}

/*=================================================
 *  Class DiskChanger
 *=================================================*/

/*-------------------------------------------------
 * default constructor
 *-------------------------------------------------*/
DiskChanger::DiskChanger()
{
   memset(changer_name, 0, sizeof(changer_name));
   memset(work_dir, 0, sizeof(work_dir));
   magazine_bays = 0;
   virtual_drives = 0;
   slots_per_mag = 0;
   slots = 0;
   changer_lock = NULL;
   lasterr = 0;
   memset(lasterrmsg, 0, sizeof(lasterrmsg));
}

/*-------------------------------------------------
 * destructor
 *-------------------------------------------------*/
DiskChanger::~DiskChanger()
{
   internalUnlock();
}

/*-------------------------------------------------
 *  Method to initialize changer parameters and state of magazines and virtual drives.
 *  On success, returns zero and has a lock on the changer. On error, returns negative.
 *  In either case, obtains a lock on the changer unless the lock operation itself fails.
 *  The lock will be released when the DiskChanger object is destroyed, or else by
 *  calling the Unlock() member function.
 *------------------------------------------------*/
int DiskChanger::Initialize(VchangerConfig &conf)
{
   int n, i, rc;
   VolumeLabel lab;
   char *used;
   char chngr[128], sname[PATH_MAX], tmp[PATH_MAX];

   /* Set parameters from config file */
   strncpy(changer_name, conf.changer_name, sizeof(changer_name));
   strncpy(work_dir, conf.work_dir, sizeof(work_dir));
   magazine_bays = conf.magazine_bays;
   slots_per_mag = conf.slots_per_mag;
   virtual_drives = conf.virtual_drives;
   slots = conf.slots;
   lasterr = 0;
   /* Make sure we have a lock on this changer */
   if (!changer_lock) {
      if (!Lock()) {
         lasterr = -1;
         snprintf(lasterrmsg, sizeof(lasterrmsg),
               "failed to obtain lock for changer %s", changer_name);
         return lasterr;
      }
   }
   /* Determine which magazines are currently mounted in magazine bays */
   used = (char*)malloc(conf.known_magazines + 1);
   memset(used, 0, conf.known_magazines + 1);
   for (n = 1; n <= magazine_bays; n++) {
      /* find a partition for this magazine */
      for (i = 1; i <= conf.known_magazines; i++) {
         if (used[i] == '\0') {
            if (magazine[n].set(conf.magazine[i], conf.automount_dir) == 0) {
               used[i] = '1';
               break;
            }
            used[i] = '1'; /* don't keep looking at unmounted magazines */
         }
      }
      if (i > conf.known_magazines) {
         /* no known magazines left, so leave remaining indices empty */
         break;
      }
   }
   free(used);

   /* Read last known state of virtual drives */
   for (i = 0; i < virtual_drives; i++) {
      drive[i].drive = i;
      strncpy(drive[i].statedir, work_dir, sizeof(drive[i].statedir));
      if (drive[i].restore()) {
         lasterr = -1;
         snprintf(lasterrmsg, sizeof(lasterrmsg),
               "access denied for drive %d state file", i);
         return lasterr;
      }
      if (drive[i].IsLoaded()) {
         /* If drive state is loaded, then check that its symlink exists */
         GetDriveSymlinkDir(i, sname, sizeof(sname));
         n = readlink(sname, tmp, sizeof(tmp));
         tmp[n] = 0;
         if (n < 1) {
            /* symlink is missing, try to re-create */
            n = MagNum2MagBay(drive[i].vol.mag_number);
            if (n > 0) {
               if (SetDriveSymlink(i, magazine[n].mountpoint)) {
                  /* failed to create drive's symlink */
                  lasterr = -1;
                  snprintf(lasterrmsg, sizeof(lasterrmsg),
                        "failed to create symlink for drive %d", i);
                  return lasterr;
               }
            } else {
               /* drive was loaded from magazine that is no longer mounted
                * so change state to unloaded
                */
               drive[i].unset();
            }
         } else {
            /* symlink exists, but may be dangling */
            n = MagNum2MagBay(drive[i].vol.mag_number);
            if (n < 1) {
               /* symlink is pointing to unmounted magazine */
               if (RemoveDriveSymlink(i)) {
                  /* failed to unlink symlink */
                  lasterr = -1;
                  snprintf(lasterrmsg, sizeof(lasterrmsg),
                        "failed to remove symlink for drive %d", i);
                  return lasterr;
               }
               if (drive[i].unset()) {
                  /* failed to update stateN file */
                  lasterr = -1;
                  snprintf(lasterrmsg, sizeof(lasterrmsg),
                        "i/o error writing state%d file", i);
                  return lasterr;
               }
            }
         }
      }
   }
   /* The magazines the drives are currently loaded from are still mounted, but they may be at
    * a different mountpoint than they were previously. If the magazines have been plugged
    * in and out, the OS has re-booted, etc., then udev could assigned them to different
    * device nodes than they were assigned when the drive state was last saved. So we
    * need to make sure that the drive symlinks point to the CORRECT mountpoints.
    */
   for (i = 0; i < virtual_drives; i++) {
      if (!drive[i].IsLoaded()) {
         continue; /* skip unloaded drives */
      }
      GetDriveSymlinkDir(i, sname, sizeof(sname));
      n = readlink(sname, tmp, sizeof(tmp));
      tmp[n] = 0;
      if (n < 1) {
         /* failed to read symlink contents */
         lasterr = -1;
         snprintf(lasterrmsg, sizeof(lasterrmsg),
               "failed to read symlink target for drive %d", i);
         return lasterr;
      }
      n = MagNum2MagBay(drive[i].vol.mag_number);
      if (strcmp(tmp, magazine[n].mountpoint)) {
         /* magazine mountpoint changed, so fix symlink */
         if (RemoveDriveSymlink(i)) {
            /* failed to unlink symlink */
            lasterr = -1;
            snprintf(lasterrmsg, sizeof(lasterrmsg),
                  "failed to remove symlink for drive %d", i);
            return lasterr;
         }
         if (SetDriveSymlink(i, magazine[n].mountpoint)) {
            /* failed to create symlink */
            lasterr = -1;
            snprintf(lasterrmsg, sizeof(lasterrmsg),
                  "failed to create symlink for drive %d", i);
            return lasterr;
         }
      }
   }
   /* Now make sure the loaded slots on each magazine match the drive state */
   for (n = 1; n <= magazine_bays; n++) {
      if (magazine[n].mag_number < 1) {
         continue; /* skip uninitialized or unmounted magazines */
      }
      /* Check that any loaded slots on this magazine match the current state
       * of the virtual drives */
      for (i = 0; i < virtual_drives; i++) {
         /* Attempt to read a loadedN file for this drive on this magazine */
         rc = ReadMagazineLoaded(magazine[n].mountpoint, i, lab);
         if (rc) {
            lasterr = rc;
            if (rc == -2) {
               snprintf(lasterrmsg, sizeof(lasterrmsg),
                     "loaded%d file on magazine %d contains an invalid volume label", i, n);
            } else {
               snprintf(lasterrmsg, sizeof(lasterrmsg),
                     "i/o error reading loaded%d file on magazine %d", i, n);
            }
            return lasterr;
         }
         if (lab.empty()) {
            /* magazine thinks no slot loaded into this drive */
            if (drive[i].vol.mag_number == magazine[n].mag_number) {
               /* but drive state does, so fix magazine */
               if (DoLoad(n, i, drive[i].vol)) {
                  /* unable to do magazine level load, so fix drive state */
                  if (drive[i].unset()) {
                     lasterr = -1;
                     snprintf(lasterrmsg, sizeof(lasterrmsg),
                           "i/o error writing state%d file", i);
                     return lasterr;
                  }
                  if (RemoveDriveSymlink(i)) {
                     /* failed to remove symlink */
                     lasterr = -1;
                     snprintf(lasterrmsg, sizeof(lasterrmsg),
                           "failed to remove symlink for drive %d", i);
                     return lasterr;
                  }
               }
            }
         } else {
            /* magazine thinks one of its slots is loaded into this drive */
            if (drive[i].vol != lab) {
               /* drive state thinks a different volume is loaded than magazine does */
               if (drive[i].vol.mag_number == lab.mag_number) {
                  /* but it is a different volume on the same magazine, so the
                   * magazine needs to be fixed to match the drive state
                   */
                  if (DoUnload(n, i, lab)) {
                     /* magazine level unload failed */
                     lasterr = -1;
                     snprintf(lasterrmsg, sizeof(lasterrmsg),
                           "failed to unload drive %d into magazine %d slot %d", i, n,
                           lab.mag_slot);
                     return lasterr;
                  }
                  if (DoLoad(n, i, drive[i].vol)) {
                     /* magazine level load failed */
                     lasterr = -1;
                     snprintf(lasterrmsg, sizeof(lasterrmsg),
                           "failed to load drive %d from magazine %d slot %d", i, n,
                           drive[i].vol.mag_slot);
                     return lasterr;
                  }
               } else {
                  /* drive is loaded from another magazine, so fix this magazine */
                  if (DoUnload(n, i, lab)) {
                     /* magazine level unload failed */
                     lasterr = -1;
                     snprintf(lasterrmsg, sizeof(lasterrmsg),
                           "failed to unload drive %d from magazine %d slot %d", i, n,
                           lab.mag_slot);
                     return lasterr;
                  }
               }
            }
         }
      }
   }

   return 0;
}

/*-------------------------------------------------
 *  Method to load virtual drive 'drv' from virtual slot 'slot'.
 *  Returns lasterr. On success, sets lasterr=0, otherwise sets lasterr
 *  to a negative error number.
 *------------------------------------------------*/
int DiskChanger::LoadDrive(int drv, int slot)
{
   VolumeLabel lab;
   int ldrv, lslot;
   int mag_ndx;

   if (!changer_lock) {
      lasterr = -1;
      snprintf(lasterrmsg, sizeof(lasterrmsg), "changer not initialized");
      return lasterr;
   }
   lasterr = 0;
   if (drv < 0 || drv >= virtual_drives) {
      lasterr = -1;
      snprintf(lasterrmsg, sizeof(lasterrmsg), "drive number %d out of range",
            drv);
      return lasterr;
   }
   if (slot < 1 || slot > slots) {
      lasterr = -1;
      snprintf(lasterrmsg, sizeof(lasterrmsg),
            "invalid slot number %d max = %d", slot, slots);
      return lasterr;
   }
   /* Check if requested slot is already loaded into a drive */
   ldrv = GetSlotDrive(slot);
   if (ldrv == drv) {
      return 0; /* requested slot already loaded in requested drive */
   }

   if (ldrv >= 0) {
      /* requested slot is already loaded in another drive */
      lasterr = -1;
      snprintf(lasterrmsg, sizeof(lasterrmsg),
            "slot %d already loaded in drive %d", slot, ldrv);
      return lasterr;
   }
   /* Check if requested drive is already loaded */
   lslot = GetDriveSlot(drv);
   if (lslot > 0) {
      /* requested drive is already loaded from another slot */
      lasterr = -1;
      snprintf(lasterrmsg, sizeof(lasterrmsg),
            "drive %d already loaded from slot %d.", drv, lslot);
      return lasterr;
   }
   /* Do magazine level load */
   VirtualSlot2VolumeLabel(slot, lab);
   mag_ndx = MagNum2MagBay(lab.mag_number);
   if (mag_ndx < 1) {
      /* No magazine is mounted at the bay containing this slot */
      lasterr = -1;
      snprintf(lasterrmsg, sizeof(lasterrmsg),
            "magazine bay containing slot %d has no magazine mounted", slot);
      return lasterr;
   }
   if (DoLoad(mag_ndx, drv, lab)) {
      /* Could not perform magazine level load (ie. renaming of volume file) */
      lasterr = -1;
      snprintf(lasterrmsg, sizeof(lasterrmsg),
            "failed to rename volume file while loading drive %d from slot %d",
            drv, slot);
      return lasterr;
   }
   /* Create symlink to mounted magazine */
   if (SetDriveSymlink(drv, magazine[mag_ndx].mountpoint)) {
      /* Could not create symlink, so leave drive unloaded */
      DoUnload(mag_ndx, drv, lab);
      lasterr = -1;
      snprintf(lasterrmsg, sizeof(lasterrmsg),
            "could not create symlink for drive %d", drv);
      return lasterr;
   }
   /* update drive state */
   if (drive[drv].set(lab)) {
      lasterr = -1;
      snprintf(lasterrmsg, sizeof(lasterrmsg),
            "i/o error writing file state%d", drv);
      return lasterr;
   }

   return 0;
}

/*-------------------------------------------------
 *  Method to unload volume in virtual drive 'drv' into the virtual slot
 *  from whence it came. Returns lasterr. On success, sets lasterr = 0,
 *  otherwise sets lasterr to a negative error number.
 *------------------------------------------------*/
int DiskChanger::UnloadDrive(int drv)
{
   VolumeLabel lab;
   int mag_ndx;

   if (!changer_lock) {
      lasterr = -1;
      snprintf(lasterrmsg, sizeof(lasterrmsg), "changer not initialized");
      return lasterr;
   }
   lasterr = 0;
   if (drv < 0 || drv >= virtual_drives) {
      lasterr = -1;
      snprintf(lasterrmsg, sizeof(lasterrmsg), "drive number %d out of range",
            drv);
      return lasterr;
   }
   /* See if drive is loaded */
   if (!drive[drv].IsLoaded()) {
      return 0; /* drive already unloaded */
   }
   /* Do magazine level unload */
   mag_ndx = MagNum2MagBay(drive[drv].vol.mag_number);
   if (mag_ndx > 0) {
      if (DoUnload(mag_ndx, drv, drive[drv].vol)) {
         lasterr = -1;
         snprintf(lasterrmsg, sizeof(lasterrmsg), "failed to unload drive %d", drv);
         return lasterr;
      }
   }
   /* Remove drive's symlink */
   if (RemoveDriveSymlink(drv)) {
      lasterr = -1;
      snprintf(lasterrmsg, sizeof(lasterrmsg),
            "failed to remove symlink for drive %d", drv);
      return lasterr;
   }
   /* update drive state */
   if (drive[drv].unset()) {
      lasterr = -1;
      snprintf(lasterrmsg, sizeof(lasterrmsg),
            "i/o error writing file state%d", drv);
      return lasterr;
   }

   return 0;
}

/*-------------------------------------------------
 *  Method to get virtual slot curently loaded in virtual drive 'drv'.
 *  If virtual drive is loaded, sets lasterr to zero, and returns the
 *  virtual slot number. On error, sets lasterr negative for a fatal
 *  error or positive for a warning and returns negative value. Sets
 *  lasterr = 1 (warning) when the virtual drive is not "loaded".
 *------------------------------------------------*/
int DiskChanger::GetDriveSlot(int drv)
{
   int vslot;
   if (!changer_lock) {
      lasterr = -1;
      snprintf(lasterrmsg, sizeof(lasterrmsg), "changer not initialized");
      return lasterr;
   }
   lasterr = 0;
   if (drv >= virtual_drives) {
      lasterr = -1;
      snprintf(lasterrmsg, sizeof(lasterrmsg), "drive number %d out of range",
            drv);
      return -1;
   }
   vslot = VolumeLabel2VirtualSlot(drive[drv].vol);
   if (vslot < 1) {
      vslot = -1;
   }
   return vslot;
}

/*-------------------------------------------------
 *  Method to get virtual drive that virtual slot 'slot'
 *  is loaded into. On success, returns drive slot is loaded into.
 *  Otherwise, returns negative value.
 *------------------------------------------------*/
int DiskChanger::GetSlotDrive(int slot)
{
   VolumeLabel lab;
   int n;

   if (!changer_lock) {
      lasterr = -1;
      snprintf(lasterrmsg, sizeof(lasterrmsg), "changer not initialized");
      return lasterr;
   }
   lasterr = 0;
   if (slot < 1 || slot > slots) {
      lasterr = -1;
      snprintf(lasterrmsg, sizeof(lasterrmsg), "invalid slot number %d", slot);
      return -1;
   }
   VirtualSlot2VolumeLabel(slot, lab);
   for (n = 0; n < virtual_drives; n++) {
      if (drive[n].IsLoaded() && drive[n].vol == lab) {
         return n;
      }
   }
   return -1;
}

/*
 *  Create new magazine in magazine bay 'bay'. If magnum < 0 then assign a
 *  new magazine number, else use given magazine number. If magazine already
 *  exists or on error, return negative value. On success, return magazine
 *  number assigned.
 */
int DiskChanger::CreateMagazine(int bay, int magnum)
{
   int mag, n;
   mode_t old_mask;
   VolumeLabel lab;
   char vlabel[PATH_MAX], fname[PATH_MAX];
   FILE *fs;

   if (!changer_lock) {
      lasterr = -1;
      snprintf(lasterrmsg, sizeof(lasterrmsg), "changer not initialized");
      return lasterr;
   }
   lasterr = 0;
   if ((bay < 1) || (bay > magazine_bays)) {
      snprintf(lasterrmsg, sizeof(lasterrmsg),
            "magazine bay %d out of range", bay);
      lasterr = -1;
      return lasterr;
   }

   /* Check for magazine not mounted */
   if (strlen(magazine[bay].mountpoint) == 0) {
      lasterr = -1;
      snprintf(lasterrmsg, sizeof(lasterrmsg),
            "magazine bay %d has no magazine mounted", bay);
      return lasterr;
   }
   /* Check for existing magazine index file */
   if (magazine[bay].mag_number > 0) {
      lasterr = -1;
      snprintf(lasterrmsg, sizeof(lasterrmsg),
            "magazine in bay %d (%s) already initialized as magazine %d",
            bay, magazine[bay].mountpoint, magazine[bay].mag_number);
      return lasterr;
   }
   /* Determine magazine number to assign new magazine */
   mag = magnum;
   if (mag < 1) {
      mag = ReadNextMagazineNumber();
      if (mag < 1) {
         lasterr = -1;
         snprintf(lasterrmsg, sizeof(lasterrmsg),
               "could not assign next magazine number");
         return lasterr;
      }
   }
   /* Create new magazine's index file */
   if (WriteMagazineIndex(magazine[bay].mountpoint, mag)) {
      lasterr = -1;
      snprintf(lasterrmsg, sizeof(lasterrmsg),
            "could not create index file on %s", magazine[bay].mountpoint);
      return lasterr;
   }
   /* Create empty volumes on the magazine */
   old_mask = umask(MAG_VOLUME_MASK);
   for (n = 1; n <= slots_per_mag; n++) {
      lab.set(changer_name, mag, n);
      snprintf(fname, sizeof(fname), "%s%s%s", magazine[bay].mountpoint,
            DIR_DELIM, lab.GetLabel(vlabel, sizeof(vlabel)));
      fs = fopen(fname, "w");
      if (!fs) {
         lasterr = -1;
         snprintf(lasterrmsg, sizeof(lasterrmsg),
               "could not write magazine volume %s", fname);
         return lasterr;
      }
      fclose(fs);
   }
   /* Create empty loadedN files */
   for (n = 0; n < virtual_drives; n++) {
      snprintf(fname, sizeof(fname), "%s%sloaded%d", magazine[bay].mountpoint, DIR_DELIM, n);
      fs = fopen(fname, "w");
      if (!fs) {
         lasterr = -1;
         snprintf(lasterrmsg, sizeof(lasterrmsg),
               "could not write loadedN file for drive %d", n);
         return lasterr;
      }
      fprintf(fs, "\n");
      fclose(fs);
   }
   umask(old_mask);
   /* Update nextmag file */
   if (magnum > 0) {
      /* If user supplied mag number > current nextmag, then cause nextmag to be updated */
      n = ReadNextMagazineNumber();
      if (n > 0 && n <= mag)
         magnum = -1;
   }
   if (magnum < 1) {
      if (WriteNextMagazineNumber(mag + 1)) {
         lasterr = -1;
         snprintf(lasterrmsg, sizeof(lasterrmsg),
               "could not update nextmag file");
         return lasterr;
      }
   }
   /* update magazine mounted state */
   magazine[bay].mag_number = mag;
   return mag;
}

/*-------------------------------------------------
 *  Method to get last error encountered. Returns last error number
 *  and copies corresponding error message text to 'msg'.
 *------------------------------------------------*/
int DiskChanger::GetLastError(char * msg, size_t msg_size)
{
   *msg = 0;
   if (lasterr) {
      strncpy(msg, lasterrmsg, msg_size);
   }
   return lasterr;
}

/*-------------------------------------------------
 *  Method to unlock changer device. Sets lasterr to zero and
 *  deletes lock file.
 *------------------------------------------------*/
void DiskChanger::Unlock()
{
   lasterr = 0;
   internalUnlock();
}

/*-------------------------------------------------
 *  Method to lock changer device using a lock file such that only
 *  one vchanger process at a time may process changer commands.
 *  If another process has the lock, then this process will sleep
 *  100 ms before trying again. This try/wait loop will continue
 *  until the lock is obtained or 'timeout' ms have expired.
 *  On success, returns true. Otherwise, on error or timeout sets
 *  lasterr negative and returns false.
 *------------------------------------------------*/
bool DiskChanger::Lock(long timeout)
{
   struct timeval tv1, tv2;
   int rc;
   char lockfile[PATH_MAX];

   if (changer_lock) {
      return true;
   }
   snprintf(lockfile, sizeof(lockfile), "%s%s%s.lock", work_dir, DIR_DELIM,
         changer_name);
   gettimeofday(&tv1, NULL);
   rc = exclusive_fopen(lockfile, &changer_lock);
   while (rc == EEXIST) {
      GetLockfileOwner(lockfile);
      /* sleep before trying again */
      millisleep(100);
      gettimeofday(&tv2, NULL);
      if (timeval_et_ms(&tv1, &tv2) >= timeout)
         return false;
      rc = exclusive_fopen(lockfile, &changer_lock);
   }
   if (rc) {
      return false;
   }
   fprintf(changer_lock, "%d", getpid());
   fflush(changer_lock);
   return true;
}

/*-------------------------------------------------
 *  Protected method to unlock changer device without affecting lasterr.
 *------------------------------------------------*/
void DiskChanger::internalUnlock()
{
   char lockfile[PATH_MAX];

   if (!changer_lock) {
      return;
   }
   snprintf(lockfile, sizeof(lockfile), "%s%s%s.lock", work_dir, DIR_DELIM,
         changer_name);
   fclose(changer_lock);
   unlink(lockfile);
   changer_lock = NULL;
}

/*-------------------------------------------------
 *  Protected method to set default values for changer device.
 *------------------------------------------------*/
void DiskChanger::SetDefaults()
{
   /* Sets parameters to default values */
   strncpy(changer_name, DEFAULT_CHANGER_NAME, sizeof(changer_name));
   strncpy(work_dir, DEFAULT_WORK_DIR, sizeof(work_dir));
   virtual_drives = DEFAULT_VIRTUAL_DRIVES;
   slots_per_mag = DEFAULT_SLOTS_PER_MAG;
   slots = magazine_bays * slots_per_mag;
}

/*-------------------------------------------------
 *  Method to return in 'lnkdir' the symlink name to use for virtual drive
 *  'drv'. Returns zero on success, else non-zero if 'drv' is out of range.
 *------------------------------------------------*/
int DiskChanger::GetDriveSymlinkDir(int drv, char *lnkdir, size_t lnkdir_size)
{
   if (drv >= virtual_drives) {
      *lnkdir = 0;
      return -1;
   }
   snprintf(lnkdir, lnkdir_size, "%s%s%d", work_dir, DIR_DELIM, drv);
   return 0;
}

/*
 *  Returns the next available magazine number from the nextmag file.
 *  Returns negative value on error.
 */
int DiskChanger::ReadNextMagazineNumber()
{
   int mag;
   FILE *fs;
   char *gstr;
   char buf[128], nmag[PATH_MAX];

   /* Get nextmag path */
   snprintf(nmag, sizeof(nmag), "%s%snextmag", work_dir, DIR_DELIM);
   fs = fopen(nmag, "r");
   if (!fs) {
      return 1;
   }
   /* Read the nextmag file */
   gstr = fgets(buf, sizeof(buf), fs);
   fclose(fs);
   if (!gstr) {
      return -1; /* i/o error reading nextmag */
   }
   mag = strtol(buf, NULL, 10);
   if (mag < 1) {
      mag = 1;
   }
   return mag;
}

/*
 *  Writes the next available magazine number in 'magnum to the
 *  nextmag file. Returns zero on success or negative value on error.
 */
int DiskChanger::WriteNextMagazineNumber(int magnum)
{
   FILE *fs;
   char nmag[PATH_MAX];

   /* Get nextmag path */
   snprintf(nmag, sizeof(nmag), "%s%snextmag", work_dir, DIR_DELIM);
   fs = fopen(nmag, "w");
   if (!fs) {
      return -1;
   }
   if (fprintf(fs, "%d", magnum) < 0) {
      /* Unable to write to file */
      fclose(fs);
      return -1;
   }
   fclose(fs);
   return 0;
}

/*
 *  Writes the index file for magazine at 'mountpt', assigning it the magazine number given
 *  in 'magnum'. On success returns zero, otherwise negative value on error.
 */
int DiskChanger::WriteMagazineIndex(const char *mountpt, int magnum)
{
   char fname[PATH_MAX];
   snprintf(fname, sizeof(fname), "%s%sindex", mountpt, DIR_DELIM);
   FILE *fs = fopen(fname, "w");
   if (!fs) {
      return -1;
   }
   if (fprintf(fs, "%s_%04d", changer_name, magnum) < 0) {
      fclose(fs);
      return -1;
   }
   fclose(fs);
   return 0;
}

/*
 *  Reads the loadedN file on magazine 'mountpt' for drive 'drive'. If the loadedN file contains
 *  a volume label, then returns the volume info in 'lab', otherwise clears 'lab'. On success,
 *  returns zero. On i/o error, returns -1. On invalid label in loadedN file, returns -2.
 */
int DiskChanger::ReadMagazineLoaded(const char *mountpt, int drive, VolumeLabel &lab)
{
   FILE *fs;
   char loaded[PATH_MAX], buff[256];
   lab.clear();
   snprintf(loaded, sizeof(loaded), "%s%sloaded%d", mountpt, DIR_DELIM, drive);
   fs = fopen(loaded, "r");
   if (!fs) {
      /* there should be a loaded file. Try to write one */
      fs = fopen(loaded, "w");
      if (!fs) {
         /* unable to open for writing */
         return -1;
      }
      if (fprintf(fs, "\n") < 1) {
         /* unable to write loaded file */
         fclose(fs);
         return -1;
      }
      /* missing loaded file was created, same as if unloaded */
      fclose(fs);
      return 0;
   }
   if (!fgets(buff, sizeof(buff), fs)) {
      /* unable to read loaded file */
      fclose(fs);
      return -1;
   }
   fclose(fs);
   strip_whitespace(buff, sizeof(buff));
   if (!strlen(buff)) {
      /* driveN file was empty */
      return 0;
   }
   if (lab.set(buff)) {
      /* invalid volume label in loaded file */
      return -2;
   }
   return 0;
}

/*
 *  Writes the volume label contained in 'lab' to the loadedN file for drive 'drive'
 *  on magazine 'mountpt'. If 'lab' is cleared, then clears the contents of loadedN.
 *  On success, returns zero. Otherwise, on i/o error, returns negative value.
 */
int DiskChanger::WriteMagazineLoaded(const char *mountpt, int drive, VolumeLabel &lab)
{
   char loaded[PATH_MAX], vlabel[128];
   snprintf(loaded, sizeof(loaded), "%s%sloaded%d", mountpt, DIR_DELIM, drive);
   FILE *fs = fopen(loaded, "w");
   if (!fs) {
      /* unable to open loaded file for writing */
      return -1;
   }
   if (fprintf(fs, "%s\n", lab.GetLabel(vlabel, sizeof(vlabel))) < 1) {
      /* unable to write loaded file */
      fclose(fs);
      return -1;
   }
   fclose(fs);
   return 0;
}

/*
 * Return magazine bay containing the magazine with magazine number magnum
 */
int DiskChanger::MagNum2MagBay(int magnum)
{
   int n;
   if (magnum < 1) return -1;
   for (n = 1; n <= magazine_bays; n++) {
      if (magazine[n].mag_number == magnum) {
         return n;
      }
   }
   return -1;
}

/*
 *  Return virtual slot number, given magazine number and magazine slot
 */
int DiskChanger::MagSlot2VirtualSlot(int magnum, int magslot)
{
   int vslot, ndx = MagNum2MagBay(magnum);
   if (ndx < 1) {
      return -1;
   }
   vslot = ((ndx - 1) * slots_per_mag) + magslot;
   return vslot;
}

/*
 *  Translates a virtual slot number in 'slot' to a magazine bay and magazine slot.
 *  The magazine slot is returned, and 'mag_bay' set to the magazine bay.
 */
int DiskChanger::VirtualSlot2MagSlot(int slot, int &mag_bay)
{
   mag_bay = ((slot - 1) / slots_per_mag) + 1;
   return ((slot - 1) % slots_per_mag) + 1;
}

/*
 *  Translate virtual slot number in 'slot' to volume label struct in 'lab'.
 *  Returns zero on success.
 */
int DiskChanger::VirtualSlot2VolumeLabel(int slot, VolumeLabel &lab)
{
   int mag_bay, mag_slot;
   char vname[256];
   mag_slot = VirtualSlot2MagSlot(slot, mag_bay);
   snprintf(vname, sizeof(vname), "%s_%04d_%04d", changer_name, magazine[mag_bay].mag_number, mag_slot);
   return lab.set(vname);
}

/*
 *  Translate volume label struct in 'lab' to virtual slot number.
 *  Returns virtual slot on success, else negative number on error.
 */
int DiskChanger::VolumeLabel2VirtualSlot(const VolumeLabel &lab)
{
   if (lab.mag_number < 1) {
      return -1;
   }
   return MagSlot2VirtualSlot(lab.mag_number, lab.mag_slot);
}

/*
 *  Do actual loading of drive at the magazine level by renaming volume file on the
 *  magazine to driveN and writing loaded volume name to loadedN file
 */
int DiskChanger::DoLoad(int ndx, int drv, VolumeLabel &lab)
{
   char dpath[PATH_MAX], vpath[PATH_MAX], vlabel[256];

   if (lab.empty()) {
      return -1;  /* attempted to load unset volume label */
   }
   lab.GetLabel(vlabel, sizeof(vlabel));
   snprintf(vpath, sizeof(vpath), "%s%s%s", magazine[ndx].mountpoint,
         DIR_DELIM, vlabel);
   snprintf(dpath, sizeof(dpath), "%s%sdrive%d", magazine[ndx].mountpoint,
         DIR_DELIM, drv);
   if (access(dpath, F_OK) == 0) {
      /* driveN file already exists */
      return -1;
   }
   /* Rename volume file to driveN */
   if (rename(vpath, dpath)) {
      /* renaming volume file failed */
      return -1;
   }
   /* Write magazine loadedN file */
   if (WriteMagazineLoaded(magazine[ndx].mountpoint, drv, lab)) {
      /* failed to write loadedN file */
      rename(dpath, vpath);
      return -1;
   }
   return 0;
}

/*
 *  Do actual unloading of drive at the magazine level by renaming driveN file on the
 *  magazine to volume filename and clearing loadedN file
 */
int DiskChanger::DoUnload(int ndx, int drv, VolumeLabel &lab)
{
   VolumeLabel vempty;
   char dpath[PATH_MAX], vpath[PATH_MAX], vlabel[256];

   if (lab.empty()) {
      return -1;  /* attempted to unload drive to unset volume slot */
   }
   lab.GetLabel(vlabel, sizeof(vlabel));
   snprintf(vpath, sizeof(vpath), "%s%s%s", magazine[ndx].mountpoint,
         DIR_DELIM, vlabel);
   snprintf(dpath, sizeof(dpath), "%s%sdrive%d", magazine[ndx].mountpoint,
         DIR_DELIM, drv);
   /* Rename driveN file to volume filename */
   if (rename(dpath, vpath)) {
      return -1;  /* failed to rename driveN file */
   }
   /* clear loadedN file */
   if (WriteMagazineLoaded(magazine[ndx].mountpoint, drv, vempty)) {
      /* failed to clear loadedN file */
      rename(vpath, dpath);
      return -1;
   }
   return 0;
}

/*
 *  Create symlink 'statedir/N' pointing to mountpoint of magazine
 */
int DiskChanger::SetDriveSymlink(int drv, const char *mountpoint)
{
   int rc;
   char sname[PATH_MAX];
   snprintf(sname, sizeof(sname), "%s%s%d", work_dir, DIR_DELIM, drv);
   rc = symlink(mountpoint, sname);
   if (rc) {
      print_stderr("junct err %u, %s\n", junct_err, junct_msg);
   }
   return rc;
}

/*
 *  Remove symlink 'statedir/N', where N is drive number
 */
int DiskChanger::RemoveDriveSymlink(int drv)
{
   char sname[PATH_MAX];
   snprintf(sname, sizeof(sname), "%s%s%d", work_dir, DIR_DELIM, drv);
   return DeleteSymlink(sname);
}
