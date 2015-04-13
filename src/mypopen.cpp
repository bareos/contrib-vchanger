/* mypopen.cpp
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

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_CTYPE_H
#include <ctype.h>
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#include "loghandler.h"
#include "mypopen.h"

/*
 *  Function to parse command line into argv and argc.
 *  cmd is command line to be executed. Arguments in cmd will be
 *  parsed and copied to buffer. On output, each element of argv[]
 *  will point to a NULL-terminated string located in buffer.
 *  argv[0] will point to the command and argc will be updated to
 *  indicate the number of arguments found (including the command
 *  in argv[0]). On input, if *argv_in is NULL, then memory for
 *  the argv[] array will be allocated and the caller will be
 *  responsible for freeing it. If *argv_in is non-null, then
 *  *argc must be set to the max number of elements that *argv_in
 *  can hold, including the terminating NULL pointer.
 */
static int mypopen_args(const char *cmd, char* (&argv)[50], int &argc, char *buffer, size_t buffer_sz)
{
   char quote, *b, *b1;
   const char *p;
   int maxargs = 50;

   /* Initialize arg array */
   if (argc < 2) return -1;
   maxargs = argc;
   argc = 0;
   memset(argv, 0, maxargs * sizeof(char*));
   /* Initialize arg buffer */
   if (strlen(cmd) + 1 >= buffer_sz) return -1;
   *buffer = '\0';

   /* Parse args */
   b = buffer;
   p = cmd;
   b1 = b;
   --maxargs; /* Ensure room for trailing NULL arg ptr */
   while (*p) {
      /* skip whitespace between args */
      if (isspace(*p)) {
         ++p;
         continue;
      }
      /* Copy next arg to buffer */
      if (*p == '"' || *p == '\'') {
         /* Quoted string is treated as one arg */
         quote = *p;
         ++p;
         while (*p && *p != quote) {
            if (*p == '\\') {
               ++p;
               if (*p == 0) break;
            }
            *b = *p;
            ++b;
            ++p;
         }
      } else {
         /* Arg is space-delimited */
         while (*p && !isspace(*p)) {
            *b = *p;
            ++b;
            ++p;
         }
      }
      *b = '\0';
      ++b;
      /* Set argument array pointer */
      argv[argc] = b1;
      /* Position to next arg */
      b1 = b;
      argc += 1;
      if (argc >= maxargs) {
         /* Max args exceeded */
         argc = 0;
         return -1;
      }
   }
   /* Check for no args at all */
   if (argc == 0) {
      return -1;
   }

   return 0;
}


/*
 *  Function to close all pipes that may be open
 */
static void CloseAllPipes(int *pipe_in, int *pipe_out, int *pipe_err)
{
   if (pipe_in[0] >= 0) close(pipe_in[0]);
   if (pipe_in[1] >= 0) close(pipe_in[1]);
   if (pipe_out[0] >= 0) close(pipe_out[0]);
   if (pipe_out[1] >= 0) close(pipe_out[1]);
   if (pipe_err[0] >= 0) close(pipe_err[0]);
   if (pipe_err[1] >= 0) close(pipe_err[1]);
}



/*
 *  Function to fork a child process, specifying the command and arguments to be
 *  run in the child and the child's standard i/o files. If fno_stdXXX is NULL,
 *  then the child will inherit the parent's stdXXX. If *fno_stdXXX is -1, then
 *  a pipe will be created with the child's stdXXX being one end of the pipe and
 *  the other end of the pipe being passed back to the caller in *fno_stdXXX. If
 *  *fno_stdXXX is zero or greater, then it specifies a file descriptor that will be
 *  used as the corresponding stdXXX in the child.
 *  On success, returns the pid of the child. On error, returns -1 and sets errno.
 */
