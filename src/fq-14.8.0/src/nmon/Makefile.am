all_include_dirs = -g\
	-I$(top_srcdir)/src/lib
bin_PROGRAMS = \
		nmon
# 
if HAVE_SOCKET
all_libraries = -lm -ldl -lnsl -lrt -lpthread -lsocket -lm -lcurses
else
all_libraries = -lm -ldl -lrt -lpthread -lm -lcurses
endif

#
nmon_SOURCES = nmon.c
nmon_CFLAGS = $(all_include_dirs)
nmon_LDADD = $(all_libraries) ./../lib/libfq.la
#
EXTRA_DIST = 
noinst_PROGRAMS = 
