/*
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

#include "config.h"

#ifndef HAVE_LOCALTIME_R

#ifdef HAVE_WINDOWS_H
#include "targetver.h"
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include "compat/localtime_r.h"

/*
 *  Emulate SUSv2 localtime_r() function using C89 localtime(). On Win32
 *  localtime() is thread safe. For all others, note that this keeps the
 *  caller from using the unsafe buffer over any extended period,
 *  but it is still not thread safe and will fail if two threads call this
 *  function nearly simultaneously.
 */
struct tm* localtime_r(const time_t *timep, struct tm *buf)
{
   struct tm *tm1 = localtime(timep);
   if (!tm1) return NULL;
   memcpy(buf, tm1, sizeof(struct tm));
   return buf;
}

#endif
