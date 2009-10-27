/* junctions.c
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
 *  Functions to simulate *nix symlinks (to directories only) using
 *  win32 junctions
 */

#include "config.h"
#ifdef HAVE_WINDOWS_H
#include "compat_util.h"
#include "junctions.h"
#include <winioctl.h>

extern char junct_msg[4096];
extern unsigned int junct_err;

/*
 *  Static function to obtain privileges for opening a directory
 */
static DWORD ObtainDirPrivs()
{
   DWORD rc;
   HANDLE hToken = NULL;
   TOKEN_PRIVILEGES tp;

   if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken)) {
      return GetLastError();
   }
   if (!LookupPrivilegeValue(NULL, SE_RESTORE_NAME, &tp.Privileges[0].Luid)) {
      rc = GetLastError();
      CloseHandle(hToken);
      return rc;
   }
   tp.PrivilegeCount = 1;
   tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
   if (!AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), NULL, NULL)) {
      rc = GetLastError();
      CloseHandle(hToken);
      return rc;
   }
   CloseHandle(hToken);
   return 0;
}

/*
 *  Static function to check that 'path' is a directory. If 'path'
 *  does not exist, then create directory named 'path'
 */
static DWORD CheckDirectory(WCHAR *path)
{
   DWORD attr = GetFileAttributesW(path);
   if (attr == INVALID_FILE_ATTRIBUTES) {
      return GetLastError();
   }
   if ((attr & FILE_ATTRIBUTE_DIRECTORY) == 0) {
      return ERROR_BAD_PATHNAME;
   }
   return 0;
}

/*
 *  Create win32 junction point
 */
DWORD CreateJunction(const char *junction_in, const char *path_in)
{
   DWORD rc;
   size_t n, tlen;
   WCHAR *substitute, *printname;
   char buf[16 * 1024], pbuf[2048], *errmsg;
   WCHAR *junction = NULL, *path = NULL;
   size_t junction_sz = 0, path_sz = 0;
   REPARSE_MOUNTPOINT_DATA_BUFFER *reparse = (REPARSE_MOUNTPOINT_DATA_BUFFER*)buf;
   WCHAR tbuf[2048] = L"\\??\\";
   WCHAR *target = (WCHAR*)tbuf;
   HANDLE hDir = NULL;
   BOOL target_is_volume = FALSE;
   junct_err = 0;

   /* Normalize target path */
   if (strlen(path_in) > 4 && path_in[0] == '\\' && (path_in[1] == '?' || path_in[1] == '\\')
         && path_in[2] == '?' && path_in[3] == '\\') {
      /* Leave path alone if specified as absolute path */
      strcpy(buf, "\\??\\");
      strncpy(buf+4, path_in, sizeof(buf) - 4);
   } else {
      /* Copy target path using normalized delimiter */
      for (n = 0; path_in[n]; n++)
      {
         if (path_in[n] == '/') {
            buf[n] = '\\';
         } else {
            buf[n] = path_in[n];
         }
      }
      buf[n] = 0;
      if (buf[0] != '\\' && buf[1] == ':') {
         if (buf[2] == 0) {
            buf[2] = '\\';
            buf[3] = 0;
         }
         if (buf[2] == '\\' && buf[3] == 0) {
            /* Translate drive designator to volume GUID */
            strncpy(pbuf, buf, sizeof(pbuf));
            if (!GetVolumeNameForVolumeMountPointA(pbuf, buf, sizeof(buf))) {
               rc = GetLastError();
               junct_err = rc;
               FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
                     | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, rc,
                     MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                     (LPSTR)&errmsg, 0, NULL);
               snprintf(junct_msg, sizeof(junct_msg), "%s", errmsg);
               return rc;
            }
            buf[1] = '?';
            target_is_volume = TRUE;
         }
      }
   }
   /* Convert target path to UTF-16 */
   if (!UTF8ToUTF16(buf, &path, &path_sz)) {
      junct_err = ERROR_BAD_PATHNAME;
      snprintf(junct_msg, sizeof(junct_msg), "bad path %s", buf);
      return ERROR_BAD_PATHNAME;
   }

   /* Convert junction path to UTF-16 */
   if (!UTF8ToUTF16(junction_in, &junction, &junction_sz)) {
      junct_err = ERROR_BAD_PATHNAME;
      snprintf(junct_msg, sizeof(junct_msg), "bad path %s", junction_in);
      free(path);
      return ERROR_BAD_PATHNAME;
   }

   /* If target path is absolute path, use as is */
   if (path[0] == L'\\' && path[1] == L'?' && path[2] == L'?' && path[3] == L'\\') {
      target = path;
   } else {
      /* otherwise, make it an absolute path */
      wcsncat(target, path, sizeof(tbuf)/sizeof(WCHAR) - wcslen(target) - 1);
      if (target[wcslen(target)-1] != L'\\') {
         wcsncat(target, L"\\", sizeof(tbuf)/sizeof(WCHAR) - wcslen(target) - 1);
      }
   }

   /* Check target is directory */
   rc = 0;
   if (!target_is_volume) {
      rc = CheckDirectory(target);
   }
   if (rc) {
      snprintf(junct_msg, sizeof(junct_msg), "target %s not a directory", target);
      junct_err = rc;
      free(junction);
      free(path);
      return rc;
   }

   /* Check junction path is directory */
   rc = CheckDirectory(junction);
   if (rc == ERROR_FILE_NOT_FOUND || rc == ERROR_PATH_NOT_FOUND) {
      /* Try to create directory if non-existing */
      rc = 0;
      if (!CreateDirectoryW(junction, NULL)) {
         rc = GetLastError();
         junct_err = rc;
         FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
               | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, rc,
               MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
               (LPSTR)&errmsg, 0, NULL);
         snprintf(junct_msg, sizeof(junct_msg), "%s", errmsg);
      }
   }
   if (rc) {
      junct_err = rc;
      snprintf(junct_msg, sizeof(junct_msg), "path not found %s", path);
      free(junction);
      free(path);
      return rc;
   }

   /* Obtain privilege for opening directory */
   rc = ObtainDirPrivs();
   if (rc) {
      free(junction);
      free(path);
      junct_err = rc;
      snprintf(junct_msg, sizeof(junct_msg), "failed to obtain privs");
      return rc;
   }

   /* Open the directory */
   hDir = CreateFileW(junction, GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
         FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, NULL);
   if (hDir == INVALID_HANDLE_VALUE) {
      rc = GetLastError();
      junct_err = rc;
      FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
            | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, junct_err,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPSTR)&errmsg, 0, NULL);
      snprintf(junct_msg, sizeof(junct_msg), "%s", errmsg);
      free(junction);
      free(path);
      return rc;
   }

   /* Build the reparse data buffer */
   tlen = wcslen(target) * 2;
   memset(buf, 0, sizeof(buf));
   reparse->ReparseTag = IO_REPARSE_TAG_MOUNT_POINT;
   reparse->SubstituteNameOffset = 0;
   reparse->SubstituteNameLength = tlen;
   substitute = (WCHAR*)reparse->ReparseTarget;
   wcsncpy(substitute, target, 4096);
   reparse->PrintNameOffset = tlen + 2;
   printname = substitute + ((tlen+2) / sizeof(WCHAR));
   if (target_is_volume) {
      reparse->PrintNameLength = 0;
      *printname = 0;
   } else {
      reparse->PrintNameLength = wcslen(path) * sizeof(WCHAR);
      wcsncpy(printname, path, 4096);
   }
   reparse->ReparseDataLength = reparse->SubstituteNameLength + reparse->PrintNameLength + 12;

   /* Set the junction's reparse data */
   if (!DeviceIoControl(hDir, FSCTL_SET_REPARSE_POINT, reparse, reparse->ReparseDataLength + REPARSE_DATA_BUFFER_HEADER_SIZE,
               NULL, 0, &rc, NULL)) {
      rc = GetLastError();
      junct_err = rc;
      FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
            | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, junct_err,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPSTR)&errmsg, 0, NULL);
      snprintf(junct_msg, sizeof(junct_msg), "%s", errmsg);
      CloseHandle(hDir);
      RemoveDirectoryW(junction);
      free(junction);
      free(path);
      return rc;
   }

   CloseHandle(hDir);
   free(junction);
   free(path);
   return 0;
}

