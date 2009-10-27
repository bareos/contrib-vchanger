/*  loghandler.h
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
#ifndef _LOGHANDLER_H_
#define _LOGHANDLER_H_ 1

#ifdef HAVE_WINDOWS_H
#include "compat_util.h"
#endif
#include <stdio.h>
#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif

class LogHandler
{
public:
   LogHandler();
   ~LogHandler();
#ifndef _SPLINT_
   void OpenLog(int facility, const char *ident = NULL,
	 	int max_level = LOG_WARNING, int syslog_options = 0);
#endif
   void OpenLog(const char *fname, int max_level = LOG_WARNING);
   void OpenLog(FILE *fslog, int max_level = LOG_WARNING);
   void CloseLog();
   void Error(const char *fmt, ... );
   void Critical(const char *fmt, ... );
   void Alert(const char *fmt, ... );
   void Warning(const char *fmt, ... );
   void Notice(const char *fmt, ... );
   void Info(const char *fmt, ... );
   bool UsingSyslog() { return use_syslog; }
protected:
   void WriteLog(int priority, const char *fmt, va_list &vl);
protected:
   bool use_syslog;
   bool must_close_logfile;
   int max_debug_level;
   FILE *logfile;
};

#endif /* _LOGHANDLER_H_ */
