/*
 *
 *  This file is part of vchanger by Josh Fisher.
 *
 *  vchanger copyright (C) 2008-2013 Josh Fisher
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

#ifndef _LOCALTIME_R_H_
#define _LOCALTIME_R_H_

#ifndef HAVE_LOCALTIME_R
/* For systems without localtime_r function, use internal version */
#ifdef __cplusplus
extern "C" {
#endif
struct tm* localtime_r(const time_t *timep, struct tm *buf);
#ifdef __cplusplus
}
#endif
#endif


#endif /* _LOCALTIME_R_H_ */
