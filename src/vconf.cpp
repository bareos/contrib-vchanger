/* vconf.cpp
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
 *  Provides class for reading configuration file and providing access
 *  to config variables.
 */

#include "vchanger.h"
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#include "util.h"
#include "vconf.h"
#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif
#ifdef HAVE_WINDOWS_H
#include <shlobj.h>
#endif

/*-------------------------------------------
 * Config file keywords
 *-------------------------------------------*/
#define NUM_VCONF_KEYWORDS 10
static const char vconf_keywords[NUM_VCONF_KEYWORDS][32] = { "CHANGER_NAME",
      "STATE_DIR", "WORK_DIR", "MAGAZINE", "LOGFILE", "VIRTUAL_DRIVES",
      "SLOTS_PER_MAGAZINE", "MAGAZINE_BAYS", "AUTOMOUNT_DIR", "LOG_LEVEL" };

/*-------------------------------------------
 * Local function to work around oddball Win32 mkdir()
 *-------------------------------------------*/
static int my_mkdir(const char *path, mode_t mode)
{
#ifdef HAVE_WINDOWS_H
   return mkdir(path);
#else
   return mkdir(path, mode);
#endif
}

/*-------------------------------------------
 * Local function to replace standard C strncat() with safer version
 *-------------------------------------------*/
static char* my_strncat(char *dest, const char *src, size_t n)
{
   size_t doff = strlen(dest);
   size_t i;

   if (!n || doff >= n) return dest;
   --n;
   for (i = 0; doff < n && src[i] != '\0' ; i++)
       dest[doff++] = src[i];
   dest[doff] = '\0';
   return dest;
}

/*-------------------------------------------------
 *  Function to extract a word from line of text
 *------------------------------------------------*/
#define EOL (-2)
static int get_word(char **line, char *val, size_t val_sz)
{
   int c, quotechar;
   size_t n = 0;
   bool quoted = false;
   *val = 0;
   /* find start of word, skipping whitespace and comments */
   while (true) {
      c = (*line)[0];
      *line += 1;
      if (c == 0) {
         return EOF;
      }
      if (c == '\n') {
         break;
      }
      if (c == '\r' || isspace(c)) {
         continue;
      }
      if (c != '#') {
         break;
      }
      while (c && c != '\n') {
         c = (*line)[0];
         *line += 1;
      }
      if (c == '\n') {
         break;
      }
      return EOF;
   }
   if (c == '\n') {
      val[n++] = '\n';
      val[n] = 0;
      return EOL;
   }
   if (c == '=') {
      val[n++] = c;
      val[n] = 0;
      return 1;
   }
   if (c == '"') {
      quoted = true;
   } else if (c == '\'') {
      quoted = true;
   }
   if (quoted) {
      quotechar = c;
      c = (*line)[0];
      *line += 1;
      if (c == 0) {
         return EOF;
      }
      while (n < val_sz && c && c != '\n' && c != quotechar) {
         val[n++] = c;
         c = (*line)[0];
         *line += 1;
      }
      if (c && c != quotechar) {
         *line -= 1;
      }
   } else {
      while (n < val_sz && c && c != '#' && c != '\n' && c != '='
            && !isspace(c)) {
         val[n++] = c;
         c = (*line)[0];
         *line += 1;
      }
      if (c) {
         *line -= 1;
      }
   }
   if (n >= val_sz) {
      n = val_sz - 1;
   }
   val[n] = 0;
   return n;
}

/*-------------------------------------------------
 *  Function to check if path exists and has mode for current uid
 *------------------------------------------------*/
static bool check_path_access(const char *path, int mode)
{
   struct stat st;
   if (!path || !strlen(path)) {
      return false;
   }
   if (stat(path, &st) == 0 && access(path, mode) == 0) {
      return true;
   }
   return false;
}

/*================================================
 *  Class VchangerConfig
 *================================================*/

/*--------------------------------------------------
 * Default constructor
 *------------------------------------------------*/
