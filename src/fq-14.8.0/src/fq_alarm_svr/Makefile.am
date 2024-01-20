all_include_dirs = -g\
    -I$(top_srcdir)/src/lib
bin_PROGRAMS = \
	fq_alarm_svr

if HAVE_SOCKET
all_libraries = -lm -ldl -lnsl -lrt -lpthread -lsocket -lm
else
all_libraries = -lm -ldl -lrt -lpthread -lm
endif

fq_alarm_svr_SOURCES = fq_alarm_svr.c fq_alarm_svr_conf.c ums_common_conf.c fq_alarm_svr_conf.h ums_common_conf.h queue_ctl_lib.c queue_ctl_lib.h 
fq_alarm_svr_CFLAGS = $(all_include_dirs)
fq_alarm_svr_LDADD = $(all_libraries) ./../lib/libfq.la

EXTRA_INST = SS.json admin_phone.map channel_co_queue.map
EXTRA_DIST = SS.json admin_phone.map channel_co_queue.map

noinst_PROGRAMS =
