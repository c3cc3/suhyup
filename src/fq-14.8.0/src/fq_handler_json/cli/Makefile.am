all_include_dirs = -g\
    -I$(top_srcdir)/src/lib
bin_PROGRAMS = \
	fq_handler_cli_json

if HAVE_SOCKET
all_libraries = -lm -ldl -lnsl -lrt -lpthread -lsocket -lm
else
all_libraries = -lm -ldl -lrt -lpthread -lm
endif

fq_handler_cli_json_SOURCES = fq_handler_cli_json.c 
fq_handler_cli_json_CFLAGS = $(all_include_dirs)
fq_handler_cli_json_LDADD = $(all_libraries) ./../../lib/libfq.la

EXTRA_INST = test.dat run_get.sh run_put.sh
EXTRA_DIST = test.dat run_get.sh run_put.sh

noinst_PROGRAMS =