/*
 *  When 'path' is a reparse point, return zero put the substitute directory
 *  name in 'target'. Otherwise, return win32 error code and leave
 *  target untouched.
 */
DWORD ReadReparsePoint(const char *path_in, char *target_out, size_t target_out_size)
{
   DWORD rc, dwRetLen, vpath_len;
   int n;
   HANDLE hFile;
   char buf[MAXIMUM_REPARSE_DATA_BUFFER_SIZE];
   WCHAR *p, *vpath, *path = NULL;
   char *target = NULL;
   size_t path_sz = 0, target_sz = 0;
   REPARSE_MOUNTPOINT_DATA_BUFFER* rdata = (REPARSE_MOUNTPOINT_DATA_BUFFER*)buf;

   /* Convert path_in to UTF-16 */
   if (!UTF8ToUTF16(path_in, &path, &path_sz)) {
      return ERROR_BAD_PATHNAME;
   }

   /* Open the file or directory with no special access permissions, otherwise Vista or higher
    * will not allow it */
   hFile = CreateFileW(path, FILE_READ_EA, FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
         NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT,
         NULL);
   if (hFile == INVALID_HANDLE_VALUE) {
      rc = GetLastError();
      free(path);
      return rc;
   }

   /* Get the reparse data via I/O control */
   if (!DeviceIoControl(hFile, FSCTL_GET_REPARSE_POINT, NULL, 0, rdata, sizeof(buf), &dwRetLen, NULL)) {
      rc = GetLastError();
      CloseHandle(hFile);
      free(path);
      return rc;
   }
   CloseHandle(hFile);
   free(path);

   /* Check that it is a Microsoft reparse tag, otherwise it is some 3rd party file
    * system and we don't understand what the reparse data relates to. */
   if (IsReparseTagMicrosoft(rdata->ReparseTag)) {
      /* Get target of a mountpoint (windows XP or higher) */
      if (rdata->ReparseTag == IO_REPARSE_TAG_MOUNT_POINT) {
         /* Check for volume name (as opposed to directory path) */
         p = (WCHAR*)rdata->ReparseTarget;
         if (p[0] == '\\' && p[1] == '?' && p[2] == '?') p[1] = '\\';
         if (wcsncmp(p, L"\\\\?\\Volume{", 11) == 0) {
            /* Convert volume name to mountpoint path (ie. drive letter) */
            vpath = (WCHAR*)malloc(8192);
            if (GetVolumePathNamesForVolumeNameW(p, vpath, 4096, &vpath_len)) {
               if (!UTF16ToUTF8(vpath, &target, &target_sz)) {
                  free(vpath);
                  return ERROR_INVALID_PARAMETER;
               }
            }
            free(vpath);
         }
         if (!target && !UTF16ToUTF8(p, &target, &target_sz)) {
            return ERROR_INVALID_PARAMETER;
         }
         strncpy(target_out, target, target_out_size);
         free(target);
         n = strlen(target_out);
         if (n && target_out[n-1] == '\\') {
            target_out[n-1] = 0;
         }
      } else return ERROR_INVALID_PARAMETER; /* not a mountpoint */
   } else return ERROR_INVALID_PARAMETER; /* unknown reparse data type */
   return 0;
}

