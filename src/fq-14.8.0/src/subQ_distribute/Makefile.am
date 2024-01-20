all_include_dirs = -g\
    -I$(top_srcdir)/src/lib
bin_PROGRAMS = \
	subQ_distribute

if HAVE_SOCKET
all_libraries = -lm -ldl -lnsl -lrt -lpthread -lsocket -lm
else
all_libraries = -lm -ldl -lrt -lpthread -lm
endif

subQ_distribute_SOURCES = subQ_dist_main.c subQ_dist_conf.c queue_ctl_lib.c ums_common_conf.c carrier_msg_lib.c subQ_dist_conf.h ums_common_conf.h queue_ctl_lib.h carrier_msg_lib.h
subQ_distribute_CFLAGS = $(all_include_dirs)
subQ_distribute_LDADD = $(all_libraries) ./../lib/libfq.la

EXTRA_INST = subQ_dist.conf sub_queue.list ums_common.conf KT.qformat
EXTRA_DIST = subQ_dist.conf sub_queue.list ums_common.conf KT.qformat

noinst_PROGRAMS =
