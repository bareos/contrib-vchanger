#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.63])
AC_INIT([vchanger], [1.0.0], [jfisher@jaybus.com])
AC_DEFINE([COPYRIGHT_NOTICE],["AC_PACKAGE_NAME Copyright (c) 2006-2015 Josh Fisher"],[Copyright notice])
AC_CONFIG_SRCDIR([src])
AC_CONFIG_LIBOBJ_DIR([src/compat])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE

# Checks for programs.
AC_PROG_CC
AC_PROG_CXX
AC_PROG_INSTALL
AC_CHECK_TOOL([STRIP],[strip], [])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_C_TYPEOF
AC_HEADER_STDBOOL
AC_TYPE_PID_T
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UID_T
AC_TYPE_INT32_T
AC_TYPE_UINT32_T
AC_TYPE_INT64_T
AC_TYPE_UINT64_T
AC_TYPE_MODE_T
AC_SYS_LARGEFILE

# Add needed Windows libraries to LIBS
# LDFLAGS="$LDFLAGS -L/usr/i686-pc-mingw32/sys-root/mingw/lib"
#AC_SEARCH_LIBS([CloseHandle],[kernel32],)
#AC_SEARCH_LIBS([OpenProcessToken],[advapi],)
#AC_SEARCH_LIBS([SHGetFolderPath],[shell32],)

# Use multithreading
AC_SEARCH_LIBS([pthread_create], [pthread], [], [AC_MSG_ERROR(["could not find libpthread"])])
# Support using libudev or libblkid for UUID lookup
AC_SEARCH_LIBS([udev_new],[udev],[AC_CHECK_HEADERS([libudev.h])])
AC_CHECK_HEADER([blkid/blkid.h],
  [ AC_SEARCH_LIBS([blkid_evaluate_tag],[blkid],
      [AC_DEFINE([HAVE_BLKID_EVALUATE_TAG],,[Have blkid_evaluate_tag function])],
      [ AC_SEARCH_LIBS([blkid_get_devname],[blkid],
          [AC_DEFINE([HAVE_BLKID_GET_DEVNAME],,[Have blkid_get_devname function])])
      ])
  ])

# Checks for header files.
AC_HEADER_DIRENT
AC_CHECK_HEADERS_ONCE([stdio.h stdlib.h string.h stdarg.h stddef.h])
AC_CHECK_HEADERS_ONCE([time.h sys/time.h sys/timespec.h sys/wait.h limits.h locale.h syslog.h])
AC_CHECK_HEADERS_ONCE([sys/types.h strings.h alloca.h sys/bitypes.h getopt.h utime.h sys/stat.h])
AC_CHECK_HEADERS_ONCE([inttypes.h ctype.h errno.h unistd.h varargs.h mntent.h])
AC_CHECK_HEADERS_ONCE([sys/param.h sys/mount.h sys/ucred.h grp.h pwd.h dirent.h fcntl.h])
AC_CHECK_HEADERS_ONCE([sys/select.h optarg.h pthread.h libgen.h io.h])
AC_CHECK_HEADER([windows.h], [AC_DEFINE([HAVE_WINDOWS_H],,[have header windows.h])])
AC_CHECK_HEADER([winioctl.h], [AC_DEFINE([HAVE_WINIOCTL_H],,[have header winioctl.h])], [], [#include <windows.h>])
AC_CHECK_HEADER([winsock.h], [AC_DEFINE([HAVE_WINSOCK_H],,[have header winsock.h])], [], [#include <windows.h>])
AC_CHECK_HEADER([winreg.h], [AC_DEFINE([HAVE_WINREG_H],,[have header winreg.h])], [], [#include <windows.h>])
AC_CHECK_HEADER([shlobj.h], [AC_DEFINE([HAVE_SHLOBJ_H],,[have header shlobj.h])], [], [#include <windows.h>])
# Checks for functions.
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([setlocale getmntent getmntent_r getfsstat])

AC_REPLACE_FUNCS([getline gettimeofday getuid localtime_r pipe readlink sleep symlink syslog])

AC_CONFIG_FILES([Makefile src/Makefile doc/Makefile scripts/Makefile])

AC_OUTPUT
