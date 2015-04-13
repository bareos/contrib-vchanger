/*  errhandler.h
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
*/
#ifndef _ERRHANDLER_H_
#define _ERRHANDLER_H_ 1

#include "tstring.h"

class ErrorHandler
{
public:
   ErrorHandler() : err(0) {}
   ~ErrorHandler() {}
   inline void clear() { err = 0; msg.clear(); }
   void SetError(int errnum, const char *fmt, ...);
   void SetErrorWithErrno(int errnum, const char *fmt, ...);
   inline const char* GetErrorMsg() const { return msg.c_str(); }
   inline int GetError() { return err; }
protected:
   int err;
   tString msg;
};

#endif /* _ERRHANDLER_H_ */
