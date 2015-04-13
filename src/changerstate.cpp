/* changerstate.cpp
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
 *  Provides a class to track state of virtual drives using files in
 *  the vchanger state directory.
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
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_STDDEF_H
#include <stddef.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif
#ifdef HAVE_CTYPE_H
#include <ctype.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include "compat/getline.h"
#include "compat/readlink.h"
#include "compat/symlink.h"
#include "vconf.h"
#include "loghandler.h"
#include "errhandler.h"
#include "util.h"
#define __CHANGERSTATE_SOURCE 1
#include "changerstate.h"
#include "uuidlookup.h"

///////////////////////////////////////////////////
//  Class MagazineSlot
///////////////////////////////////////////////////

MagazineSlot::MagazineSlot(const MagazineSlot &b)
{
   mag_bay = b.mag_bay;
   mag_slot = b.mag_slot;
   label = b.label;
}

MagazineSlot& MagazineSlot::operator=(const MagazineSlot &b)
{
   if (&b != this) {
      mag_bay = b.mag_bay;
      mag_slot = b.mag_slot;
      label = b.label;
   }
   return *this;
}

bool MagazineSlot::operator==(const MagazineSlot &b)
{
   if (&b == this) return true;
   if (label == b.label) return true;
   return false;
}

bool MagazineSlot::operator!=(const MagazineSlot &b)
{
   if (&b == this) return false;
   if (label != b.label) return true;
   return false;
}

/*-------------------------------------------------
 *  Method to clear object values
 *-------------------------------------------------*/
void MagazineSlot::clear()
{
   mag_bay = -1;
   mag_slot = -1;
   label.clear();
}



///////////////////////////////////////////////////
//  Class MagazineState
///////////////////////////////////////////////////

MagazineState::MagazineState(const MagazineState &b)
{
   mag_bay = b.mag_bay;
   num_slots = b.num_slots;
   start_slot = b.start_slot;
   prev_num_slots = b.prev_num_slots;
   prev_start_slot = b.prev_start_slot;
   mag_dev = b.mag_dev;
   mountpoint = b.mountpoint;
   mslot = b.mslot;
   verr = b.verr;
}

MagazineState& MagazineState::operator=(const MagazineState &b)
{
   if (&b != this) {
      mag_bay = b.mag_bay;
      num_slots = b.num_slots;
      start_slot = b.start_slot;
      prev_num_slots = b.prev_num_slots;
      prev_start_slot = b.prev_start_slot;
      mag_dev = b.mag_dev;
      mountpoint = b.mountpoint;
      mslot = b.mslot;
      verr = b.verr;
   }
   return *this;
}

void MagazineState::clear()
{
   /* Notice that device and bay number are not cleared */
   num_slots = 0;
   start_slot = 0;
   mountpoint.clear();
   mslot.clear();
   verr.clear();
}

/*-------------------------------------------------
 *  Method to save current state of magazine bay to a file in
 *  the work directory named "bay" + bay_number.
 *  On success returns zero, otherwise sets lasterr and
 *  returns errno.
 *-------------------------------------------------*/
int MagazineState::save()
{
   mode_t old_mask;
   int rc;
   FILE *FS;
   char sname[4096];

   if (mag_bay < 0) {
      verr.SetErrorWithErrno(EINVAL, "cannot save state of invalid magazine %d", mag_bay);
      log.Error("ERROR! %s", verr.GetErrorMsg());
      return EINVAL;
   }
   /* Build path to state file */
   snprintf(sname, sizeof(sname), "%s%sbay_state-%d", conf.work_dir.c_str(), DIR_DELIM, mag_bay);
   /* Remove magazine state files for unmounted magazines */
   if (mountpoint.empty() || mslot.empty()) {
      unlink(sname);
      return 0;
   }
   /* Write state file for mounted magazine */
   old_mask = umask(027);
   FS = fopen(sname, "w");
   if (!FS) {
      /* Unable to open state file for writing */
      rc = errno;
      umask(old_mask);
      verr.SetErrorWithErrno(rc, "cannot open magazine %d state file for writing", mag_bay);
      log.Error("ERROR! %s", verr.GetErrorMsg());
      return rc;
   }
   /* Save magazine device (directory or UUID), number of volumes, and start of
    * virtual slot range it is assigned */
   if (fprintf(FS, "%s,%d,%d\n", mag_dev.c_str(), num_slots, start_slot) < 0) {
      /* I/O error writing state file */
      rc = errno;
      fclose(FS);
      unlink(sname);
      umask(old_mask);
      verr.SetErrorWithErrno(rc, "cannot write to magazine %d state file", mag_bay);
      log.Error("ERROR! %s", verr.GetErrorMsg());
      return rc;
   }
   fclose(FS);
   umask(old_mask);
   log.Notice("saved state of magazine %d", mag_bay);
   return 0;
}


