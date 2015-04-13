/* vconf.cpp
 *
 *  This file is part of vchanger by Josh Fisher.
 *
 *  vchanger copyright (C) 2008-2014 Josh Fisher
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
 *  Provides class for reading configuration file and providing access
 *  to config variables.
 */

#include "config.h"
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_WINDOWS_H
#include "targetver.h"
#include <windows.h>
#include <shlobj.h>
#include <direct.h>
#define DIR_DELIM "\\"
#define DIR_DELIM_C '\\'
#define MAG_VOLUME_MASK 0
#else
#define DIR_DELIM "/"
#define DIR_DELIM_C '/'
#define MAG_VOLUME_MASK S_IWGRP|S_IRWXO
#endif

#include "loghandler.h"
#include "util.h"
#define __VCONF_SOURCE 1
#include "vconf.h"

/* Global configuration object */
VchangerConfig conf;

/*-------------------------------------------
 * Config file keywords and defaults
 *-------------------------------------------*/

#define VK_STORAGE_NAME "storage resource"
#define VK_MAGAZINE "magazine"
#define VK_WORK_DIR "work dir"
#define VK_LOGFILE "logfile"
#define VK_LOG_LEVEL "log level"
#define VK_USER "user"
#define VK_GROUP "group"
#define VK_BCONSOLE "bconsole"
#define VK_BCONSOLE_CONFIG "bconsole config"
#define VK_DEF_POOL "default pool"


/*================================================
 *  Class VchangerConfig
 *================================================*/

/*--------------------------------------------------
 * Default constructor
 *------------------------------------------------*/
VchangerConfig::VchangerConfig() : log_level(DEFAULT_LOG_LEVEL)
{
#ifdef HAVE_WINDOWS_H
   char tmp[4096];
   wchar_t wtmp[2048];
#endif
   storage_name = DEFAULT_STORAGE_NAME;
   /* Set default Bacula work directory and vchanger config file path */
#ifdef HAVE_WINDOWS_H
   /* Bacula installs its work directory in the FOLDERID_ProgramData folder, which
    * is in a different location depending on Windows version. */
   wtmp[0] = 0;
   memset(tmp, 0, sizeof(tmp));
   if (SHGetFolderPathW(NULL, CSIDL_COMMON_APPDATA, NULL, SHGFP_TYPE_CURRENT, wtmp) == S_OK) {
      wcstombs(tmp, wtmp, sizeof(tmp) - 1);
      snprintf(DEFAULT_STATEDIR, sizeof(DEFAULT_STATEDIR), "%s%svchanger", tmp, DIR_DELIM);
      snprintf(DEFAULT_LOGDIR, sizeof(DEFAULT_LOGDIR), "%s%svchanger", tmp, DIR_DELIM);
   }
#else
   snprintf(DEFAULT_STATEDIR, sizeof(DEFAULT_STATEDIR), "%s/spool/vchanger", LOCALSTATEDIR);
   snprintf(DEFAULT_LOGDIR, sizeof(DEFAULT_LOGDIR), "%s/log/vchanger", LOCALSTATEDIR);
#endif
   tFormat(work_dir, "%s%s%s", DEFAULT_STATEDIR, DIR_DELIM, storage_name.c_str());
   /* Set default logfile path and log level */
   tFormat(logfile, "%s%s%s.log", DEFAULT_LOGDIR, DIR_DELIM, storage_name.c_str());
   /* Set default runas user and group */
   user = DEFAULT_USER;
   group = DEFAULT_GROUP;
   /* Set default bconsole binary path */
   bconsole = DEFAULT_BCONSOLE;
   /* Set default pool for created volumes */
   def_pool = DEFAULT_POOL;
   /* Define config file keywords */
   keyword.AddKeyword(VK_MAGAZINE, INIKEYWORDTYPE_MULTISZ);
   keyword.AddKeyword(VK_WORK_DIR, INIKEYWORDTYPE_SZ);
   keyword.AddKeyword(VK_LOGFILE, INIKEYWORDTYPE_SZ);
   keyword.AddKeyword(VK_LOG_LEVEL, INIKEYWORDTYPE_LONG);
   keyword.AddKeyword(VK_USER, INIKEYWORDTYPE_SZ);
   keyword.AddKeyword(VK_GROUP, INIKEYWORDTYPE_SZ);
   keyword.AddKeyword(VK_BCONSOLE, INIKEYWORDTYPE_SZ);
   keyword.AddKeyword(VK_BCONSOLE_CONFIG, INIKEYWORDTYPE_SZ);
   keyword.AddKeyword(VK_STORAGE_NAME, INIKEYWORDTYPE_SZ);
   keyword.AddKeyword(VK_DEF_POOL, INIKEYWORDTYPE_SZ);
}

/*-------------------------------------------------
 *  Method to read config file and set config values from keyword
 *  value pairs. On success, returns true. Otherwise returns false.
 *------------------------------------------------*/
