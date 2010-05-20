/* uuidlookup.c
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
 */

#include "config.h"
#include "compat_util.h"
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_WINDOWS_H
#include <winioctl.h>
#else
#ifdef HAVE_UUID_UUID_H
#include <uuid/uuid.h>
#endif
#ifdef HAVE_BLKID_BLKID_H
#include <blkid/blkid.h>
#endif
#ifdef HAVE_MNTENT_H
#include <mntent.h>
#else
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/ucred.h>
#endif
#endif
#include "uuidlookup.h"

#ifdef HAVE_WINDOWS_H
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
   UTF16ToUTF8(pname, &utf8name, &n);
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

/*
 *  Lookup mount point for device 'devname' and place in 'mountp'.
 *  On success, returns zero. On error, returns negative value :
 *      -1    system error
 *      -2    parameter error
 *      -3    devname not mounted
 */
static int GetDevMountpoint(char *mountp, size_t mountp_sz, const char *devname)
{
   FILE *fs;
#ifdef HAVE_GETMNTENT_R
   char *buf;
   struct mntent ent;
#else
   struct mntent *ent;
#endif
   int rc = -3;

   mountp[0] = '\0';
   if (!devname || !strlen(devname)) return -2;

   fs = setmntent("/proc/mounts", "r");
   if (fs == NULL) {
      /* if system doesn't have /proc then try /etc/mtab */
      fs = setmntent("/etc/mtab", "r");
   }
   if (fs == NULL) return -1; /* unknown non-POSIX system?? */

#ifdef HAVE_GETMNTENT_R
   buf = malloc(4096);
   if (!buf) return -1;
   while (getmntent_r(fs, &ent, buf, 2048)) {
      if (strcasecmp(devname, ent.mnt_fsname) == 0) {
         strncpy(mountp, ent.mnt_dir, mountp_sz);
         rc = 0;
         break;
      }
   }
   free(buf);
#else
   ent = getmntent(fs);
   while (ent)
   {
      if (strcasecmp(devname, ent->mnt_fsname) == 0) {
         strncpy(mountp, ent->mnt_dir, mountp_sz);
         rc = 0;
         break;
      }
   }
#endif
   endmntent(fs);
   return rc;
}

#else

/*
 *  Lookup mount point for device 'devname' and place in 'mountp'.
 *  On success, returns zero. On error, returns negative value :
 *      -1    system error
 *      -2    parameter error
 *      -3    devname not mounted
 */
static int GetDevMountpoint(char *mountp, size_t mountp_sz, const char *devname)
{
   struct statfs *fs;
   int mcount, fs_size, n, rc = -3;

   mountp[0] = '\0';
   if (!devname || !strlen(devname)) return -2;
   mcount = getfsstat(NULL, 0, MNT_NOWAIT);
   if (mcount < 1) return -1;
   fs_size = (mcount + 1) * sizeof(struct statfs);
   fs = (struct statfs*)malloc(fs_size);
   if (!fs) return -1;
   mcount = getfsstat(fs, fs_size, MNT_NOWAIT);
   for (n = 0; n < mcount; n++)
   {
      if (strcasecmp(devname, fs[n].f_mntfromname) == 0) {
         strncpy(mountp, fs[n].f_mnttoname, mountp_sz);
         rc = 0;
      }
   }
   free(fs);
   return rc;
}
#endif

/*
 *  Locates disk partition containing file system with UUID given by 'uuid_str'.
 *  If found, and the filesystem is mounted, returns the first mountpoint found in
 *  string 'mountp'. On success, returns zero. On error, returns negative value :
 *      -1    system error
 *      -2    parameter error
 *      -3    volume with given uuid not found
 *      -4    volume not mounted
 */
int GetMountpointFromUUID(char *mountp, size_t mountp_sz, const char *uuid_str)
{
   int rc;
   char *dev_name;

   if (!uuid_str || !strlen(uuid_str)) return -2;

   /* Get device with requested UUID from libblkid */
   dev_name = blkid_get_devname(NULL, "UUID", uuid_str);
   if (!dev_name) return -3;  /* no device with UUID found */

   /* find mount point for device */
   rc = GetDevMountpoint(mountp, mountp_sz, dev_name);
   free(dev_name);
   return rc;
}

#endif

