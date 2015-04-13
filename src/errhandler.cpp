/* errhandler.cpp
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
 *  Provides a class for logging messages to a file
 */

#include "config.h"
#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif
#ifdef HAVE_VARARGS_H
#include <varargs.h>
#endif

#include "errhandler.h"

///////////////////////////////////////////////////
//  Class ErrorHandler
///////////////////////////////////////////////////

/*-------------------------------------------------
 *  Method to set error and error message
 *------------------------------------------------*/
void ErrorHandler::SetError(int errnum, const char *fmt, ...)
{
   va_list vl;
   char buf[4096];
   err = errnum;
   va_start(vl, fmt);
   vsnprintf(buf, sizeof(buf), fmt, vl);
   va_end(vl);
   msg = buf;
}

/*-------------------------------------------------
 *  Method to set error and error message and append the text
 *  returned from strerror(errnum).
 *------------------------------------------------*/
void ErrorHandler::SetErrorWithErrno(int errnum, const char *fmt, ...)
{
   va_list vl;
   char buf[4096];
   err = errnum;
   va_start(vl, fmt);
   vsnprintf(buf, sizeof(buf), fmt, vl);
   va_end(vl);
   msg = buf;
   msg += ": ";
   msg += strerror(err);
}
