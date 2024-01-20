/*
** fq_replace.c
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#include "fq_mem.h"
#include "fq_tokenizer.h"
#include "fq_replace.h"
#include "fq_common.h"

int fq_replace( fq_logger_t *l, char *src, char *find_str,  char *replace_str,  char *dst, int dst_buf_len)
{
	fq_token_t tok;
	mem_obj_t *m=NULL; // source
	mem_obj_t *t=NULL; // target
	int idx=0;

	FQ_TRACE_ENTER(l);
	
	open_mem_obj( l, &m , dst_buf_len);
	open_mem_obj( l, &t , dst_buf_len);

	m->mseek( m, 0);
	m->mcopy( m, src, strlen(src));

	m->mseek( m, 0);
	while( !m->meof( m ) ) {
		char  c;
		c = m->mgetc( m );
		if( c == find_str[0] ) {
			/* Already first character was compared, so We compare string from next. */
			if( strncmp(&m->p[m->c] , &find_str[1], strlen(find_str)-1) == 0 ) {
				int i;
				for( i=0; i<strlen(find_str)-1; i++) {
					c = m->mgetc(m); /* source data flush */
				}
				t->mcopy(t, replace_str, strlen(replace_str)); /* copy replace char to target(t) */
			}
			else {
				t->mputc( t, c); /* move src to target */
			}
		}
		else {
			if( c == 0x00 ) break;
			t->mputc( t, c);
		}
	}

	t->mgets(t, dst);

	close_mem_obj(l, &m);
	close_mem_obj(l, &t);

	FQ_TRACE_EXIT(l);
	return(0);
}

#ifdef DT_DIR
static int my_select(const struct dirent *s_dirent)
{ /* filter */
    int g_my_select = DT_DIR;
    int g_my_select_mask = 0;

    if(s_dirent != ((struct dirent *)0)) {
        if((s_dirent->d_type & g_my_select) == g_my_select_mask)
            return(1);
    }
    return(0);
}
#endif

int fq_dir_replace( fq_logger_t *l, char *dirname, char *find_filename_sub_string, char *find_str, char *replace_str)
{
	int s_check, s_index;
	struct dirent **s_dirlist;

	FQ_TRACE_ENTER(l);
	s_check = scandir(dirname, (struct dirent ***)(&s_dirlist), my_select, alphasort);

	if(s_check >= 0 ) {
		for(s_index = 0;s_index < s_check;s_index++) {
			char fullname[512];
			char new_fullname[256];
			FILE *fp=NULL;
			struct stat fbuf;
			int rc;

			sprintf(fullname, "%s/%s", dirname, (char *)s_dirlist[s_index]->d_name);
			rc = fq_file_replace( l, fullname, find_str, replace_str);
			if( rc < 0 ) {
				fq_log(l, FQ_LOG_ERROR, "fq_file_replace('%s') failed.", fullname);
				FQ_TRACE_EXIT(l);
				return(-1);
			}
			free((void *)s_dirlist[s_index]);
		}

		if(s_dirlist != ((void *)0)) {
			free((void *)s_dirlist);
		}
	}
	
	FQ_TRACE_EXIT(l);

	return(0);
}
int fq_file_replace( fq_logger_t *l, char *path_file, char *find_str, char *replace_str)
{
	char new_fullname[256];
	char *buf=NULL;
	char *errmsg=NULL;
	int  flen;
	char *dst=NULL;
	int	 rc;
	FILE *fp=NULL;

	FQ_TRACE_ENTER(l);

	sprintf(new_fullname, "%s.new", path_file );

	fp = fopen(new_fullname, "w");
	if( fp == NULL ) {
		fq_log(l, FQ_LOG_ERROR, "fopen('%s') error.", new_fullname);
		FQ_TRACE_EXIT(l);
		return(-1);
	}

	if( load_a_file_2_buf(l, path_file, &buf, &flen, &errmsg) < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "load_a_file_2_buf('%s') error. reason=[%s].", path_file, errmsg);
	}

	dst = calloc(flen*2,  sizeof(char));

	rc = fq_replace( l, buf, find_str, replace_str, dst, (flen*2));
	if( rc < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "fq_replace() error. rc=[%d]\n", rc);
		SAFE_FREE(buf);
		SAFE_FREE(errmsg);
		if( fp ) fclose(fp);
		FQ_TRACE_EXIT(l);
		return(-1);
	}

	fprintf(fp, "%s", dst);
	fflush(fp);
	fclose(fp);

	SAFE_FREE(buf);
	SAFE_FREE(dst);

	unlink(path_file);
	rename(new_fullname, path_file);

	fq_log(l, FQ_LOG_DEBUG, "%s is replaced. [%s].", path_file);

	FQ_TRACE_EXIT(l);

	return(0);
}

/* Warning */
/* After using, free ptr_dst */
int load_a_file_2_buf(fq_logger_t *l, char *path_file, char**ptr_dst, int *flen, char** ptr_errmsg)
{
	int fdin, rc=-1;
	struct stat statbuf;
	off_t len=0;
	char	*p=NULL;
	int	n;

	FQ_TRACE_ENTER(l);

	if(( fdin = open(path_file ,O_RDONLY)) < 0 ) { /* option : O_RDWR */
		*ptr_errmsg = strdup("open() error.");
		goto L0;
	}

	if(( fstat(fdin, &statbuf)) < 0 ) {
		*ptr_errmsg = strdup("stat() error.");
	}

	len = statbuf.st_size;
	*flen = len;

	p = calloc(len+1, sizeof(char));
	if(!p) {
		*ptr_errmsg = strdup("memory leak!! Check your free memory!");	
		goto L1;
	}

	if(( n = read( fdin, p, len)) != len ) {
		*ptr_errmsg = strdup("read() error.");
		goto L1;
    }

	*ptr_dst = p;

	rc = 0;
L1:
	SAFE_FD_CLOSE(fdin);
L0:
	FQ_TRACE_EXIT(l);

	return(rc);
}
