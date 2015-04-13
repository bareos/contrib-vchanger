/*  compat_syslog.h
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

#ifndef _COMPAT_SYSLOG_H_
#define _COMPAT_SYSLOG_H_


#ifndef HAVE_SYSLOG
/*  For systems without syslog(), provide fake one */

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
void vsyslog(int type, const char *fmt, va_list ap);
void openlog(const char *app, int option, int facility);
void closelog(void);

#ifdef __cplusplus
}
#endif
#endif

#endif  /* __SYSLOG_H */
