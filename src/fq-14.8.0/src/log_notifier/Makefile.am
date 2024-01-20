all_include_dirs = -g\
    -I$(top_srcdir)/src/lib
bin_PROGRAMS = \
	log_notifier

if HAVE_SOCKET
all_libraries = -lm -ldl -lnsl -lrt -lpthread -lsocket -lm
else
all_libraries = -lm -ldl -lrt -lpthread -lm
endif

log_notifier_SOURCES = LogNotifier.c 
log_notifier_CFLAGS = $(all_include_dirs)
log_notifier_LDADD = $(all_libraries) ./../lib/libfq.la

EXTRA_INST = log_files.list 
EXTRA_DIST = log_files.list

noinst_PROGRAMS =


