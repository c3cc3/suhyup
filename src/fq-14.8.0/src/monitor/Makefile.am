all_include_dirs = -g\
	-I$(top_srcdir)/src/lib
bin_PROGRAMS = \
		monitor
# 
if HAVE_SOCKET
all_libraries = -lm -ldl -lnsl -lrt -lpthread -lsocket -lm -lcurses
else
all_libraries = -lm -ldl -lrt -lpthread -lm -lcurses
endif

#
monitor_SOURCES = monitor.c screen.c fuser_popen.c screen.h fuser_popen.h monitor.h
monitor_CFLAGS = $(all_include_dirs)
monitor_LDADD = $(all_libraries) ./../lib/libfq.la
#
EXTRA_DIST = gdb.manual
noinst_PROGRAMS = 
