/* util.cpp
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
 * This file simply provides some utility functions
 */

#include "vchanger.h"
#include <stdarg.h>
#include <fcntl.h>
#include <time.h>
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_WINDOWS_H
#include <psapi.h>
#include <shlobj.h>
#else
#include <grp.h>
#include <pwd.h>
#endif
#include "util.h"

/*-------------------------------------------------
 *  Function to parse string 'cmdline' and build argc and argv
 *------------------------------------------------*/
void build_arglist(char *cmdline, int *argc, char *argv[], int max_argv)
{
   size_t n, q = 0;
   bool quote = false;
   memset(argv, 0, max_argv * sizeof(char*));
   *argc = 0;
   for (n = 0; cmdline[n] && (cmdline[n] == ' ' || cmdline[n] == '\t'); n++)
      ;
   while (cmdline[n]) {
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
#ifdef HAVE_WINDOWS_H
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
   return ((tv2->tv_sec - tv1->tv_sec) * 1000) + ((tv2->tv_usec - tv1->tv_usec)
         / 1000);
}

/*-------------------------------------------------
 *  Function to strip whitespace from beginning and
 *  end of string buff in place.
 *------------------------------------------------*/
size_t strip_whitespace(char *buff, size_t buff_size)
{
   int n;
   for (n = strlen(buff); n > 0 && isspace(buff[n - 1]); n--)
      ;
   buff[n] = 0;
   if (!n) {
      return 0;
   }
   for (n = 0; buff[n] && isspace(buff[n]); n++)
      ;
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
   fd = open(fname, O_CREAT | O_EXCL | O_RDWR, 0644);
   if (fd > 0) {
      *fs = fdopen(fd, "w");
      if (*fs) {
         return 0;
      } else
         close(fd);
   }
   result = errno;
   return result;
}

/*-------------------------------------------------
 *  Function to determine if user is root
 *------------------------------------------------*/
int is_root_user()
{
#ifdef HAVE_WINDOWS_H
   /* Since Bacula SD will run as SYSTEM, just assume admin rights on Windows */
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
#ifdef HAVE_WINDOWS_H
   /* Bacula SD will, as far as I know, run as a local SYSTEM account and so
    * vchanger will be invoked as SYSTEM. This means to manually run vchanger
    * you must be administrator. So we simply ignore user/group switching
    * for win32. */
   return 1;
#else
   struct passwd *pw;
   struct group *gr;
   uid_t new_uid;
   gid_t new_gid;

   /* lookup the user name */
   if ((pw = getpwnam(user_name)) == NULL) {
      return 0;
   }
   new_uid = pw->pw_uid;
   /* lookup the group name if specified */
   if (group_name && strlen(group_name)) {
      if ((gr = getgrnam(group_name)) == NULL) {
         return 0;
      }
      new_gid = gr->gr_gid;
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
