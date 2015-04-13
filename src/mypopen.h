/*  mypopen.h
 *
 *  Copyright (C) 2013-2014 Josh Fisher
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

#ifndef _MYPOPEN_H_
#define _MYPOPEN_H_ 1

#include "tstring.h"

int mypopen_raw(const char *command, int *fno_stdin, int *fno_stdout, int *fno_stderr);
inline int mypopen_raw(const tString &command, int *fno_stdin, int *fno_stdout, int *fno_stderr)
   { return mypopen_raw(command.c_str(), fno_stdin, fno_stdout, fno_stderr); }
int mypopen(const char *command, FILE **cmd_stdin = NULL, FILE **cmd_stdout = NULL,
            FILE **cmd_stderr = NULL);
inline int mypopen(const tString &command, FILE **cmd_stdin = NULL, FILE **cmd_stdout = NULL,
      FILE **cmd_stderr = NULL)
   { return mypopen(command.c_str(), cmd_stdin, cmd_stdout, cmd_stderr); }
int mypopenrw(const char *command, const char *cmd_in = NULL, const char *cmd_out = NULL, const char *cmd_err = NULL);
inline int mypopenrw(const tString &command, const char *cmd_in = NULL, const char *cmd_out = NULL, const char *cmd_err = NULL)
   { return mypopenrw(command.c_str(), cmd_in, cmd_out, cmd_err); }

#endif /* _MYPOPEN_H_ */
