/* loghandler.cpp
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

#include "config.h"
#include "compat_defs.h"
#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif
#ifndef HAVE_LOCALTIME_R
#include "compat/localtime_r.h"
#endif
#define LOGHANDLER_SOURCE 1
#include "loghandler.h"

LogHandler log;

LogHandler::LogHandler() : use_syslog(false), max_debug_level(LOG_WARNING), errfs(stderr)
{
#ifdef HAVE_PTHREAD_H
   pthread_mutex_init(&mut, NULL);
#endif
}

LogHandler::~LogHandler()
{
   if (use_syslog) closelog();
#ifdef HAVE_PTHREAD_H
   pthread_mutex_destroy(&mut);
#endif
}

void LogHandler::OpenLog(FILE *fs, int max_level)
{
   Lock();
   if (use_syslog) closelog();
   if (max_level < LOG_EMERG || max_level > LOG_DEBUG) max_level = LOG_DEBUG;
   errfs = fs;
   use_syslog = false;
   max_debug_level = max_level;
   Unlock();
}

void LogHandler::OpenLog(const char *ident, int facility, int max_level,
                         int syslog_options)
{
   Lock();
   if (use_syslog) closelog();
   if (facility < LOG_KERN || facility > LOG_LOCAL7) facility = LOG_DAEMON;
   if (max_level < LOG_EMERG || max_level > LOG_DEBUG) max_level = LOG_DEBUG;
   openlog(ident, syslog_options, facility);
   use_syslog = true;
   errfs = stderr;
   if (errfs == NULL) errfs = stderr;
   max_debug_level = max_level;
   Unlock();
}

void LogHandler::Emergency(const char *fmt, ...)
{
   va_list vl;
   va_start(vl, fmt);
   WriteLog(LOG_EMERG, fmt, vl);
   va_end(vl);
}

void LogHandler::Alert(const char *fmt, ...)
{
   va_list vl;
   va_start(vl, fmt);
   WriteLog(LOG_ALERT, fmt, vl);
   va_end(vl);
}

void LogHandler::Critical(const char *fmt, ...)
{
   va_list vl;
   va_start(vl, fmt);
   WriteLog(LOG_CRIT, fmt, vl);
   va_end(vl);
}

void LogHandler::Error(const char *fmt, ...)
{
   va_list vl;
   va_start(vl, fmt);
   WriteLog(LOG_ERR, fmt, vl);
   va_end(vl);
}

void LogHandler::Warning(const char *fmt, ...)
{
   va_list vl;
   va_start(vl, fmt);
   WriteLog(LOG_WARNING, fmt, vl);
   va_end(vl);
}

void LogHandler::Notice(const char *fmt, ...)
{
   va_list vl;
   va_start(vl, fmt);
   WriteLog(LOG_NOTICE, fmt, vl);
   va_end(vl);
}

void LogHandler::Info(const char *fmt, ...)
{
   va_list vl;
   va_start(vl, fmt);
   WriteLog(LOG_INFO, fmt, vl);
   va_end(vl);
}

void LogHandler::Debug(const char *fmt, ...)
{
   va_list vl;
   va_start(vl, fmt);
   WriteLog(LOG_DEBUG, fmt, vl);
   va_end(vl);
}

void LogHandler::MajorDebug(const char *fmt, ...)
{
#ifdef MAJOR_DEBUG
   va_list vl;
   va_start(vl, fmt);
   WriteLog(LOG_DEBUG, fmt, vl);
   va_end(vl);
#endif
}

// Method to acquire mutex lock
void LogHandler::Lock()
{
#ifdef HAVE_PTHREAD_H
   pthread_mutex_lock(&mut);
#endif
}

// Method to release mutex lock
void LogHandler::Unlock()
{
#ifdef HAVE_PTHREAD_H
   pthread_mutex_unlock(&mut);
#endif
}

// Method to write to log
void LogHandler::WriteLog(int priority, const char *fmt, va_list vl)
{
   size_t n;
   struct tm bt;
   time_t t;
   char buf[1024];
   Lock();
   if (priority > max_debug_level || priority < LOG_EMERG || !fmt) {
      Unlock();
      return;
   }
   t = time(NULL);
   localtime_r(&t, &bt);
   strftime(buf, 100, "%b %d %T: ", &bt);
   strncpy(buf + strlen(buf), fmt, sizeof(buf) - strlen(buf));
   if (use_syslog) vsyslog(priority, buf, vl);
   else {
      n = strlen(buf);
      if (!n) {
         fprintf(errfs, "\n");
      } else {
         vfprintf(errfs, buf, vl);
         if (buf[n - 1] != '\n') {
            fprintf(errfs, "\n");
         }
         fflush(errfs);
      }
   }
   Unlock();
}
