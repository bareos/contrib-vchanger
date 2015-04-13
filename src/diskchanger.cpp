/* diskchanger.cpp
 *
 *  This file is part of vchanger by Josh Fisher.
 *
 *  vchanger copyright (C) 2008-2015 Josh Fisher
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
#include "compat_defs.h"
#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "compat/gettimeofday.h"
#include "compat/readlink.h"
#include "compat/sleep.h"
#include "compat/symlink.h"
#include "util.h"
#include "loghandler.h"
#include "bconsole.h"
#include "diskchanger.h"


/*=================================================
 *  Class DiskChanger
 *=================================================*/

/*-------------------------------------------------
 * destructor
 *-------------------------------------------------*/
DiskChanger::~DiskChanger()
{
   Unlock();
}


/*-------------------------------------------------
 *  Protected method to read previous state of magazine bays.
 *  Returns zero on success, else negative and sets lasterr
 *-------------------------------------------------*/
void DiskChanger::InitializeMagazines()
{
   int n;
   MagazineState m;

   magazine.clear();
   for (n = 0; (size_t)n < conf.magazine.size(); n++) {
      m.SetBay(n, conf.magazine[n].c_str());
      m.prev_num_slots = 0;
      m.prev_start_slot = 0;
      magazine.push_back(m);
      /* Restore previous slot count and starting virtual slot */
      magazine[n].restore();
      /* Get mountpoint and build magazine slot array  */
      magazine[n].Mount();
   }
}


/*-------------------------------------------------
 *  Protected method to find the start of an empty range of
 *  'count' virtual slots, adding slots if needed.
 *  Returns the starting slot number of the range found.
 *------------------------------------------------*/
int DiskChanger::FindEmptySlotRange(int count)
{
   VirtualSlot vs;
   int start = 0, n = 1, found = 0;
   /* Find next empty slot */
   while (n < (int)vslot.size() && vslot[n].mag_bay >= 0) n++;
   start = n;
   while (n < (int)vslot.size() && found < count) {
      if (vslot[n].mag_bay < 0) {
         ++found;
         ++n;
      } else {
         found = 0;
         while (n < (int)vslot.size() && vslot[n].mag_bay >= 0) n++;
         start = n;
      }
   }
   if (found >= count) return start;
   while (found < count) {
      vs.vs = n++;
      vslot.push_back(vs);
      ++found;
   }
   return start;
}

/*-------------------------------------------------
 *  Protected method to initialize array of virtual slot and
 *  assign magazine volumes to virtual slots. When possible,
 *  volumes are assigned to the same slot they were in
 *  previously.
 *------------------------------------------------*/
