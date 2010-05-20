/* diskchanger.cpp
 *
 *  This file is part of vchanger by Josh Fisher.
 *
 *  vchanger copyright (C) 2008-2010 Josh Fisher
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

#ifdef HAVE_WINDOWS_H
char junct_msg[4096];
unsigned int junct_err;
#endif


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
   result = (pid_t) strtol(tmp, NULL, 10);
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
 *  The lock will be released when the DiskChanger object is destroyed, or by directly
 *  calling the Unlock() member function.
 *------------------------------------------------*/
int DiskChanger::Initialize(VchangerConfig &conf)
{
   int n, i, rc, bay;
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
   if (!Lock()) {
      lasterr = -1;
      snprintf(lasterrmsg, sizeof(lasterrmsg), "failed to obtain lock for changer %s",
            changer_name);
      vlog.Error(lasterrmsg);
      return lasterr;
   }
   /* Restore magazine bays from last used state */
   used = (char*) malloc((conf.known_magazines + 1));
   memset(used, 0, conf.known_magazines + 1);
   for (n = 1; n <= magazine_bays; n++) {
      magazine[n].bay_ndx = n;
      if (magazine[n].restore()) {
         lasterr = -1;
         snprintf(lasterrmsg, sizeof(lasterrmsg), "system error reading bay%d file", n);
         vlog.Error(lasterrmsg);
         return lasterr;
      }
      if (!strlen(magazine[n].mag_name)) {
         vlog.Info("Bay %d was previously unloaded", n);
         continue; /* bay was unloaded */
      }
      /* Find last loaded magazine in the list of magazines assigned to this changer */
      for (i = 1; i <= conf.known_magazines; i++) {
         if (used[i]) continue;
         if (strcasecmp(magazine[n].mag_name, conf.magazine[i]) == 0) break;
      }
      if (i <= conf.known_magazines) {
         used[i] = '1';
         rc = magazine[n].set(conf.magazine[i], conf.automount_dir);
         if (rc == -3) {
            /* bay last had magazine that is no longer mounted */
            vlog.Info("Init: magazine %s previously loaded in bay %d is no longer mounted",
                        conf.magazine[i], n);
            magazine[n].clear();
         } else if (rc < 0) {
            /* there was an error "loading" the magazine into the bay */
            switch (rc) {
            case -5:
               vlog.Warning("Init: load magazine in bay %d failed, %s is corrupt", n, conf.magazine[i]);
               break;
            case -6:
               vlog.Warning("Init: permissions error loading magazine %s in bay %d", conf.magazine[i], n);
               break;
            default:
               vlog.Warning("Init: i/o error loading magazine %s in bay %d", conf.magazine[i], n);
            }
         } else {
            vlog.Info("Init: magazine %s still loaded in bay %d", magazine[n].mag_name, magazine[n].bay_ndx);
         }
      } else {
         /* bay was last loaded with a magazine that is no longer defined
          * as one of the magazines assigned to this changer */
         vlog.Info("Init: magazine %s previously loaded in bay %d is no longer defined for this changer",
                     conf.magazine[i], n);
         magazine[n].clear();
      }
   }

   /* For each unloaded bay, try to load one of the magazines assigned to this changer */
   for (n = 1; n <= magazine_bays; n++) {
      if (strlen(magazine[n].mag_name)) continue;  /* skip loaded bays */
      /* find a magazine for this bay */
      for (i = 1; i <= conf.known_magazines; i++) {
         if (used[i]) continue;
         used[i] = '1';
         rc = magazine[n].set(conf.magazine[i], conf.automount_dir);
         if (rc == -3) {
            vlog.Info("Init: magazine %s not mounted", conf.magazine[i]);
            continue; /* magazine not mounted */
         }
         if (rc < 0) {
            /* there was an error inserting the magazine into the bay */
            switch (rc) {
            case -5:
               vlog.Warning("Init: load magazine in bay %d failed, %s is corrupt", n, conf.magazine[i]);
               break;
            case -6:
               vlog.Warning("Init: permissions error loading magazine %s in bay %d", conf.magazine[i], n);
               break;
            default:
               vlog.Warning("Init: system error loading magazine %s in bay %d", conf.magazine[i], n);
            }
         } else {
            vlog.Info("Init: loaded magazine %d=%s in bay %d", magazine[n].mag_number,
                  magazine[n].mag_name, magazine[n].bay_ndx);
            break;
         }
      }
      if (i > conf.known_magazines) {
         /* no more magazines assigned to this changer, so leave remaining bays empty */
         break;
      }
   }
   free(used);
   /* confirm loaded magazines for debug purposes only */
   for (n = 1; n <= magazine_bays; n++) {
      vlog.Info("confirm bay %d loaded with mag %d = %s", magazine[n].bay_ndx, magazine[n].mag_number,
            magazine[n].mag_name);
   }

   /* Compare last known state of virtual drives. If virtual drive's symlink points
    * to a magazine that is no longer loaded in a bay (ie. mounted), then change drive
    * state to unloaded. If a virtual drive is loaded from a magazine that has a
    * different mountpoint than it previously did, then fix the symlink to point to
    * the correct mountpoint. For drives in the unloaded state, make sure their symlink
    * is deleted. */
   for (i = 0; i < virtual_drives; i++) {
      drive[i].drive = i;
      /* Restore drive's last state */
      if (drive[i].restore()) {
         lasterr = -1;
         snprintf(lasterrmsg, sizeof(lasterrmsg), "error reading state%d file", i);
         vlog.Error(lasterrmsg);
         return lasterr;
      }
      GetDriveSymlinkDir(i, sname, sizeof(sname));
      if (!drive[i].empty()) {
         /* If drive was previously loaded, then check that its symlink exists */
         vlog.Info("Init: drive %d last state was loaded (mag %d slot %d)",
                  i, drive[i].vol.mag_number, drive[i].vol.mag_slot);
         n = readlink(sname, tmp, sizeof(tmp));
         if (n < 1) {
            /* symlink is missing, so drive state should be unloaded */
            vlog.Warning("Init: symlink missing for drive %d. Setting virtual drive state to unloaded", i);
            if (drive[i].clear()) {
               /* failed to update stateN file */
               lasterr = -1;
               snprintf(lasterrmsg, sizeof(lasterrmsg), "i/o error writing virtual drive state%d file", i);
               vlog.Error(lasterrmsg);
               return lasterr;
            }
         } else {
            /* symlink exists, but may be dangling (ie. pointing to a no longer mounted magazine)  */
            tmp[n] = 0;
            n = MagNum2MagBay(drive[i].vol.mag_number);
            if (n < 1) {
               /* symlink is pointing to unmounted magazine, so set drive state unloaded */
               vlog.Info("Init: symlink for drive %d pointing to no longer mounted magazine. Setting"
                     " virtual drive state to unloaded.", i);
               if (RemoveDriveSymlink(i)) {
                  /* failed to unlink symlink */
                  lasterr = -1;
                  snprintf(lasterrmsg, sizeof(lasterrmsg), "failed to remove symlink for virtual drive %d",
                        i);
                  vlog.Error(lasterrmsg);
                  return lasterr;
               }
               if (drive[i].clear()) {
                  /* failed to update stateN file */
                  lasterr = -1;
                  snprintf(lasterrmsg, sizeof(lasterrmsg), "i/o error writing virtual drive state%d file", i);
                  vlog.Error(lasterrmsg);
                  return lasterr;
               }
            } else {
               /* Drive is loaded and its symlink points to a mounted magazine, but the
                * mountpoint could have changed. */
               if (strcmp(tmp, magazine[n].mountpoint)) {
                  /* Magazine mountpoint changed since drive was loaded */
                  vlog.Info("Init: symlink for drive %d previously pointed to %s, but magazine mountpoint"
                        " has changed to %s. Fixing symlink.", i, tmp, magazine[n].mountpoint);
                  if (RemoveDriveSymlink(i)) {
                     /* failed to unlink symlink */
                     lasterr = -1;
                     snprintf(lasterrmsg, sizeof(lasterrmsg),
                           "failed to remove symlink for virtual drive %d", i);
                     vlog.Error(lasterrmsg);
                     return lasterr;
                  }
                  if (SetDriveSymlink(i, magazine[n].mountpoint)) {
                     /* failed to create symlink */
                     lasterr = -1;
                     snprintf(lasterrmsg, sizeof(lasterrmsg),
                           "failed to create symlink for virtual drive %d", i);
                     vlog.Error(lasterrmsg);
                     return lasterr;
                  }
               }
            }
         }
      } else {
         /* Unloaded drives should not have a symlink  */
         vlog.Info("Init: drive %d last state was unloaded", i);
         n = readlink(sname, tmp, sizeof(tmp));
         if (n > 0) {
            /* remove symlink for unloaded drives */
            vlog.Info("Init: virtual drive %d is unloaded, so should not have symlink. Removing symlink.", i);
            if (RemoveDriveSymlink(i)) {
               /* failed to unlink symlink */
               lasterr = -1;
               snprintf(lasterrmsg, sizeof(lasterrmsg), "failed to remove symlink for virtual drive %d", i);
               vlog.Error(lasterrmsg);
               return lasterr;
            }
         }
      }
   }

   /* Compare drive states to actual magazine volume files. If a drive is loaded with a volume
    * file on a magazine, then make sure that no other magazines think one of their volume
    * files is loaded in that same drive. This can happen because the user has swapped magazines
    * and the new magazine partition had a volume loaded into that drive the last time it was
    * detached. In short, make sure the state of the virtual drives matches the currently
    * attached magazines. */
   for (i = 0; i < virtual_drives; i++) {
      bay = 0;
      if (!drive[i].empty()) {
         /* make sure magazine and drive state agree and no magazine in
          * another bay has a loadedN file for this drive
          */
         bay = MagNum2MagBay(drive[i].vol.mag_number);
         rc = ReadMagazineLoaded(magazine[bay].mountpoint, i, lab);
         if (rc) {
            lasterr = rc;
            if (rc == -2) {
               snprintf(lasterrmsg, sizeof(lasterrmsg),
                     "loaded%d file on magazine in bay %d contains an invalid volume label", i, bay);
            } else {
               snprintf(lasterrmsg, sizeof(lasterrmsg),
                     "i/o error reading loaded%d file on magazine in bay %d", i, bay);
            }
            vlog.Error(lasterrmsg);
            return lasterr;
         }
         if (drive[i].vol != lab) {
            /* Drive state is different from magazine state */
            if (!lab.empty()) {
               /* Magazine thinks a different slot is loaded in this drive */
               vlog.Info("Init: drive %d state has magazine %d slot %d loaded, but the magazine has"
                     " slot %d loaded. Renaming drive%d file on magazine.",
                        i, drive[i].vol.mag_number, drive[i].vol.mag_slot, lab.mag_slot, i);
               if (DoUnload(bay, i, lab)) {
                  /* magazine level unload failed */
                  lasterr = -1;
                  snprintf(lasterrmsg, sizeof(lasterrmsg),
                        "failed to unload drive %d into magazine bay %d slot %d", i, bay,
                        lab.mag_slot);
                  vlog.Error(lasterrmsg);
                  return lasterr;
               }
            } else {
               vlog.Info("Init: drive %d state has magazine %d slot %d loaded, but according to the magazine"
                     " the drive is not loaded", i, drive[i].vol.mag_number, drive[i].vol.mag_slot);
            }
            /* Magazine thinks drive is unloaded, so drive state is changed to match */
            vlog.Info("Init: changing drive %d state to unloaded to fix mismatch with state of magazine %d",
                  i, drive[i].vol.mag_number);
            if (RemoveDriveSymlink(i)) {
               /* failed to unlink symlink */
               lasterr = -1;
               snprintf(lasterrmsg, sizeof(lasterrmsg), "failed to remove symlink for drive %d", i);
               vlog.Error(lasterrmsg);
              return lasterr;
            }
            if (drive[i].clear()) {
               /* failed to update stateN file */
               lasterr = -1;
               snprintf(lasterrmsg, sizeof(lasterrmsg), "i/o error writing state%d file", i);
               vlog.Error(lasterrmsg);
               return lasterr;
            }
            bay = 0;  /* drive is now unloaded */
         } else {
            vlog.Info("Init: drive %d is loaded from virtual slot %d (magazine %d slot %d in bay %d)",
                     i, VolumeLabel2VirtualSlot(drive[i].vol), drive[i].vol.mag_number,
                     drive[i].vol.mag_slot, bay);
         }
      } else {
         vlog.Info("Info: drive %d is unloaded", i);
      }
      /* Magazines other than the one the drive is loaded from should
       * not have a slot loaded in this drive (ie. should not have a driveN file ) */
      for (n = 1; n <= magazine_bays; n++) {
         if (n == bay) continue; /* drive is loaded from magazine in this bay */
         rc = ReadMagazineLoaded(magazine[n].mountpoint, i, lab);
         if (!lab.empty()) {
            /* magazine should not have a slot loaded for this drive. This happens
             * when magazines are detached with drives still loaded and then are
             * later re-attached */
            vlog.Info("Info: magazine %d in bay %d should not have drive %d loaded. Fixing magazine.",
                     lab.mag_number, n, i);
            if (DoUnload(n, i, lab)) {
               /* magazine level unload failed */
               lasterr = -1;
               snprintf(lasterrmsg, sizeof(lasterrmsg),
                     "failed to unload drive %d into slot %d of magazine %d in bay %d",
                     i, lab.mag_slot, lab.mag_number, n);
               vlog.Error(lasterrmsg);
               return lasterr;
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
   int bay;

   if (!changer_lock) {
      lasterr = -1;
      snprintf(lasterrmsg, sizeof(lasterrmsg), "changer not initialized");
      vlog.Error(lasterrmsg);
      return lasterr;
   }
   lasterr = 0;
   if (drv < 0 || drv >= virtual_drives) {
      lasterr = -1;
      snprintf(lasterrmsg, sizeof(lasterrmsg), "drive number %d out of range", drv);
      vlog.Error(lasterrmsg);
      return lasterr;
   }
   if (slot < 1 || slot > slots) {
      lasterr = -1;
      snprintf(lasterrmsg, sizeof(lasterrmsg), "invalid slot number %d (max = %d)", slot, slots);
      vlog.Error(lasterrmsg);
      return lasterr;
   }
   /* Check if requested slot is already loaded into a drive */
   ldrv = GetSlotDrive(slot);
   if (ldrv == drv) {
      lslot = VirtualSlot2MagSlot(slot, bay);
      vlog.Info("Load: drive %d loaded from virtual slot %d (magazine %d slot %d)", drv, slot,
            magazine[bay].mag_number, lslot);
      return 0; /* requested slot already loaded in requested drive */
   }

   if (ldrv >= 0) {
      /* requested slot is already loaded in another drive */
      lasterr = -1;
      snprintf(lasterrmsg, sizeof(lasterrmsg), "slot %d already loaded in drive %d", slot, ldrv);
      vlog.Error(lasterrmsg);
      return lasterr;
   }
   /* Check if requested drive is already loaded */
   lslot = GetDriveSlot(drv);
   if (lslot > 0) {
      /* requested drive is already loaded from another slot */
      lasterr = -1;
      snprintf(lasterrmsg, sizeof(lasterrmsg), "drive %d already loaded from slot %d.", drv, lslot);
      vlog.Error(lasterrmsg);
      return lasterr;
   }
   /* Do magazine level load */
   if (VirtualSlot2VolumeLabel(slot, lab)) {
      /* failed converting slot number to label */
      lasterr = -1;
      snprintf(lasterrmsg, sizeof(lasterrmsg),
            "failed to convert slot %d to valid volume file name", slot);
      vlog.Error(lasterrmsg);
      return lasterr;
   }
   bay = MagNum2MagBay(lab.mag_number);
   if (bay < 1) {
      /* No magazine is mounted at the bay containing this slot */
      lasterr = -1;
      snprintf(lasterrmsg, sizeof(lasterrmsg),
            "magazine bay containing slot %d has no magazine mounted", slot);
      vlog.Error(lasterrmsg);
      return lasterr;
   }
   if (DoLoad(bay, drv, lab)) {
      /* Could not perform magazine level load (ie. renaming of volume file) */
      lasterr = -1;
      snprintf(lasterrmsg, sizeof(lasterrmsg),
            "failed to rename volume file while loading drive %d from slot %d", drv, slot);
      vlog.Error(lasterrmsg);
      return lasterr;
   }
   /* Create symlink to mounted magazine */
   if (SetDriveSymlink(drv, magazine[bay].mountpoint)) {
      /* Could not create symlink, so leave drive unloaded */
      DoUnload(bay, drv, lab);
      lasterr = -1;
      snprintf(lasterrmsg, sizeof(lasterrmsg), "could not create symlink for drive %d", drv);
      vlog.Error(lasterrmsg);
      return lasterr;
   }
   /* update drive state */
   if (drive[drv].set(lab)) {
      lasterr = -1;
      snprintf(lasterrmsg, sizeof(lasterrmsg), "i/o error writing file state%d", drv);
      vlog.Error(lasterrmsg);
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
   int bay;

   if (!changer_lock) {
      lasterr = -1;
      snprintf(lasterrmsg, sizeof(lasterrmsg), "changer not initialized");
      vlog.Error(lasterrmsg);
      return lasterr;
   }
   lasterr = 0;
   if (drv < 0 || drv >= virtual_drives) {
      lasterr = -1;
      snprintf(lasterrmsg, sizeof(lasterrmsg), "drive number %d out of range", drv);
      vlog.Error(lasterrmsg);
      return lasterr;
   }
   /* See if drive is loaded */
   if (drive[drv].empty()) {
      return 0; /* drive already unloaded */
   }
   /* Do magazine level unload */
   bay = MagNum2MagBay(drive[drv].vol.mag_number);
   if (bay > 0) {
      if (DoUnload(bay, drv, drive[drv].vol)) {
         lasterr = -1;
         snprintf(lasterrmsg, sizeof(lasterrmsg), "failed to unload drive %d", drv);
         vlog.Error(lasterrmsg);
         return lasterr;
      }
   }
   /* Remove drive's symlink */
   if (RemoveDriveSymlink(drv)) {
      lasterr = -1;
      snprintf(lasterrmsg, sizeof(lasterrmsg), "failed to remove symlink for drive %d", drv);
      vlog.Error(lasterrmsg);
      return lasterr;
   }
   /* update drive state */
   if (drive[drv].clear()) {
      lasterr = -1;
      snprintf(lasterrmsg, sizeof(lasterrmsg), "i/o error writing file state%d", drv);
      vlog.Error(lasterrmsg);
      return lasterr;
   }

   return 0;
}

/*-------------------------------------------------
 *  Method to get virtual slot curently loaded in virtual drive 'drv'.
 *  If virtual drive is loaded, sets lasterr to zero, and returns the
 *  virtual slot number. If unloaded, sets lasterr to zero and returns
 *  zero. On error, sets lasterr and returns negative value.
 *------------------------------------------------*/
int DiskChanger::GetDriveSlot(int drv)
{
   int vslot;
   if (!changer_lock) {
      lasterr = -1;
      snprintf(lasterrmsg, sizeof(lasterrmsg), "changer not initialized");
      vlog.Error(lasterrmsg);
      return lasterr;
   }
   lasterr = 0;
   if (drv >= virtual_drives) {
      lasterr = -1;
      snprintf(lasterrmsg, sizeof(lasterrmsg), "drive number %d out of range", drv);
      vlog.Error(lasterrmsg);
      return lasterr;
   }
   return VolumeLabel2VirtualSlot(drive[drv].vol);
}

/*-------------------------------------------------
 *  Method to get virtual drive that virtual slot 'slot'
 *  is loaded into. If slot is loaded into a drive, returns the
 *  drive the slot is loaded into. If slot is not loaded
 *  into aq drive, returns -1.On error, sets lasterr
 *  non-zero and returns -1.
 *------------------------------------------------*/
int DiskChanger::GetSlotDrive(int slot)
{
   VolumeLabel lab;
   int n;

   lasterr = 0;
   if (!changer_lock) {
      lasterr = -1;
      snprintf(lasterrmsg, sizeof(lasterrmsg), "changer not initialized");
      vlog.Error(lasterrmsg);
      return -1;
   }
   if (slot < 1 || slot > slots) {
      lasterr = -1;
      snprintf(lasterrmsg, sizeof(lasterrmsg), "invalid slot number %d", slot);
      vlog.Error(lasterrmsg);
      return -1;
   }
   VirtualSlot2VolumeLabel(slot, lab);
   for (n = 0; n < virtual_drives; n++) {
      if (!drive[n].empty() && drive[n].vol == lab) {
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
      vlog.Error(lasterrmsg);
      return lasterr;
   }
   lasterr = 0;
   if ((bay < 1) || (bay > magazine_bays)) {
      lasterr = -1;
      snprintf(lasterrmsg, sizeof(lasterrmsg), "magazine bay %d out of range", bay);
      vlog.Error(lasterrmsg);
      return lasterr;
   }

   /* Check for magazine not mounted */
   if (strlen(magazine[bay].mountpoint) == 0) {
      lasterr = -1;
      snprintf(lasterrmsg, sizeof(lasterrmsg), "magazine bay %d has no magazine mounted", bay);
      vlog.Error(lasterrmsg);
      return lasterr;
   }
   /* Check for existing magazine index file */
   if (magazine[bay].mag_number > 0) {
      lasterr = -1;
      snprintf(lasterrmsg, sizeof(lasterrmsg),
            "magazine in bay %d (%s) already initialized as magazine %d", bay,
            magazine[bay].mountpoint, magazine[bay].mag_number);
      vlog.Error(lasterrmsg);
      return lasterr;
   }
   /* Determine magazine number to assign new magazine */
   mag = magnum;
   if (mag < 1) {
      mag = ReadNextMagazineNumber();
      if (mag < 1) {
         lasterr = -1;
         snprintf(lasterrmsg, sizeof(lasterrmsg), "could not assign next magazine number");
         vlog.Error(lasterrmsg);
         return lasterr;
      }
   }
   /* Create new magazine's index file */
   if (WriteMagazineIndex(magazine[bay].mountpoint, mag)) {
      lasterr = -1;
      snprintf(lasterrmsg, sizeof(lasterrmsg), "could not create index file on %s",
            magazine[bay].mountpoint);
      vlog.Error(lasterrmsg);
      return lasterr;
   }
   /* Create empty volumes on the magazine */
   old_mask = umask(MAG_VOLUME_MASK);
   for (n = 1; n <= slots_per_mag; n++) {
      lab.set(changer_name, mag, n);
      snprintf(fname, sizeof(fname), "%s%s%s", magazine[bay].mountpoint, DIR_DELIM, lab.GetLabel(
            vlabel, sizeof(vlabel)));
      fs = fopen(fname, "w");
      if (!fs) {
         lasterr = -1;
         snprintf(lasterrmsg, sizeof(lasterrmsg), "could not write magazine volume %s", fname);
         vlog.Error(lasterrmsg);
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
         snprintf(lasterrmsg, sizeof(lasterrmsg), "could not write loadedN file for drive %d", n);
         vlog.Error(lasterrmsg);
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
      if (n > 0 && n <= mag) magnum = -1;
   }
   if (magnum < 1) {
      if (WriteNextMagazineNumber(mag + 1)) {
         lasterr = -1;
         snprintf(lasterrmsg, sizeof(lasterrmsg), "could not update nextmag file");
         vlog.Error(lasterrmsg);
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
 *  one process at a time may execute changer commands on the
 *  same autochanger. If another process has the lock, then this process
 *  will sleep 1 second before trying again. This try/wait loop will continue
 *  until the lock is obtained or 'timeout' seconds have expired. If
 *  timeout = 0 then only tries to obtain lock once. If timeout < 0
 *  then doesn't return until the lock is obtained.
 *  On success, returns true. Otherwise on error or timeout, sets
 *  lasterr negative and returns false.
 *------------------------------------------------*/
bool DiskChanger::Lock(const long timeout)
{
   long timeoutms;
   struct timeval tv1, tv2;
   int rc;
   char lockfile[PATH_MAX];

   if (changer_lock) return true;
   timeoutms = timeout * 1000;
   snprintf(lockfile, sizeof(lockfile), "%s%s%s.lock", work_dir, DIR_DELIM, changer_name);
   gettimeofday(&tv1, NULL);
   rc = exclusive_fopen(lockfile, &changer_lock);
   if (timeoutms == 0 && rc != 0) return false;  /* timeout=0 means no wait */
   while (rc == EEXIST) {
      GetLockfileOwner(lockfile);
      /* sleep before trying again */
      millisleep(1000);
      if (timeoutms > 0) {
         /* check for timeout when timeout > 0 */
         gettimeofday(&tv2, NULL);
         if (timeval_et_ms(&tv1, &tv2) >= timeoutms) return false;
      }
      rc = exclusive_fopen(lockfile, &changer_lock);
   }
   if (rc) return false;  /* i/o error opening lock file */
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

   if (!changer_lock) return;
   snprintf(lockfile, sizeof(lockfile), "%s%s%s.lock", work_dir, DIR_DELIM, changer_name);
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
   if (magnum < 1) return 0;
   for (n = 1; n <= magazine_bays; n++) {
      if (magazine[n].mag_number == magnum) {
         return n;
      }
   }
   return 0;
}

/*
 *  Return virtual slot number, given magazine number and magazine slot
 */
int DiskChanger::MagSlot2VirtualSlot(int magnum, int magslot)
{
   int vslot, bay = MagNum2MagBay(magnum);
   if (bay < 1) {
      return 0;
   }
   vslot = ((bay - 1) * slots_per_mag) + magslot;
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
 *  Translate virtual slot number in 'slot' to volume label in 'lab'.
 *  Returns zero on success.
 */
int DiskChanger::VirtualSlot2VolumeLabel(int slot, VolumeLabel &lab)
{
   int mag_bay, mag_slot;
   char vname[256];
   mag_slot = VirtualSlot2MagSlot(slot, mag_bay);
   snprintf(vname, sizeof(vname), "%s_%04d_%04d", changer_name, magazine[mag_bay].mag_number,
         mag_slot);
   return lab.set(vname);
}

/*
 *  Translate volume label struct in 'lab' to virtual slot number.
 *  Returns virtual slot on success, else negative number on error.
 */
int DiskChanger::VolumeLabel2VirtualSlot(const VolumeLabel &lab)
{
   if (lab.mag_number < 1) {
      return 0;
   }
   return MagSlot2VirtualSlot(lab.mag_number, lab.mag_slot);
}

/*
 *  Do actual loading of drive 'drv' at the magazine level by renaming volume file 'lab'
 *  on the magazine in bay 'bay' to driveN and writing loaded volume name to loadedN file
 */
int DiskChanger::DoLoad(int bay, int drv, VolumeLabel &lab)
{
   char dpath[PATH_MAX], vpath[PATH_MAX], vlabel[256];

   if (lab.empty()) {
      return -1; /* attempted to load unset volume label */
   }
   lab.GetLabel(vlabel, sizeof(vlabel));
   snprintf(vpath, sizeof(vpath), "%s%s%s", magazine[bay].mountpoint, DIR_DELIM, vlabel);
   snprintf(dpath, sizeof(dpath), "%s%sdrive%d", magazine[bay].mountpoint, DIR_DELIM, drv);
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
   if (WriteMagazineLoaded(magazine[bay].mountpoint, drv, lab)) {
      /* failed to write loadedN file */
      rename(dpath, vpath);
      return -1;
   }
   return 0;
}

/*
 *  Do actual unloading of drive 'drv' at the magazine level by renaming driveN file on the
 *  magazine in bay 'bay' to the volume filename given by 'lab' and clearing loadedN file
 */
int DiskChanger::DoUnload(int bay, int drv, VolumeLabel &lab)
{
   VolumeLabel vempty;
   char dpath[PATH_MAX], vpath[PATH_MAX], vlabel[256];

   if (lab.empty()) {
      return -1; /* attempted to unload drive to unset volume slot */
   }
   lab.GetLabel(vlabel, sizeof(vlabel));
   snprintf(vpath, sizeof(vpath), "%s%s%s", magazine[bay].mountpoint, DIR_DELIM, vlabel);
   snprintf(dpath, sizeof(dpath), "%s%sdrive%d", magazine[bay].mountpoint, DIR_DELIM, drv);
   /* Rename driveN file to volume filename */
   if (rename(dpath, vpath)) {
      return -1; /* failed to rename driveN file */
   }
   /* clear loadedN file */
   if (WriteMagazineLoaded(magazine[bay].mountpoint, drv, vempty)) {
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
#ifdef HAVE_WINDOWS_H
   if (rc) {
      print_stderr("junct err %u, %s\n", junct_err, junct_msg);
   }
#endif
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
