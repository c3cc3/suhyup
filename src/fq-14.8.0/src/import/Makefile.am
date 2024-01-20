all_include_dirs = -g\
    -I$(top_srcdir)/src/lib
bin_PROGRAMS = \
	mv_file_to_fq

if HAVE_SOCKET
all_libraries = -lm -ldl -lnsl -lrt -lpthread -lsocket -lm
else
all_libraries = -lm -ldl -lrt -lpthread -lm
endif

mv_file_to_fq_SOURCES = mv_file_to_fq.c 
mv_file_to_fq_CFLAGS = $(all_include_dirs)
mv_file_to_fq_LDADD = $(all_libraries) ./../lib/libfq.la

EXTRA_INST = mv_file_to_fq.conf mv_file_to_fq.c.single mv_file_b_to_fq_multi.c.multi
EXTRA_DIST = mv_file_to_fq.conf mv_file_to_fq.c.single mv_file_b_to_fq_multi.c.multi

noinst_PROGRAMS =


