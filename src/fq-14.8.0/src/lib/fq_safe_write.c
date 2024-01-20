#include "config.h"
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#include <libgen.h> /* for dirname, basename */
#ifdef HAVE_STRING_H
#include <string.h>
#include <stdbool.h>
#endif
#include "fq_defs.h"
#include "fq_logger.h"
#include "fq_types.h"
#include "fq_common.h"
#include "fq_flock.h"

#include "fq_socket.h"
#include "fq_safe_write.h"

extern int errno;

static int on_write( safe_file_obj_t *obj, char *what)
{
	int n;

	FQ_TRACE_ENTER(obj->l);

	if( obj->binary_flag == true ) {
		fq_log(obj->l, FQ_LOG_ERROR, "You are not a text file object." );
		FQ_TRACE_EXIT(obj->l);
		return(FALSE);
	}
	obj->lock_obj->on_flock(obj->lock_obj);

	obj->fp=freopen(obj->filename, "a", obj->fp);
	if(obj->fp == NULL) {
		fq_log(obj->l, FQ_LOG_ERROR, "Fatal: freopen('%s') error. reason=[%s].", 
			obj->filename, strerror(errno));
		obj->lock_obj->on_funlock(obj->lock_obj);
		FQ_TRACE_EXIT(obj->l);
		return(FALSE);
	}

	n = fputs(what, obj->fp);
	fflush(obj->fp);
	obj->lock_obj->on_funlock(obj->lock_obj);
	obj->w_cnt++;
	fq_log(obj->l, FQ_LOG_DEBUG, "[%ld]-th written success: len=[%d].", obj->w_cnt, strlen(what));

	FQ_TRACE_EXIT(obj->l);
	return(TRUE);
}

static int on_write_b( safe_file_obj_t *obj, unsigned char *what, size_t len)
{
	int n;
	unsigned char header[5];

	FQ_TRACE_ENTER(obj->l);

	if( obj->binary_flag == false ) {
		fq_log(obj->l, FQ_LOG_ERROR, "You are not a binary file object." );
		FQ_TRACE_EXIT(obj->l);
		return(FALSE);
	}

	if(!is_file(obj->filename)) {
		obj->fp = fopen(obj->filename, "ab");
		if(obj->fp == NULL) {
			fq_log(obj->l, FQ_LOG_ERROR, "Fatal: fopen('%s') error. reason=[%s].", 
				obj->filename, strerror(errno));
			FQ_TRACE_EXIT(obj->l);
			return(FALSE);
		}
	}

	obj->lock_obj->on_flock(obj->lock_obj);

#if 1 /* We add two characters, STX, ETX. bitween front of body and rear of body. */
	set_bodysize( header, 4, len+2, BINARY_HEADER_TYPE);
	unsigned char *final_msg=NULL;
	final_msg =calloc( (4+2+len+1), sizeof(unsigned char));
	memcpy(final_msg, header, 4);
	memset(final_msg+4, 0x02, 1);
	memcpy(final_msg+5, what, len);
	memset(final_msg+(len+4+1), 0x03, 1); 
	fwrite(final_msg, sizeof(unsigned char), 4+len+2, obj->fp);
#else
	set_bodysize( header, 4, len, BINARY_HEADER_TYPE);
	fwrite(header, sizeof(unsigned char), 4, obj->fp);
	fwrite(what, sizeof(unsigned char), len, obj->fp);
#endif

	fflush(obj->fp);
	obj->lock_obj->on_funlock(obj->lock_obj);
	obj->w_cnt++;
	fq_log(obj->l, FQ_LOG_DEBUG, "[%ld]-th written success: len=[%d].", obj->w_cnt, strlen(what));

	FQ_TRACE_EXIT(obj->l);
	return(TRUE);
}

int open_file_safe(fq_logger_t *l, char *filename, bool binary_flag, safe_file_obj_t **obj)
{
	int ret;
	safe_file_obj_t *rc = NULL;

	FQ_TRACE_ENTER(l);
	rc = (safe_file_obj_t *)calloc(1, sizeof(safe_file_obj_t));
	if( !rc ) {
		fq_log(l, FQ_LOG_ERROR, "Fatal: calloc() error. reason=[%s].", strerror(errno));
		FQ_TRACE_EXIT(l);
		return(-1);
	}
	rc->l = l;
	rc->w_cnt = 0L;
	rc->fp = NULL;

	rc->filename = strdup(filename);
	if( binary_flag == false ) {
		rc->fp = fopen(rc->filename, "a");
		if(rc->fp == NULL) {
			fq_log(l, FQ_LOG_ERROR, "Fatal: fopen('%s') error. reason=[%s].", 
				rc->filename, strerror(errno));
			SAFE_FREE(rc);
			FQ_TRACE_EXIT(l);
			return(-2);
		}
	}
	else {
		rc->fp = fopen(rc->filename, "ab");
		if(rc->fp == NULL) {
			fq_log(l, FQ_LOG_ERROR, "Fatal: fopen('%s') error. reason=[%s].", 
				rc->filename, strerror(errno));
			SAFE_FREE(rc);
			FQ_TRACE_EXIT(l);
			return(-2);
		}
	}
	rc->binary_flag = binary_flag;


	char *ts1 = strdup(rc->filename);
	char *ts2 = strdup(rc->filename);

	char *dir = dirname(ts1);
	char *fname = basename(ts2);

	ret = open_flock_obj(l, dir, fname, EN_FLOCK, &rc->lock_obj);
	if( ret != TRUE ) {
		fq_log(l, FQ_LOG_ERROR, "Fatal: open_flock_obj('%s') error.", rc->filename);

		SAFE_FREE(rc->filename);
		if(rc->fp) fclose(rc->fp);
		SAFE_FREE(rc);

		FQ_TRACE_EXIT(l);
		return(-3);
	}

	rc->on_write = on_write;
	rc->on_write_b = on_write_b;

	*obj = rc;

	if(ts1) free(ts1);
	if(ts2) free(ts2);
	
	FQ_TRACE_EXIT(l);
	return(TRUE);
}

void close_file_safe( safe_file_obj_t **obj)
{
	SAFE_FREE( (*obj)->filename );
	if((*obj)->fp) fclose((*obj)->fp);
	SAFE_FREE( (*obj) );
}

#if 0
int main()
{
	FILE *fp=NULL;
	int rc;
	fq_logger_t *l=NULL;
	safe_file_obj_t *obj=NULL;

	rc = fq_open_file_logger(&l, "/tmp/write_log_flock.log", get_log_level("trace"));
	CHECK(rc==TRUE);

	rc = open_file_safe(l, "/tmp/msg.dat", &obj);
	if(rc != TRUE ) {
		printf("erorr: open_file_safe().\n");
		return(-1);
	}

	while(1) {
		char buf[4096];
		char newline_msg[4096];

		memset(buf, 0x00, sizeof(buf));
		sprintf(newline_msg, "%s\n", get_seed_ratio_rand_str( buf, 2048, "0123456789ABCDEFGHIJ"));

		rc = obj->on_write( obj, newline_msg);
		if( rc != TRUE ) {
			printf("on_write() error.\n");
			break;
		}
		usleep(1000);
	}

	if(obj) close_file_safe(&obj);
	if(l) fq_close_file_logger(&l);

	return(0);
}
#endif
