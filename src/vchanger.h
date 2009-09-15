/* vchanger.h
 *
 *  This file is part of the vchanger package
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

#ifndef _VCHANGER_H
#define _VCHANGER_H 1

#if defined(HAVE_WIN32)
#if defined(_MSC_VER) && (_MSC_VER >= 1400)
#pragma warning(disable : 4996)
#endif
#include "targetver.h"
#include "winconfig.h"
#define DIR_DELIM "\\"
#define DIR_DELIM_C '\\'
#define MAG_VOLUME_MASK 0
#else
#include "config.h"
#define DIR_DELIM "/"
#define DIR_DELIM_C '/'
#define MAG_VOLUME_MASK S_IWGRP|S_IRWXO
#endif


#define _REENTRANT    1
#define _THREAD_SAFE  1
#define _POSIX_PTHREAD_SEMANTICS 1

/* System includes */
#if HAVE_STDINT_H
#include <stdint.h>
#endif
#if HAVE_STDARG_H
#include <stdarg.h>
#endif
#include <stdio.h>
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_UNISTD_H
#  ifdef HAVE_HPUX_OS
#  undef _INCLUDE_POSIX1C_SOURCE
#  endif
#include <unistd.h>
#endif
#if HAVE_ALLOCA_H
#include <alloca.h>
#endif
#if defined(_MSC_VER)
#include <io.h>
#include <direct.h>
#include <process.h>
typedef int pid_t;
#endif
#include <errno.h>
#include <fcntl.h>

/* O_NOATIME is defined at fcntl.h when supported */
#ifndef O_NOATIME
#define O_NOATIME 0
#endif

#if defined(_MSC_VER)
#include <getopt.h>
#endif

#ifdef xxxxx
#ifdef HAVE_GETOPT_LONG
#include <getopt.h>
#else
#include "lib/getopt.h"
#endif
#endif

#include <string.h>
#if HAVE_STRINGS_H
#include <strings.h>
#endif
#include <signal.h>
#include <ctype.h>
#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif
#if HAVE_LIMITS_H
#include <limits.h>
#endif
//#include <pwd.h>
//#include <grp.h>
#include <time.h>
#include <sys/types.h>
#ifdef HAVE_SYS_BITYPES_H
#include <sys/bitypes.h>
#endif
//#include <sys/ioctl.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#if HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#if defined(HAVE_WIN32)
#include "w32compat_util.h"
#include "junctions.h"
//#include "w32compat.h"
#endif

#endif
