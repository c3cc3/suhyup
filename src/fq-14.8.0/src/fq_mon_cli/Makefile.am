all_include_dirs = -g\
    -I$(top_srcdir)/src/lib
bin_PROGRAMS = \
	fqmon

if HAVE_SOCKET
all_libraries = -lm -ldl -lnsl -lrt -lpthread -lsocket -lm -lcurses
else
all_libraries = -lm -ldl -lrt -lpthread -lm -lcurses
endif

fqmon_SOURCES = fqmon.c screen.c fuser_popen.c monitor.h screen.h fuser_popen.h
fqmon_CFLAGS = $(all_include_dirs)
fqmon_LDADD = $(all_libraries) ./../lib/libfq.la

EXTRA_INST = 
EXTRA_DIST = 

noinst_PROGRAMS =
