all_include_dirs = -g\
    -I$(top_srcdir)/src/lib
bin_PROGRAMS = \
	fq_handler_svr_json

if HAVE_SOCKET
all_libraries = -lm -ldl -lnsl -lrt -lpthread -lsocket -lm
else
all_libraries = -lm -ldl -lrt -lpthread -lm
endif

fq_handler_svr_json_SOURCES = fq_handler_svr_json.c fq_handler_conf.c fq_handler_conf.h
fq_handler_svr_json_CFLAGS = $(all_include_dirs)
fq_handler_svr_json_LDADD = $(all_libraries) ./../../lib/libfq.la

EXTRA_INST = fq_handler_svr_json.conf run.sh use_filesystem.list
EXTRA_DIST = fq_handler_svr_json.conf run.sh use_filesystem.list

noinst_PROGRAMS =