#ifndef HAVE_WINDOWS_H
static int do_mypopen_raw(const char *cline, int *fno_stdin, int *fno_stdout, int *fno_stderr)
{
   int rc, pipe_in[2], pipe_out[2], pipe_err[2];
   int n, pid = -1, argc = 50;
   char* argv[50];
   char argbuf[4096];

   /* Sanity check parameters */
   if (!cline || !cline[0]) {
      errno = EINVAL;
      return -1;
   }
   for (n = 0; n < 2; n++)
   {
      pipe_in[n] = -1;
      pipe_out[n] = -1;
      pipe_err[n] = -1;
   }

   /* Build argv array from command line string */
   if (mypopen_args(cline, argv, argc, argbuf, sizeof(argbuf))) {
      /* Invalid args, so terminate child */
      log.Debug("popen: invalid cmdline args for child");
      errno = EINVAL;
      return -1;
   }

   /* Setup child's stdin */
   if (fno_stdin) {
      if (*fno_stdin < 0) {
         /* Caller requests a pipe by specifying descriptor -1 */
         rc = pipe(pipe_in);
         if (rc) {
            /* error creating pipe */
            return -1;
         }
         log.Debug("popen: child stdin uses pipe (%d -> %d)", pipe_in[0], pipe_in[1]);
      } else if (*fno_stdin == STDIN_FILENO) {
         /* Caller specified stdin so just let child inherit it */
         fno_stdin = NULL;
      }
   }

   /* Setup child's stdout */
   if (fno_stdout) {
      if (*fno_stdout < 0) {
         /* Caller requests a pipe by specifying descriptor -1 */
         rc = pipe(pipe_out);
         if (rc) {
            /* error creating pipe */
            rc = errno;
            CloseAllPipes(pipe_in, pipe_out, pipe_err);
            errno = rc;
            return -1;
         }
         log.Debug("popen: child stdout uses pipe (%d -> %d)", pipe_out[0], pipe_out[1]);
      } else {
         if (*fno_stdout == STDOUT_FILENO) fno_stdout = NULL;
         else fsync(*fno_stdout);
      }
   }

   /* Setup child's stderr */
   if (fno_stderr) {
      if (*fno_stderr < 0) {
         /* Caller requests a pipe by specifying descriptor -1 */
         rc = pipe(pipe_err);
         if (rc) {
            /* error creating pipe */
            rc = errno;
            CloseAllPipes(pipe_in, pipe_out, pipe_err);
            errno = rc;
            return -1;
         }
         log.Debug("popen: child stderr uses pipe (%d -> %d)", pipe_err[0], pipe_err[1]);
      } else {
         if (*fno_stderr == STDERR_FILENO) fno_stderr = NULL;
         else fsync(*fno_stderr);
      }
   }

   /* fork a child process to run the command in */
   log.Debug("popen: forking now");
   pid = fork();
   switch (pid)
   {
   case -1: /* error creating process */
      rc = errno;
      CloseAllPipes(pipe_in, pipe_out, pipe_err);
      errno = rc;
      return -1;

   case 0: /* child is running */
      /* close pipe ends always used by parent */
      log.Debug("popen: child closing pipe ends %d,%d,%d used by parent", pipe_in[1], pipe_out[0], pipe_err[0]);
      if (pipe_in[1] >= 0) close(pipe_in[1]);
      if (pipe_out[0] >= 0) close(pipe_out[0]);
      if (pipe_err[0] >= 0) close(pipe_err[0]);
      /* assign child's stdin */
      if (fno_stdin) {
         if (*fno_stdin < 0) {
            /* Read end of pipe will be child's stdin */
            log.Debug("popen: child will read stdin from %d", pipe_in[0]);
            dup2(pipe_in[0], STDIN_FILENO);
            close(pipe_in[0]);
         } else {
            /* Child's stdin will read from caller's input file */
            dup2(*fno_stdin, STDIN_FILENO);
            close(*fno_stdin);
         }
      }
      /* assign child's stdout */
      if (fno_stdout) {
         if (*fno_stdout < 0) {
            /* Write end of pipe will be child's stdout */
            log.Debug("popen: child will write stdout to %d", pipe_out[1]);
            dup2(pipe_out[1], STDOUT_FILENO);
            close(pipe_out[1]);
         } else {
            /* Child's stdout will write to caller's output file */
            dup2(*fno_stdout, STDOUT_FILENO);
            close(*fno_stdout);
         }
      }
      /* assign child's stderr */
      if (fno_stderr) {
         if (*fno_stderr < 0) {
            /* Write end of pipe will be child's stderr */
            log.Debug("popen: child will write stderr to %d", pipe_err[1]);
            dup2(pipe_err[1], STDERR_FILENO);
            close(pipe_err[1]);
         } else {
            /* Child's stderr will write to caller's error file */
            dup2(*fno_stderr, STDERR_FILENO);
            close(*fno_stderr);
         }
      }
      /* now run the command */
      log.Debug("popen: child executing '%s'", argv[0]);
      execvp(argv[0], argv);
      /* only gets here if execvp fails */
      return -1;
   }

   /* parent is running this */

   /* close pipe ends always used by child */
   log.Debug("popen: parent closing pipe ends %d,%d,%d used by child", pipe_in[0], pipe_out[1], pipe_err[1]);
   if (pipe_in[0] >= 0) close(pipe_in[0]);
   if (pipe_out[1] >= 0) close(pipe_out[1]);
   if (pipe_err[1] >= 0) close(pipe_err[1]);

   /* Pass pipe ends being used back to caller */
   if (fno_stdin) {
      if (*fno_stdin < 0) {
         /* Caller will be writing to child's stdin through pipe */
         *fno_stdin = pipe_in[1];
         log.Debug("popen: parent writes child's stdin to %d", pipe_in[1]);
      }
   }
   if (fno_stdout) {
      if (*fno_stdout < 0) {
         /* Caller will be reading from child's stdout through pipe */
         *fno_stdout = pipe_out[0];
         log.Debug("popen: parent reads child's stdout from %d", pipe_out[0]);
      }
   }
   if (fno_stderr) {
      if (*fno_stderr < 0) {
         /* Caller will be reading from child's stderr through pipe */
         *fno_stderr = pipe_err[0];
         log.Debug("popen: parent reads child's stderr from %d", pipe_err[0]);
      }
   }
   //sleep(2);
   log.Debug("popen: parent returning pid=%d of child", pid);
   return pid;
}

