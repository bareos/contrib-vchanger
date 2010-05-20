/* changerstate.cpp
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
 *  Provides a class to track state of virtual drives using files in
 *  the vchanger state directory.
*/

#include <stdio.h>
#include <stddef.h>
#include <sys/stat.h>
#include <dirent.h>
#include "vchanger.h"
#include "vchanger_common.h"
#include "util.h"
#include "changerstate.h"
#include "uuidlookup.h"


///////////////////////////////////////////////////
//  Class VolumeLabel
///////////////////////////////////////////////////

VolumeLabel::VolumeLabel()
{
   mag_number = -1;
   mag_slot = -1;
   memset(chgr_name, 0, sizeof(chgr_name));
}

char* VolumeLabel::GetLabel(char *buf, size_t buf_sz)
{
   if (!buf) return NULL;
   buf[0] = '\0';
   if (mag_slot < 1 || mag_number < 1 || !strlen(chgr_name)) {
      return buf;
   }
   snprintf(buf, buf_sz, "%s_%04d_%04d", chgr_name, mag_number, mag_slot);
   return buf;
}


int VolumeLabel::set(const char *chgr, int magnum, int magslot)
{
   clear();
   if (!chgr || magnum < 1 || magslot < 1) {
      return -1;
   }
   mag_number = magnum;
   mag_slot = magslot;
   strncpy(chgr_name, chgr, sizeof(chgr_name));
   strip_whitespace(chgr_name, sizeof(chgr_name));
   if (!strlen(chgr_name)) {
      clear();
      return -1;
   }
   return 0;
}

int VolumeLabel::set(const char *vname)
{
   int n, p, len;
   char buf[512];

   clear();
   if (!vname) return -1;  /* can't be null */
   strncpy(buf, vname, sizeof(buf));
   len = strip_whitespace(buf, sizeof(buf));
   if (len < 11) return -1;  /* must be at least 11 chars */
   /* Find '_' before magazine slot number */
   for (p = len - 1; p > 0 && isdigit(buf[p]); p--);
   if (p <= 0 || buf[p] != '_') {
      vlog.Info("Couldn't find _ before slot number in %s", vname);
      return -1;
   }
   mag_slot = strtol(buf + (p+1), NULL, 10);
   if (mag_slot < 1) {
      vlog.Info("Invalid slot %d in %s", mag_slot, vname);
      clear();
      return -1;
   }
   buf[p] = 0;
   for (n = p - 1; n > 0 && isdigit(buf[n]); n--);
   if (n < 1 || buf[n] != '_') {
      vlog.Info("Couldn't find _ before mag number in %s", vname);
	   clear();
      return -1;
   }
   p = n + 1;
   mag_number = strtol(buf + p, NULL, 10);
   if (mag_number < 1) {
      vlog.Info("Invalid mag number %d in %s", mag_number, vname);
      clear();
      return -1;
   }
   buf[n] = 0;
   if (strlen(buf) == 0) {
      vlog.Info("Empty changer name in %s", vname);
	   clear();
	   return -1;
   }
   strncpy(chgr_name, buf, sizeof(chgr_name));
   return 0;
}

void VolumeLabel::clear()
{
   mag_number = -1;
   mag_slot = -1;
   memset(chgr_name, 0, sizeof(chgr_name));
}

bool VolumeLabel::operator==(const VolumeLabel &b)
{
   if (&b == this) {
      return true;
   }
   if (mag_slot != b.mag_slot) {
      return false;
   }
   if (mag_number != b.mag_number) {
      return false;
   }
   if (strcmp(chgr_name, b.chgr_name)) {
      return false;
   }
   return true;
}

bool VolumeLabel::operator!=(const VolumeLabel &b)
{
   if (&b == this) {
      return false;
   }
   if (mag_slot != b.mag_slot) {
      return true;
   }
   if (mag_number != b.mag_number) {
      return true;
   }
   if (strcmp(chgr_name, b.chgr_name)) {
      return true;
   }
   return false;
}

