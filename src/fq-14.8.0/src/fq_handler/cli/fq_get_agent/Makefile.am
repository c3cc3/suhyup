all_include_dirs = -g\
    -I$(top_srcdir)/src/lib
bin_PROGRAMS = \
	fq_get_agent

if HAVE_SOCKET
all_libraries = -lm -ldl -lnsl -lrt -lpthread -lsocket -lm
else
all_libraries = -lm -ldl -lrt -lpthread -lm
endif

fq_get_agent_SOURCES = main.c my_conf.c my_conf.h
fq_get_agent_CFLAGS = $(all_include_dirs)
fq_get_agent_LDADD = $(all_libraries) ./../../../lib/libfq.la

EXTRA_INST = my.conf
EXTRA_DIST = my.conf

noinst_PROGRAMS =