#else
static int do_mypopen_raw(const char *cline, int *fno_stdin, int *fno_stdout, int *fno_stderr)
{
   int rc, pipe_in[2], pipe_out[2], pipe_err[2];
   int save_in = -1, save_out = -1, save_err = -1;
   int child_in = -1, child_out = -1, child_err = -1;
   int n, pid = -1, argc = 50;
   char *argv[50], argbuf[4096];

   /* Sanity check parameters */
   if (!cline || !cline[0]) {
      errno = EINVAL;
      return -1;
   }
   for (n = 0; n < 2; n++)
   {
      pipe_in[n] = -1;
      pipe_out[n] = -1;
      pipe_err[n] = -1;
   }

   /* Build argv array from command line string */
   if (mypopen_args(cline, argv, argc, argbuf, sizeof(argbuf))) {
      /* Invalid args, so terminate child */
      log.Debug("popen: invalid cmdline args for child");
      errno = EINVAL;
      return -1;
   }

   /* Determine child's stdin */
   if (fno_stdin) {
      if (*fno_stdin < 0) {
         /* Caller requests a pipe for writing to child's stdin */
         rc = _pipe(pipe_in, 4096, O_NOINHERIT);
         if (rc) {
            /* error creating pipe */
            return -1;
         }
         child_in = pipe_in[0];
         log.Debug("popen: child stdin uses pipe (%d -> %d)", pipe_in[0], pipe_in[1]);
      } else {
         if (*fno_stdin != STDIN_FILENO) {
            /* Caller supplied an open file to use as child's stdin */
            child_in = *fno_stdin;
         }
      }
   }

   /* Determine child's stdout */
   if (fno_stdout) {
      if (*fno_stdout < 0) {
         /* Caller requests a pipe for reading from child's stdout */
         rc = _pipe(pipe_out, 4096, O_NOINHERIT);
         if (rc) {
            /* error creating pipe */
            rc = errno;
            CloseAllPipes(pipe_in, pipe_out, pipe_err);
            errno = rc;
            return -1;
         }
         child_out = pipe_out[1];
         log.Debug("popen: child stdout uses pipe (%d -> %d)", pipe_out[0], pipe_out[1]);
      } else {
         if (*fno_stdout != STDOUT_FILENO) {
            /* Caller supplied open file to use as child's stdout */
            child_out = *fno_stdout;
         }
      }
   }

   /* Setup child's stderr */
   if (fno_stderr) {
      if (*fno_stderr < 0) {
         /* Caller requests a pipe for reading from child's stderr */
         rc = _pipe(pipe_err, 5096, O_NOINHERIT);
         if (rc) {
            /* error creating pipe */
            rc = errno;
            CloseAllPipes(pipe_in, pipe_out, pipe_err);
            errno = rc;
            return -1;
         }
         child_err = pipe_err[1];
         log.Debug("popen: child stderr uses pipe (%d -> %d)", pipe_err[0], pipe_err[1]);
      } else {
         if (*fno_stderr != STDERR_FILENO) {
            /* Caller supplied an open file to use as child's stderr */
            child_err = *fno_stderr;
         }
      }
   }

   /* Save original stdin and make stdin a duplicate of file child is to use */
   if (child_in >= 0) {
      save_in = _dup(STDIN_FILENO);
      if (save_in < 0) {
         rc = errno;
         CloseAllPipes(pipe_in, pipe_out, pipe_err);
         errno = rc;
         return -1;
      }
      if(_dup2(child_in, STDIN_FILENO) != 0) {
         rc = errno;
         close(save_in);
         CloseAllPipes(pipe_in, pipe_out, pipe_err);
         errno = rc;
         return -1;
      }
   }

   /* Save original stdout and make stdout a duplicate of file child is to use */
   if (child_out >= 0) {
      save_out = _dup(STDOUT_FILENO);
      if (save_out < 0) {
         rc = errno;
         if (save_in >= 0) {
            _dup2(save_in, STDIN_FILENO);
            close(save_in);
            pipe_in[0] = -1;
         }
         CloseAllPipes(pipe_in, pipe_out, pipe_err);
         errno = rc;
         return -1;
      }
      if(_dup2(child_out, STDOUT_FILENO) != 0) {
         rc = errno;
         close(save_out);
         if (save_in >= 0) {
            _dup2(save_in, STDIN_FILENO);
            close(save_in);
            pipe_in[0] = -1;
         }
         CloseAllPipes(pipe_in, pipe_out, pipe_err);
         errno = rc;
         return -1;
      }
   }

   /* Save original stderr and make stderr a duplicate of file child is to use */
   if (child_err >= 0) {
      save_err = _dup(STDERR_FILENO);
      if (save_err < 0) {
         rc = errno;
         if (save_in >= 0) {
            _dup2(save_in, STDIN_FILENO);
            close(save_in);
            pipe_in[0] = -1;
         }
         if (save_out >= 0) {
            _dup2(save_out, STDOUT_FILENO);
            close(save_out);
            pipe_out[1] = -1;
         }
         CloseAllPipes(pipe_in, pipe_out, pipe_err);
         errno = rc;
         return -1;
      }
      if(_dup2(child_err, STDERR_FILENO) != 0) {
         rc = errno;
         close(save_out);
         if (save_in >= 0) {
            _dup2(save_in, STDIN_FILENO);
            close(save_in);
            pipe_in[0] = -1;
         }
         if (save_out >= 0) {
            _dup2(save_out, STDOUT_FILENO);
            close(save_out);
            pipe_out[1] = -1;
         }
        CloseAllPipes(pipe_in, pipe_out, pipe_err);
         errno = rc;
         return -1;
      }
   }

   /* Spawn the child process */
   rc = 0;
   pid = _spawnvp(_P_NOWAIT, argv[0], argv);
   if (pid < 0) {
      /* spawn failed */
      rc = errno;
   }

   /* Restore parent's stdin, stdout, and stderr */
   if (save_in >= 0) {
      _dup2(save_in, STDIN_FILENO);
      close(save_in);
      pipe_in[0] = -1;
   }
   if (save_out >= 0) {
      _dup2(save_out, STDOUT_FILENO);
      close(save_out);
      pipe_out[1] = -1;
   }
   if (save_err >= 0) {
      _dup2(save_err, STDERR_FILENO);
      close(save_err);
      pipe_err[1] = -1;
   }

   if (rc) {
      CloseAllPipes(pipe_in, pipe_out, pipe_err);
      return -1;
   }

   /* Pass requested pipe ends back to caller */
   if (pipe_in[1] >= 0) *fno_stdin = pipe_in[1];
   if (pipe_out[0] >= 0) *fno_stdout = pipe_out[0];
   if (pipe_err[0] >= 0) *fno_stderr = pipe_err[0];

   log.Debug("popen: parent returning pid=%d of child", pid);
   return pid;
}