void DiskChanger::InitializeVirtSlots()
{
   int s, m, v, last;
   VirtualSlot vs;
   bool found;

   /* Create all known slots as initially empty */
   vslot.clear();
   for (s = 0; s <= dconf.max_slot; s++) {
      vs.vs = s;
      vslot.push_back(vs);
   }
   /* Re-create virtual slots that existed previously if possible */
   for (m = 0; m < (int)magazine.size(); m++) {
      /* Create slots if needed to match max slot used by previous magazines */
      last = magazine[m].prev_start_slot + magazine[m].prev_num_slots;
      if (last >= (int)vslot.size()) {
         vs.clear();
         while ((int)vslot.size() <= last) {
            vs.vs = (int)vslot.size();
            vslot.push_back(vs);
         }
      }
      /* Check this magazine's slots */
      if (magazine[m].empty()) {
         log.Info("magazine %d is not mounted", m);
         /* magazine is not currently mounted, so will have no slots assigned */
         if (magazine[m].prev_start_slot) {
            /* Since it was previously mounted, an 'update slots' is needed */
            log.Warning("update slots needed. magazine %d no longer mounted; previous: %d volumes in slots %d-%d", m,
                  magazine[m].prev_num_slots, magazine[m].prev_start_slot,
                  magazine[m].prev_start_slot + magazine[m].prev_num_slots - 1);
            needs_update = true;
         }
         continue;
      }
      /* Magazine is currently mounted, so check for change in slot assignment */
      log.Info("magazine %d has %d volumes on %s", m, magazine[m].num_slots,
                  magazine[m].mountpoint.c_str());
      if (magazine[m].num_slots != magazine[m].prev_num_slots) {
         /* Number of volumes has changed or magazine was not previously mounted, so
          * needs new slot assignment and also 'update slots' will be needed */
         log.Warning("update slots needed. magazine %d has %d volumes, previously had %d", m,
                  magazine[m].num_slots, magazine[m].prev_num_slots);
         needs_update = true;
         continue;
      }
      if (magazine[m].num_slots == 0) {
         /* Magazine has no volumes so needs no slot assignment */
         continue;
      }
      /* Magazine is mounted, was previously mounted, and has the same volume count,
       * so attempt to assign to the same slots previously assigned */
      found = false;
      for (v = magazine[m].prev_start_slot; v < (int)vslot.size(); v++) {
         if (!vslot[v].empty()) {
            found = true;
            break;
         }
      }
      if (found) {
         /* Slot used previously has already been assigned to another magazine.
          * Magazine will need to be assigned a new slot range, so an
          * 'update slots' will also be needed. */
         log.Warning("update slots needed. magazine %d previous slots %d-%d are not available", m,
                  magazine[m].prev_start_slot, magazine[m].prev_start_slot + magazine[m].prev_num_slots - 1);
         needs_update = true;
         continue;
      }
      /* Assign this magazine's volumes to the same slots as previously assigned */
      magazine[m].start_slot = magazine[m].prev_start_slot;
      for (s = 0; s < magazine[m].num_slots; s++) {
         v = magazine[m].start_slot + s;
         vslot[v].mag_bay = m;
         vslot[v].mag_slot = s;
      }
      log.Notice("%d volumes on magazine %d assigned slots %d-%d", magazine[m].num_slots, m,
            magazine[m].start_slot, magazine[m].start_slot + magazine[m].num_slots - 1);
   }

   /* Assign slots to mounted magazines that have not already been assigned. */
   for (m = 0; m < (int)magazine.size(); m++) {
      if (magazine[m].empty() || magazine[m].start_slot > 0) continue;
      if (magazine[m].num_slots == 0) continue;
      magazine[m].start_slot = FindEmptySlotRange(magazine[m].num_slots);
      for (s = 0; s < magazine[m].num_slots; s++) {
         v = magazine[m].start_slot + s;
         vslot[v].mag_bay = m;
         vslot[v].mag_slot = s;
      }
      log.Notice("%d volumes on magazine %d assigned slots %d-%d", magazine[m].num_slots, m,
            magazine[m].start_slot, magazine[m].start_slot + magazine[m].num_slots - 1);
   }

   /* Save updated state of magazines */
   for (m = 0; m < (int)magazine.size(); m++) {
      magazine[m].save();
   }
   /* Update dynamic configuration info */
   if ((int)vslot.size() >= dconf.max_slot) {
      dconf.max_slot = (int)vslot.size() - 1;
      dconf.save();
   }
}


/*-------------------------------------------------
 *  Protected method to initialize state of virtual drives.
 *  On success, returns zero. On error, returns non-zero.
 *------------------------------------------------*/
int DiskChanger::InitializeDrives()
{
   int n, rc, max_drive = -1;
   DIR *d;
   struct dirent *de;
   DriveState ds;
   tString tmp;

   /* For each drive for which a state file exists. try to restore its state */
   d = opendir(conf.work_dir.c_str());
   if (!d) {
      rc = errno;
      verr.SetErrorWithErrno(rc, "error %d accessing work directory", rc);
      log.Error("ERROR! %s", verr.GetErrorMsg());
      return rc;
   }
   de = readdir(d);
   while (de) {
      /* Find next drive state file */
      tmp = de->d_name;
      if (tmp.find("drive_state-") == 0) {
         tmp.erase(0, 12);
         if (tmp.find_first_of("0123456789") == tString::npos) {
            de = readdir(d);
            continue;
         }
         if (tmp.find_first_not_of("0123456789") != tString::npos) {
            de = readdir(d);
            continue;
         }
         n = (int)strtol(tmp.c_str(), NULL, 10);
         if (n > max_drive) max_drive = n;
      }
      de = readdir(d);
   }
   closedir(d);
   if (max_drive < 0) {
      /* No drive state files exist, so create at least one drive */
      max_drive = 0;
   }

   /* Restore last known state of virtual drives where possible.  */
   for (n = 0; n <= max_drive; n++) {
      ds.drv = n;
      drive.push_back(ds);
      /* Attempt to restore drive's last state */
      if (RestoreDriveState(n)) {
         log.Error("ERROR! %s", verr.GetErrorMsg());
      }
   }
   return 0;
}


