/*  loghandler.h
 *
 *  Copyright (C) 2013-2014 Josh Fisher
 *
 *  This program is free software. You may redistribute it and/or modify
 *  it under the terms of the GNU General Public License, as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  See the file "COPYING".  If not,
 *  see <http://www.gnu.org/licenses/>.
*/

#ifndef _LOGHANDLER_H_
#define _LOGHANDLER_H_ 1

#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#else
#include "compat/syslog.h"
#endif
#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif
#ifdef HAVE_PTHREAD_H
#include <pthread.h>
#endif

class LogHandler
{
public:
   LogHandler();
   virtual ~LogHandler();
   void OpenLog(FILE *fs, int max_level = LOG_WARNING);
   void OpenLog(const char *ident = NULL, int facility = LOG_DAEMON,
      int max_level = LOG_WARNING, int syslog_options = 0);
   void Emergency(const char *fmt, ... );
   void Alert(const char *fmt, ... );
   void Critical(const char *fmt, ... );
   void Error(const char *fmt, ... );
   void Warning(const char *fmt, ... );
   void Notice(const char *fmt, ... );
   void Info(const char *fmt, ... );
   void Debug(const char *fmt, ... );
   void MajorDebug(const char *fmt, ... );
   inline bool UsingSyslog() { return use_syslog; }
protected:
   void Lock();
   void Unlock();
   void WriteLog(int priority, const char *fmt, va_list vl);
protected:
   bool use_syslog;
   int max_debug_level;
   FILE *errfs;
#ifdef HAVE_PTHREAD_H
   pthread_mutex_t mut;
#endif
};

#ifndef LOGHANDLER_SOURCE
extern LogHandler log;
#endif

#endif /* _LOGHANDLER_H_ */