/*-------------------------------------------------
 *  Method to restore state of magazine from a file in the work
 *  directory named "bay_state-N", where N is the bay number.
 *  On success returns zero, otherwise sets lasterr and
 *  returns errno.
 *-------------------------------------------------*/
int MagazineState::restore()
{
   int rc;
   tString line, word;
   struct stat st;
   FILE *FS;
   size_t p;
   char sname[4096];

   if (mag_bay < 0) {
      verr.SetErrorWithErrno(EINVAL, "cannot restore state of invalid magazine %d", mag_bay);
      log.Error("ERROR! %s", verr.GetErrorMsg());
      return EINVAL;
   }
   clear();
   prev_num_slots = 0;
   prev_start_slot = 0;
   snprintf(sname, sizeof(sname), "%s%sbay_state-%d", conf.work_dir.c_str(), DIR_DELIM, mag_bay);

   /* Check for existing state file */
   if (stat(sname, &st)) {
      /* magazine bay state file not found, so bay did not previously
       * contain a magazine */
      return 0;
   }
   /* Read bay state file */
   FS = fopen(sname, "r");
   if (!FS) {
      /* No read permission? */
      rc = errno;
      verr.SetErrorWithErrno(rc, "cannot open magazine %d state file for reading", mag_bay);
      log.Error("ERROR! %s", verr.GetErrorMsg());
      return rc;
   }
   if (tGetLine(line, FS) == NULL) {
      rc = errno;
      if (!feof(FS)) {
         /* error reading bay state file */
         fclose(FS);
         verr.SetErrorWithErrno(rc, "error reading magazine %d state file", mag_bay);
         log.Error("ERROR! %s", verr.GetErrorMsg());
         return rc;
      }
   }
   fclose(FS);

   /* Get magazine device (UUID or path specified in config) */
   tStrip(tRemoveEOL(line));
   p = 0;
   if (tParseCSV(word, line, p) <= 0) {
      /* bay state file should not be empty, assume it didn't exist */
      log.Warning("WARNING! magazine %d state file was empty, deleting it", mag_bay);
      unlink(sname);
      return 0;
   }
   if (mag_dev != word) {
      /* Order of mag bays has changed in config file so ignore old state */
      unlink(sname);
      return 0;
   }

   /* Get number of slots */
   if (tParseCSV(word, line, p) <= 0) {
      /* Bay state file is corrupt.
       * Treat as if it was not mounted at last invocation */
      clear();
      log.Warning("WARNING! magazine %d state file corrupt, deleting it", mag_bay);
      unlink(sname);
      return 0;
   }
   if (!isdigit(word[0])) {
      /* Corrupt bay state file, assume it doesn't exist */
      clear();
      unlink(sname);
      log.Warning("WARNING! magazine %d state file has invalid number of slots field, deleting it", mag_bay);
      return 0;
   }
   prev_num_slots = (int)strtol(word.c_str(), NULL, 10);
   if (prev_num_slots < 0) {
      /* Corrupt bay state file, assume it doesn't exist */
      clear();
      prev_num_slots = 0;
      unlink(sname);
      log.Warning("WARNING! magazine %d state file has invalid number of slots field, deleting it", mag_bay);
      return 0;
   }

   /* Get virtual slot number offset */
   if (tParseCSV(word, line, p) <= 0) {
      /* Bay state file is corrupt.
       * Treat as if it was not mounted at last invocation */
      clear();
      prev_num_slots = 0;
      log.Warning("WARNING! magazine %d state file corrupt, deleting it", mag_bay);
      unlink(sname);
      return 0;
   }
   if (!isdigit(word[0])) {
      /* Corrupt bay state file, assume it doesn't exist */
      clear();
      prev_num_slots = 0;
      unlink(sname);
      log.Warning("WARNING! magazine %d state file has invalid virtual slot assignment field, deleting it",
            mag_bay);
      return 0;
   }
   prev_start_slot = (int)strtol(word.c_str(), NULL, 10);
   if (prev_start_slot <= 0) {
      /* Corrupt bay state file, assume it doesn't exist */
      clear();
      prev_num_slots = 0;
      prev_start_slot = 0;
      unlink(sname);
      log.Warning("WARNING! magazine %d state file has invalid virtual slot assignment field, deleting it",
            mag_bay);
      return 0;
   }
   log.Notice("restored state of magazine %d", mag_bay);
   return 0;
}


