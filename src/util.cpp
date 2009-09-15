/* util.cpp
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
 *
 * This file simply provides some utility functions
*/

#include "vchanger.h"
#include "util.h"
#include <stdarg.h>
#ifdef HAVE_WIN32
#include <psapi.h>
#include <shlobj.h>
#else
#include <grp.h>
#include <pwd.h>
#endif


/*-------------------------------------------------
 *  Function to parse string 'cmdline' and build argc and argv
 *------------------------------------------------*/
void build_arglist(char *cmdline, int *argc, char *argv[], int max_argv)
{
   size_t n, q = 0;
   bool quote = false;
   memset(argv, 0, max_argv * sizeof(char*));
   *argc = 0;
   for (n = 0; cmdline[n] && (cmdline[n] == ' ' || cmdline[n] == '\t'); n++);
   while (cmdline[n])
   {
      if (cmdline[n] == ' ' || cmdline[n] == '\t') {
	 if (quote) {
	    if (!q) {
	       q = n;
	    }
	 } else if (q) {
	    argv[(*argc)++] = cmdline + q;
	    if (*argc >= max_argv) {
	       return;
	    }
	    cmdline[n] = '\0';
	    q = 0;
	 }
      } else if (cmdline[n] == '"' || cmdline[n] == '\'') {
	 if (quote) {
	    argv[(*argc)++] = cmdline + q;
	    if (*argc >= max_argv) {
	       return;
	    }
	    cmdline[n] = '\0';
	    q = 0;
	    quote = false;
	 } else {
	    quote = true;
	 }
      } else if (!q) {
	 q = n;
      }
      n++;
   }
   if (q) {
      argv[(*argc)++] = cmdline + q;
   }
}


/*-------------------------------------------------
 *  Function to print to stderr
 *------------------------------------------------*/
void print_stderr(const char *format, ...)
{
   va_list args;
   va_start(args, format);
   vfprintf(stderr, format, args);
   va_end(args);
}


/*-------------------------------------------------
 *  Function to print to stdout
 *------------------------------------------------*/
void print_stdout(const char *format, ...)
{
   va_list args;
   va_start(args, format);
   vfprintf(stdout, format, args);
   va_end(args);
}


/*-------------------------------------------------
 *  Function to sleep for milliseconds.
 *------------------------------------------------*/
void millisleep(long msecs)
{
#ifdef WIN32
   Sleep(msecs);
#else
   timespec ts;
   ts.tv_sec = msecs / 1000;
   ts.tv_nsec = (msecs % 1000) * 1000000;
   nanosleep(&ts, NULL);
#endif
}


/*-------------------------------------------------
 *  Function to get et between two struct timeval with
 *  millisecond precision.
 *------------------------------------------------*/
long timeval_et_ms(struct timeval *tv1, struct timeval *tv2)
{
   if (!tv1 || !tv2) {
      return 0;
   }
   return ((tv2->tv_sec - tv1->tv_sec) * 1000) + ((tv2->tv_usec - tv1->tv_usec) / 1000);
}


/*-------------------------------------------------
 *  Function to strip whitespace from beginning and
 *  end of string buff in place.
 *------------------------------------------------*/
size_t strip_whitespace(char *buff, size_t buff_size)
{
   int n;
   for (n = strlen(buff); n > 0 && isspace(buff[n-1]); n--);
   buff[n] = 0;
   if (!n) {
      return 0;
   }
   for (n = 0; buff[n] && isspace(buff[n]); n++);
   memmove(buff, buff + n, strlen(buff + n) + 1);
   return strlen(buff);
}


/*-------------------------------------------------
 *  Function to open file 'fname' for write in exclusive mode.
 *  On success, returns the opened stream. Otherwise, returns
 *  NULL.
 *------------------------------------------------*/
int exclusive_fopen(const char *fname, FILE **fs)
{
   int result, fd;
   fd = open(fname, O_CREAT|O_EXCL|O_RDWR, 0644);
   if (fd > 0) {
      *fs = fdopen(fd, "w");
      if (*fs) {
	 return 0;
      }
      else close(fd);
   }
   result = errno;
   return result;
}


/*-------------------------------------------------
 *  Function to determine if process exists
 *------------------------------------------------*/
