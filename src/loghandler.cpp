/* loghandler.cpp
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
 *
 *  Provides a class for logging messages to a file
 */

#include "vchanger.h"
#include "loghandler.h"
#include <stdarg.h>
#include <string.h>
#include <time.h>

///////////////////////////////////////////////////
//  Class LogHandler
///////////////////////////////////////////////////

LogHandler::LogHandler()
{
   use_syslog = false;
   must_close_logfile = false;
   max_debug_level = LOG_ERR;
   logfile = NULL;
}

LogHandler::~LogHandler()
{
#ifndef _SPLINT_
   if (use_syslog) closelog();
#endif
   if (logfile && must_close_logfile) {
      fclose(logfile);
   }
}

#ifndef _SPLINT_
/*-------------------------------------------------
 *  Method to open logging to a syslog facility
 *------------------------------------------------*/
void LogHandler::OpenLog(int facility, const char *ident, int max_level, int syslog_options)
{
   CloseLog();
   if (facility < LOG_DAEMON || facility > LOG_USER) {
      facility = LOG_DAEMON;
   }
   if (max_level < LOG_ERR || max_level > LOG_INFO) {
      max_level = LOG_INFO;
   }
   openlog(ident, syslog_options, facility);
   use_syslog = true;
   max_debug_level = max_level;
}
#endif

/*-------------------------------------------------
 *  Method to open logging to a file
 *------------------------------------------------*/
void LogHandler::OpenLog(const char *fname, int max_level)
{
   CloseLog();
   if (max_level < LOG_ERR || max_level > LOG_INFO) {
      max_level = LOG_INFO;
   }
   max_debug_level = max_level;
   if (fname && strlen(fname)) {
      logfile = fopen(fname, "a");
      if (logfile) must_close_logfile = true;
   }
}

/*-------------------------------------------------
 *  Method to open logging to an already opened file
 *------------------------------------------------*/
void LogHandler::OpenLog(FILE *fslog, int max_level)
{
   CloseLog();
   if (max_level < LOG_ERR || max_level > LOG_INFO) {
      max_level = LOG_INFO;
   }
   max_debug_level = max_level;
   logfile = fslog;
}

/*-------------------------------------------------
 *  Method to close the log object. Closes the logfile or
 *  syslog.
 *------------------------------------------------*/
void LogHandler::CloseLog()
{
#ifndef _SPLINT_
   if (use_syslog) {
      closelog();
   }
#endif
   if (logfile) {
      if (must_close_logfile) {
         fclose(logfile);
      }
      logfile = NULL;
   }
   must_close_logfile = false;
   use_syslog = false;
}

/*-------------------------------------------------
 *  Method to log message of LOG_ERR level
 *------------------------------------------------*/
void LogHandler::Error(const char *fmt, ...)
{
   va_list vl;
   va_start(vl, fmt);
   WriteLog(LOG_ERR, fmt, vl);
   va_end(vl);
}

/*-------------------------------------------------
 *  Method to log message of LOG_CRIT level
 *------------------------------------------------*/
void LogHandler::Critical(const char *fmt, ...)
{
   va_list vl;
   va_start(vl, fmt);
   WriteLog(LOG_CRIT, fmt, vl);
   va_end(vl);
}

/*-------------------------------------------------
 *  Method to log message of LOG_ALERT level
 *------------------------------------------------*/
void LogHandler::Alert(const char *fmt, ...)
{
   va_list vl;
   va_start(vl, fmt);
   WriteLog(LOG_ALERT, fmt, vl);
   va_end(vl);
}

/*-------------------------------------------------
 *  Method to log message of LOG_WARNING level
 *------------------------------------------------*/
void LogHandler::Warning(const char *fmt, ...)
{
   va_list vl;
   va_start(vl, fmt);
   WriteLog(LOG_WARNING, fmt, vl);
   va_end(vl);
}

/*-------------------------------------------------
 *  Method to log message of LOG_NOTICE level
 *------------------------------------------------*/
void LogHandler::Notice(const char *fmt, ...)
{
   va_list vl;
   va_start(vl, fmt);
   WriteLog(LOG_NOTICE, fmt, vl);
   va_end(vl);
}

/*-------------------------------------------------
 *  Method to log message of LOG_INFO level
 *------------------------------------------------*/
void LogHandler::Info(const char *fmt, ...)
{
   va_list vl;
   va_start(vl, fmt);
   WriteLog(LOG_INFO, fmt, vl);
   va_end(vl);
}

/*-------------------------------------------------
 *  Protected method to handle actual log i/o.
 *------------------------------------------------*/
void LogHandler::WriteLog(int priority, const char *fmt, va_list &vl)
{
   size_t n;
   char dstr[256];
   time_t tcur;
#ifndef _SPLINT_
   char buff[4096];
#endif
   if (!fmt) {
      return;
   }
   if (priority > max_debug_level || priority < LOG_ERR) {
      return;
   }
#ifndef _SPLINT_
   if (use_syslog) {
      vsnprintf(buff, sizeof(buff), fmt, vl);
      syslog(priority, "%s", buff);
      return;
   }
#endif
   if (logfile) {
      tcur = time(NULL);
      ctime_r(&tcur, dstr);
      n = strlen(dstr);
      if (n) {
         dstr[n - 1] = 0;
         fprintf(logfile, "%s ", dstr);
      }
      vfprintf(logfile, fmt, vl);
      if (!strlen(fmt) || fmt[strlen(fmt) - 1] != '\n') fprintf(logfile, "\n");
   }
}