/*-------------------------------------------------
 *  Protected method to ensure that there are at least n drives
 *------------------------------------------------*/
void DiskChanger::SetMaxDrive(int need)
{
   int n = (int)drive.size();
   DriveState ds;
   while (n <= need) {
      ds.drv = n++;
      drive.push_back(ds);
   }
}

/*
 *  Method to create symlink for drive pointing to currently loaded volume file
 */
int DiskChanger::CreateDriveSymlink(int drv)
{
   int mag, mslot, rc;
   tString sname, fname;
   char lname[4096];

   if (drv < 0 || drv >= (int)drive.size()) {
      verr.SetError(EINVAL, "cannot create symlink for invalid drive %d", drv);
      return EINVAL;
   }
   if (drive[drv].vs <= 0) {
      verr.SetError(ENOENT, "cannot create symlink for unloaded drive %d", drv);
      return ENOENT;
   }
   mag = vslot[drive[drv].vs].mag_bay;
   mslot = vslot[drive[drv].vs].mag_slot;
   fname = magazine[mag].GetVolumePath(mslot);
   if (fname.empty()) {
      verr.SetError(ENOENT, "cannot create symlink for unloaded drive %d", drv);
      return ENOENT;
   }
   tFormat(sname, "%s%s%d", conf.work_dir.c_str(), DIR_DELIM, drv);
   rc = readlink(sname.c_str(), lname, sizeof(lname));
   if (rc > 0) {
      if (rc >= (int)sizeof(lname)) {
         verr.SetError(ENAMETOOLONG, "symlink target too long on readlink for drive %d", drv);
         return ENAMETOOLONG;
      }
      lname[rc] = 0;
      if (fname == lname) {
         /* symlink already exists */
         log.Info("found symlink for drive %d -> %s", drv, fname.c_str());
         return 0;
      }
      /* Symlink points to wrong mountpoint, so delete and re-create */
      if (RemoveDriveSymlink(drv)) return EEXIST;
   }
   if (symlink(fname.c_str(), sname.c_str())) {
      rc = errno;
      verr.SetErrorWithErrno(rc, "error %d creating symlink for drive %d", rc, drv);
      return rc;
   }
   log.Notice("created symlink for drive %d -> %s", drv, fname.c_str());
   return 0;
}

/*-------------------------------------------------
 *  Method to delete this drive's symlink.
 *  On success returns zero, else on error sets lasterr and
 *  returns errno.
 *-------------------------------------------------*/
int DiskChanger::RemoveDriveSymlink(int drv)
{
   int rc;
   tString sname;

   if (drv < 0 || drv >= (int)drive.size()) {
      verr.SetError(EINVAL, "cannot delete symlink for invalid drive %d", drv);
      return EINVAL;
   }
   /* Remove symlink pointing to loaded volume file */
   tFormat(sname, "%s%s%d", conf.work_dir.c_str(), DIR_DELIM, drv);
   if (unlink(sname.c_str())) {
      if (errno == ENOENT) return 0;  /* Ignore if not found */
      /* System error preventing deletion of symlink */
      rc = errno;
      verr.SetErrorWithErrno(errno, "error %d deleting symlink for drive %d: ", rc, drv);
      return rc;
   }
   log.Notice("deleted symlink for drive %d", drv);
   return 0;
}


/*-------------------------------------------------
 *  Method to save current drive state, device string and
 *  volume label (filename), to a file in the work
 *  directory named "drive_state-N", where N is the drive number.
 *  On success returns zero, else on error sets lasterr and
 *  returns errno.
 *-------------------------------------------------*/
