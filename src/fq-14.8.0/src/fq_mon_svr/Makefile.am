all_include_dirs = -g\
    -I$(top_srcdir)/src/lib
bin_PROGRAMS = \
	fq_mon_svr

if HAVE_SOCKET
all_libraries = -lm -ldl -lnsl -lrt -lpthread -lsocket -lm
else
all_libraries = -lm -ldl -lrt -lpthread -lm
endif

fq_mon_svr_SOURCES = fq_mon_svr.c fq_mon_svr_conf.c fuser_popen.c ums_common_conf.c fq_mon_svr_conf.h fuser_popen.h monitor.h ums_common_conf.h
fq_mon_svr_CFLAGS = $(all_include_dirs)
fq_mon_svr_LDADD = $(all_libraries) ./../lib/libfq.la

EXTRA_INST = exclude.list fq_mon_svr.conf ALARM_MSG.json admin_phone.list
EXTRA_DIST = exclude.list fq_mon_svr.conf ALARM_MSG.json admin_phone.list

noinst_PROGRAMS =