VchangerConfig::VchangerConfig()
{
   memset(work_dir, 0, sizeof(work_dir));
   memset(changer_name, 0, sizeof(changer_name));
   memset(logfile, 0, sizeof(logfile));
   memset(automount_dir, 0, sizeof(automount_dir));
   memset(magazine, 0, (MAX_MAGAZINES + 1) * sizeof(char*));
   log_level = 0;
   known_magazines = 0;
   magazine_bays = 0;
   virtual_drives = 0;
   slots_per_mag = 0;
   slots = 0;
}

/*--------------------------------------------------
 * Destructor
 *------------------------------------------------*/
VchangerConfig::~VchangerConfig()
{
   int n;
   for (n = 1; n <= MAX_MAGAZINES; n++) {
      if (magazine[n]) {
         free(magazine[n]);
      }
   }
}

/*-------------------------------------------------
 *  Protected method to set default values for config file
 *------------------------------------------------*/
void VchangerConfig::SetDefaults()
{
   int n;
#if HAVE_WINDOWS_H
   char tmp[PATH_MAX];

   /* Bacula installs its work directory in the CSIDL_COMMON_APPDATA folder, which
    * is in a different location depending on Windows version.
    * "\Documents and Settings\All Users\Application Data\Bacula\Work" on XP */
   if (SHGetFolderPath(NULL, CSIDL_COMMON_APPDATA, NULL, SHGFP_TYPE_CURRENT, (TCHAR*)tmp) == S_OK)
	   strncpy(work_dir, tmp, sizeof(work_dir));
   else
	   work_dir[0] = 0;
   my_strncat(work_dir, DEFAULT_WORK_DIR, sizeof(work_dir));
#else

   strncpy(work_dir, DEFAULT_WORK_DIR, sizeof(work_dir));
#endif
   virtual_drives = DEFAULT_VIRTUAL_DRIVES;
   magazine_bays = DEFAULT_MAGAZINE_BAYS;
   slots_per_mag = DEFAULT_SLOTS_PER_MAG;
   for (n = 1; n <= known_magazines; n++) {
      if (magazine[n]) {
         free(magazine[n]);
      }
      magazine[n] = NULL;
   }
   known_magazines = 0;
   slots = 0;
   strncpy(changer_name, DEFAULT_CHANGER_NAME, sizeof(changer_name));
   strncpy(logfile, DEFAULT_LOGFILE, sizeof(logfile));
   log_level = DEFAULT_LOGLEVEL;
}

/*-------------------------------------------------
 *  Protected method to read one key,value pair from config file 'fs'.
 *  On success, copies keyword value string to 'val' and returns the
 *  keyword's token. Otherwise, copies error message to 'val' and
 *  return zero.
 *------------------------------------------------*/
int VchangerConfig::vconf_getline(FILE *fs, char *val, size_t val_sz)
{
   size_t vn;
   int n, c, tok = EOF;
   char kw[128], buf[1024];
   char *p, *line = NULL;
   size_t line_sz = 0;
   *val = 0;
   /* Get next non-blank line */
   c = EOL;
   while (c == EOL) {
      if (getline(&line, &line_sz, fs) < 0) {
         return EOF;
      }
      p = line;
      /* find first word */
      c = get_word(&p, kw, sizeof(kw));
      if (c == EOF) {
         return EOF;
      }
   }
   /* Check for valid keyword */
   for (n = 0; n < NUM_VCONF_KEYWORDS; n++) {
      if (strncasecmp(kw, vconf_keywords[n], sizeof(kw)) == 0)
         break;
   }
   if (n >= NUM_VCONF_KEYWORDS) {
      snprintf(val, val_sz, "%s not a valid keyword", kw);
      return 0;
   }
   tok = n + 1; /* found valid keyword */
   /* find '=' operator */
   c = get_word(&p, buf, sizeof(buf));
   if (strncasecmp(buf, "=", sizeof(buf))) {
      snprintf(val, val_sz, "missing '=' operator after %s", kw);
      return 0;
   }
   /* Extract value */
   vn = 0;
   c = get_word(&p, buf, sizeof(buf));
   while (c != EOF && c != EOL) {
      if (vn) {
         val[vn++] = ' ';
      }
      for (n = 0; vn < val_sz && n <= c; n++) {
         val[vn++] = buf[n];
      }
      c = get_word(&p, buf, sizeof(buf));
   }
   return tok;
}

