/*  gettimeofday.c
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

#ifndef HAVE_GETTIMEOFDAY

#ifdef HAVE_WINDOWS_H
#include "targetver.h"
#include <windows.h>
#endif
#ifdef HAV_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#include "compat/gettimeofday.h"


#ifndef HAVE_WINDOWS_H
/*
 *  Emulate POSIX.1-2001 gettimeofday() function using the time() function.
 */
int gettimeofday(struct timeval *tv, struct timezone* tz)
{
   if (tv != NULL) {
      tv->tv_sec = time(NULL);
      tv->tv_usec = 0;
   }
   if (tz != NULL) {
      if (!tzflag) {
         tzset();
         tzflag++;
      }
      // Adjust for the timezone west of Greenwich
      tz->tz_minuteswest = _timezone / 60;
      tz->tz_dsttime = _daylight;
   }
   return 0;
}

#else

/*
 *  Emulate POSIX.1-2001 gettimeofday() function using the win32
 *  GetSystemTimeAsFIleTime() system function.
*/
int gettimeofday(struct timeval *tv, struct timezone* tz)
{
   FILETIME ft;
   int64_t tmpres = 0;
   static int tzflag = 0;

   if (tv != NULL) {
      GetSystemTimeAsFileTime(&ft);

      /* The GetSystemTimeAsFileTime returns the number of 100 nanosecond
       * intervals since Jan 1, 1601 in a structure. Copy the high bits to
       * the 64 bit tmpres, shift it left by 32 then or in the low 32 bits.
       */
      tmpres |= ft.dwHighDateTime;
      tmpres <<= 32;
      tmpres |= ft.dwLowDateTime;

      /* Convert to microseconds by dividing by 10 */
      tmpres /= 10;

      /* The Unix epoch starts on Jan 1 1970.  Need to subtract the difference
       * in seconds from Jan 1 1601.
       */
      tmpres -= DELTA_EPOCH_IN_MICROSECS;

      /* Finally change microseconds to seconds and place in the seconds value.
       * The modulus picks up the microseconds.
       */
      tv->tv_sec = tmpres / 1000000L;
      tv->tv_usec = (tmpres % 1000000L);
   }

   if (NULL != tz) {
      if (!tzflag) {
         _tzset();
         tzflag++;
      }
      // Adjust for the timezone west of Greenwich
      tz->tz_minuteswest = _timezone / 60;
      tz->tz_dsttime = _daylight;
   }

   return 0;
}
#endif

#endif
