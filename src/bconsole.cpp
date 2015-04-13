/* bconsole.cpp
 *
 *  Copyright (C) 2008-2014 Josh Fisher
 *
 *  This program is free software. You may redistribute it and/or modify
 *  it under the terms of the GNU General Public License, as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  See the file "COPYING".  If not,
 *  see <http://www.gnu.org/licenses/>.
*/

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#else
#ifdef HAVE_WINSOCK_H
#include <winsock.h>
#endif
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_CTYPE_H
#include <ctype.h>
#endif

#include "loghandler.h"
#include "mypopen.h"
#include "vconf.h"
#include "bconsole.h"


/*
 *  Function to issue command in Bacula console.
 *  Returns zero on success, or negative if there was an error running the command
 *  or a timeout occurred.
 */
int issue_bconsole_command(const char *bcmd)
{
   int pid, rc, fno_in = -1, fno_out = -1;
   struct timeval tv;
   fd_set rfd;
   tString cmd, tmp;
   char buf[16];

   /* Build command line */
   cmd = conf.bconsole;
   if (cmd.empty()) return 0;
   if (!conf.bconsole_config.empty()) {
      cmd += " ";
      cmd += conf.bconsole_config;
   }
   /* Start bconsole process */
   log.Debug("bconsole: running '%s'", bcmd);
   pid = mypopen_raw(cmd.c_str(), &fno_in, &fno_out, NULL);
   if (pid < 0) {
      rc = errno;
      log.Error("bconsole: run failed errno=%d", rc);
      return -1;
   }
   /* Send command to bconsole's stdin */
   tFormat(tmp, "%s\nquit\n", bcmd);
   if (write(fno_in, tmp.c_str(), tmp.size()) < 0) {
      rc = errno;
      log.Error("bconsole: send to bconsole's stdin failed errno=%d", rc);
      close(fno_in);
      close(fno_out);
      errno = rc;
      return -1;
   }
   close(fno_in);

#ifndef HAVE_WINDOWS_H
   /* Wait for data available on bconsole's stdout */
   FD_ZERO(&rfd);
   FD_SET(fno_out, &rfd);
   tv.tv_sec = 10;
   tv.tv_usec = 0;
   rc = select(fno_out + 1, &rfd, NULL, NULL, &tv);
   if (rc < 0) {
      rc = errno;
      close(fno_out);
      log.Error("bconsole: select() had errno %d", rc);
      errno = rc;
      return -1;
   }
   if (rc == 0) {
      close(fno_out);
      log.Error("bconsole: timeout waiting for bconsole output");
      errno = ETIMEDOUT;
      return -1;
   }
#endif

   /* Read stdout from bconsole */
   memset(buf, 0, sizeof(buf));
   tmp.clear();
   rc = read(fno_out, buf, 1);
   while (rc > 0) {
      tmp += buf[0];
      rc = read(fno_out, buf, 1);
   }
   log.Debug("bconsole: output:\n%s", tmp.c_str());
   if (rc) {
      rc = errno;
      log.Error("bconsole: read stdout errno=%d", rc);
   }
   close(fno_out);
   return 0;
}
