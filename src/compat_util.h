/*  compat_util.h
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
*/

#ifndef COMPAT_UTIL_H_
#define COMPAT_UTIL_H_

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

#define LOG_EMERG   0
#define LOG_ALERT   1
#define LOG_CRIT    2
#define LOG_ERR     3
#define LOG_WARNING 4
#define LOG_NOTICE  5
#define LOG_INFO    6
#define LOG_DEBUG   7
#define LOG_KERN    0
#define LOG_USER    8
#define LOG_MAIL    16
#define LOG_DAEMON  24
#define LOG_AUTH    32
#define LOG_SYSLOG  40
#define LOG_LPR     48
#define LOG_NEWS    56
#define LOG_UUCP    64
#define LOG_CRON    72
#define LOG_AUTHPRIV 80
#define LOG_FTP     88
#define LOG_LOCAL0  128
#define LOG_LOCAL1  136
#define LOG_LOCAL2  144
#define LOG_LOCAL3  152
#define LOG_LOCAL4  160
#define LOG_LOCAL5  168
#define LOG_LOCAL6  176
#define LOG_LOCAL7  184
#define LOG_PID     1
#define LOG_CONS    2
#define LOG_ODELAY  4
#define LOG_NDELAY  8
#define LOG_NOWAIT  16
#define LOG_PERROR  32

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

#endif  /* _COMPAT_UTIL_H */