int DiskChanger::SaveDriveState(int drv)
{
   mode_t old_mask;
   FILE *FS;
   int rc, mag, mslot;
   tString sname;

   if (drv < 0 || drv >= (int)drive.size()) {
      verr.SetError(EINVAL, "cannot save state of invalid drive %d", drv);
      return EINVAL;
   }
   /* Delete old state file */
   tFormat(sname, "%s%sdrive_state-%d", conf.work_dir.c_str(), DIR_DELIM, drv);
   if (drive[drv].empty()) {
      if (access(sname.c_str(), F_OK) == 0) {
         log.Notice("deleted state file for drive %d", drv);
      }
      unlink(sname.c_str());
      return 0;
   }
   old_mask = umask(027);
   FS = fopen(sname.c_str(), "w");
   if (!FS) {
      /* Unable to open state file */
      rc = errno;
      umask(old_mask);
      verr.SetErrorWithErrno(rc, "failed opening state file for drive %d", drv);
      return rc;
   }
   mag = vslot[drive[drv].vs].mag_bay;
   mslot = vslot[drive[drv].vs].mag_slot;
   if (fprintf(FS, "%s,%s\n", magazine[mag].mag_dev.c_str(),
               magazine[mag].GetVolumeLabel(mslot)) < 0) {
      /* I/O error writing state file */
      rc = errno;
      fclose(FS);
      umask(old_mask);
      verr.SetErrorWithErrno(rc, "error %d writing state file for drive %d", rc, drv);
      return rc;
   }
   fclose(FS);
   umask(old_mask);
   log.Notice("wrote state file for drive %d", drv);
   return 0;
}


/*-------------------------------------------------
 *  Method to restore drive state from a file in the work directory
 *  named "drive_state-N", where N is drive_number. If the volume
 *  previously loaded is available, then restore drive to the loaded
 *  state, otherwise set drive unloaded and remove the symlink and
 *  state file for this drive.
 *  On success returns zero, else on error sets lasterr and
 *  returns errno.
 *-------------------------------------------------*/
int DiskChanger::RestoreDriveState(int drv)
{
   int rc, v, m, ms;
   tString line, dev, labl;
   size_t p;
   struct stat st;
   FILE *FS;
   tString sname;

   if (drv < 0 || drv >= (int)drive.size()) {
      verr.SetError(EINVAL, "cannot restore state of invalid drive %d", drv);
      return EINVAL;
   }
   drive[drv].clear();

   /* Check for existing state file */
   tFormat(sname, "%s%sdrive_state-%d", conf.work_dir.c_str(), DIR_DELIM, drv);
   if (stat(sname.c_str(), &st)) {
      /* drive state file not found, so drive is not loaded */
      RemoveDriveSymlink(drv);
      log.Info("drive %d previously unloaded", drv);
      return 0;
   }
   /* Read loaded volume info from state file */
   FS = fopen(sname.c_str(), "r");
   if (!FS) {
      /* Error opening state file, so leave drive unloaded */
      verr.SetError(EACCES, "drive %d state file is not readable", drv);
      return EACCES;
   }
   if (tGetLine(line, FS) == NULL) {
      if (!feof(FS)) {
         /* i/o error reading line from state file. Change state to unloaded */
         rc = ferror(FS);
         fclose(FS);
         unlink(sname.c_str());
         RemoveDriveSymlink(drv);
         verr.SetErrorWithErrno(rc, "error %d reading state file for drive %d", rc, drv);
         return rc;
      }
   }
   fclose(FS);
   tStrip(tRemoveEOL(line));
   /* Extract the device drive was last loaded from */
   p = 0;
   rc = tParseCSV(dev, line, p);
   if (rc != 1 || dev.empty()) {
      /* Device string not found. Change state to unloaded. */
      verr.SetError(EINVAL, "deleting corrupt state file for drive %d", drv);
      unlink(sname.c_str());
      RemoveDriveSymlink(drv);
      return EINVAL;
   }
   /* Extract label of volume drive was last loaded from */
   rc = tParseCSV(labl, line, p);
   if (rc != 1 || labl.empty()) {
      /* Label string not found. Change state to unloaded. */
      verr.SetError(EINVAL, "deleting corrupt state file for drive %d", drv);
      unlink(sname.c_str());
      RemoveDriveSymlink(drv);
      return EINVAL;
   }

   /* Find virtual slot assigned the volume file last loaded in drive */
   for (v = 1; v < (int)vslot.size(); v++) {
      if (labl == GetVolumeLabel(v)) break;
   }
   if (v >= (int)vslot.size()) {
      /* Volume last loaded is no longer available. Change state to unloaded. */
      log.Notice("volume %s no longer available, unloading drive %d",
                  labl.c_str(), drv);
      unlink(sname.c_str());
      RemoveDriveSymlink(drv);
      return 0;
   }
   drive[drv].vs = v;

   /* Ensure symlink exists or create it */
   if ((rc = CreateDriveSymlink(drv)) != 0) {
      /* Unable to create symlink */
      drive[drv].vs = -1;
      log.Error("ERROR! %s", verr.GetErrorMsg());
      return rc;
   }

   /* Assign drive to virtual slot */
   vslot[v].drv = drv;
   m = vslot[v].mag_bay;
   ms = vslot[v].mag_slot;
   log.Notice("drive %d previously loaded from slot %d (%s)", drv, v, magazine[m].GetVolumeLabel(ms));
   return 0;
}


