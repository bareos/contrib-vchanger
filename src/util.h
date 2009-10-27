/*  util.h
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
#ifndef _UTIL_H_
#define _UTIL_H_ 1

#include "config.h"
#include <stdio.h>

/* Utility Functions */
void build_arglist(char *cmdline, int *argc, char *argv[], int max_argv);
void print_stderr(const char *format, ...);
void print_stdout(const char *format, ...);
void millisleep(long msecs);
long timeval_et_ms(struct timeval *tv1, struct timeval *tv2);
size_t strip_whitespace(char *buff, size_t buff_size);
int exclusive_fopen(const char *fname, FILE **fs);
int is_root_user();
int become_another_user(const char *user_name, const char *group_name);

#endif /* _UTIL_H_ */
