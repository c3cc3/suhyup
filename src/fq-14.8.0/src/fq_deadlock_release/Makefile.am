all_include_dirs = -g\
    -I$(top_srcdir)/src/lib
bin_PROGRAMS = \
	fq_deadlock_release

if HAVE_SOCKET
all_libraries = -lm -ldl -lnsl -lrt -lpthread -lsocket -lm
else
all_libraries = -lm -ldl -lrt -lpthread -lm
endif

fq_deadlock_release_SOURCES = fq_deadlock_release.c 
fq_deadlock_release_CFLAGS = $(all_include_dirs)
fq_deadlock_release_LDADD = $(all_libraries) ./../lib/libfq.la

EXTRA_INST = ALARM_MSG.json
EXTRA_DIST = ALARM_MSG.json

noinst_PROGRAMS =
