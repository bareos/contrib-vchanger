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
 *  Returns zero on success, or errno if there was an error running the command
 *  or a timeout occurred.
 */
int issue_bconsole_command(const char *bcmd)
{
   int pid, rc, n, len, fno_in = -1, fno_out = -1;
   struct timeval tv;
   fd_set rfd;
   tString cmd, tmp;
   char buf[4096];

#ifndef HAVE_WINDOWS_H
   /* Build command line */
   cmd = conf.bconsole;
   if (cmd.empty()) return 0;
   if (!conf.bconsole_config.empty()) {
      cmd += " -c ";
      cmd += conf.bconsole_config;
   }
   cmd += " -n -u 30";
   /* Start bconsole process */
   log.Debug("bconsole: running '%s'", bcmd);
   pid = mypopen_raw(cmd.c_str(), &fno_in, &fno_out, NULL);
   if (pid < 0) {
      rc = errno;
      log.Error("bconsole: run failed errno=%d", rc);
      errno = rc;
      return rc;
   }
   /* Wait for bconsole to accept input */
   tv.tv_sec = 30;
   tv.tv_usec = 0;
   FD_ZERO(&rfd);
   FD_SET(fno_in, &rfd);
   rc = select(fno_in + 1, NULL, &rfd, NULL, &tv);
   if (rc == 0) {
      log.Error("bconsole: timed out waiting to send command");
      close(fno_in);
      close(fno_out);
      errno = ETIMEDOUT;
      return ETIMEDOUT;
   }
   if (rc < 0) {
      rc = errno;
      log.Error("bconsole: errno=%d waiting to send command", rc);
      close(fno_in);
      close(fno_out);
      errno = rc;
      return rc;
   }
   /* Send command to bconsole's stdin */
   len = strlen(bcmd);
   n = 0;
   while (n < len) {
      rc = write(fno_in, bcmd + n, len - n);
      if (rc < 0) {
         rc = errno;
         log.Error("bconsole: send to bconsole's stdin failed errno=%d", rc);
         close(fno_in);
         close(fno_out);
         errno = rc;
         return rc;
      }
      n += rc;
   }
   close(fno_in);

   /* Read stdout from bconsole */
   memset(buf, 0, sizeof(buf));
   tmp.clear();
   while (true) {
      tv.tv_sec = 30;
      tv.tv_usec = 0;
      FD_ZERO(&rfd);
      FD_SET(fno_out, &rfd);
      rc = select(fno_out + 1, &rfd, NULL, NULL, &tv);
      if (rc == 0) {
         log.Error("bconsole: timed out waiting for bconsole output");
         close(fno_out);
         errno = ETIMEDOUT;
         return ETIMEDOUT;
      }
      if (rc < 0) {
         rc = errno;
         log.Error("bconsole: errno=%d waiting for bconsole output", rc);
         close(fno_out);
         errno = rc;
         return rc;
      }
      rc = read(fno_out, buf, 4095);
      if (rc < 0) {
         rc = errno;
         log.Error("bconsole: errno=%d reading bconsole stdout", rc);
         close(fno_out);
         errno = rc;
         return rc;
      } else if (rc > 0) {
         buf[rc] = 0;
         tmp += buf;
      } else break;
   }
   close(fno_out);
   log.Debug("bconsole: output:\n%s", tmp.c_str());

   /* Wait for bconsole process to finish */
   pid = waitpid(pid, &rc, 0);
   if (!WIFEXITED(rc)) {
      log.Error("bconsole: abnormal exit of bconsole process");
      return EPIPE;
   }
   if (WEXITSTATUS(rc)) {
      log.Error("bconsole: exited with rc=%d", WEXITSTATUS(rc));
      return WEXITSTATUS(rc);
   }
   return 0;
#else
   return EINVAL;
#endif
}