/*-------------------------------------------------
 *  Method to read config file and set config values from keyword
 *  value pairs. On success, returns true. Otherwise returns false.
 *------------------------------------------------*/
bool VchangerConfig::Read(const char *cfile)
{
   FILE *fs = NULL;
   struct stat st;
   char val[1024];
   char cfgfile[PATH_MAX];
   int tok;
   size_t n, line_num = 0;
   bool workdir_set = false;

   SetDefaults();
   if (cfile && strlen(cfile)) {
      /* use filename given as config file */
      strncpy(cfgfile, cfile, sizeof(cfgfile));
   } else {
      /* use default config file */
#if HAVE_WINDOWS_H
      if (SHGetFolderPath(NULL, CSIDL_COMMON_APPDATA, NULL, SHGFP_TYPE_CURRENT, (TCHAR*)cfgfile) != S_OK)
    	  cfgfile[0] = 0;
      my_strncat(cfgfile, DEFAULT_CONFIG_FILE, sizeof(cfgfile));
#else
      strncpy(cfgfile, DEFAULT_CONFIG_FILE, sizeof(cfgfile));
#endif
   }

   /* Does config file exist */
   if (stat(cfgfile, &st) != 0) {
      print_stderr("could not find config file %s\n", cfgfile);
      return false;
   }
   /* check read access of default config file */
   if (access(cfgfile, R_OK) != 0) {
      print_stderr("no read access to config file %s\n", cfgfile);
      return false;
   }
   /* open it for read-only */
   fs = fopen(cfgfile, "r");
   if (!fs) {
      print_stderr("could not open config file %s\n", cfgfile);
      return false;
   }
   /* If a config file was opened, then read it */
   tok = vconf_getline(fs, val, sizeof(val));
   while (tok != EOF) {
      ++line_num;
      if (tok == 0) {
         print_stderr("%s at line %d\n", val, line_num);
         return false;
      }
      switch (tok) {
      case 1: /* changer_name */
         strncpy(changer_name, val, sizeof(changer_name));
         break;
      case 2: /* work_dir */
      case 3: /* work_dir */
         strncpy(work_dir, val, sizeof(work_dir));
         workdir_set = true;
         break;
      case 4: /* magazine */
         if (!strlen(val)) {
            print_stderr("Config error: magazine specifies empty path\n");
            return false;
         }
         ++known_magazines;
         if (known_magazines > MAX_MAGAZINES) {
            print_stderr("Config error: a maximum of %d magazines may be assigned", MAX_MAGAZINES);
            return false;
         }
         if (strncasecmp(val, "UUID:", 5)) {
            /* magazine specified as a path */
            magazine[known_magazines] = (char*)malloc(strlen(val) + 1);
            strncpy(magazine[known_magazines], val, strlen(val) + 1);
         } else {
            /* magazine specified as a UUID */
            if (strlen(val) <= 8) {
               print_stderr("Config error: magazine specifies invalid UUID '%s'", val);
            }
            magazine[known_magazines] = (char*)malloc(strlen(val) + 1);
            strncpy(magazine[known_magazines], val, strlen(val) + 1);
         }
         break;
      case 5: /* logfile */
         strncpy(logfile, val, sizeof(logfile));
         break;
      case 6: /* virtual_drives */
         virtual_drives = (int)strtol(val, NULL, 10);
         break;
      case 7: /* slots_per_magazine */
         slots_per_mag = (int)strtol(val, NULL, 10);
         break;
      case 8: /* num_magazines */
         magazine_bays = (int)strtol(val, NULL, 10);
         break;
      case 9: /* automount_dir */
         strncpy(automount_dir, val, sizeof(automount_dir));
         break;
      case 10:  /* log_level */
         if (strncasecmp(val, "LOG_EMERG", 9) == 0) log_level = LOG_EMERG;
         if (strncasecmp(val, "LOG_ALERT", 9) == 0) log_level = LOG_ALERT;
         if (strncasecmp(val, "LOG_CRIT", 8) == 0) log_level = LOG_CRIT;
         if (strncasecmp(val, "LOG_ERR", 7) == 0) log_level = LOG_ERR;
         if (strncasecmp(val, "LOG_WARNING", 11) == 0) log_level = LOG_WARNING;
         if (strncasecmp(val, "LOG_NOTICE", 10) == 0) log_level = LOG_NOTICE;
         if (strncasecmp(val, "LOG_INFO", 8) == 0) log_level = LOG_INFO;
         if (strncasecmp(val, "LOG_DEBUG", 9) == 0) log_level = LOG_DEBUG;
         break;
      default:
         print_stderr("Config error: unknown keyword found?\n");
         return false;
      }
      tok = vconf_getline(fs, val, sizeof(val));
   }
   fclose(fs);

   /* Sanity check values */

   /* check changer_name makes sense */
   if (!strlen(changer_name)) {
      print_stderr("Config error: changer_name not specified\n");
      return false;
   }
   /* If not specified in config file, set default work_dir to changer_name
    * in the Bacula work directory */
   if (!workdir_set) {
      my_strncat(work_dir, DIR_DELIM, sizeof(work_dir));
      my_strncat(work_dir, changer_name, sizeof(work_dir));
   }
   /* check that work dir exists and is writable. Create if needed. */
   if (!check_path_access(work_dir, R_OK | W_OK)) {
      if (my_mkdir(work_dir, 0770)) {
         print_stderr("Cannot access work dir %s\n", work_dir);
         return false;
      }
   }
   /* check logfile (if any) exists and is writeable by this uid */
   if (strlen(logfile)) {
      if (logfile[0] != DIR_DELIM_C) {
         strncpy(val, logfile, sizeof(val));
         snprintf(logfile, sizeof(logfile), "%s%s%s", work_dir, DIR_DELIM, logfile);
      }
      if (!check_path_access(logfile, W_OK)) {
         fs = fopen(logfile, "w");
         if (!fs) {
            print_stderr("Could not create logfile %s\n", logfile);
            return false;
         }
         fclose(fs);
      }
      if (!check_path_access(logfile, W_OK)) {
         print_stderr("Logfile %s not accessible\n", logfile);
         return false;
      }
   }
   /* check that one or more known magazines have been assigned */
   if (known_magazines < 1) {
      print_stderr("Config error: no magazines have been assigned\n");
      return false;
   }
   /* check that number of magazine bays is valid */
   if (magazine_bays < 1) {
      print_stderr("Config error: magazine_bays must be greater than zero\n");
      return false;
   }
   /* check that number of magazine bays is not too large */
   if (magazine_bays > MAX_MAGAZINE_BAYS) {
      print_stderr("Config error: magazine_bays exceeds the maximum of %d\n", MAX_MAGAZINE_BAYS);
      return false;
   }
   /* check number of slots per magazine makes sense */
   if (slots_per_mag < 1) {
      print_stderr("Config error: slots_per_magazine must be greater than zero\n");
      return false;
   }
   if (slots_per_mag > MAX_SLOTS) {
      print_stderr("Config error: slots_per_magazine exceeds the maximum of %d\n",
            MAX_SLOTS);
      return false;
   }
   slots = magazine_bays * slots_per_mag;
   /* check number of virtual drives makes sense */
   if (virtual_drives < 1) {
      print_stderr("Config error: 'virtual_drives' must be greater than zero");
      return false;
   }
   if (virtual_drives > MAX_DRIVES) {
      print_stderr("Config error: 'virtual_drives' exceeds the maximum of %d", MAX_DRIVES);
      return false;
   }
   if (virtual_drives > slots) {
      print_stderr("Config error: 'virtual_drives' cannot be greater than the number of virtual slots");
      return false;
   }
#ifndef HAVE_WINDOWS_H
   /* Check that automount_dir exists */
   if (strlen(automount_dir)) {
      if (stat(automount_dir, &st)) {
         print_stderr("Config error: could not stat 'automount_dir' %s", automount_dir);
         return false;
      }
      /* remove trailing slash */
      n = strlen(automount_dir) - 1;
      if (automount_dir[n] == '/') {
         automount_dir[n] = '\0';
      }
   }
#else
   /* Windows does not use the automount_dir */
   memset(automount_dir, 0, sizeof(automount_dir));
#endif

   return true;
}