#endif

/*
 *  Function to fork a child process, specifying the command and arguments to be
 *  run in the child and the child's standard i/o files. If fno_stdXXX is NULL,
 *  then the child will inherit the parent's stdXXX. If *fno_stdXXX is -1, then
 *  a pipe will be created with the child's stdXXX being one end of the pipe and
 *  the other end of the pipe being passed back to the caller in *fno_stdXXX. If
 *  *fno_stdXXX zero or greater, then it specifies a file descriptor that will be
 *  used as stdXXX in the child.
 *  On success, returns the pid of the child. On error, returns -1 and sets errno.
 */
int mypopen_raw(const char *command, int *fno_stdin, int *fno_stdout, int *fno_stderr)
{
   return do_mypopen_raw(command, fno_stdin, fno_stdout, fno_stderr);
}


/*
 *  Function to fork a child process, specifying the command to be run in the child
 *  and acquiring open file streams to the child's stdXXX files. If any of fs_XXX are
 *  NULL, then the child will inherit the parent's corresponding stdXXX. If fs_XXX points
 *  to an open file stream, then the child will use that file as its stdXXX. If fs_XXX
 *  points to NULL, then on return *fs_XXX will be an open file connected by a pipe to
 *  the child's corresponding stdXXX.
 *  On success, returns the pid of the child process. On error, returns -1 and
 *  sets errno appropriately.
 */