bool VchangerConfig::Read(const char *cfile)
{
   int rc, n;
   IniFile tmp_ini = keyword;

   tmp_ini.ClearKeywordValues();
   if (!cfile || !cfile[0]) {
      log.Error("config file not specified");
      return false;
   }
   /* Does config file exist */
   if (access(cfile, R_OK)) {
      log.Error("could not access config file %s", cfile);
      return false;
   }
   /* Read config file values */
   rc = tmp_ini.Read(cfile);
   if (rc) {
      if (rc > 0) log.Error("Parse error in %s at line %d", cfile, rc);
      else log.Error("could not open config file  %s", cfile);
      return false;
   }
   /* Update keyword values */
   keyword.Update(tmp_ini);

   /* Get bacula-dir.conf storage resource name associated with this changer name */
   if (keyword[VK_STORAGE_NAME].IsSet()) {
      storage_name = (const char*)keyword[VK_STORAGE_NAME];
      tStrip(storage_name);
      if (storage_name.empty()) {
         log.Error("config file keyword '%s' must specify a non-empty string", VK_STORAGE_NAME);
         return false;
      }
      /* Update defaults for this changer name */
      tFormat(work_dir, "%s%s%s", DEFAULT_STATEDIR, DIR_DELIM, storage_name.c_str());
      tFormat(logfile, "%s%s%s.log", DEFAULT_LOGDIR, DIR_DELIM, storage_name.c_str());
   }

   /* Get work directory for this changer */
   if (keyword[VK_WORK_DIR].IsSet()) {
      work_dir = (const char*)keyword[VK_WORK_DIR];
      tStrip(work_dir);
      if (work_dir.empty()) {
         log.Error("config file keyword '%s' must specify a non-empty string", VK_WORK_DIR);
         return false;
      }
   }

   /* Get logfile path */
   if (keyword[VK_LOGFILE].IsSet()) {
      logfile = (const char*)keyword[VK_LOGFILE];
      tStrip(logfile);
      if (logfile.empty()) {
         log.Error("config file keyword '%s' must specify a non-empty string", VK_LOGFILE);
         return false;
      }
   }

   /* Get log level */
   if (keyword[VK_LOG_LEVEL].IsSet()) {
      log_level = (int)keyword[VK_LOG_LEVEL];
      if (log_level < 0 || log_level > 7) {
         log.Error("config file keyword '%s' must specify a value between 0 and 7 inclusive", VK_LOG_LEVEL);
         return false;
      }
   }

   /* Get user to run as */
   if (keyword[VK_USER].IsSet()) {
      user = (const char*)keyword[VK_USER];
      tStrip(user);
      if (user.empty()) {
         log.Error("keyword '%s' value cannot be empty", VK_USER);
         return false;
      }
   }

   /* Get group to run as */
   if (keyword[VK_GROUP].IsSet()) {
      group = (const char*)keyword[VK_GROUP];
      tStrip(group);
      if (group.empty()) {
         log.Error("keyword '%s' value cannot be empty", VK_GROUP);
         return false;
      }
   }

   /* Get path of bconsole binary */
   if (keyword[VK_BCONSOLE].IsSet()) {
      bconsole = (const char*)keyword[VK_BCONSOLE];
      tStrip(bconsole);
   }

   /* Get path of bconsole config file */
   if (keyword[VK_BCONSOLE_CONFIG].IsSet()) {
      bconsole_config = (const char*)keyword[VK_BCONSOLE_CONFIG];
      tStrip(bconsole_config);
   }

   /* Get default pool */
   if (keyword[VK_DEF_POOL].IsSet()) {
      def_pool = (const char*)keyword[VK_DEF_POOL];
      tStrip(def_pool);
      if (def_pool.empty()) {
         log.Error("keyword '%s' value cannot be empty", VK_DEF_POOL);
         return false;
      }
   }

   /* Get list of assigned magazines */
   if (keyword[VK_MAGAZINE].IsSet()) {
      magazine = keyword[VK_MAGAZINE];
   }
   if (magazine.empty()) {
      log.Error("config file keyword '%s' must appear at least once", VK_MAGAZINE);
      return false;
   }
   for (n = 0; n < (int)magazine.size(); n++) {
      tStrip(magazine[n]);
      if (magazine[n].empty()) {
         log.Error("config file keyword '%s' cannot be set to the empty string", VK_MAGAZINE);
         return false;
      }
   }
   return true;
}

/*-------------------------------------------------
 *  Method to validate config file values and commit default
 *  values for any keywords not specified
 *------------------------------------------------*/
bool VchangerConfig::Validate()
{
   mode_t old_mask;

   /* Validate work directory is usable */
   if (access(work_dir.c_str(), W_OK)) {
      if (errno == ENOENT) {
         /* If not found then try to create work dir */
         old_mask = umask(027);
#ifndef HAVE_WINDOWS_H
         if (mkdir(work_dir.c_str(), 0770)) {
#else
         if (_mkdir(work_dir.c_str())) {
#endif
            log.Error("could not create work directory '%s'", work_dir.c_str());
            umask(old_mask);
            return false;
         }
         umask(old_mask);
      } else {
         log.Error("could not access work directory '%s'", work_dir.c_str());
         return false;
      }
   }

   /* Validate bconsole config file is readable */
   if (bconsole.empty()) bconsole_config.clear();
   if (!bconsole_config.empty()) {
      if (access(bconsole_config.c_str(), R_OK)) {
         /* If bconsole config doesn't exist or is not readable, disable use of bconsole */
         log.Warning("cannot read bconsole config file. Disabling Bacula interaction.");
         bconsole.clear();
         bconsole_config.clear();
      }
   }

   return true;
}
