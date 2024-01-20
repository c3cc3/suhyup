all_include_dirs = -g\
    -I$(top_srcdir)/src/lib
bin_PROGRAMS = \
	webgate

if HAVE_SOCKET
all_libraries = -lm -ldl -lnsl -lrt -lpthread -lsocket -lm
else
all_libraries = -lm -ldl -lrt -lpthread -lm
endif

webgate_SOURCES = webgate.c 
webgate_CFLAGS = $(all_include_dirs)
webgate_LDADD = $(all_libraries) ./../lib/libfq.la

EXTRA_INST = webgate.conf ERR_CODE.map ERR_REPORT.json.fmt webgate.sh fq_categories.map
EXTRA_DIST = webgate.conf ERR_CODE.map ERR_REPORT.json.fmt webgate.sh fq_categories.map

noinst_PROGRAMS =


