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

#include "vchanger.h"
#include "util.h"
#include "changerstate.h"
#include "diskchanger.h"
#include "vchanger_common.h"
#include <sys/stat.h>
#include <unistd.h>

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
#ifdef HAVE_WIN32
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
   memset(state_dir, 0, sizeof(state_dir));
   memset(mag_number, 0, MAX_MAGAZINES * sizeof(int));
   memset(loaded, 0, MAX_MAGAZINES * sizeof(int));
   num_magazines = 0;
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
   int n, i, magnum, magslot;
   char chngr[128], sname[PATH_MAX];

   /* Set params from config file */
   strncpy(changer_name, conf.changer_name, sizeof(changer_name));
   strncpy(state_dir, conf.state_dir, sizeof(state_dir));
   num_magazines = conf.num_magazines;
   for (n = 1; n <= num_magazines; n++)
   {
      magazine[n].mag_number = -1;
      strncpy(magazine[n].mountpoint, conf.magazine[n], PATH_MAX);
   }
   slots_per_mag = conf.slots_per_mag;
   virtual_drives = conf.virtual_drives;
   slots = conf.slots;
   lasterr = 0;
   /* Make sure we have locked the changer */
   if (!changer_lock) {
      if (!Lock()) {
	 lasterr = -1;
	 snprintf(lasterrmsg, sizeof(lasterrmsg), "failed to obtain lock for changer %s",
	       		changer_name);
	 return lasterr;
      }
   }
   /* Read last state of virtual drives */
   for (n = 0; n < virtual_drives; n++)
   {
      drive[n].drive = n;
      strncpy(drive[n].statedir, state_dir, sizeof(drive[n].statedir));
      drive[n].restore();
   }
   /* Determine which of the magazines are currently mounted */
   for (n = 1; n <= num_magazines; n++)
   {
      magazine[n].mag_number = ReadMagazineIndex(magazine[n].mountpoint);
   }
   /* Any drives that were previously loaded from slots on a magazine that is no
    * longer mounted must be unloaded */
   for (i = 0; i < virtual_drives; i++)
   {
      if (drive[i].mag_number < 0) {
	 continue;
      }
      for (n = 1; n <= num_magazines && magazine[n].mag_number != drive[i].mag_number; n++);
      if (n <= num_magazines) {
	 continue;
      }
      RemoveDriveSymlink(i);
      drive[i].clear();
   }
   /* The magazines the drives are loaded from are mounted, but the magazines may be at a
    * different mountpoint than they were previously. If the magazines have been plugged
    * in and out, the OS has re-booted, etc., then udev could assigned them to different
    * device nodes than they were assigned when the drive state was last saved. So we
    * need to make sure that the drive symlinks point to the correct mountpoints. */
   for (i = 0; i < virtual_drives; i++)
   {
      if (drive[i].mag_number < 1) {
	 continue;
      }
      GetDriveSymlinkDir(i, sname, sizeof(sname));
      if (ReadMagazineIndex(sname) == drive[i].mag_number) {
	 continue;
      }
      /* "unload" drives with symlink pointing to wrong magazine */
      RemoveDriveSymlink(i);
      drive[i].mag_slot = -1;
      drive[i].mag_number = -1;
   }
   /* Now make sure the loaded slots on each magazine match the drive state */
   for (n = 1; n <= num_magazines; n++)
   {
      if (magazine[n].mag_number < 1) continue;   /* skip unmounted magazines */
      /* Check that any loaded slots on this magazine match the last known state
       * of the virtual drives */
      for (i = 0; i < virtual_drives; i++)
      {
	 /* Attempt to read a loadedN file for this drive on this magazine */
	 magslot = ReadMagazineLoaded(magazine[n].mountpoint, i, chngr, magnum);
	 if (drive[i].mag_number != magazine[n].mag_number) {
	    /* Drive was previously not loaded or was loaded from a different magazine,
	     * so this magazine should NOT have a slot loaded into this drive */
	    if (magslot < 1) {
	       continue; // ...and it doesn't, so its ok
	    }
	    /* ...or it does, so perhaps magazines have been changed out. To correct,
	     * manually "unload" on this magazine only, not touching the drive symlink */
	    if (DoUnload(n, i, magslot)) {
	       snprintf(lasterrmsg, sizeof(lasterrmsg), "Error correcting status of drive %d on %s",
		     	i, magazine[n].mountpoint);
	       lasterr = -1;
	       return -1;
	    }
	    continue;
	 }
	 /* This drive is loaded with a slot from this magazine, so make sure that
	  * the correct slot is actually "loaded" on the magazine. */
	 if (magslot == drive[i].mag_slot) {
	    continue; /* It is */
	 }
	 if (magslot < 1) {
	    /* On the magazine, no slot is loaded into this drive, so do a load
	     * operation to make the magazine match the drive state */
	    if (DoLoad(n, i, drive[i].mag_slot)) {
	       snprintf(lasterrmsg, sizeof(lasterrmsg),
		     	"Could not write loaded%d file on %s", i, magazine[n].mountpoint);
	       lasterr = -1;
	       return -1;
	    }
	 } else {
	    /* The slot actually loaded on the magazine is not the same slot that the
	     * drive state specifies. On the magazine, unload the drive and then load
	     * it with the correct slot */
	    DoUnload(n, i, magslot);
	    DoLoad(n, i, drive[i].mag_slot);
	 }
      }
   }

   /* update drive state */
   for (i = 0; i < virtual_drives; i++)
   {
      if (drive[i].mag_number > 0) {
	 drive[i].save();
      } else {
	 drive[i].clear();
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
   int ldrv, lslot;
   int mag_ndx, mag_slot;

   if (!changer_lock) {
      lasterr = -1;
      snprintf(lasterrmsg, sizeof(lasterrmsg), "changer not initialized");
      return lasterr;
   }
   lasterr = 0;
   if (drv < 0 || drv >= virtual_drives) {
      lasterr = -1;
      snprintf(lasterrmsg, sizeof(lasterrmsg), "drive number %d out of range", drv);
      return lasterr;
   }
   if (slot < 1 || slot > slots) {
      lasterr = -1;
      snprintf(lasterrmsg, sizeof(lasterrmsg), "invalid slot number %d max = %d", slot, slots);
      return lasterr;
   }
   /* Check if requested slot is already loaded into a drive */
   ldrv = GetSlotDrive(slot);
   if (ldrv == drv) {
      return 0;   /* requested slot already loaded in requested drive */
   }
   if (ldrv >= 0) {
      /* requested slot is loaded in another drive */
      lasterr = -1;
      snprintf(lasterrmsg, sizeof(lasterrmsg), "slot %d already loaded in drive %d", slot, ldrv);
      return lasterr;
   }
   /* Check if requested drive is already loaded */
   lslot = GetDriveSlot(drv);
   if (lslot > 0) {
      /* requested drive is already loaded from another slot */
      lasterr = -1;
      snprintf(lasterrmsg, sizeof(lasterrmsg), "drive %d already loaded from slot %d.", drv, lslot);
      return lasterr;
   }
   /* Do magazine level load */
   mag_slot = VirtualSlot2MagSlot(slot, mag_ndx);
   if (DoLoad(mag_ndx, drv, mag_slot)) {
      /* Could not perform load (renaming of volume file) */
      lasterr = -1;
      snprintf(lasterrmsg, sizeof(lasterrmsg), "failed to rename volume file while loading drive %d from slot %d",
	    	drv, slot);
      return lasterr;
   }
   /* Create symlink to mounted magazine */
   if (SetDriveSymlink(drv, magazine[mag_ndx].mountpoint)) {
      /* Could not create symlink, so leave drive unloaded */
      DoUnload(mag_ndx, drv, mag_slot);
      lasterr = -1;
      snprintf(lasterrmsg, sizeof(lasterrmsg), "could not create symlink for drive %d", drv);
      return lasterr;
   }
   /* update drive state */
   drive[drv].set(changer_name, magazine[mag_ndx].mag_number, mag_slot);
   drive[drv].save();

   return 0;
}


/*-------------------------------------------------
 *  Method to unload volume in virtual drive 'drv' into the virtual slot
 *  from whence it came. Returns lasterr. On success, sets lasterr = 0,
 *  otherwise sets lasterr to a negative error number.
 *------------------------------------------------*/
int DiskChanger::UnloadDrive(int drv)
{
   int lslot, mag_ndx, mag_slot;

   if (!changer_lock) {
      lasterr = -1;
      snprintf(lasterrmsg, sizeof(lasterrmsg), "changer not initialized");
      return lasterr;
   }
   lasterr = 0;
   if (drv < 0 || drv >= virtual_drives) {
      lasterr = -1;
      snprintf(lasterrmsg, sizeof(lasterrmsg), "drive number %d out of range", drv);
      return lasterr;
   }
   /* Get slot drive is currently loaded with */
   lslot = GetDriveSlot(drv);
   if (lslot < 1) {
      return 0;   /* drive already unloaded */
   }
   /* Do magazine level unload */
   mag_slot = VirtualSlot2MagSlot(lslot, mag_ndx);
   if (DoUnload(mag_ndx, drv, mag_slot)) {
      lasterr = -1;
      snprintf(lasterrmsg, sizeof(lasterrmsg), "failed to unload drive %d", drv);
      return lasterr;
   }
   /* Remove drive's symlink */
   if (RemoveDriveSymlink(drv)) {
      lasterr = -1;
      snprintf(lasterrmsg, sizeof(lasterrmsg), "failed to remove symlink for drive %d", drv);
      return lasterr;
   }
   /* update drive state */
   drive[drv].clear();

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
      snprintf(lasterrmsg, sizeof(lasterrmsg), "drive number %d out of range", drv);
      return -1;
   }
   vslot = MagSlot2VirtualSlot(drive[drv].mag_number, drive[drv].mag_slot);
   if (vslot < 1) {
      return 0;
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
   int n, magndx, magslot;

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
   magslot = VirtualSlot2MagSlot(slot, magndx);
   for (n = 0; n < virtual_drives; n++)
   {
      if (drive[n].mag_number == magazine[magndx].mag_number
	    		&& drive[n].mag_slot == magslot) {
	 return n;
      }
   }
   return -1;
}


/*
 *  Create new magazine for magazine index mndx. If magnum < 0 then assign a
 *  new magazine number, else use given magazine number. If magazine already
 *  exists or on error, return negative value. On success, return magazine
 *  number assigned.
 */
int DiskChanger::CreateMagazine(int mndx, int magnum)
{
   int mag, n;
   mode_t old_mask;
   char vlabel[PATH_MAX], fname[PATH_MAX];
   FILE *fs;

   if (!changer_lock) {
      lasterr = -1;
      snprintf(lasterrmsg, sizeof(lasterrmsg), "changer not initialized");
      return lasterr;
   }
   lasterr = 0;
   if ((mndx < 1) || (mndx > num_magazines)) {
      snprintf(lasterrmsg, sizeof(lasterrmsg), "magazine index %d out of range", mndx);
      lasterr = -1;
      return lasterr;
   }
   /* Check for existing magazine index file */
   mag = ReadMagazineIndex(magazine[mndx].mountpoint);
   if (mag > 0) {
      lasterr = -1;
      snprintf(lasterrmsg, sizeof(lasterrmsg), "magazine at index %d (%s) already initialized as magazine %d",
	    	mndx, magazine[mndx].mountpoint, mag);
      return lasterr;
   }
   /* Determine magazine number to assign new magazine */
   mag = magnum;
   if (mag < 1) {
      mag = ReadNextMagazineNumber();
      if (mag < 1) {
	 lasterr = -1;
	 snprintf(lasterrmsg, sizeof(lasterrmsg), "could not assign next magazine number");
	 return lasterr;
      }
   }
   /* Create new magazine's index file */
   if (WriteMagazineIndex(magazine[mndx].mountpoint, mag)) {
      lasterr = -1;
      snprintf(lasterrmsg, sizeof(lasterrmsg), "could not create index file on %s", magazine[mndx].mountpoint);
      return lasterr;
   }
   /* Create empty volumes on the magazine */
   old_mask = umask(MAG_VOLUME_MASK);
   for (n = 1; n <= slots_per_mag; n++)
   {
      BuildVolumeName(vlabel, sizeof(vlabel),changer_name, mag, n);
      snprintf(fname, sizeof(fname), "%s%s%s", magazine[mndx].mountpoint, DIR_DELIM, vlabel);
      fs = fopen(fname, "w");
      if (!fs) {
	 lasterr = -1;
	 snprintf(lasterrmsg, sizeof(lasterrmsg), "could not write magazine volume %s", fname);
	 return lasterr;
      }
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
	 snprintf(lasterrmsg, sizeof(lasterrmsg), "could not update nextmag file");
	 return lasterr;
      }
   }
   /* update magazine mounted state */
   magazine[mndx].mag_number = mag;
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
   snprintf(lockfile, sizeof(lockfile), "%s%s%s.lock", state_dir, DIR_DELIM, changer_name);
   gettimeofday(&tv1, NULL);
   rc = exclusive_fopen(lockfile, &changer_lock);
   while (rc == EEXIST)
   {
      GetLockfileOwner(lockfile);
      /* sleep before trying again */
      millisleep(100);
      gettimeofday(&tv2, NULL);
      if (timeval_et_ms(&tv1, &tv2) >= timeout) return false;
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
   snprintf(lockfile, sizeof(lockfile), "%s%s%s.lock", state_dir, DIR_DELIM, changer_name);
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
   strncpy(state_dir, DEFAULT_STATE_DIR, sizeof(state_dir));
   virtual_drives = DEFAULT_VIRTUAL_DRIVES;
   slots_per_mag = DEFAULT_SLOTS_PER_MAG;
   slots = num_magazines * slots_per_mag;
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
   snprintf(lnkdir, lnkdir_size, "%s%s%d", state_dir, DIR_DELIM, drv);
   return 0;
}


/*
 *  Returns the next available magazine index number from the nextmag file.
 *  Returns negative value on error.
 */
int DiskChanger::ReadNextMagazineNumber()
{
   int mag;
   FILE *fs;
   char *gstr;
   char buf[128], nmag[PATH_MAX];

   /* Get nextmag path */
   snprintf(nmag, sizeof(nmag), "%s%snextmag", state_dir, DIR_DELIM);
   fs = fopen(nmag, "r");
   if (!fs) {
      return 1;
   }
   /* Read the nextmag file */
   gstr = fgets(buf, sizeof(buf), fs);
   fclose(fs);
   if (!gstr) {
      return -1;  /* i/o error reading nextmag */
   }
   mag = strtol(buf, NULL, 10);
   if (mag < 1) {
      mag = 1;
   }
   return mag;
}


/*
 *  Writes the next available magazine index number in 'magnum to the
 *  nextmag file. Returns zero on success or negative value on error.
 */
int DiskChanger::WriteNextMagazineNumber(int magnum)
{
   FILE *fs;
   char nmag[PATH_MAX];

   /* Get nextmag path */
   snprintf(nmag, sizeof(nmag), "%s%snextmag", state_dir, DIR_DELIM);
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
 *  Reads the file named 'index' in the directory 'mountpoint'. If index file exists and
 *  the magazine belongs to this changer, returns the magazine number. Otherwise returns
 *  a negative value.
 */
int DiskChanger::ReadMagazineIndex(const char *mountpoint)
{
   int n, rc;
   char fname[PATH_MAX], buff[256];
   snprintf(fname, sizeof(fname), "%s%sindex", mountpoint, DIR_DELIM);
   FILE *fs = fopen(fname, "r");
   if (!fs) {
      return -1;
   }
   if (!fgets(buff, sizeof(buff), fs)) {
      /* i/o error reading index file */
      fclose(fs);
      return -1;
   }
   fclose(fs);
   n = strip_whitespace(buff, sizeof(buff));
   if (!n || n > 254) {
      return -1; /* invalid index string */
   }
   /* Find changer name */
   for (n = 0; n < sizeof(buff) && buff[n] && buff[n] != '_'; n++);
   if (buff[n] != '_') {
      return -1; /* invalid index string */
   }
   buff[n] = 0;
   if (strcasecmp(buff, changer_name)) {
      return -1; /* magazine belongs to another changer */
   }
   rc = strtol(buff + (n+1), NULL, 10);
   if (rc < 1) {
      rc = -1;  /* garbled index file contents or not a magazine */
   }
   return rc;
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
 *  Reads the magazine 'loadedN' file for drive N. If loadedN file exists on the magazine,
 *  return the magazine slot number. Otherwise returns negative value.
 */
int DiskChanger::ReadMagazineLoaded(const char *mountpt, int drive,
					char *chgr_name, int &mag_num)
{
   int n, mag_slot;
   char loaded[PATH_MAX], buff[256];
   snprintf(loaded, sizeof(loaded), "%s%sloaded%d", mountpt, DIR_DELIM, drive);
   chgr_name[0] = 0;
   mag_num = 0;
   FILE *fs = fopen(loaded, "r");
   if (!fs) {
      return -1;
   }
   if (!fgets(buff, sizeof(buff), fs)) {
      fclose(fs);
      return -1;
   }
   fclose(fs);
   n = strip_whitespace(buff, sizeof(buff));
   if (ParseVolumeName(buff, chgr_name, sizeof(chgr_name), mag_num, mag_slot)) {
      return -1;
   }
   return mag_slot;
}


/*
 *  Writes 'magslot' to the magazine 'loaded' file for drive 'drive'.
 *  On success, returns zero. Otherwise returns negative value.
 */
int DiskChanger::WriteMagazineLoaded(const char *mountpt, int drive, int magslot)
{
   char loaded[PATH_MAX];
   snprintf(loaded, sizeof(loaded), "%s%sloaded%d", mountpt, DIR_DELIM, drive);
   FILE *fs = fopen(loaded, "w");
   if (!fs) {
      return -1;
   }
   if (fprintf(fs, "%d", magslot) < 0) {
      fclose(fs);
      return -1;
   }
   fclose(fs);
   return 0;
}


/*
 * Return magazine index of magazine with magazine number magnum
 */
int DiskChanger::MagNum2MagIndex(int magnum)
{
   int n;
   for (n = 1; n <= num_magazines; n++)
   {
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
   int vslot, ndx = MagNum2MagIndex(magnum);
   if (ndx < 1) {
      return -1;
   }
   vslot = ((ndx - 1) * slots_per_mag) + magslot;
   return vslot;
}


/*
 *  Return magazine slot number given virtual slot number. The returned
 *  magazine slot number is relative to the magazine continaing it, so the
 *  magazine index (not the magzine number) is returned in 'mag_ndx'.
 */
int DiskChanger::VirtualSlot2MagSlot(int slot, int &mag_ndx)
{
   mag_ndx = ((slot - 1) / slots_per_mag) + 1;
   return ((slot - 1) % slots_per_mag) + 1;
}


/*
 *  Do actual loading of drive by renaming volume file on the magazine to driveN
 *  and creating loadedN file
 */
int DiskChanger::DoLoad(int ndx, int drv, int magslot)
{
   FILE *fs;
   char dname[PATH_MAX], vname[PATH_MAX], lname[PATH_MAX], vlabel[256];

   BuildVolumeName(vlabel, sizeof(vlabel), changer_name, magazine[ndx].mag_number, magslot);
   snprintf(vname, sizeof(vname), "%s%s%s", magazine[ndx].mountpoint, DIR_DELIM, vlabel);
   snprintf(dname, sizeof(dname), "%s%sdrive%d", magazine[ndx].mountpoint, DIR_DELIM, drv);
   snprintf(lname, sizeof(lname), "%s%sloaded%d", magazine[ndx].mountpoint, DIR_DELIM, drv);
   fs = fopen(lname, "w");
   if (!fs) {
      return -1;
   }
   if (fputs(vlabel, fs) < 0) {
      fclose(fs);
      unlink(lname);
      return -1;
   }
   fclose(fs);
   if (rename(vname, dname)) {
      unlink (lname);
      return -1;
   }
   return 0;
}


/*
 *  Do actual unloading of drive by renaming driveN file on the magazine to volume filename
 *  and deleting loadedN file
 */
int DiskChanger::DoUnload(int ndx, int drv, int magslot)
{
   char dname[PATH_MAX], vname[PATH_MAX], lname[PATH_MAX], vlabel[256];

   BuildVolumeName(vlabel, sizeof(vlabel), changer_name, magazine[ndx].mag_number, magslot);
   snprintf(vname, sizeof(vname), "%s%s%s", magazine[ndx].mountpoint, DIR_DELIM, vlabel);
   snprintf(dname, sizeof(dname), "%s%sdrive%d", magazine[ndx].mountpoint, DIR_DELIM, drv);
   snprintf(lname, sizeof(lname), "%s%sloaded%d", magazine[ndx].mountpoint, DIR_DELIM, drv);
   if (rename(dname, vname)) {
      return -1;
   }
   unlink(lname);
   return 0;
}


/*
 *  Create symlink 'statedir/N' pointing to mountpoint of magazine
 */
int DiskChanger::SetDriveSymlink(int drv, const char *mountpoint)
{
   int rc;
   char sname[PATH_MAX];
   snprintf(sname, sizeof(sname), "%s%s%d", state_dir, DIR_DELIM, drv);
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
   snprintf(sname, sizeof(sname), "%s%s%d", state_dir, DIR_DELIM, drv);
   return DeleteSymlink(sname);
}
