all_include_dirs = -g\
    -I$(top_srcdir)/src/lib
bin_PROGRAMS = \
	fq_alarm_client

if HAVE_SOCKET
all_libraries = -lm -ldl -lnsl -lrt -lpthread -lsocket -lm
else
all_libraries = -lm -ldl -lrt -lpthread -lm
endif

fq_alarm_client_SOURCES = fq_alarm_client.c fq_alarm_client_conf.c ums_common_conf.c fq_alarm_client_conf.h ums_common_conf.h queue_ctl_lib.c queue_ctl_lib.h 
fq_alarm_client_CFLAGS = $(all_include_dirs)
fq_alarm_client_LDADD = $(all_libraries) ./../lib/libfq.la

EXTRA_INST = ALARM_MSG.json
EXTRA_DIST = ALARM_MSG.json

noinst_PROGRAMS =
