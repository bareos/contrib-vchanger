/*  getline.c
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

#include "config.h"

#ifndef HAVE_GETLINE

#ifdef HAVE_WINDOWS_H
#include "targetver.h"
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#include "compat/getline.h"

/*
 *   Emulate GNU extension function getline().
 */
ssize_t getline(char **lineptr, size_t *lineptr_sz, FILE *stream)
{
   int c;
   size_t n = 0;

   c = fgetc(stream);
   if (c == EOF) {
      return -1;
   }
   if (*lineptr == NULL) {
      *lineptr_sz = 4096;
      *lineptr = (char*)malloc(*lineptr_sz);
      if (*lineptr == NULL) {
         return -1;
      }
   }
   while (c != EOF && c != '\n')
   {
      if (n + 1 >= *lineptr_sz) {
         *lineptr_sz += 4096;
         *lineptr = (char*)realloc(*lineptr, *lineptr_sz);
         if (*lineptr == NULL) {
            return -1;
         }
      }
      (*lineptr)[n++] = (char)c;
      c = fgetc(stream);
   }
   if (c == '\n') {
      if (n && (*lineptr)[n-1] == '\r') {
         (*lineptr)[n-1] = '\n';
      } else {
         (*lineptr)[n++] = '\n';
      }
   }
   (*lineptr)[n] = 0;
   return (ssize_t)n;
}
#endif