/*
 *  When 'path' is a reparse point, return TRUE. Otherwise FALSE.
 */
BOOL IsReparsePoint(const char *path_in) {
   DWORD attr;
   WCHAR *path = NULL;
   size_t path_sz = 0;
   BOOL rc = FALSE;

   if (UTF8ToUTF16(path_in, &path, &path_sz)) {
      attr = GetFileAttributesW(path);
      if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_REPARSE_POINT)
            == FILE_ATTRIBUTE_REPARSE_POINT) {
         rc = TRUE;
      }
      free(path);
   }
   return rc;
}

/*
 *  Remove reparse data from directory 'path'
 */
DWORD DeleteReparsePoint(const char *path)
{
   DWORD rc;
   HANDLE hDir;
   char buf[16 * 1024];
   REPARSE_MOUNTPOINT_DATA_BUFFER *reparse = (REPARSE_MOUNTPOINT_DATA_BUFFER*)buf;
   WCHAR *junction = NULL;
   size_t junction_sz = 0;

   if (!UTF8ToUTF16(path, &junction, &junction_sz)) {
      return ERROR_BAD_PATHNAME;
   }

   /* Obtain privilege for opening directory */
   rc = ObtainDirPrivs();
   if (rc) {
      free(junction);
      return rc;
   }

   /* Open the directory */
   hDir = CreateFileW(junction, GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
         FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, NULL);
   if (hDir == INVALID_HANDLE_VALUE) {
      rc = GetLastError();
      free(junction);
      return rc;
   }

   /* Build the reparse data buffer */
   memset(buf, 0, sizeof(buf));
   reparse->ReparseTag = IO_REPARSE_TAG_MOUNT_POINT;

   /* Unset the junction's reparse data */
   if (!DeviceIoControl(hDir, FSCTL_DELETE_REPARSE_POINT, reparse, REPARSE_DATA_BUFFER_HEADER_SIZE,
               NULL, 0, &rc, NULL)) {
      rc = GetLastError();
      CloseHandle(hDir);
      free(junction);
      return rc;
   }

   CloseHandle(hDir);
   free(junction);
   return 0;
}

/*
 *  Emulates the POSIX.1-2001 symlink call on win32, but only for directories
 */
int symlink(const char *target_dir, const char *path)
{
   DWORD rc = CreateJunction(path, target_dir);
   errno = w32errno(rc);
   if (rc) {
      return -1;
   }
   return 0;
}

/*
 *  Emulates a unlink(path_to_symlink) call. Needed because,
 *  on win32, reparse data is tied to a directory, rather than a file,
 *  so a normal unlink().
 */
int unlink_symlink(const char *path)
{
   WCHAR *wpath = NULL;
   size_t wpath_sz = 0;
   DWORD rc = DeleteReparsePoint(path);
   errno = w32errno(rc);
   if (rc) {
      return -1;
   }
   if (!UTF8ToUTF16(path, &wpath, &wpath_sz)) {
      rc = ERROR_BAD_PATHNAME;
      errno = w32errno(rc);
      return -1;
   }
   if (!RemoveDirectoryW(wpath)) {
      rc = GetLastError();
      errno = w32errno(rc);
      free(wpath);
      return -1;
   }
   free(wpath);
   return 0;
}

/*
 *  Emulates a POSIX.1-2001 readlink() function on win32.
 */
ssize_t readlink(const char *path, char *buf, size_t bufsiz)
{
   char target[2048];
   DWORD rc = ReadReparsePoint(path, target, sizeof(target));
   if (rc) {
      errno = w32errno(rc);
      return -1;
   }
   if (strncmp(target, "\\\\?\\", 4) == 0) {
      strncpy(buf, target + 4, bufsiz);
   } else {
      strncpy(buf, target, bufsiz);
   }
   return strlen(buf);
}
#endif
