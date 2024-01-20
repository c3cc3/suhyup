all_include_dirs = -g\
    -I$(top_srcdir)/src/lib
bin_PROGRAMS = \
	fq_callback_svr

if HAVE_SOCKET
all_libraries = -lm -ldl -lnsl -lrt -lpthread -lsocket -lm
else
all_libraries = -lm -ldl -lrt -lpthread -lm
endif

fq_callback_svr_SOURCES = fq_callback_svr.c my_conf.c my_conf.h
fq_callback_svr_CFLAGS = $(all_include_dirs)
fq_callback_svr_LDADD = $(all_libraries) ./../lib/libfq.la

EXTRA_INST = my.conf Makefile.std Make.defines.std
EXTRA_DIST = my.conf Makefile.std Make.defines.std

noinst_PROGRAMS =


