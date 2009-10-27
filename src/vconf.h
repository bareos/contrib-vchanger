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
#define DEFAULT_MAGAZINE_BAYS 1
#define DEFAULT_CHANGER_NAME "vchanger"
#define DEFAULT_LOGFILE ""
#define MAX_MAGAZINE_BAYS 32
#define MAX_DRIVES 16
#define MAX_SLOTS 9999
#define MAX_MAGAZINES 128
#ifdef HAVE_WINDOWS_H
#define DEFAULT_CONFIG_FILE "\\Bacula\\vchanger.conf"
#define DEFAULT_WORK_DIR "\\Bacula\\Work"
#else
#define DEFAULT_CONFIG_FILE "/etc/bacula/vchanger.conf"
#define DEFAULT_WORK_DIR "/var/lib/bacula"
#endif

/* Configuration values */

class VchangerConfig
{
public:
   char changer_name[128];
   char work_dir[PATH_MAX];
   char *magazine[MAX_MAGAZINES + 1];
   char logfile[PATH_MAX];
   char automount_dir[PATH_MAX];
   int known_magazines;
   int magazine_bays;
   int virtual_drives;
   int slots_per_mag;
   int slots;
public:
   VchangerConfig();
   ~VchangerConfig();
   bool Read(const char *cfile);
private:
   void SetDefaults();
   int vconf_getline(FILE *fs, char *val, size_t val_size);
};

#endif /* _VCONF_H_ */
