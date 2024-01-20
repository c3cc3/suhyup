all_include_dirs = -g\
    -I$(top_srcdir)/src/lib
bin_PROGRAMS = \
	fq_get_agent

if HAVE_SOCKET
all_libraries = -lm -ldl -lnsl -lrt -lpthread -lsocket -lm
else
all_libraries = -lm -ldl -lrt -lpthread -lm
endif

fq_get_agent_SOURCES = fq_get_agent.c 
fq_get_agent_CFLAGS = $(all_include_dirs)
fq_get_agent_LDADD = $(all_libraries) ./../../lib/libfq.la

EXTRA_INST = fq_get_agent.sh fq_get_agent.conf
EXTRA_DIST = fq_get_agent.sh fq_get_agent.conf

noinst_PROGRAMS =


