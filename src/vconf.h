/*  vconf.h
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
*/

#ifndef _VCONF_H_
#define _VCONF_H_ 1

#include "inifile.h"

#define DEFAULT_LOG_LEVEL 3
#define DEFAULT_USER "bacula"
#define DEFAULT_GROUP "tape"
#define DEFAULT_BCONSOLE "/usr/sbin/bconsole"
#define DEFAULT_STORAGE_NAME "vchanger"
#define DEFAULT_POOL "Scratch"

/* Configuration values */

class VchangerConfig
{
public:
   IniFile keyword;
   tString work_dir;
   tString config_file;
   tString logfile;
   int log_level;
   tString user;
   tString group;
   tString bconsole;
   tString bconsole_config;
   tString storage_name;
   tString def_pool;
   tStringArray magazine;
public:
   VchangerConfig();
   virtual ~VchangerConfig() {}
   bool Read(const char *cfile);
   inline bool Read(const tString &cfile) { return Read(cfile.c_str()); }
   bool Validate();
};

#ifndef __VCONF_SOURCE
extern VchangerConfig conf;
extern char DEFAULT_LOGDIR[4096];
extern char DEFAULT_STATEDIR[4096];
#else
char DEFAULT_LOGDIR[4096];
char DEFAULT_STATEDIR[4096];
#endif

#endif /* _VCONF_H_ */
