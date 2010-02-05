AUTOMAKE_OPTIONS = foreign
bin_PROGRAMS = vchanger
vchanger_SOURCES = vchanger_common.h vchanger_common.cpp \
					compat_util.h compat_util.c \
					junctions.h junctions.c uuidlookup.c \
					util.h util.cpp \
					vconf.h vconf.cpp \
					loghandler.h loghandler.cpp \
					diskchanger.h diskchanger.cpp \
					changerstate.h changerstate.cpp \
					vchanger.cpp
if BUILDWIN32
vchanger_LDADD = -lpsapi
vchanger_LDFLAGS = -static-libgcc
else
vchanger_CPPFLAGS = -I/usr/local/include
vchanger_LDADD = -L/usr/local/lib -luuid -lblkid
endif
