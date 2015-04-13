/*  readlink.c
 *
 *  This file is part of vchanger by Josh Fisher.
 *
 *  vchanger copyright (C) 2013 Josh Fisher
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
 */

#include "config.h"

#ifndef HAVE_READLINK

#ifdef HAVE_WINDOWS_H
#include "targetver.h"
#include <windows.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_STDDEF_H
#include <stddef.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#include "win32_util.h"
#include "compat/readlink.h"

#ifdef HAVE_WINDOWS_H

/*-------------------------------------------------
 *  Emulate POSIX.1-2001 readlink() function using win32/win64 CreateFileW()
 *  and GetFinalPathNameByHandleW() functions.
 *  On success returns the length of the target path string. On
 *  error returns -1 and sets errno.
 *-------------------------------------------------*/
ssize_t readlink(const char *path_in, char *buf, size_t bufsiz)
{
   HANDLE hFS;
   DWORD dw;
   wchar_t *p16, *path = NULL;
   size_t path_sz = 0;

   /* Convert path string to UTF16 encoding */
   if (!AnsiToUTF16(path_in, &path, &path_sz)) {
      errno = w32errno(ERROR_BAD_PATHNAME);
      return -1;
   }
   /* Check attributes of path to see if it is a symlink */
   dw = GetFileAttributesW(path);
   if (dw == INVALID_FILE_ATTRIBUTES) {
      /* Error getting attributes */
      errno = w32errno(GetLastError());
      free(path);
      return -1;
   }
   if ((dw & FILE_ATTRIBUTE_REPARSE_POINT) == 0) {
      /* path does not point to a symlink */
      errno = EINVAL;
      return -1;
   }
   /* Get an open handle to the symlink's target file */
   hFS = CreateFileW(path, FILE_READ_ATTRIBUTES, FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
                     NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
   if (hFS == INVALID_HANDLE_VALUE) {
      /* Failed to open file */
      errno = w32errno(GetLastError());
      free(path);
      return -1;
   }
   /* Get the full final UTF-16 path name of the opened file */
   free(path);
   path = (wchar_t*)malloc(65536);
   dw = GetFinalPathNameByHandleW(hFS, path, 32768, FILE_NAME_OPENED);
   if (dw == 0) {
      /* Failed to get final path */
      errno = w32errno(GetLastError());
      CloseHandle(hFS);
      free(path);
      return -1;
   }
   /* Close file handle and get pointer to UTF-16 target path */
   CloseHandle(hFS);
   p16 = path;
   if (path[0] == L'\\' && path[2] == L'?' && path[3] == L'\\') {
      p16 = path + 4;
      dw -= 4;
   }
   if (dw > bufsiz) {
      /* Caller's buffer too small */
      errno = EINVAL;
      free(path);
      return -1;
   }
   /* Convert target path to ANSI in caller's buffer */
   if (!UTF16ToAnsi(p16, &buf, &bufsiz)) {
      /* Failed to convert to ANSI */
      errno = EINVAL;
      free(path);
      return -1;
   }
   free(path);
   return strlen(buf);
 }

#endif

#endif
