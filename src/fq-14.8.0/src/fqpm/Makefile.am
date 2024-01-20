all_include_dirs = -g\
    -I$(top_srcdir)/src/lib
bin_PROGRAMS = \
	fqpm

if HAVE_SOCKET
all_libraries = -lm -ldl -lnsl -lrt -lpthread -lsocket -lm -lcurses
else
all_libraries = -lm -ldl -lrt -lpthread -lm -lcurses
endif

fqpm_SOURCES = fqpm.c 
fqpm_CFLAGS = $(all_include_dirs)
fqpm_LDADD = $(all_libraries) ./../lib/libfq.la

EXTRA_INST = fqpm.conf
EXTRA_DIST = fqpm.conf

noinst_PROGRAMS =