/*-------------------------------------------------
 *  Method to initialize changer parameters and state of magazines,
 *  virtual slots, and virtual drives.
 *  On success, returns zero. On error, returns negative.
 *  In either case, obtains a lock on the changer unless the lock operation
 *  itself fails. The lock will be released when the DiskChanger object
 *  is destroyed.
 *------------------------------------------------*/
int DiskChanger::Initialize()
{
   /* Make sure we have a lock on this changer */
   if (Lock()) return verr.GetError();
   magazine.clear();
   vslot.clear();
   drive.clear();
   dconf.restore();
   needs_update = false;

   /* Initialize array of mounted magazines */
   InitializeMagazines();

   /* Initialize array of virtual slots */
   InitializeVirtSlots();

   /* Initialize array of virtual drives */
   if (InitializeDrives()) return verr.GetError();

   return 0;
}


/*-------------------------------------------------
 *  Method to load virtual drive 'drv' from virtual slot 'slot'.
 *  Returns zero on success, else sets lasterr and
 *  returns negative.
 *------------------------------------------------*/
int DiskChanger::LoadDrive(int drv, int slot)
{
   int rc, m, ms;

   if (!changer_lock) {
      verr.SetError(EINVAL, "changer not initialized");
      log.Error("ERROR! %s", verr.GetErrorMsg());
      return EINVAL;
   }
   if (drv < 0) {
      verr.SetError(EINVAL, "invalid drive number %d", drv);
      log.Error("ERROR! %s", verr.GetErrorMsg());
      return EINVAL;
   }
   SetMaxDrive(drv);
   if (slot < 1 || slot >= (int)vslot.size()) {
      verr.SetError(EINVAL, "cannot load drive %d from invalid slot %d", drv, slot);
      log.Error("ERROR! %s", verr.GetErrorMsg());
      return EINVAL;
   }
   if (!drive[drv].empty()) {
      if (drive[drv].vs == slot) return 0;  /* already loaded from this slot */
      verr.SetError(EBUSY, "drive %d already loaded from slot %d", drv, slot);
      log.Error("ERROR! %s", verr.GetErrorMsg());
      return EBUSY;
   }
   if (vslot[slot].empty()) {
      verr.SetError(EINVAL, "cannot load drive %d from empty slot %d", drv, slot);
      log.Error("ERROR! %s", verr.GetErrorMsg());
      return ENOENT;
   }
   /* Create symlink for drive pointing to volume file */
   drive[drv].vs = slot;
   if ((rc = CreateDriveSymlink(drv))) {
      drive[drv].vs = -1;
      log.Error("ERROR! %s", verr.GetErrorMsg());
      return rc;
   }
   /* Save state of newly loaded drive */
   if ((rc = SaveDriveState(drv)) != 0) {
      /* Error writing drive state file */
      RemoveDriveSymlink(drv);
      drive[drv].vs = -1;
      log.Error("ERROR! %s", verr.GetErrorMsg());
      return rc;
   }
   /* Assign virtual slot to drive */
   vslot[slot].drv = drv;
   m = vslot[slot].mag_bay;
   ms = vslot[slot].mag_slot;
   log.Notice("loaded drive %d from slot %d (%s)", drv, slot, magazine[m].GetVolumeLabel(ms));
   return 0;
}


/*-------------------------------------------------
 *  Method to unload volume in virtual drive 'drv'. Deletes symlink
 *  and state file for the drive.
 *  On success, returns zero. Otherwise sets lasterr and returns
 *  errno.
 *------------------------------------------------*/
