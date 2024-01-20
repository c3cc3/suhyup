all_include_dirs = -g\
	-I$(top_srcdir)/src/lib
bin_PROGRAMS = \
		wmonitor
# 
if HAVE_SOCKET
all_libraries = -lm -ldl -lnsl -lrt -lpthread -lsocket -lm -lcurses
else
all_libraries = -lm -ldl -lrt -lpthread -lm -lcurses
endif

#
wmonitor_SOURCES = wmonitor.c screen.c fuser_popen.c screen.h fuser_popen.h wmonitor.h
wmonitor_CFLAGS = $(all_include_dirs)
wmonitor_LDADD = $(all_libraries) ./../lib/libfq.la
#
EXTRA_DIST = 
noinst_PROGRAMS = 