/*-------------------------------------------------
 *  Method to update a magazine from vchanger version 0.x format
 *  to the format used with version 1.0.0 or higher.
 *  Return values are:
 *       0    Success
 *-------------------------------------------------*/
int MagazineState::UpdateMagazineFormat()
{
   FILE *fs;
   DIR *dir;
   struct dirent *de;
   struct stat st;
   tString str, fname, lname, vname;
   int drv;

   /* Rename driveN files to their volume file name */
   dir = opendir(mountpoint.c_str());
   if (!dir) return -1;
   de = readdir(dir);
   while (de) {
      /* Skip if not regular file */
      tFormat(fname, "%s%s%s", mountpoint.c_str(), DIR_DELIM, de->d_name);
      stat(fname.c_str(), &st);
      if (!S_ISREG(st.st_mode)) {
         de = readdir(dir);
         continue;
      }
      /* Skip index file */
      if (tCaseCmp(de->d_name, "index") == 0) {
         de = readdir(dir);
         continue;
      }
      str = de->d_name;
      if (str.find("drive") == 0) {
         str.erase(0, 5);
         if (str.find_first_of("0123456789") == tString::npos) {
            de = readdir(dir);
            continue;
         }
         if (str.find_first_not_of("0123456789") != tString::npos) {
            de = readdir(dir);
            continue;
         }
         drv = (int)strtol(str.c_str(), NULL, 10);
         tFormat(lname, "%s%sloaded%d", mountpoint.c_str(), DIR_DELIM, drv);
         fs = fopen(lname.c_str(), "r");
         if (fs == NULL) {
            verr.SetErrorWithErrno(errno, "failed to find loaded%d file when updating magazine %d", drv, mag_bay);
            log.Error("ERROR! %s", verr.GetErrorMsg());
            de = readdir(dir);
            continue;
         }
         tGetLine(str, fs);
         fclose(fs);
         if (str.empty()) {
            verr.SetError(-1, "loaded%d file empty when updating magazine %d", drv, mag_bay);
            log.Error("ERROR! %s", verr.GetErrorMsg());
            de = readdir(dir);
            continue;
         }
         tStrip(tRemoveEOL(str));
         tFormat(vname, "%s%s%s", mountpoint.c_str(), DIR_DELIM, str.c_str());
         if (rename(fname.c_str(), vname.c_str())) {
            verr.SetError(EINVAL, "unable to rename 'drive%d' on magazine %d",
                           drv, mag_bay);
            log.Error("ERROR! %s", verr.GetErrorMsg());
         }
      }
      de = readdir(dir);
   }
   closedir(dir);

   /* Delete loadedN files */
   dir = opendir(mountpoint.c_str());
   de = readdir(dir);
   while (de) {
      str = de->d_name;
      /* Skip if not regular file */
      tFormat(fname, "%s%s%s", mountpoint.c_str(), DIR_DELIM, de->d_name);
      stat(fname.c_str(), &st);
      if (!S_ISREG(st.st_mode)) {
         de = readdir(dir);
         continue;
      }
      if (str.find("loaded") == 0) {
         str.erase(0, 6);
         if (str.find_first_of("0123456789") == tString::npos) {
            de = readdir(dir);
            continue;
         }
         if (str.find_first_not_of("0123456789") != tString::npos) {
            de = readdir(dir);
            continue;
         }
         unlink(fname.c_str());
      }
      de = readdir(dir);
   }
   closedir(dir);

   /* Delete index file */
   tFormat(fname, "%s%sindex", mountpoint.c_str(), DIR_DELIM);
   unlink(fname.c_str());

   log.Warning("magaine %d updated from old format", mag_bay);
   return 0;
}


