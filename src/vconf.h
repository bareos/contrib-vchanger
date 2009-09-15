/*  vconf.h
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

#ifndef _VCONF_H_
#define _VCONF_H_ 1

#define DEFAULT_VIRTUAL_DRIVES 1
#define DEFAULT_SLOTS_PER_MAG 10
#define DEFAULT_CHANGER_NAME "vchanger"
#define DEFAULT_LOGFILE ""
#define MAX_SLOTS 999
#define MAX_DRIVES 99
#define MAX_MAGAZINES 33
#ifdef HAVE_WIN32
#define DEFAULT_CONFIG_FILE "\\Bacula\\vchanger.conf"
#define DEFAULT_STATE_DIR "\\Bacula\\Work"
#else
#define DEFAULT_CONFIG_FILE "/etc/bacula/vchanger.conf"
#define DEFAULT_STATE_DIR "/var/lib/bacula"
#endif

/* Configuration values */

class VchangerConfig
{
public:
   char changer_name[128];
   char state_dir[PATH_MAX];
   char *magazine[MAX_MAGAZINES];
   char logfile[PATH_MAX];
   int32_t num_magazines;
   int32_t virtual_drives;
   int32_t slots_per_mag;
   int32_t slots;
public:
   VchangerConfig();
   ~VchangerConfig();
   bool Read(const char *cfile);
private:
   void SetDefaults();
   int vconf_getline(FILE *fs, char *val, size_t val_size);
};

#endif /* _VCONF_H_ */
