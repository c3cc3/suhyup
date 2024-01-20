all_include_dirs = -g\
    -I$(top_srcdir)/src/lib
bin_PROGRAMS = \
	fqpm_clear

if HAVE_SOCKET
all_libraries = -lm -ldl -lnsl -lrt -lpthread -lsocket -lm -lcurses
else
all_libraries = -lm -ldl -lrt -lpthread -lm -lcurses
endif

fqpm_clear_SOURCES = fqpm_clear.c 
fqpm_clear_CFLAGS = $(all_include_dirs)
fqpm_clear_LDADD = $(all_libraries) ./../../lib/libfq.la

EXTRA_INST = 
EXTRA_DIST = 

noinst_PROGRAMS =
