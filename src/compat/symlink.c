/*  symlink.c
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

#ifndef HAVE_SYMLINK

#ifdef HAVE_WINDOWS_H
#include "targetver.h"
#include <windows.h>
#include "win32_util.h"
#endif
/*
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
*/
#include "compat/symlink.h"

#ifdef HAVE_WINDOWS_H

/*-------------------------------------------------
 *  Emulate POSIX.1-2001 symlink() function using win32/win64 CreateSymbolicLinkW() function.
 *  On success returns zero, else negative on error.
 *-------------------------------------------------*/
int symlink(const char *oldpath, const char *newpath)
{
   DWORD rc, attr, dwFlags;
   wchar_t *woldpath = NULL, *wnewpath = NULL;
   size_t woldpath_sz = 0, wnewpath_sz = 0;

   /* Convert path strings to UTF16 encoding */
   if (!AnsiToUTF16(oldpath, &woldpath, &woldpath_sz)) {
      rc = ERROR_BAD_PATHNAME;
      errno = w32errno(rc);
      return -1;
   }
   if (!AnsiToUTF16(newpath, &wnewpath, &wnewpath_sz)) {
      rc = ERROR_BAD_PATHNAME;
      errno = w32errno(rc);
      free(woldpath);
      return -1;
   }
   /* Determine if oldpath is a file or directory */
   attr = GetFileAttributesW(woldpath);
   if (attr == INVALID_FILE_ATTRIBUTES) {
      free(woldpath);
      free(wnewpath);
      errno = w32errno(GetLastError());
      return -1;
   }
   dwFlags = (attr & FILE_ATTRIBUTE_DIRECTORY) == 0 ? 0 : 1;
   /* Create the symlink */
   if (CreateSymbolicLinkW(wnewpath, woldpath, dwFlags)) {
      free(woldpath);
      free(wnewpath);
      return 0;
   }
   free(woldpath);
   free(wnewpath);
   errno = w32errno(GetLastError());
   return -1;
}

#endif

#endif
