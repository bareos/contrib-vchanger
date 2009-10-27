/*  compat_util.h
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
*/

#include "config.h"

#ifdef HAVE_WINDOWS_H
#include "targetver.h"
#include <windows.h>
#include <winerror.h>
#endif

#include <sys/time.h>
#include <sys/types.h>
#include <stdio.h>

#ifdef HAVE_WINDOWS_H
/* Some utility functions needed for Windows version */

#define EPOCH_FILETIME (116444736000000000LL) // 100 ns intervals between FILETIME epoch and Unix epoch

#ifdef __cplusplus
extern "C" {
#endif

time_t ftime2utime(const FILETIME *time);
wchar_t* UTF8ToUTF16(const char *str, wchar_t **u16str, size_t *u16size);
char *UTF16ToUTF8(const wchar_t *u16str, char **str, size_t *str_size);
int w32errno(DWORD werr);

#ifdef __cplusplus
}
#endif
#endif

#ifndef HAVE_SYSLOG_H
/*  For systems without syslog, provide fake one */

#define LOG_DAEMON  0
#define LOG_ERR     1
#define LOG_CRIT    2
#define LOG_ALERT   3
#define LOG_WARNING 4
#define LOG_NOTICE  5
#define LOG_INFO    6
#define LOG_LOCAL0  10
#define LOG_LOCAL1  11
#define LOG_LOCAL2  12
#define LOG_LOCAL3  13
#define LOG_LOCAL4  14
#define LOG_LOCAL5  15
#define LOG_LOCAL6  16
#define LOG_LOCAL7  17
#define LOG_LPR     20
#define LOG_MAIL    21
#define LOG_NEWS    22
#define LOG_UUCP    23
#define LOG_USER    24
#define LOG_CONS    0
#define LOG_PID     0

#ifdef __cplusplus
extern "C" {
#endif

void syslog(int type, const char *fmt, ...);
void openlog(const char *app, int, int);
void closelog(void);

#ifdef __cplusplus
}
#endif
#endif

#ifndef HAVE_CTIME_R
/* For systems without ctime_r function, use internal version */
#ifdef __cplusplus
extern "C" {
#endif
char *ctime_r(const time_t *timep, char *buf);
#ifdef __cplusplus
}
#endif
#endif

#ifndef HAVE_GETLINE
/* For systems without getline function, use internal version */
#ifdef __cplusplus
extern "C" {
#endif
ssize_t getline(char **lineptr, size_t *n, FILE *stream);
#ifdef __cplusplus
}
#endif
#endif