int mypopen(const char *cmd, FILE **fs_in, FILE **fs_out, FILE **fs_err)
{
   int pid, fno_in = -1, fno_out = -1, fno_err = -1;
   int *in_p = NULL, *out_p = NULL, *err_p = NULL;
   if (fs_in) {
      in_p = &fno_in;
      if (*fs_in) {
         fno_in = fileno(*fs_in);
         if (fno_in == STDIN_FILENO) {
            in_p = NULL;
            fs_in = NULL;
         }
      }
   }
   if (fs_out) {
      out_p = &fno_out;
      if (*fs_out) {
         fno_out = fileno(*fs_out);
         if (fno_out == STDOUT_FILENO) {
            out_p = NULL;
            fs_out = NULL;
         }
      }
   }
   if (fs_err) {
      err_p = &fno_err;
      if (*fs_err) {
         fno_err = fileno(*fs_err);
         if (fno_err == STDERR_FILENO) {
            err_p = NULL;
            fs_err = NULL;
         }
      }
   }
   pid = do_mypopen_raw(cmd, in_p, out_p, err_p);
   if (pid < 0) return -1;
   if (fs_in && *fs_in == NULL) *fs_in = fdopen(fno_in, "w");
   if (fs_out && *fs_out == NULL) *fs_out = fdopen(fno_out, "r");
   if (fs_err && *fs_err == NULL) *fs_err = fdopen(fno_err, "r");
   return pid;
}


