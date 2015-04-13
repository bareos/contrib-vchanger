/*  gettimeofday.h
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

#ifndef _GETTIMEOFDAY_H
#define _GETTIMEOFDAY_H

#ifndef HAVE_GETTIMEOFDAY
/* For systems without gettimeofday function, use internal version */

#ifdef HAVE_WINDOWS_H
#define EPOCH_FILETIME (116444736000000000LL) // 100 ns intervals between FILETIME epoch and Unix epoch
#if defined(_MSC_VER) || defined(_MSC_EXTENSIONS)
  #define DELTA_EPOCH_IN_MICROSECS  11644473600000000Ui64
#else
  #define DELTA_EPOCH_IN_MICROSECS  11644473600000000ULL
#endif

#define suseconds_t long
struct timeval
{
   time_t  tv_sec;
   suseconds_t tv_usec;
};

struct timezone
{
   int tz_minuteswest;
   int tz_dsttime;
};
#endif

#ifdef __cplusplus
extern "C" {
#endif
int gettimeofday(struct timeval *tv, struct timezone *tz);
#ifdef __cplusplus
}
#endif
#endif


#endif  /* _GETTIMEOFDAY_H */
