AUTOMAKE_OPTIONS = foreign
AM_CFLAGS = -DLOCALSTATEDIR='"${localstatedir}"'
AM_CXXFLAGS = -DLOCALSTATEDIR='"${localstatedir}"'
AM_LDFLAGS = @WINLDADD@
bin_PROGRAMS = vchanger
vchanger_SOURCES = compat/getline.c compat/gettimeofday.c \
					compat/localtime_r.c \
					compat/readlink.c \
					compat/symlink.c compat/sleep.c compat/syslog.c \
					win32_util.c uuidlookup.c bconsole.cpp \
					tstring.cpp inifile.cpp mypopen.cpp \
					vconf.cpp loghandler.cpp errhandler.cpp \
					util.cpp changerstate.cpp diskchanger.cpp \
					vchanger.cpp