VolumeLabel& VolumeLabel::operator=(const VolumeLabel &b)
{
   if (&b == this) {
      return *this;
   }
   mag_slot = b.mag_slot;
   mag_number = b.mag_number;
   strncpy(chgr_name, b.chgr_name, sizeof(chgr_name));
   return *this;
}


///////////////////////////////////////////////////
//  Class DriveState
///////////////////////////////////////////////////

DriveState::DriveState()
{
   drive = -1;
}

int DriveState::clear()
{
   vol.clear();
   return save();
}

int DriveState::set(const VolumeLabel &vlabel)
{
   return set(&vlabel);
}

int DriveState::set(const VolumeLabel *vlabel)
{
   if (!vlabel) {
      /* attempt to set bogus volume label */
      clear();
      return -2;
   }
   vol = *vlabel;
   if (vol.mag_number < 1 || vol.mag_slot < 1 || !strlen(vol.chgr_name)) {
      /* attempt to set bogus volume label */
      clear();
      return -2;
   }
   if (save()) {
      /* failed to write state file */
      vol.clear();
      return -1;
   }
   return 0;
}

int DriveState::set(const char *name)
{
   if (vol.set(name)) {
      /* attempt to set bogus volume label */
      clear();
      return -2;
   }
   if (save()) {
      /* failed to write state file */
      vol.clear();
      return -1;
   }
   return 0;
}


int DriveState::save()
{
   FILE *FS;
   char sname[PATH_MAX], vname[256];
   if (drive < 0) {
      /* drive has not been defined */
      return -1;
   }
   snprintf(sname, sizeof(sname), "%s%sstate%d", conf.work_dir, DIR_DELIM, drive);
   FS = fopen(sname, "w");
   if (!FS) {
      /* Unable to write to state file */
      return -1;
   }
   if (!vol.empty()) {
      if (fprintf(FS, "%s\n", vol.GetLabel(vname, sizeof(vname))) < 0) {
         /* I/O error writing state file */
         fclose(FS);
         return -1;
      }
   }
   fclose(FS);
   return 0;
}


int DriveState::restore()
{
   char sname[PATH_MAX], vname[256];
   int magnum, magslot;
   struct stat st;
   FILE *FS;

   vol.clear();
   if (drive < 0 || drive >= conf.virtual_drives) return -1;

   /* Check for existing state file */
   snprintf(sname, sizeof(sname), "%s%sstate%d", conf.work_dir, DIR_DELIM, drive);
   if (stat(sname, &st)) {
      /* state file not found, try to create unloaded state */
      if (save()) {
         /* unable to write state file */
         return -1;
      }
      /* On first use, drive is in unloaded state */
      return 0;
   }
   /* Read loaded volume label from state file */
   FS = fopen(sname, "r");
   if (!FS) return -1;   /* No read permission */
   if (!fgets(vname, sizeof(vname), FS)) {
      if (feof(FS)) {
         vname[0] = '\0';
      } else {
         /* error reading state file */
         fclose(FS);
         return -1;
      }
   }
   fclose(FS);
   /* remove trailing newline */
   strip_whitespace(vname, sizeof(vname));
   /* restore loaded volume label from state file */
   if (!strlen(vname)) {
      /* Last state was unloaded if state file empty */
      return 0;
   }
   if (vol.set(vname)) {
      /* bogus volume label in state file */
      clear();
      return -1;
   }
   return 0;
}


///////////////////////////////////////////////////
//  Class MagazineState
///////////////////////////////////////////////////

MagazineState::MagazineState()
{
   bay_ndx = 0;
   mag_number = 0;
   memset(changer, 0, sizeof(changer));
   memset(mag_name, 0, sizeof(mag_name));
   memset(mountpoint, 0, sizeof(mountpoint));
}

MagazineState::~MagazineState()
{
}

void MagazineState::clear()
{
   mag_number = 0;
   memset(mag_name, 0, sizeof(mag_name));
   memset(mountpoint, 0, sizeof(mountpoint));
   save();
}

