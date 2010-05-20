/* uuidlookup.h
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
 */

#ifndef __UUIDLOOKUP_H_
#define __UUIDLOOKUP_H_

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

int GetMountpointFromUUID(char *mountp, size_t mountp_sz, const char *uuid_str);

#ifdef __cplusplus
}
#endif

#endif  /*  __UUIDLOOKUP_H_  */
