/* junctions.h
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

#ifndef __JUNCTIONS_H_
#define __JUNCTIONS_H_

#include "targetver.h"
#include <windows.h>
#include <sys/types.h>


typedef struct {
   DWORD ReparseTag;
   DWORD ReparseDataLength;
   WORD SubstituteNameOffset;
   WORD SubstituteNameLength;
   WORD PrintNameOffset;
   WORD PrintNameLength;
   WCHAR ReparseTarget[1];
} REPARSE_MOUNTPOINT_DATA_BUFFER;

//#define REPARSE_DATA_BUFFER_HEADER_SIZE 8
//#define MAXIMUM_REPARSE_DATA_BUFFER_SIZE  ( 16 * 1024 )

#ifdef __cplusplus
extern "C" {
#endif

DWORD CreateJunction(const char *junction, const char *target);
DWORD ReadReparsePoint(const char *path, char *target, size_t target_size);
DWORD DeleteReparsePoint(const char *path);
BOOL IsReparsePoint(const char *path);
int symlink(const char *target, const char *path);
int unlink_symlink(const char *path);
ssize_t readlink(const char *path, char *buf, size_t bufsiz);

#ifdef __cplusplus
}
#endif

#endif  /*  __JUNCTIONS_H_  */