int DiskChanger::UnloadDrive(int drv)
{
   int rc;

   if (!changer_lock) {
      verr.SetError(EINVAL, "changer not initialized");
      log.Error("ERROR! %s", verr.GetErrorMsg());
      return EINVAL;
   }
   if (drv < 0) {
      verr.SetError(EINVAL, "invalid drive number %d", drv);
      log.Error("ERROR! %s", verr.GetErrorMsg());
      return EINVAL;
   }
   SetMaxDrive(drv);
   if (drive[drv].empty()) {
      /* Drive is already empty so assume successful */
      return 0;
   }
   /* Remove drive's symlink */
   if ((rc = RemoveDriveSymlink(drv)) != 0) {
      log.Error("ERROR! %s", verr.GetErrorMsg());
      return rc;
   }
   /* Remove virtual slot assignment */
   vslot[drive[drv].vs].drv = -1;
   drive[drv].vs = -1;
   /* Update drive state file (will delete state file due to negative slot number) */
   if ((rc = SaveDriveState(drv)) != 0) {
      log.Error("ERROR! %s", verr.GetErrorMsg());
      return rc;
   }
   log.Notice("unloaded drive %d", drv);
   return 0;
}


/*-------------------------------------------------
 *  Method to create new volume files in virtual slots 'slot1' through 'slot2'.
 *  Use volume labels (barcodes) of the form prefix + '_' + mag_slot_number, where
 *  mag_slot_number is the magazine relative slot number of the magazine slot that
 *  the virtual slot maps to. If 'label_prefix' is blank, then use the magazine name
 *  of the magazine the virtual slot is mapped onto as the prefix.
 *  Returns zero on success, else returns negative and sets lasterr.
 *------------------------------------------------*/
int DiskChanger::CreateVolumes(int bay, int count, int start, const char *label_prefix_in)
{
   MagazineSlot vol;
   tString label, label_prefix(label_prefix_in);
   int i;

   if (!changer_lock) {
      verr.SetError(EINVAL, "changer not initialized");
      log.Error("ERROR! %s", verr.GetErrorMsg());
      return -1;
   }
   if (bay < 0 || bay >= (int)magazine.size()) {
      verr.SetError(EINVAL, "invalid magazine");
      log.Error("ERROR! %s", verr.GetErrorMsg());
      return -1;
   }
   if (count < 1) count = 1;
   tStrip(tRemoveEOL(label_prefix));
   if (label_prefix.empty()) {
      /* Default prefix is storage-name_magazine-number */
      tFormat(label_prefix, "%s_%d", conf.storage_name.c_str(), bay);
   }
   if (start < 0) {
      /* Find highest uniqueness number for this filename prefix */
      for (i = magazine[bay].num_slots * 5; i > 0; i--) {
         tFormat(label, "%s_%d", label_prefix.c_str(), i);
         if (magazine[bay].GetVolumeSlot(label) >= 0) break;
      }
      start = i;
   }
   for (i = 0; i < count; i++) {
      tFormat(label, "%s_%d", label_prefix.c_str(), start);
      if (!magazine[bay].empty()) {
         while (magazine[bay].GetVolumeSlot(label) >= 0) {
            ++start;
            tFormat(label, "%s_%d", label_prefix.c_str(), start);
         }
      }
      fprintf(stdout, "creating label '%s'\n", label.c_str());
      if (magazine[bay].CreateVolume(label)) {
         if (i) magazine[bay].save();
         return -1;
      }
      ++start;
   }
   /* Update magazine state */
   magazine[bay].save();
   /* New mag state will require 'update slots' and 'label barcodes' in Bacula */
   needs_update = true;
   needs_label = true;
   log.Notice("update slots needed. %d volumes added to magazine %d",count , bay);
   return 0;
}

/*-------------------------------------------------
 *  Method to cause Bacula to update its catalog to reflect
 *  changes in the available volumes
 *-------------------------------------------------*/
