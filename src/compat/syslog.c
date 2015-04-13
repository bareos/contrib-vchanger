/*  compat_syslog.c
 *
 *  This file is part of vchanger by Josh Fisher.
 *
 *  Provide syslog() replacement function for non-POSIX systems.
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

#include "config.h"

#ifndef HAVE_SYSLOG

#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif
#include "compat/syslog.h"

#ifdef HAVE_WINDOWS_H
/*  Windows doesn't do syslog() ... ignore for now. Perhaps win32 event logging
 *  could be used to emulate syslog in future.
 */
void openlog(const char *app, int option, int facility)
{
}

void closelog(void)
{
}

void syslog(int type, const char *fmt, ...)
{
}

void vsyslog(int type, const char *fmt, va_list ap)
{
}

#endif

#endif
