all_include_dirs = -g\
	-I$(top_srcdir)/src/lib
bin_PROGRAMS = \
	DeleteAppInMonitor\
	GetAppStatus\
	deFQ_monitor_sample\
	fq_agent\
	fq_monitor_svr\
	init_heartbeat_table\
	view_heartbeat_table\
	add_heartbeat_table\
	del_heartbeat_table\
	status_heartbeat_table
#
if HAVE_SOCKET
all_libraries = -lm -ldl -lnsl -lrt -lpthread -lsocket -lm
else
all_libraries = -lm -ldl -lrt -lpthread -lm
endif

#
DeleteAppInMonitor_SOURCES = DeleteAppInMonitor.c
DeleteAppInMonitor_CFLAGS = $(all_include_dirs)
DeleteAppInMonitor_LDADD = $(all_libraries) ./../lib/libfq.la
#
GetAppStatus_SOURCES = GetAppStatus.c
GetAppStatus_CFLAGS = $(all_include_dirs)
GetAppStatus_LDADD = $(all_libraries) ./../lib/libfq.la
#
deFQ_monitor_sample_SOURCES = deFQ_monitor_sample.c
deFQ_monitor_sample_CFLAGS = $(all_include_dirs)
deFQ_monitor_sample_LDADD = $(all_libraries) ./../lib/libfq.la
#
fq_agent_SOURCES = fq_agent.c
fq_agent_CFLAGS = $(all_include_dirs)
fq_agent_LDADD = $(all_libraries) ./../lib/libfq.la
#
fq_monitor_svr_SOURCES = fq_monitor_svr.c
fq_monitor_svr_CFLAGS = $(all_include_dirs)
fq_monitor_svr_LDADD = $(all_libraries) ./../lib/libfq.la
#
init_heartbeat_table_SOURCES = init_heartbeat_table.c
init_heartbeat_table_CFLAGS = $(all_include_dirs)
init_heartbeat_table_LDADD = $(all_libraries) ./../lib/libfq.la
#
view_heartbeat_table_SOURCES = view_heartbeat_table.c
view_heartbeat_table_CFLAGS = $(all_include_dirs)
view_heartbeat_table_LDADD = $(all_libraries) ./../lib/libfq.la
#
add_heartbeat_table_SOURCES = add_heartbeat_table.c
add_heartbeat_table_CFLAGS = $(all_include_dirs)
add_heartbeat_table_LDADD = $(all_libraries) ./../lib/libfq.la
#
del_heartbeat_table_SOURCES = del_heartbeat_table.c
del_heartbeat_table_CFLAGS = $(all_include_dirs)
del_heartbeat_table_LDADD = $(all_libraries) ./../lib/libfq.la
#
status_heartbeat_table_SOURCES = status_heartbeat_table.c
status_heartbeat_table_CFLAGS = $(all_include_dirs)
status_heartbeat_table_LDADD = $(all_libraries) ./../lib/libfq.la
#
EXTRA_DIST =  PS.qformat PS_each.qformat QU.aformat SU.qformat agent.conf fq_monitor_svr.conf
noinst_PROGRAMS = 
