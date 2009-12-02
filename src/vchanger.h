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

#include "config.h"
#ifdef HAVE_WINDOWS_H
#include "targetver.h"
#define DIR_DELIM "\\"
#define DIR_DELIM_C '\\'
#define MAG_VOLUME_MASK 0
#else
#define DIR_DELIM "/"
#define DIR_DELIM_C '/'
#define MAG_VOLUME_MASK S_IWGRP|S_IRWXO
#endif

#define _REENTRANT    1
#define _THREAD_SAFE  1
#define _POSIX_PTHREAD_SEMANTICS 1

/* System includes */
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#if HAVE_STRINGS_H
#include <strings.h>
#endif
#if HAVE_ALLOCA_H
#include <alloca.h>
#endif
#if HAVE_LIMITS_H
#include <limits.h>
#endif
#ifdef HAVE_SYS_BITYPES_H
#include <sys/bitypes.h>
#endif
#if HAVE_UNISTD_H
#  ifdef HAVE_HPUX_OS
#  undef _INCLUDE_POSIX1C_SOURCE
#  endif
#include <unistd.h>
#endif
#ifdef HAVE_WINDOWS_H
#include "junctions.h"
#endif

#include "compat_util.h"

#endif
