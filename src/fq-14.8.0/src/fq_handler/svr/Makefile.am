all_include_dirs = -g\
    -I$(top_srcdir)/src/lib
bin_PROGRAMS = \
	fq_handler_svr

if HAVE_SOCKET
all_libraries = -lm -ldl -lnsl -lrt -lpthread -lsocket -lm
else
all_libraries = -lm -ldl -lrt -lpthread -lm
endif

fq_handler_svr_SOURCES = fq_handler_svr.c fq_handler_conf.c fq_handler_conf.h
fq_handler_svr_CFLAGS = $(all_include_dirs)
fq_handler_svr_LDADD = $(all_libraries) ./../../lib/libfq.la

EXTRA_INST = fq_handler_svr.conf run.sh
EXTRA_DIST = fq_handler_svr.conf run.sh

noinst_PROGRAMS =
