/* uuidlookup.c
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


/*
 *  Locates disk partition containing file system with UUID given by 'uuid_str'.
 *  If found, and the filesystem is mounted, returns the first mountpoint found in
 *  'mountp'. On success, returns zero. On error, returns negative value :
 *      -1    system error
 *      -2    parameter error
 *      -3    volume with given uuid not found
 *      -4    'mountp' buffer too small
 */
static int GetDevMountpoint(char *mountp, size_t mountp_sz, const char *devname)
{
   FILE *fs;
#ifdef HAVE_MNTENT_H
#ifdef HAVE_GETMNTENT_R
   char *buf;
   struct mntent ent;
#else
   struct mntent *ent;
#endif
#endif
   int rc = -3;

   mountp[0] = '\0';
   if (!devname || !strlen(devname)) return -2;

#ifdef HAVE_MNTENT_H
   fs = setmntent("/proc/mounts", "r");
   if (fs == NULL) {
      /* if system doesn't have /proc then try /etc/mtab */
      fs = setmntent("/etc/mtab", "r");
   }
   if (fs == NULL) {
      return -1; /* unknown non-POSIX system?? */
   }
#ifdef HAVE_GETMNTENT_R
   buf = malloc(4096);
   while (getmntent_r(fs, &ent, buf, 2048)) {
      if (strcasecmp(devname, ent.mnt_fsname) == 0) {
         if (strlen(ent.mnt_dir) < mountp_sz) {
            strncpy(mountp, ent.mnt_dir, mountp_sz);
            rc = 0;
         } else {
            rc = -4;  /* mountp buffer too small */
         }
         break;
      }
   }
   free(buf);
#else
   ent = getmntent(fs);
   while (ent)
   {
      if (strcasecmp(devname, ent->mnt_fsname) == 0) {
         if (strlen(ent->mnt_dir) < mountp_sz) {
            strncpy(mountp, ent->mnt_dir, mountp_sz);
            rc = 0;
         } else {
            rc = -4;  /* mountp buffer too small */
         }
         break;
      }
   }
#endif
   endmntent(fs);
   return rc;

#else
#endif
}

/*
 *  Locates disk partition containing file system with UUID given by 'uuid_str'.
 *  If found, and the filesystem is mounted, returns the first mountpoint found in
 *  'mountp'. On success, returns zero. On error, returns negative value :
 *      -1    system error
 *      -2    parameter error
 *      -3    volume with given uuid not found
 *      -4    'mountp' buffer too small
 */
int GetMountpointFromUUID(char *mountp, size_t mountp_sz, const char *uuid_str)
{
   char devname[256], *dev_name = NULL;

   /* Get device with requested UUID from libblkid */
   dev_name = blkid_get_devname(NULL, "UUID", uuid_str);
   if (!dev_name) {
      return -3;  /* no device with UUID found */
   }
   strncpy(devname, dev_name, sizeof(devname));
   free(dev_name);

   /* find mountpoint for device */
   return GetDevMountpoint(mountp, mountp_sz, devname);
}
#endif