int is_pid_active(pid_t pid)
{
#ifdef WIN32
   DWORD n, plist[1024], plist_size, num_procs;
   EnumProcesses(plist, sizeof(plist), &plist_size);
   num_procs = plist_size / sizeof(DWORD);
   for (n = 0; n < num_procs; n++)
   {
      if (plist[n] == (unsigned int)pid) {
	 return -1;
      }
   }
#else
   char procdir[PATH_MAX];
   struct stat st;
   snprintf(procdir, sizeof(procdir), "/proc/%d", pid);
   if (stat(procdir, &st) == 0) {
      return -1;
   }
#endif
   return 0;
}


/*-------------------------------------------------
 *  Function to determine if user is root
 *------------------------------------------------*/
int is_root_user()
{
#ifdef WIN32
   /* Since Bacula SD will run as SYSTEM, just assume admin rights */
   return 1;
#else
   if (getuid()) {
      return 0;
   }
   return 1;
#endif
}


/*-------------------------------------------------
 *  Function to become another user and group
 *------------------------------------------------*/
int become_another_user(const char *user_name, const char *group_name)
{
#ifdef WIN32
   /* Bacula SD will, as far as I know, run as a local SYSTEM account and so
    * vchanger will be invoked as SYSTEM. This means to manually run vchanger
    * you must be administrator. So we simply ignore user/group switching
    * for win32. */
   return 1;
#else
   struct passwd pws, *pw;
   struct group grs, *gr;
   char *buf;
   uid_t new_uid;
   gid_t new_gid;
   int pwbuf_sz = sysconf(_SC_GETPW_R_SIZE_MAX);
   int grbuf_sz = sysconf(_SC_GETGR_R_SIZE_MAX);

   /* lookup the user name */
   buf = (char*)malloc(pwbuf_sz);
   if (getpwnam_r(user_name, &pws, buf, pwbuf_sz, &pw)) {
      free(buf);
      return 0;
   }
   if (!pw) {
      free(buf);
      return 0;
   }
   new_uid = pw->pw_uid;
   free(buf);
   /* lookup the group name if specified */
   if (group_name && strlen(group_name)) {
      buf = (char*)malloc(grbuf_sz);
      if (getgrnam_r(group_name, &grs, buf, grbuf_sz, &gr)) {
	 free(buf);
	 return 0;
      }
      if (!gr) {
	 free(buf);
	 return 0;
      }
      new_gid = gr->gr_gid;
      free(buf);
      /* Set the gid to run as */
      if (setgid(new_gid)) {
	 return 0;
      }
   }
   /* switch user */
   if (setuid(new_uid)) {
      return 0;
   }
   return 1;
#endif
}


/*-------------------------------------------------
 *  Builds volume name in vname, given changer name, magazine number, and magazine slot.
 *------------------------------------------------*/
char* BuildVolumeName(char *vname, size_t vname_sz, const char *chngr, int magnum, int magslot)
{
   snprintf(vname, vname_sz, "%s_%04d_%04d", chngr, magnum, magslot);
   return vname;
}


/*-------------------------------------------------
 *  Parse changer name, magazine number, and magazine slot from volume name
 *------------------------------------------------*/
int ParseVolumeName(const char *vname, char *chgr_name, size_t chgr_name_sz,
					int &mag_num, int &mag_slot)
{
   int n, p, len;
   char buf[512];

   if (chgr_name) chgr_name[0] = 0;
   mag_num = -1;
   mag_slot = -1;
   if (!vname) {
      return -1;
   }
   len = strlen(vname);
   if (!len) {
      return -1;
   }
   strncpy(buf, vname, sizeof(buf));
   for (p = 0; buf[p] && buf[p] != '_'; p++);
   if (!buf[p]) return -1;
   buf[p] = 0;
   if (chgr_name) {
      strncpy(chgr_name, buf, chgr_name_sz);
   }
   ++p;
   for (n = p; buf[n] && buf[n] != '_'; n++);
   if (!buf[n]) {
      return -1;
   }
   buf[n] = 0;
   mag_num = (int)strtol(buf + p, NULL, 10);
   p = n + 1;
   mag_slot = (int)strtol(buf + p, NULL, 10);
   return 0;
}

