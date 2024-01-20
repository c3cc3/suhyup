all_include_dirs = -g\
    -I$(top_srcdir)/src/lib
bin_PROGRAMS = \
	fq_handler_cli

if HAVE_SOCKET
all_libraries = -lm -ldl -lnsl -lrt -lpthread -lsocket -lm
else
all_libraries = -lm -ldl -lrt -lpthread -lm
endif

fq_handler_cli_SOURCES = fq_handler_cli.c 
fq_handler_cli_CFLAGS = $(all_include_dirs)
fq_handler_cli_LDADD = $(all_libraries) ./../../lib/libfq.la

EXTRA_INST = test.dat run_get.sh run_put.sh
EXTRA_DIST = test.dat run_get.sh run_put.sh

noinst_PROGRAMS =