/*
 *  Assigns magazine given by 'mag_name' to this magazine bay and determines if
 *  it is mounted, and whether it is initialized and assigned to a autochanger.
 *  If 'mag_name' begins with "UUID:", then it specifies a file system on a
 *  disk partition to be used as the virtual magazine. Otherwise, 'mag_name'
 *  specifies a directory to be used as the virtual magazine. If a UUID is
 *  given, then the system is queried to determine the mountpoint of the
 *  filesystem with the given UUID. On POSIX systems, the 'automount_dir'
 *  specifies the directory under which autofs or some other automount daemon
 *  creates mountpoints for magazines specified by UUID. It is assumed that the
 *  automount is configured to mount partitions at 'automount_dir'/UUID, where
 *  UUID is the partition's filesystem UUID. When both 'automount_dir' is non-blank
 *  and 'mag_name' specifies a UUID, an attempt is made to read the mountpoint
 *  directory in order to cause the partition to be automounted.
 *  Return values are:
 *       0    Magazine is mounted and initialized
 *       1    Magazine is mounted, but is not initialized
 *      -1    system error
 *      -2    parameter error
 *      -3    mag_name not found or not mounted
 *      -5    permission denied
 *      -6    magazine index file is corrupt
 */
int MagazineState::set(const char *mag_name_in, const char *automount_dir)
{
   int rc;
   DIR *dir;
   FILE *fs;
   char tmp[PATH_MAX], magname[PATH_MAX];

   clear();
   if (bay_ndx < 1 || !mag_name_in || !strlen(mag_name_in)) {
      return -2;
   }
   strncpy(magname, mag_name_in, sizeof(magname));
   if (strncasecmp(magname, "UUID:", 5)) {
      /* magazine specified as file system path */
      strncpy(mountpoint, magname, sizeof(mountpoint));
      dir = opendir(mountpoint);
      if (!dir) {
         /* could not open mountpoint dir */
         vlog.Info("MagazineState::set() - opendir error=%d for %s", errno, mountpoint);
         memset(mountpoint, 0 ,sizeof(mountpoint));
         if (errno == EACCES) return -5;
         if (errno == ENOTDIR || errno == ENOENT) return -3;
         return -1;
      }
      closedir(dir);
      rc = ReadMagazineIndex();
      if (rc < 0) {
         /* could not read mountpoint dir */
         vlog.Info("MagazineState::set() - ReadMagazineIndex returned %d for %s", rc, mountpoint);
         memset(mountpoint, 0 ,sizeof(mountpoint));
         return rc;
     }
     strncpy(mag_name, magname, sizeof(mag_name));
     if (save()) {
        /* could not save magazine bay state */
        memset(mountpoint, 0 ,sizeof(mountpoint));
        memset(mag_name, 0 ,sizeof(mag_name));
        memset(changer, 0 ,sizeof(changer));
        mag_number = 0;
        return -1;
     }
     return rc;
   }
   /* magazine specified as UUID */
   /* if using an automount_dir, then it is assumed that autofs is
    * configured to automount the magazine's device at automount_dir/UUID.
    * We first try to open the the magazine's index file to trigger an automount.
    */
   if (automount_dir && strlen(automount_dir)) {
      snprintf(tmp, sizeof(tmp), "%s/%s/index", automount_dir, magname + 5);
      fs = fopen(tmp, "r");
      if (!fs) {
         /* could not open mountpoint dir */
         if (errno == EACCES) {
            vlog.Error("MagazineState::set() - EACCES error opening %s", tmp);
            return -5;
         }
         if (errno == ENOTDIR) {
            vlog.Info("MagazineState::set() - ENOTDIR error opening %s", tmp);
            return -3;
         }
      } else fclose(fs);
   }
   /* In either case, we lookup the mountpoint by UUID */
   rc = GetMountpointFromUUID(mountpoint, sizeof(mountpoint), magname + 5);
   if (rc) {
      /* magazine not mounted or system error */
      vlog.Info("MagazineState::set() - GetMountpointFromUUID returned %d", rc);
      return rc;
   }
   rc = ReadMagazineIndex();
   if (rc < 0) {
      /* magazine not mounted or system error */
      vlog.Info("MagazineState::set() - ReadMagazineIndex returned %d", rc);
      return rc;
   }
   strncpy(mag_name, magname, sizeof(mag_name));
   if (save()) {
      /* could not save magazine bay state */
      memset(mountpoint, 0 ,sizeof(mountpoint));
      memset(mag_name, 0 ,sizeof(mag_name));
      memset(changer, 0 ,sizeof(changer));
      mag_number = 0;
      return -1;
   }
   return rc;
}

