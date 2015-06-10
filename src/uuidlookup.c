/* uuidlookup.c
 *
 *  This file is part of vchanger by Josh Fisher.
 *
 *  vchanger copyright (C) 2008-2013 Josh Fisher
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
 */

#include "config.h"
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#ifdef HAVE_CTYPE_H
#include <ctype.h>
#endif

#include "uuidlookup.h"

#ifdef HAVE_WINDOWS_H

#include "targetver.h"
#include <windows.h>
#include <winioctl.h>
#include "win32_util.h"

/*
 *  Locates disk partition containing file system with volume serial number
 *  given by 'uuid_str'. If found, and the filesystem is mounted, returns the
 *  first mountpoint found in 'mountp'. On success, returns zero. On error,
 *  returns negative value :
 *      -1    system error
 *      -2    parameter error
 *      -3    volume with given uuid not found
 *      -4    'mountp' buffer too small
 */
int GetMountpointFromUUID(char *mountp, size_t mountp_sz, const char *uuid_str)
{
   HANDLE hnd, hFile;
   wchar_t volname[2048];
   wchar_t pname[2048];
   char tmp[128], *utf8name = NULL;
   BOOL rc;
   DWORD volser, count, eov, uuid;
   size_t n;

   /* Convert uuid_str to binary. Note that win32 volume serial number is not
    * in standard UUID format, but is the form xxxx-xxxx, where x is a hex digit.
    * Win32 returns volume serial number as 32-bit unsigned binary.
    */
   mountp[0] = '\0';
   strncpy(tmp, uuid_str, sizeof(tmp));
   if (strlen(uuid_str) != 9 || tmp[4] != '-') {
      /* invalid volume serial number */
      return -2;
   }
   for (n = 4; n < 9; n++) {
      tmp[n] = tmp[n+1];
   }
   uuid = strtoul(tmp, NULL, 16);

   /* Enumerate volumes on this system looking for matching volume serial number.
    * Skip unmounted volumes.
    */
   hnd = FindFirstVolumeW(volname, 2048);
   if (hnd == INVALID_HANDLE_VALUE) {
      /* out of memory?? permissions problem?? */
      return -1;
   }
   rc = TRUE;
   while (rc) {
      /* make sure it's a proper volume name */
      if (volname[0] == L'\\' && volname[1] == L'\\' && volname[2] == L'?'
            && volname[3] == L'\\' && volname[eov] == L'\\') {
         /* Get volume's first mountpoint */
         if (GetVolumePathNamesForVolumeNameW(volname, pname, 2048, &count)) {
            /* retrieve serial from volume information */
            if (GetVolumeInformationW(pname, NULL, 0, &volser, NULL, NULL, NULL, 0)) {
               /* see if this volume matches requested UUID */
               if (volser == uuid) {
                  /* found matching volume. UTF16 mountpoint path is in pname. */
                  break;
               }
            }
         }
      }
      /* get next volume name */
      rc = FindNextVolumeW(hnd, volname, 2048);
   }
   FindVolumeClose(hnd);
   if (!rc) {
      return -3;  /* volume not found */
   }

   /* Convert UTF16 mountpoint path to UTF8 */
   UTF16ToAnsi(pname, &utf8name, &n);
   if (n > mountp_sz) {
      free(utf8name);
      return -4;  /* mountp buffer too small */
   }
   strncpy(mountp, utf8name, mountp_sz);
   free(utf8name);
   return 0;
}

#else

#ifdef HAVE_MNTENT_H
#include <mntent.h>

/*
 *  Lookup mount point for device 'devname' and place in 'mountp'
 *  using SunOS 4.1.3+ function getmntent().
 *  On success, returns zero. On error, returns negative value :
 *      -1    system error
 *      -2    parameter error
 *      -4    devname not mounted
 *      -5    mountp buffer too small
 */
static int GetDevMountpoint(char *mountp, size_t mountp_sz, const char *devname)
{
   size_t n;
   FILE *fs;
   struct mntent *ent;
   int rc;

   mountp[0] = '\0';
   if (!mountp_sz || !devname || !strlen(devname)) return -2;

   /* Read either /proc/mounts or /etc/mtab depending on build system's glibc */
   fs = setmntent(_PATH_MOUNTED, "r");
   if (fs == NULL) return -1; /* unknown non-POSIX system?? */

   rc = -4;
   ent = getmntent(fs);
   while (ent)
   {
      if (strcasecmp(devname, ent->mnt_fsname) == 0) {
         n = strlen(ent->mnt_dir);
         if (n >= mountp_sz) {
            rc = -5;
            break;
         }
         memmove(mountp, ent->mnt_dir, n);
         mountp[n] = 0;
         rc = 0;
         break;
      }
      ent = getmntent(fs);
   }
   endmntent(fs);
   return rc;
}

#else

#ifdef HAVE_GETFSSTAT

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#ifdef HAVE_SYS_MOUNT_H
#include <sys/mount.h>
#endif
#ifdef HAVE_SYS_UCRED_H
#include <sys/ucred.h>
#endif

/*
 *  Lookup mount point for device 'devname' and place in 'mountp'
 *  using BSD4.4+ function getfsstat().
 *  On success, returns zero. On error, returns negative value :
 *      -1    system error
 *      -2    parameter error
 *      -4    devname not mounted
 */