/*-------------------------------------------------
 *  Method to determine mountpoint of magazine and assign its volume files
 *  to magazine slots. Regular files on the magazine are assigned slots in
 *  ascending alphanumeric order by filename beginning with magazine slot zero.
 *  If the magazine's device string begins with "UUID:" (case insensitive),
 *  then it specifies the UUID of a file system on a disk partition to be used
 *  as the virtual magazine. Otherwise, it specifies a directory to be used as
 *  the virtual magazine. If a UUID is given, then the system is queried to
 *  determine the mountpoint of the filesystem with the given UUID. The magazine
 *  device must already be mounted or configured to be auto-mounted.
 *  Return values are:
 *       0    Magazine assigned successfully
 *      -1    system error
 *      -2    parameter error
 *      -3    magname not found or not mounted
 *      -5    permission denied
 *-------------------------------------------------*/
int MagazineState::Mount()
{
   int rc, s;
   DIR *dir;
   struct dirent *de;
   struct stat st;
   tString fname, line, path;
   MagazineSlot v;
   std::list<tString> vname;
   std::list<tString>::iterator p;
   char buf[4096];

   clear();
   if (tCaseFind(mag_dev, "uuid:") != 0) {
      /* magazine specified as filesystem path */
      mountpoint = mag_dev;
   } else {
      /* magazine specified as UUID, so query OS for mountpoint */
      rc = GetMountpointFromUUID(buf, sizeof(buf), mag_dev.substr(5).c_str());
      mountpoint = buf;
      if (rc == -3 || rc == -4) {
         /* magazine device not found or not mounted */
         mountpoint.clear();
         return -3;
      }
      if (rc) {
         verr.SetError(rc, "system error determining mountpoint from UUID");
         log.Error("ERROR! %s", verr.GetErrorMsg());
         mountpoint.clear();
         return -1;
      }
   }

   /* Check mountpoint exists */
   if (access(mountpoint.c_str(), F_OK) != 0) {
      /* Mountpoint not found */
      mountpoint.clear();
      return -3;
   }

   /* Ensure access to magazine mountpoint */
   if (access(mountpoint.c_str(), W_OK) != 0) {
      verr.SetError(rc, "no write access to directory %s", mountpoint.c_str());
      log.Error("%s", verr.GetErrorMsg());
      mountpoint.clear();
      return -5;
   }

   /* If this magazine contains a file named index then assume it was
    * created by an old version of vchanger and prepare it for use
    * by removing meta-information files. */
   tFormat(fname, "%s%sindex", mountpoint.c_str(), DIR_DELIM);
   if (access(fname.c_str(), F_OK) == 0) {
      UpdateMagazineFormat();
   }

   /* Build list of this magazine's volume files */
   dir = opendir(mountpoint.c_str());
   if (!dir) {
      /* could not open mountpoint dir */
      rc = errno;
      verr.SetErrorWithErrno(rc, "cannot open directory '%s'", mountpoint.c_str());
      log.Error("ERROR! %s", verr.GetErrorMsg());
      mountpoint.clear();
      if (rc == ENOTDIR || rc == ENOENT) return -3;
      if (rc == EACCES) return -5;
      return -1;
   }
   de = readdir(dir);
   while (de) {
      /* Skip if not regular file */
      tFormat(path, "%s%s%s", mountpoint.c_str(), DIR_DELIM, de->d_name);
      stat(path.c_str(), &st);
      if (!S_ISREG(st.st_mode)) {
         de = readdir(dir);
         continue;
      }
      /* Writable regular files on magazine are considered volume files */
      if (access(path.c_str(), W_OK) == 0) {
         vname.push_back(de->d_name);
      }
      de = readdir(dir);
   }
   closedir(dir);
   if (vname.empty()) {
      /* Magazine is ready for use but has no volumes */
      start_slot = 0;
      num_slots = 0;
      return 0;
   }
   /* Assign volume files to slots in alphanumeric order */
   vname.sort();
   s = 0;
   for (p = vname.begin(); p != vname.end(); p++) {
      v.mag_bay = mag_bay;
      v.label = *p;
      v.mag_slot = s++;
      mslot.push_back(v);
   }
   num_slots = (int)mslot.size();
   return 0;
}


