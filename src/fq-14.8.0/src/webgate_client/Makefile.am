all_include_dirs = -g\
    -I$(top_srcdir)/src/lib
bin_PROGRAMS = \
	webgate_client

if HAVE_SOCKET
all_libraries = -lm -ldl -lnsl -lrt -lpthread -lsocket -lm
else
all_libraries = -lm -ldl -lrt -lpthread -lm
endif

webgate_client_SOURCES = webgate_client.c 
webgate_client_CFLAGS = $(all_include_dirs)
webgate_client_LDADD = $(all_libraries) ./../lib/libfq.la

EXTRA_INST = run.sh
EXTRA_DIST = run.sh

noinst_PROGRAMS =
