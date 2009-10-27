/* changerstate.cpp
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
 *  Provides a class to track state of virtual drives using files in
 *  the vchanger state directory.
*/

#include <stdio.h>
#include "vchanger.h"
#include "util.h"
#include "changerstate.h"
#include "uuidlookup.h"
#include <sys/stat.h>


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
   buf[0] = 0;
   if (mag_slot < 0 || mag_number < 1 || !strlen(chgr_name)) {
      return buf;
   }
   snprintf(buf, buf_sz, "%s_%04d_%04d", chgr_name, mag_number, mag_slot);
   return buf;
}


int VolumeLabel::set(const char *chgr, int magnum, int magslot)
{
   clear();
   if (!chgr || !strlen(chgr) || magnum < 1 || magslot < 1) {
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
   if (!vname) {
      return -1;
   }
   len = strlen(vname);
   if (len < 11) {
      return -1;
   }
   strncpy(buf, vname, sizeof(buf));
   for (p = len - 1; p > 5 && buf[p] != '_'; p--);
   if (p < 6) {
      return -1;
   }
   mag_slot = strtol(buf + (p+1), NULL, 10);
   buf[p] = 0;
   for (n = p - 1; n > 0 && buf[n] != '_'; n--);
   if (n < 1) {
	   clear();
      return -1;
   }
   p = n + 1;
   mag_number = strtol(buf + p, NULL, 10);
   buf[n] = 0;
   strip_whitespace(buf, sizeof(buf));
   if (strlen(buf) == 0 || mag_number < 1 || mag_slot < 0) {
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
   statedir[0] = 0;
   is_set = false;
}

DriveState::~DriveState()
{
}

int DriveState::unset()
{
   vol.clear();
   is_set = false;
   return save();
}

int DriveState::set(const VolumeLabel &vlabel)
{
   vol = vlabel;
   if (vol.mag_number < 1 || vol.mag_slot < 0 || !strlen(vol.chgr_name)) {
      /* attempt to set bogus volume label */
      unset();
      return -2;
   }
   if (save()) {
      /* failed to write state file */
      vol.clear();
      is_set = false;
      return -1;
   }
   is_set = true;
   return 0;
}

int DriveState::set(const VolumeLabel *vlabel)
{
   if (!vlabel) {
      /* attempt to set bogus volume label */
      unset();
      return -2;
   }
   vol = *vlabel;
   if (vol.mag_number < 1 || vol.mag_slot < 0 || !strlen(vol.chgr_name)) {
      /* attempt to set bogus volume label */
      unset();
      return -2;
   }
   if (save()) {
      /* failed to write state file */
      vol.clear();
      is_set = false;
      return -1;
   }
   is_set = true;
   return 0;
}

int DriveState::set(const char *name)
{
   if (vol.set(name)) {
      /* attempt to set bogus volume label */
      unset();
      return -2;
   }
   if (save()) {
      /* failed to write state file */
      vol.clear();
      is_set = false;
      return -1;
   }
   is_set = true;
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
   snprintf(sname, sizeof(sname), "%s%sstate%d", statedir, DIR_DELIM, drive);
   FS = fopen(sname, "w");
   if (!FS) {
      /* Unable to write to state file */
      return -1;
   }
   if (fprintf(FS, "%s\n", vol.GetLabel(vname, sizeof(vname))) < 0) {
      /* I/O error writing state file */
      fclose(FS);
      return -1;
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
   is_set = false;
   if (drive < 0) {
      return -1;
   }
   /* Check for existing state file */
   snprintf(sname, sizeof(sname), "%s%sstate%d", statedir, DIR_DELIM, drive);
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
   if (!FS) {
      /* No read permission */
      return -1;
   }
   if (!fgets(vname, sizeof(vname), FS)) {
      /* error reading state file */
      fclose(FS);
      return -1;
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
      unset();
      return -1;
   }
   is_set = true;
   return 0;
}


///////////////////////////////////////////////////
//  Class MagazineState
///////////////////////////////////////////////////

MagazineState::MagazineState()
{
   mag_number = -1;
   memset(changer, 0, sizeof(changer));
   memset(mountpoint, 0, sizeof(mountpoint));
}

MagazineState::~MagazineState()
{
}

void MagazineState::clear()
{
   mag_number = -1;
   memset(changer, 0, sizeof(changer));
   memset(mountpoint, 0, sizeof(mountpoint));
}

/*
 *  Assigns magazine given by 'magname' to this magazine and determines if
 *  it is mounted, and whether it is initialized and assigned to a autochanger.
 *  If 'magname' begins with "UUID:", then it specifies a file system on a
 *  disk partition to be used as the virtual magazine. Otherwise, 'magname'
 *  specifies a directory to be used as the virtual magazine. If a UUID is
 *  given, then the system is queried to determine the mountpoint of the
 *  filesystem with the given UUID. On POSIX systems, the 'automount_dir'
 *  specifies the directory under which autofs creates mountpoints for
 *  magazines specified by UUID. If given, then a stat is performed on
 *  the directory {automount_dir}/{UUID value} before checking whether the
 *  device is mounted. This allows determining the mountpoint of magazine
 *  partitions being handled by autofs.
 *  Return values are:
 *       0    Magazine is mounted (may or may not be initialized)
 *      -1    system error
 *      -2    parameter error
 *      -3    filesystem not found
 */
int MagazineState::set(const char *mag_name, const char *automount_dir)
{
   int rc;
   struct stat st;
   char tmp[PATH_MAX], magname[PATH_MAX];

   clear();
   if (!mag_name || !strlen(mag_name)) {
      return -1;
   }
   strncpy(magname, mag_name, sizeof(magname));
   if (strncasecmp(magname, "UUID:", 5)) {
      /* magazine specified as file system path */
      strncpy(mountpoint, magname, sizeof(mountpoint));
      if (stat(mountpoint, &st)) {
         /* path not found */
         memset(mountpoint, 0 ,sizeof(mountpoint));
         return -3;
      }
   } else {
      /* magazine specified as UUID */
      if (automount_dir && strlen(automount_dir)) {
         /* if using an automount_dir, do a stat on the suspected mountpoint
          * to force autofs to mount the partition
          */
         snprintf(tmp, sizeof(tmp), "%s/%s", automount_dir, magname + 5);
         stat(tmp, &st);
      }
      rc = GetMountpointFromUUID(mountpoint, sizeof(mountpoint), magname + 5);
      if (rc) {
         /* magazine not mounted or system error */
         memset(mountpoint, 0, sizeof(mountpoint));
         return rc;
      }
   }
   if (ReadMagazineIndex(changer, sizeof(changer), mag_number)) {
      /* magazine partition is mounted, but has not been initialized */
      return 0;
   }
   return 0;
}

int MagazineState::ReadMagazineIndex(char *cname, size_t cname_sz, int &magnum)
{
   int n;
   FILE *fs;
   char indx[PATH_MAX], buf[256];

   memset(cname, 0, cname_sz);
   magnum = 0;
   if (!strlen(mountpoint)) return -1;
   snprintf(indx, sizeof(indx), "%s/index", mountpoint);
   fs = fopen(indx, "r");
   if (!fs) return -1;
   if (!fgets(buf, sizeof(buf), fs)) {
      /* i/o error reading index file */
      fclose(fs);
      return -1; /* could not read index file */
   }
   fclose(fs);
   n = strip_whitespace(buf, sizeof(buf));
   if (!n || n > 254) {
      return -2; /* invalid index file contents */
   }
   /* Find magazine index number */
   for (n = strlen(buf) - 1; n > 0 && buf[n] != '_'; n--);
   if (n == 0) {
      return -2; /* invalid index file contents */
   }
   magnum = strtol(buf + (n + 1), NULL, 10);
   if (magnum < 1) {
      return -2; /* invalid index file contents */
   }
   /* Get changer name */
   buf[n] = 0;
   strncpy(cname, buf, cname_sz);
   return 0;
}
