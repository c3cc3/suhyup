all_include_dirs = -g\
    -I$(top_srcdir)/src/lib
bin_PROGRAMS = \
	fq_agent

if HAVE_SOCKET
all_libraries = -lm -ldl -lnsl -lrt -lpthread -lsocket -lm
else
all_libraries = -lm -ldl -lrt -lpthread -lm
endif

fq_agent_SOURCES = fq_agent.c 
fq_agent_CFLAGS = $(all_include_dirs)
fq_agent_LDADD = $(all_libraries) ./../../lib/libfq.la

EXTRA_INST = fq_agent.sh fq_agent.conf fq_threads.map
EXTRA_DIST = fq_agent.sh fq_agent.conf fq_threads.map

noinst_PROGRAMS =