/*
 *  Function to fork a child process, specifying the command to be run in the child
 *  and files the child will use for stdXXX. If cmd_XXX is non-null, then it specifies
 *  the name of a file that the child is to use as its stdXXX. If the filename is empty
 *  then "/dev/null" is used. If cmd_XXX is NULL, then the child will use the caller's stdXXX.
 *  On success, returns the result code from the command. On error, returns -1 and
 *  sets errno appropriately.
 */
int mypopenrw(const char *command, const char *cmd_in, const char *cmd_out, const char *cmd_err)
{
   int pid, rc, st;
   int fno_in = -1, fno_out = -1, fno_err = -1;
   int *fno_in_p = NULL, *fno_out_p = NULL, *fno_err_p = NULL;

   /* Execute command in a child process, setting child's stdin, stdout, and stderr */
   if (cmd_in) {
      if (cmd_in[0]) fno_in = open(cmd_in, O_RDONLY);
      else fno_in = open("/dev/null", O_RDONLY);
      if (fno_in < 0) return -1;
      fno_in_p = &fno_in;
   }
   if (cmd_out) {
      if (cmd_out[0]) {
#ifndef HAVE_WINDOWS_H
         fno_out = open(cmd_out, O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP);
#else
         fno_out = _open(cmd_out, O_WRONLY|O_CREAT, _S_IWRITE);
#endif
      } else fno_out = open("/dev/null", O_WRONLY);
      if (fno_out < 0) {
         rc = errno;
         if (fno_in >= 0) close(fno_in);
         errno = rc;
         return -1;
      }
      fno_out_p = &fno_out;
   }
   if (cmd_err) {
      if (cmd_err[0]) {
#ifndef HAVE_WINDOWS_H
         fno_err = open(cmd_err, O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP);
#else
         fno_err = _open(cmd_err, O_WRONLY|O_CREAT, _S_IWRITE);
#endif
      } else fno_err = open("/dev/null", O_WRONLY);
      if (fno_err < 0) {
         rc = errno;
         if (fno_in >= 0) close(fno_in);
         if (fno_out >= 0) close(fno_out);
         errno = rc;
         return -1;
      }
      fno_err_p = &fno_err;
   }
   pid = do_mypopen_raw(command, fno_in_p, fno_out_p, fno_err_p);
   if (pid < 0) {
      /* Command execution failed */
      if (fno_in >= 0) close(fno_in);
      if (fno_out >= 0) close(fno_out);
      if (fno_err >= 0) close(fno_err);
      return -1;
   }
   /* Wait for child process to exit */
#ifndef HAVE_WINDOWS_H
   rc = waitpid(pid, &st, 0);
   while (rc < 0) {
      if (errno != EINTR) {
         if (fno_in >= 0) close(fno_in);
         if (fno_out >= 0) close(fno_out);
         if (fno_err >= 0) close(fno_err);
         return -1;
      }
      rc = waitpid(pid, &st, 0);
   }
   /* get child's result code */
   if (!WIFEXITED(st)) {
      /* Command was killed for some reason */
      return -1;
   }
   rc = WEXITSTATUS(st);

#else
   if (_cwait(&rc, pid, WAIT_CHILD) != pid) {
      if (fno_in >= 0) close(fno_in);
      if (fno_out >= 0) close(fno_out);
      if (fno_err >= 0) close(fno_err);
      return -1;
   }
#endif

   if (fno_in >= 0) {
      close(fno_in);
   }
   if (fno_out >= 0) {
      close(fno_out);
   }
   if (fno_err >= 0) {
      close(fno_err);
   }
   return rc;
}

