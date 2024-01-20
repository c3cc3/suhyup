all_include_dirs = -g\
    -I$(top_srcdir)/src/lib
bin_PROGRAMS = \
	fq_all_queue_test

if HAVE_SOCKET
all_libraries = -lm -ldl -lnsl -lrt -lpthread -lsocket -lm -lcurses
else
all_libraries = -lm -ldl -lrt -lpthread -lm -lcurses
endif

fq_all_queue_test_SOURCES = fq_all_queue_test.c ums_common_conf.c ums_common_conf.h my_conf.c my_conf.h
fq_all_queue_test_CFLAGS = $(all_include_dirs)
fq_all_queue_test_LDADD = $(all_libraries) ./../lib/libfq.la

EXTRA_INST = my.conf
EXTRA_DIST = my.conf

noinst_PROGRAMS =
