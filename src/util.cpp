/* util.cpp
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
 * This file simply provides some utility functions
 */

#include "config.h"
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_CTYPE_H
#include <ctype.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_GRP_H
#include <grp.h>
#endif
#ifdef HAVE_PWD_H
#include <pwd.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#include "util.h"

/*-------------------------------------------------
 *  Function to return elapsed time between two struct timeval values
 *  in microseconds.
 *------------------------------------------------*/
long timeval_et(struct timeval *tv1, struct timeval *tv2)
{
   if (!tv1 || !tv2) return 0;
   return ((tv2->tv_sec - tv1->tv_sec) * 1000000) + tv2->tv_usec - tv1->tv_usec;
}


/*-------------------------------------------------
 *  Function to open file 'fname' for write in exclusive access mode.
 *  On success, returns the opened stream. Otherwise, on error returns
 *  NULL.
 *------------------------------------------------*/
int exclusive_fopen(const char *fname, FILE **fs)
{
   int fd, result = 0;
   *fs = NULL;
   fd = open(fname, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
   if (fd > 0) {
      *fs = fdopen(fd, "w");
      if (*fs == NULL) {
         result = errno;
         close(fd);
         unlink(fname);
      }
   } else {
      result = errno;
   }
   return result;
}


/*-------------------------------------------------
 *  Function to copy file 'from_path' to new file 'to_path'.
 *  On success returns zero, else returns errno
 *------------------------------------------------*/
int file_copy(const char *to_path, const char *from_path)
{
   int rc;
   size_t n;
   FILE *to, *from;
   char *buf;
   size_t buf_sz = 1024 * 1024;

   if (access(to_path, F_OK) == 0) return EEXIST;
   from = fopen(from_path, "r");
   if (!from) return errno;
   to = fopen(to_path, "w");
   if (!to) {
      rc = errno;
      fclose(from);
      return rc;
   }
   buf = (char*)malloc(buf_sz);
   while ((n = fread(buf, 1, buf_sz, from)) > 0) {
      if (fwrite(buf, 1 , n, to) != n) {
         rc = ferror(to);
         fclose(to);
         unlink(to_path);
         fclose(from);
         free(buf);
         return rc;
      }
   }
   free(buf);
   fclose(to);
   if (!feof(from)) {
      rc = ferror(from);
      unlink(to_path);
      fclose(from);
      return rc;
   }
   fclose(from);
   return 0;
}

/*-------------------------------------------------
 *  Function to drop root privileges and change persona to uid:gid
 *  of the given user name and group name.
 *  On success returns zero, else on error returns errno.
 *------------------------------------------------*/
int drop_privs(const char *uname, const char *gname)
{
#ifdef HAVE_WINDOWS_H
   return 0;  /* For windows ignore user switching */
#else
   gid_t new_gid;
   struct passwd *pw;
   struct group *gr;
   if (!uname || !uname[0] || getuid()) return 0; /* Nothing to do */
   if ((pw = getpwnam(uname)) == NULL) return errno;
   if (pw->pw_uid == 0) return 0; /* already running as root */
   new_gid = pw->pw_gid;
   if (gname && gname[0]) {
      /* find given group */
      if ((gr = getgrnam(gname)) == NULL) return errno; /* no such group */
      new_gid = gr->gr_gid;
   }
   /* Set supplemental groups */
   if (initgroups(uname, new_gid)) return errno;
   /* Start running as given group */
   if (setgid(new_gid)) return errno;
   /* Drop root and run as given user */
   if (setuid(pw->pw_uid)) return errno;
   return 0;
#endif
}


/*-------------------------------------------------
 *  Function returns zero if current user not superuser
 *  else non-zero.
 *------------------------------------------------*/
int is_root_user()
{
#ifndef HAVE_WINDOWS_H
   return getuid() == 0 ? 1 : 0;
#endif
   return 0;
}
