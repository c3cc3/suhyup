all_include_dirs = -g\
	-I$(top_srcdir)/src/lib
bin_PROGRAMS = \
		shm_monitor
# 
if HAVE_SOCKET
all_libraries = -lm -ldl -lnsl -lrt -lpthread -lsocket -lm -lcurses
else
all_libraries = -lm -ldl -lrt -lpthread -lm -lcurses
endif

#
shm_monitor_SOURCES = shm_monitor.c screen.c fuser_popen.c screen.h fuser_popen.h
shm_monitor_CFLAGS = $(all_include_dirs)
shm_monitor_LDADD = $(all_libraries) ./../lib/libfq.la
#
EXTRA_DIST = gdb.manual en.sh de.sh create.sh TST.Q.info ListSHMQ.info
noinst_PROGRAMS = 