int DiskChanger::UpdateBacula()
{
   int rc;
   FILE *update_lock;
   tString cmd;
   char lockfile[4096];

   /* Check if update needed */
   if (!needs_update && !needs_label) return 0; /* Nothing to do */
   /* Create update lock lockfile */
   snprintf(lockfile, sizeof(lockfile), "%s%s%s.updatelock", conf.work_dir.c_str(), DIR_DELIM,
         conf.storage_name.c_str());
   rc = exclusive_fopen(lockfile, &update_lock);
   if (rc == EEXIST) {
      /* Update already in progress in another process, so skip */
      return 0;
   }
   if (rc) {
      /* error creating lockfile, so skip */
      if (needs_update)
         log.Error("WARNING! 'update slots' needed in bconsole");
      if (needs_label)
         log.Error("WARNING! 'label barcodes' needed in bconsole");
      return 0;
   }
   log.Debug("created update lockfile for pid %d", getpid());
   /* Remove lock so another vchanger instance invoked due to the update slots or
    * label barcodes command doens't block */
   Unlock();
   /* Perform update slots command in bconsole */
   if (needs_update) {
      /* Issue update slots command in bconsole */
      tFormat(cmd, "update slots storage=\"%s\" drive=0", conf.storage_name.c_str());
      if(issue_bconsole_command(cmd.c_str())) {
         log.Error("WARNING! 'update slots' needed in bconsole");
      }
   }
   /* Perform label barcodes command in bconsole */
   if (needs_label) {
      tFormat(cmd, "label storage=\"%s\" pool=\"%s\" barcodes\ny\nyes\n", conf.storage_name.c_str(),
            conf.def_pool.c_str());
      if (issue_bconsole_command(cmd.c_str())) {
         log.Error("WARNING! 'label barcodes' needed in bconsole");
      }
   }
   /* Obtain changer lock before removing update lock */
   if (Lock()) {
      log.Error("timeout obtaining lock for pid=%d", getpid());
      fclose(update_lock);
      unlink(lockfile);
      log.Error("force removal of update lockfile for pid %d", getpid());
      return 0;
   }
   fclose(update_lock);
   unlink(lockfile);
   log.Debug("removed update lockfile for pid %d", getpid());
   return 0;
}


/*-------------------------------------------------
 *  Method to get label of volume in this slot
 *-------------------------------------------------*/
const char* DiskChanger::GetVolumeLabel(int slot)
{
   if (slot <= 0 || slot >= (int)vslot.size()) {
      verr.SetError(-1, "volume label request from invalid slot %d", slot);
      log.Error("ERROR! %s", verr.GetErrorMsg());
      return NULL;
   }
   if (vslot[slot].empty()) return "";
   return magazine[vslot[slot].mag_bay].GetVolumeLabel(vslot[slot].mag_slot);
}


/*-------------------------------------------------
 *  Method to get filename path of volume in this slot
 *-------------------------------------------------*/
const char* DiskChanger::GetVolumePath(tString &path, int slot)
{
   path.clear();
   if (slot <= 0 || slot >= (int)vslot.size()) {
      verr.SetError(-1, "volume path request from invalid slot %d", slot);
      log.Error("ERROR! %s", verr.GetErrorMsg());
      return NULL;
   }
   if (vslot[slot].empty()) return path.c_str();
   return magazine[vslot[slot].mag_bay].GetVolumePath(path, vslot[slot].mag_slot);
}


/*-------------------------------------------------
 *  Protected method to lock changer device using a lock file such that
 *  only one process at a time may execute changer commands on the
 *  same autochanger. If another process has the lock, then this process
 *  will sleep 1 second before trying again. This try/wait loop will continue
 *  until the lock is obtained or 'timeout' seconds have expired. If
 *  timeout = 0 then only tries to obtain lock once. If timeout < 0
 *  then doesn't return until the lock is obtained.
 *  On success, returns true. Otherwise on error or timeout, sets
 *  lasterr negative and returns false.
 *------------------------------------------------*/
