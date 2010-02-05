#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.63])
AC_INIT([vchanger], [0.8.5], [jfisher@jaybus.com])
AC_ARG_ENABLE(win32,[  --enable-win32     Build for win32],
 [case "${enableval}" in
  yes) win32=true ;;
  no) win32=false ;;
  *) AC_MSG_ERROR(bad value ${enableval} for --enable-win32) ;;
  esac],[win32=false])
AM_CONDITIONAL(BUILDWIN32, test x$win32 = xtrue)
AC_CONFIG_SRCDIR([src])
AM_INIT_AUTOMAKE
AM_MAINTAINER_MODE
AM_CONFIG_HEADER([config.h])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AM_PROG_LIBTOOL
AC_PROG_INSTALL

# Checks for libraries.

# Checks for header files.
AC_HEADER_STDC
AC_HEADER_STDBOOL
AC_CHECK_HEADERS([time.h sys/time.h sys/wait.h dirent.h fcntl.h limits.h locale.h syslog.h])
AC_CHECK_HEADERS([strings.h alloca.h sys/bitypes.h getopt.h utime.h sys/stat.h])
AC_CHECK_HEADERS([uuid/uuid.h blkid/blkid.h mntent.h unistd.h windows.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_TOOL([STRIP],[strip])
AC_C_CONST
AC_C_INLINE
AC_TYPE_PID_T
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([ctime_r getmntent_r getline gettimeofday memmove memset strcasecmp setlocale strtol strtoul])

AC_CONFIG_FILES([Makefile
				src/Makefile])

AC_OUTPUT
