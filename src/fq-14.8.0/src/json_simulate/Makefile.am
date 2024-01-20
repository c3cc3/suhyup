all_include_dirs = -g\
    -I$(top_srcdir)/src/lib
bin_PROGRAMS = \
	json_maker

if HAVE_SOCKET
all_libraries = -lm -ldl -lnsl -lrt -lpthread -lsocket -lm
else
all_libraries = -lm -ldl -lrt -lpthread -lm
endif

json_maker_SOURCES = json_maker.c 
json_maker_CFLAGS = $(all_include_dirs)
json_maker_LDADD = $(all_libraries) ./../lib/libfq.la

EXTRA_INST = json_make.rule
EXTRA_DIST = json_make.rule

noinst_PROGRAMS =
