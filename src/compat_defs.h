/*  vchanger_common.h
 *
 *  This file is part of vchanger by Josh Fisher.
 *
 *  vchanger copyright (C) 2008-2015 Josh Fisher
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
#ifndef _COMPAT_DEFS_H_
#define _COMPAT_DEFS_H_ 1

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

#endif /* _VCHANGER_COMMON_H_ */