int DiskChanger::Lock(long timeout_seconds)
{
   int rc;
   time_t timeout = 0;
   char lockfile[4096];

   if (changer_lock) return 0;

   if (timeout_seconds < 0) {
      timeout_seconds = 3600 * 24 * 365;
   }
   if (timeout_seconds > 0) {
      timeout = time(NULL) + timeout_seconds;
   }
   snprintf(lockfile, sizeof(lockfile), "%s%s%s.lock", conf.work_dir.c_str(), DIR_DELIM,
         conf.storage_name.c_str());
   rc = exclusive_fopen(lockfile, &changer_lock);
   if (rc == EEXIST && timeout == 0) {
      /* timeout=0 means do not wait */
      verr.SetErrorWithErrno(rc, "cannot open lockfile");
      log.Error("ERROR! %s", verr.GetErrorMsg());
      return -1;
   }
   while (rc == EEXIST) {
      /* sleep before trying again */
      sleep(1);
      if (time(NULL) > timeout) {
         verr.SetError(EBUSY, "timeout waiting for lockfile");
         log.Error("ERROR! %s", verr.GetErrorMsg());
         return EACCES;
      }
      rc = exclusive_fopen(lockfile, &changer_lock);
   }
   if (rc) {
      verr.SetErrorWithErrno(rc, "cannot open lockfile");
      log.Error("ERROR! %s", verr.GetErrorMsg());
      return -1;
   }
   /* Write PID to lockfile and leave open for exclusive R/W */
   fprintf(changer_lock, "%d", getpid());
   fflush(changer_lock);
   log.Debug("created lockfile for pid %d", getpid());
   return 0;
}

/*-------------------------------------------------
 *  Protected method to unlock changer device
 *------------------------------------------------*/
void DiskChanger::Unlock()
{
   char lockfile[4096];
   if (!changer_lock) return;
   fclose(changer_lock);
   changer_lock = NULL;
   snprintf(lockfile, sizeof(lockfile), "%s%s%s.lock", conf.work_dir.c_str(), DIR_DELIM,
         conf.storage_name.c_str());
   log.Debug("removing lockfile for pid %d", getpid());
   unlink(lockfile);
}


/*-------------------------------------------------
 *  Method returns true if magazine is not mounted,
 *  else returns false.
 *------------------------------------------------*/
bool DiskChanger::MagazineEmpty(int mag) const
{
   if (mag < 0 || mag >= (int)magazine.size()) return true;
   return magazine[mag].empty();
}


/*-------------------------------------------------
 *  Method returns true if no magazine volume is assigned
 *  to virtual slot, else returns false.
 *------------------------------------------------*/
bool DiskChanger::SlotEmpty(int slot) const
{
   if (slot <= 0 || slot >= (int)vslot.size()) return true;
   return vslot[slot].empty();
}


/*-------------------------------------------------
 *  Method returns true if no magazine volume is loaded
 *  into drive drv, else returns false.
 *------------------------------------------------*/
bool DiskChanger::DriveEmpty(int drv) const
{
   if (drv < 0 || drv >= (int)drive.size()) return true;
   return drive[drv].empty();
}


/*-------------------------------------------------
 *  Method to get virtual slot currently loaded in virtual drive 'drv'.
 *  If drive is loaded then returns the virtual slot number of the
 *  loaded volume. If unloaded, returns zero.
 *------------------------------------------------*/
int DiskChanger::GetDriveSlot(int drv) const
{
   if (drv < 0 || drv >= (int)drive.size()) return 0;
   return drive[drv].vs;
}


/*-------------------------------------------------
 *  Method to get drive a virtual slot is currently loaded in.
 *  Returns the drive number a slot's volume is loaded in.
 *  If unloaded, returns negative.
 *------------------------------------------------*/
int DiskChanger::GetSlotDrive(int slot) const
{
   if (slot <= 0 || slot >= (int)vslot.size()) return -1;
   return vslot[slot].drv;
}


/*-------------------------------------------------
 *  Method to return number of volumes on magazine 'mag'.
 *------------------------------------------------*/
int DiskChanger::GetMagazineSlots(int mag) const
{
   if (mag < 0 || mag >= (int)magazine.size()) return 0;
   return magazine[mag].num_slots;
}


/*-------------------------------------------------
 *  Method to return the start of the virtual slot range
 *  that is assigned to magazine 'mag' volumes.
 *------------------------------------------------*/
int DiskChanger::GetMagazineStartSlot(int mag) const
{
   if (mag < 0 || mag >= (int)magazine.size()) return 0;
   return magazine[mag].start_slot;
}


/*-------------------------------------------------
 *  Method to return the mountpoint of magazine 'mag'.
 *------------------------------------------------*/
const char* DiskChanger::GetMagazineMountpoint(int mag) const
{
   if (mag < 0 || mag >= (int)magazine.size()) return "";
   return magazine[mag].mountpoint.c_str();
}