static int GetDevMountpoint(char *mountp, size_t mountp_sz, const char *devname)
{
   struct statfs *fs;
   int mcount, fs_size, n, rc = -4;

   mountp[0] = '\0';
   if (!devname || !strlen(devname)) return -2;
   mcount = getfsstat(NULL, 0, MNT_WAIT);
   if (mcount < 1) return -1;
   fs_size = (mcount + 1) * sizeof(struct statfs);
   fs = (struct statfs*)malloc(fs_size);
   if (!fs) return -1;
   mcount = getfsstat(fs, fs_size, MNT_NOWAIT);
   for (n = 0; n < mcount; n++)
   {
      if (strcasecmp(devname, fs[n].f_mntfromname) == 0) {
         strncpy(mountp, fs[n].f_mntonname, mountp_sz);
         rc = 0;
      }
   }
   free(fs);
   return rc;
}

#else

/*
 *  System has neither getmntent() nor getfsstat(). Return system error.
 */
static int GetDevMountpoint(char *mountp, size_t mountp_sz, const char *devname)
{
   return -1;
}

#endif  /* HAVE_GETFSSTAT */
#endif  /* HAVE_MNTENT_H */

#ifdef HAVE_LIBUDEV_H
#include <libudev.h>

/*
 *  Locates disk partition containing file system with UUID given by 'uuid_str'.
 *  If found, and the filesystem is mounted, returns the first mountpoint found in
 *  string 'mountp'. On success, returns zero. On error, returns negative value :
 *      -1    system error
 *      -2    parameter error
 *      -3    filesystem with given uuid not found
 *      -4    filesystem not mounted
 *      -5    mountp buffer too small
 */
int GetMountpointFromUUID(char *mountp, size_t mountp_sz, const char *uuid_str)
{
   struct udev *udev;
   struct udev_enumerate *enumerate;
   struct udev_list_entry *devices, *dev_list_entry;
   struct udev_device *dev;
   int rc = -3;
   const char *dev_name, *path, *uuid;
   size_t n, pos, dev_name_len;
   char devlink[4096];

   if (!mountp || !mountp_sz) return -2;
   if (!uuid_str || !strlen(uuid_str)) return -2;

   /* Enumerate devices with a filesystem UUID property */
   udev = udev_new();
   if (!udev) return -1;
   enumerate = udev_enumerate_new(udev);
   udev_enumerate_add_match_property(enumerate, "ID_FS_UUID", uuid_str);
   udev_enumerate_scan_devices(enumerate);
   devices = udev_enumerate_get_list_entry(enumerate);
   /* Find device with specified UUID */
   udev_list_entry_foreach(dev_list_entry, devices) {
      path = udev_list_entry_get_name(dev_list_entry);
      dev = udev_device_new_from_syspath(udev, path);
      uuid = udev_device_get_property_value(dev, "ID_FS_UUID");
      if (uuid && strcasecmp(uuid, uuid_str) == 0) {
         /* Found device with this UUID, so get its kernel device node */
         dev_name = udev_device_get_property_value(dev, "DEVNAME");
         if (dev_name == NULL) {
            /* Failed to get kernel device node */
            break;
         }
         /* Lookup mountpoint of the kernel device node */
         rc = GetDevMountpoint(mountp, mountp_sz, dev_name);
         if (rc == 0) break;
         /* If not mounted as the DEVNAME, also check if mounted as
          * a device alias name from DEVLINKS */
         dev_name = udev_device_get_property_value(dev, "DEVLINKS");
         if (dev_name == NULL) {
            /* Failed to get device alias links */
            break;
         }
         dev_name_len = strlen(dev_name);
         pos = 0;
         while (rc == -4 && pos < dev_name_len) {
            for (n = pos; n < dev_name_len && !isblank(dev_name[n]); n++) ;
            n -= pos;
            memmove(devlink, dev_name + pos, n);
            devlink[n] = 0;
            rc = GetDevMountpoint(mountp, mountp_sz, devlink);
            pos += n;
            while (pos < dev_name_len && isblank(dev_name[pos])) ++pos;
         }
         break;
      }
   }
   udev_enumerate_unref(enumerate);
   udev_unref(udev);
   return rc;
}

#else

#ifdef HAVE_BLKID_BLKID_H
#include <blkid/blkid.h>

/*
 *  Locates disk partition containing file system with UUID given by 'uuid_str'.
 *  If found, and the filesystem is mounted, returns the first mountpoint found in
 *  string 'mountp'. On success, returns zero. On error, returns negative value :
 *      -1    system error
 *      -2    parameter error
 *      -3    volume with given uuid not found
 *      -4    volume not mounted
 *      -5    mountp buffer too small
 */
int GetMountpointFromUUID(char *mountp, size_t mountp_sz, const char *uuid_str)
{
   int rc;
   char *dev_name;

   if (!mountp || !mountp_sz) return -2;
   if (!uuid_str || !strlen(uuid_str)) return -2;

   /* Get device with requested UUID from libblkid */
#ifdef HAVE_BLKID_EVALUATE_TAG
   dev_name = blkid_evaluate_tag("UUID", uuid_str, NULL);
#else
   dev_name = blkid_get_devname(NULL, "UUID", uuid_str);
#endif
   if (!dev_name) return -3;  /* no device with UUID found */

   /* find mount point for device */
   rc = GetDevMountpoint(mountp, mountp_sz, dev_name);
   free(dev_name);
   return rc;
}

#else

/*
 *  If built without libudev or libblkid support, then UUID lookup is a system error
 */
int GetMountpointFromUUID(char *mountp, size_t mountp_sz, const char *uuid_str)
{
   return -1;
}

#endif  /* HAVE_BLKID_BLKID_H */
#endif  /* HAVE_LIBUDEV_H */

#endif  /* HAVE_WINDOWS_H */