/*-------------------------------------------------
 *  Method to get path to volume file in a magazine slot
 *  On success returns path, else returns empty string
 *-------------------------------------------------*/
const char* MagazineState::GetVolumeLabel(int ms) const
{
   if (ms >= 0 && ms < (int)mslot.size() && !mslot[ms].empty()) {
      return mslot[ms].label.c_str();
   }
   return "";
}


/*-------------------------------------------------
 *  Method to get path to volume file in a magazine slot
 *  On success returns path, else returns empty string
 *-------------------------------------------------*/
tString MagazineState::GetVolumePath(int ms)
{
   tString result;
   if (ms >= 0 && ms < (int)mslot.size()) {
      tFormat(result, "%s%s%s", mountpoint.c_str(), DIR_DELIM, GetVolumeLabel(ms));
   }
   return result;
}


/*-------------------------------------------------
 *  Method to get path to volume file in a magazine slot
 *  On success returns path, else returns empty string
 *-------------------------------------------------*/
const char* MagazineState::GetVolumePath(tString &path, int ms)
{
   path = GetVolumePath(ms);
   return path.c_str();
}


/*-------------------------------------------------
 *  Method to get magazine slot containing a label.
 *  On success returns magazine slot number, else negative.
 *-------------------------------------------------*/
int MagazineState::GetVolumeSlot(const char *label)
{
   int n;
   for (n = 0; n < num_slots; n++) {
      if (mslot[n].label == label) return n;
   }
   return -1;
}


/*-------------------------------------------------
 *  Method to create a new volume file. 'vol_label_in' gives the
 *  name of the new volume file to create on the magazine. If empty,
 *  then a volume file name is generated based on the magazine's name.
 *  A new magazine slot is appended to hold the new volume and a new
 *  virtual slot is appended that maps to the new magazine slot.
 *  On success returns zero, else sets lasterr and returns negative
 *-------------------------------------------------*/
int MagazineState::CreateVolume(const char *vol_label_in)
{
   int rc = 0, slot;
   FILE *fs;
   tString fname, label(vol_label_in);
   MagazineSlot new_mslot;

   if (label.empty()) {
      slot = (int)mslot.size();
      --slot;
      while(rc == 0) {
         ++slot;
         tFormat(label, "%s_%d_%d", conf.storage_name.c_str(), mag_bay, slot);
         tFormat(fname, "%s%s%s", mountpoint.c_str(), DIR_DELIM, label.c_str());
         if (access(fname.c_str(), F_OK)) rc = errno;
         else rc = 0;
      }
   } else {
      tFormat(fname, "%s%s%s", mountpoint.c_str(), DIR_DELIM, label.c_str());
      if (access(fname.c_str(), F_OK)) rc = errno;
      else rc = 0;
      if (rc == 0) {
         verr.SetErrorWithErrno(rc, "volume %s already exists on magazine %d", label.c_str(), mag_bay);
         return EEXIST;
      }
   }
   if (rc != ENOENT) {
      verr.SetErrorWithErrno(rc, "error %d accessing volumes on magazine %d", rc, mag_bay);
      log.Error("MagazineState::CreateVolume: %s", verr.GetErrorMsg());
      return -1;
   }
   /* Create new volume file on magazine */
   fs = fopen(fname.c_str(), "w");
   if (!fs) {
      rc = errno;
      verr.SetErrorWithErrno(rc, "error %d creating volume on magazine %d", rc, mag_bay);
      log.Error("MagazineState::CreateVolume: %s", verr.GetErrorMsg());
      return -1;
   }
   fclose(fs);
   new_mslot.mag_bay = mag_bay;
   new_mslot.mag_slot = mslot.size();
   new_mslot.label = label;
   mslot.push_back(new_mslot);
   ++num_slots;
   log.Notice("created volume '%s' on magazine %d (%s)", label.c_str(), mag_bay, mag_dev.c_str());
   return 0;
}


/*-------------------------------------------------
 *  Method to assign bay number and device for this magazine
 *-------------------------------------------------*/
void MagazineState::SetBay(int bay, const char *dev)
{
   mag_bay = bay;
   mag_dev = dev;
   clear();
}



