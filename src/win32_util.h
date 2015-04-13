/*  win32_util.h
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
*/

#ifndef COMPAT_UTIL_H_
#define COMPAT_UTIL_H_

#ifdef HAVE_WINDOWS_H

/* Some utility functions needed for Windows version */

/* number of 100 ns intervals between FILETIME epoch and Unix epoch */
#define EPOCH_FILETIME (116444736000000000LL)

#ifdef __cplusplus
extern "C" {
#endif

time_t ftime2utime(const FILETIME *time);
wchar_t* AnsiToUTF16(const char *str, wchar_t **u16str, size_t *u16size);
char *UTF16ToAnsi(const wchar_t *u16str, char **str, size_t *str_size);
int w32errno(DWORD werr);

#ifdef __cplusplus
}
#endif

#endif


#endif  /* _COMPAT_UTIL_H */