int MagazineState::ReadMagazineIndex()
{
   int n;
   FILE *fs;
   char indx[PATH_MAX], buf[256];

   if (!strlen(mountpoint)) return -3;
   /* Read contents of file named 'index' on magazine partition */
   snprintf(indx, sizeof(indx), "%s/index", mountpoint);
   fs = fopen(indx, "r");
   if (!fs) {
      if (errno == EACCES) return -5; /* permission denied */
      if (errno == ENOENT) return 1;  /* index file not found */
      return -1; /* system or i/o error */
   }
   if (!fgets(buf, sizeof(buf), fs)) {
      /* i/o error reading index file */
      fclose(fs);
      return -1; /* could not read index file */
   }
   fclose(fs);

   /* parse changer name and magazine number from index file contents */
   n = strip_whitespace(buf, sizeof(buf));
   if (!n || n > 254) {
      return -6; /* invalid index file contents */
   }
   /* Find magazine index number */
   for (n = strlen(buf) - 1; n > 0 && buf[n] != '_'; n--);
   if (n == 0) {
      return -6; /* invalid index file contents */
   }
   mag_number = strtol(buf + (n + 1), NULL, 10);
   if (mag_number < 1) {
      mag_number = -1;
      return -6; /* invalid index file contents */
   }
   /* Get changer name */
   buf[n] = 0;
   strncpy(changer, buf, sizeof(changer));
   return 0;
}

int MagazineState::save()
{
   FILE *FS;
   char sname[PATH_MAX], vname[256];

   if (bay_ndx < 1) return -1;   /* bay has not been defined */
   snprintf(sname, sizeof(sname), "%s%sbay%d", conf.work_dir, DIR_DELIM, bay_ndx);
   FS = fopen(sname, "w");
   if (!FS) {
      /* Unable to open state file for writing */
      return -1;
   }
   /* Save magazine name (directory or UUID) */
   if (fprintf(FS, "%s\n", mag_name) < 0) {
      /* I/O error writing state file */
      fclose(FS);
      return -1;
   }
   fclose(FS);
   return 0;
}

int MagazineState::restore()
{
   char sname[PATH_MAX], vname[256];
   int magnum;
   struct stat st;
   FILE *FS;

   if (bay_ndx < 1) return -1;
   mag_number = 0;
   memset(mag_name, 0, sizeof(mag_name));
   memset(mountpoint, 0, sizeof(mountpoint));

   /* Check for existing state file */
   snprintf(sname, sizeof(sname), "%s%sbay%d", conf.work_dir, DIR_DELIM, bay_ndx);
   if (stat(sname, &st)) {
      /* state file not found, try to create unloaded state */
      if (save()) {
         /* unable to write state file */
         return -1;
      }
      /* On first use, bay is in unloaded state */
      return 0;
   }
   /* Read magazine name from state file */
   FS = fopen(sname, "r");
   if (!FS) return -1;   /* No read permission */
   if (!fgets(vname, sizeof(vname), FS)) {
      if (ferror(FS)) {
         /* error reading state file */
         fclose(FS);
         return -1;
      }
      vname[0] = '\0';
   }
   fclose(FS);
   /* remove trailing newline */
   strip_whitespace(vname, sizeof(vname));
   /* restore name of last loaded magazine from state file */
   strncpy(mag_name, vname, sizeof(mag_name));
   return 0;
}