///////////////////////////////////////////////////
//  Class VirtualSlot
///////////////////////////////////////////////////

VirtualSlot::VirtualSlot(const VirtualSlot &b)
{
   vs = b.vs;
   drv = b.drv;
   mag_bay = b.mag_bay;
   mag_slot = b.mag_slot;
}

VirtualSlot& VirtualSlot::operator=(const VirtualSlot &b)
{
   if (this != &b) {
      vs = b.vs;
      drv = b.drv;
      mag_bay = b.mag_bay;
      mag_slot = b.mag_slot;
   }
   return *this;
}


/*-------------------------------------------------
 *  Method to clear an virtual slot's values
 *-------------------------------------------------*/
void VirtualSlot::clear()
{
   drv = -1;
   mag_bay = -1;
   mag_slot = -1;
}



///////////////////////////////////////////////////
//  Class DriveState
///////////////////////////////////////////////////

DriveState::DriveState(const DriveState &b)
{
   drv = b.drv;
   vs = b.vs;
}

DriveState& DriveState::operator=(const DriveState &b)
{
   if (&b != this) {
      drv = b.drv;
      vs = b.vs;
   }
   return *this;
}

/*
 *  Method to clear a drive's values
 */
void DriveState::clear()
{
   /* do not clear drive number */
   vs = -1;
}



///////////////////////////////////////////////////
//  Class DynamicConfig
///////////////////////////////////////////////////

/*-------------------------------------------------
 *  Method to save dynamic configuration info to a file in
 *  the work directory named dynamic.conf.
 *-------------------------------------------------*/
void DynamicConfig::save()
{
   mode_t old_mask;
   int rc;
   FILE *FS;
   char sname[4096];

   if (max_slot < 10) max_slot = 10;
   /* Build path to dynamic.conf file */
   snprintf(sname, sizeof(sname), "%s%sdynamic.conf", conf.work_dir.c_str(), DIR_DELIM);
   /* Write dynamic config info */
   old_mask = umask(027);
   FS = fopen(sname, "w");
   if (!FS) {
      /* Unable to open dynamic.conf file for writing */
      rc = errno;
      umask(old_mask);
      log.Error("ERROR! cannot open dynamic.conf file for writing (errno=%d)", rc);
      return;
   }
   /* Save max slot number in use to dynamic configuration */
   if (fprintf(FS, "max_used_slot=%d\n", max_slot) < 0) {
      /* I/O error writing dynamic.conf file */
      rc = errno;
      fclose(FS);
      unlink(sname);
      umask(old_mask);
      log.Error("ERROR! i/o error writing dynamic.conf file (errno=%d)", rc);
      return;
   }
   fclose(FS);
   umask(old_mask);
   log.Notice("saved dynamic configuration (max used slot: %d)", max_slot);
}


/*-------------------------------------------------
 *  Method to restore dynamic configuration info.
 *-------------------------------------------------*/
void DynamicConfig::restore()
{
   int rc;
   tString line;
   struct stat st;
   FILE *FS;
   char sname[4096];

   if (max_slot < 10) max_slot = 10;
   /* Build path to dynamic.conf file */
   snprintf(sname, sizeof(sname), "%s%sdynamic.conf", conf.work_dir.c_str(), DIR_DELIM);
   /* Check for existing file */
   if (stat(sname, &st)) {
      /* dynamic configuration file not found */
      return;
   }
   /* Read dynamic.conf file */
   FS = fopen(sname, "r");
   if (!FS) {
      /* No read permission? */
      rc = errno;
      log.Error("ERROR! cannot open dynamic.conf file for restore (errno=%d)", rc);
      return;
   }
   if (tGetLine(line, FS) == NULL) {
      rc = errno;
      if (!feof(FS)) {
         /* error reading bay state file */
         fclose(FS);
         log.Error("ERROR! i/o error reading dynamic.conf file (errno=%d)", rc);
         return;
      }
   }
   fclose(FS);

   /* Get magazine device (UUID or path specified in config) */
   tStrip(tRemoveEOL(line));
   if (tCaseFind(line, "max_used_slot") == 0) {
      max_slot = (int)strtol(line.substr(14).c_str(), NULL, 10);
      if (max_slot < 10) max_slot = 10;
   }
}

