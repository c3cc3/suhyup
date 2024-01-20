/* How to read a log file safely */
/* If the process stops and runs, it reads contents from last offset */
/*
** fq_safe_read_multi.c
*/

#include "config.h"
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#include <libgen.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include "fq_defs.h"
#include "fq_logger.h"
#include "fq_types.h"
#include "fq_common.h"
#include "fq_flock.h"
#include "fq_socket.h"

#include "fq_safe_read_multi.h"

static off_t get_last_offset( char *offset_filename );

extern int errno;

static size_t get_Filesize(char *data_filename)
{
	int fd;
    struct stat statbuf;
    off_t len=0;

	fd = open(data_filename, O_RDONLY);
	if( fd < 0 ) {
		return(fd);
	}
    if(( fstat(fd, &statbuf)) < 0 ) {
		return(-99); /* error */
    }
	close(fd);

    return(statbuf.st_size);
}

static off_t on_get_offset( safe_read_file_multi_obj_t *obj)
{
	off_t offset;

	offset = obj->offset;
	return(offset);
}
static float on_progress( safe_read_file_multi_obj_t *obj)
{
	off_t offset = obj->offset;
	size_t file_size = get_Filesize(obj->filename);
	float  progress_percent;

	progress_percent = (float)offset*100.0/(float)file_size;
	return(progress_percent);
}

static int on_read( safe_read_file_multi_obj_t *obj, char **dst)
{
	int n, rc;
	char linebuf[MAX_MSG_LEN+1];
	char *p=NULL;
	size_t file_size;

	FQ_TRACE_ENTER(obj->l);

	obj->lock_obj->on_flock(obj->lock_obj);

	if(obj->open_flag && obj->fp && obj->offset_open_flag && obj->offset_fp ) {
		if( !is_file(obj->filename) ) { /* file is deleted */
			fclose(obj->fp);
			obj->fp = NULL;
			obj->open_flag = 0;
			fclose(obj->offset_fp);
			obj->offset_fp = NULL;
			obj->offset_open_flag = 0;
			obj->lock_obj->on_funlock(obj->lock_obj);
			FQ_TRACE_EXIT(obj->l);
			return( FILE_NOT_EXIST );
		}
		file_size = get_Filesize(obj->filename);
		if( file_size == obj->offset) {
			fq_log(obj->l, FQ_LOG_DEBUG, "There is no changing of a file.");
			obj->lock_obj->on_funlock(obj->lock_obj);
			FQ_TRACE_EXIT(obj->l);
			return(FILE_NO_CHANGE);
		} else if( file_size < obj->offset ) {
			fq_log(obj->l, FQ_LOG_ERROR, "The file is changed without permit. My offset is bigger than filesize.");
			obj->offset = 0;
			fclose(obj->fp);
			obj->open_flag = 0;
			obj->lock_obj->on_funlock(obj->lock_obj);
			FQ_TRACE_EXIT(obj->l);
			return(FILE_READ_RETRY);
		} else if( file_size > obj->offset ) {
			fseek(obj->fp, obj->offset, SEEK_SET);
			fgets(linebuf, MAX_MSG_LEN, obj->fp);
			obj->offset = ftell(obj->fp);
			fseek(obj->offset_fp, 0, SEEK_SET);
			fprintf(obj->offset_fp, "%013ld", obj->offset);  
			fflush(obj->offset_fp);
			*dst = strdup(linebuf);
			fq_log(obj->l, FQ_LOG_DEBUG, "read success. changed offset=[%ld]", obj->offset);

			obj->lock_obj->on_funlock(obj->lock_obj);
			FQ_TRACE_EXIT(obj->l);
			return(FILE_READ_SUCCESS);
		} else {
			fq_log(obj->l, FQ_LOG_ERROR, "Unknown case.");
			obj->lock_obj->on_funlock(obj->lock_obj);
			FQ_TRACE_EXIT(obj->l);
			return(FILE_READ_FATAL);
		}
	} else if( obj->open_flag == 0 ) {
		if( !is_file(obj->filename) ) {
			obj->lock_obj->on_funlock(obj->lock_obj);
			FQ_TRACE_EXIT(obj->l);
			return( FILE_NOT_EXIST );
		}
		obj->fp = fopen(obj->filename, "r");
		if( obj->fp == NULL) {
			fq_log(obj->l, FQ_LOG_ERROR, "Fatal: fopen('%s') error. reason=[%s]", strerror(errno));
			obj->lock_obj->on_funlock(obj->lock_obj);
			return( FILE_READ_FATAL );
		}
		obj->open_flag = 1;

		obj->lock_obj->on_funlock(obj->lock_obj);
		FQ_TRACE_EXIT(obj->l);
		return(FILE_READ_RETRY);
	} else if( obj->offset_open_flag == 0 ) {
		if( !is_file(obj->offset_filename) ) {
			obj->offset_fp = fopen(obj->offset_filename, "w");
			if( obj->offset_fp == NULL) {
				fq_log(obj->l, FQ_LOG_ERROR, "Fatal: fopen('%s') error. reason=[%s]", 
					obj->offset_filename, strerror(errno));
				obj->lock_obj->on_funlock(obj->lock_obj);
				return( FILE_READ_FATAL );
			}
			obj->offset_open_flag = 1;
			obj->offset = 0L;

			obj->lock_obj->on_funlock(obj->lock_obj);
			FQ_TRACE_EXIT(obj->l);
			return(FILE_READ_RETRY);
		} else {
			char buf[128+1];
			obj->offset_fp = fopen(obj->offset_filename, "r+");
			if( obj->offset_fp == NULL) {
				fq_log(obj->l, FQ_LOG_ERROR, "Fatal: fopen('%s') error. reason=[%s]", 
					obj->offset_filename, strerror(errno));
				obj->lock_obj->on_funlock(obj->lock_obj);
				FQ_TRACE_EXIT(obj->l);
				return( FILE_READ_FATAL );
			}
			fq_log(obj->l, FQ_LOG_DEBUG, "We get last offset.=[%ld]", obj->offset);

			fgets(buf, 128, obj->offset_fp);
			str_lrtrim(buf);
			obj->offset = atol(buf);
			obj->offset_open_flag = 1;
			obj->lock_obj->on_funlock(obj->lock_obj);
			FQ_TRACE_EXIT(obj->l);
			return(FILE_READ_RETRY);
		}
	} else {
		fq_log(obj->l, FQ_LOG_ERROR, "Fatal: Unknown case");
		obj->lock_obj->on_funlock(obj->lock_obj);
		FQ_TRACE_EXIT(obj->l);
		return( FILE_READ_FATAL );
	}
	obj->lock_obj->on_funlock(obj->lock_obj);

	FQ_TRACE_EXIT(obj->l);
	return(TRUE);
}

static int on_read_b( safe_read_file_multi_obj_t *obj, char **dst, int *ptr_msglen)
{
	int n, rc;
	char linebuf[MAX_MSG_LEN+1];
	char *p=NULL;
	size_t file_size;
	unsigned char header[5];
	int msglen;

	FQ_TRACE_ENTER(obj->l);

	if( obj->binary_flag != true ) {
		fq_log(obj->l, FQ_LOG_ERROR, "You are not binary file object.");
		FQ_TRACE_EXIT(obj->l);
		return(FILE_READ_FATAL);
	}

	obj->lock_obj->on_flock(obj->lock_obj);

	if(obj->open_flag && obj->fp && obj->offset_open_flag && obj->offset_fp ) {
		if( !is_file(obj->filename) ) { /* file is deleted */
			fclose(obj->fp);
			obj->fp = NULL;
			obj->open_flag = 0;
			fclose(obj->offset_fp);
			obj->offset_fp = NULL;
			obj->offset_open_flag = 0;
			obj->lock_obj->on_funlock(obj->lock_obj);
			FQ_TRACE_EXIT(obj->l);
			return( FILE_NOT_EXIST );
		}
		file_size = get_Filesize(obj->filename);
		if( file_size == obj->offset) {
			fq_log(obj->l, FQ_LOG_DEBUG, "There is no changing of a file.");
			obj->lock_obj->on_funlock(obj->lock_obj);
			FQ_TRACE_EXIT(obj->l);
			return(FILE_NO_CHANGE);
		} else if( file_size < obj->offset ) {
			fq_log(obj->l, FQ_LOG_ERROR, "The file is changed without permit. My offset is bigger than filesize.");
			obj->offset = 0;
			fclose(obj->fp);
			obj->open_flag = 0;
			obj->lock_obj->on_funlock(obj->lock_obj);
			FQ_TRACE_EXIT(obj->l);
			return(FILE_READ_RETRY);

		} else if( file_size > obj->offset ) {
			fseek(obj->fp, obj->offset, SEEK_SET);

			fread( header, 1, 4, obj->fp);
			msglen = get_bodysize( header, 4, BINARY_HEADER_TYPE );
			*ptr_msglen = msglen;
			fread( linebuf, 1, msglen, obj->fp);

			obj->offset = ftell(obj->fp);
			fseek(obj->offset_fp, 0, SEEK_SET);
			fprintf(obj->offset_fp, "%013ld", obj->offset);  
			fflush(obj->offset_fp);
			*dst = strdup(linebuf);
			fq_log(obj->l, FQ_LOG_DEBUG, "read success. changed offset=[%ld]", obj->offset);

			obj->lock_obj->on_funlock(obj->lock_obj);
			FQ_TRACE_EXIT(obj->l);
			return(FILE_READ_SUCCESS);
		} else {
			fq_log(obj->l, FQ_LOG_ERROR, "Unknown case.");
			obj->lock_obj->on_funlock(obj->lock_obj);
			FQ_TRACE_EXIT(obj->l);
			return(FILE_READ_FATAL);
		}
	} else if( obj->open_flag == 0 ) {
		if( !is_file(obj->filename) ) {
			obj->lock_obj->on_funlock(obj->lock_obj);
			FQ_TRACE_EXIT(obj->l);
			return( FILE_NOT_EXIST );
		}
		obj->fp = fopen(obj->filename, "r");
		if( obj->fp == NULL) {
			fq_log(obj->l, FQ_LOG_ERROR, "Fatal: fopen('%s') error. reason=[%s]", strerror(errno));
			obj->lock_obj->on_funlock(obj->lock_obj);
			return( FILE_READ_FATAL );
		}
		obj->open_flag = 1;

		obj->lock_obj->on_funlock(obj->lock_obj);
		FQ_TRACE_EXIT(obj->l);
		return(FILE_READ_RETRY);
	} else if( obj->offset_open_flag == 0 ) {
		if( !is_file(obj->offset_filename) ) {
			obj->offset_fp = fopen(obj->offset_filename, "w");
			if( obj->offset_fp == NULL) {
				fq_log(obj->l, FQ_LOG_ERROR, "Fatal: fopen('%s') error. reason=[%s]", 
					obj->offset_filename, strerror(errno));
				obj->lock_obj->on_funlock(obj->lock_obj);
				return( FILE_READ_FATAL );
			}
			obj->offset_open_flag = 1;
			obj->offset = 0L;

			obj->lock_obj->on_funlock(obj->lock_obj);
			FQ_TRACE_EXIT(obj->l);
			return(FILE_READ_RETRY);
		} else {
			char buf[128+1];
			obj->offset_fp = fopen(obj->offset_filename, "r+");
			if( obj->offset_fp == NULL) {
				fq_log(obj->l, FQ_LOG_ERROR, "Fatal: fopen('%s') error. reason=[%s]", 
					obj->offset_filename, strerror(errno));
				obj->lock_obj->on_funlock(obj->lock_obj);
				FQ_TRACE_EXIT(obj->l);
				return( FILE_READ_FATAL );
			}
			fq_log(obj->l, FQ_LOG_DEBUG, "We get last offset.=[%ld]", obj->offset);

			fgets(buf, 128, obj->offset_fp);
			str_lrtrim(buf);
			obj->offset = atol(buf);
			obj->offset_open_flag = 1;
			obj->lock_obj->on_funlock(obj->lock_obj);
			FQ_TRACE_EXIT(obj->l);
			return(FILE_READ_RETRY);
		}
	} else {
		fq_log(obj->l, FQ_LOG_ERROR, "Fatal: Unknown case");
		obj->lock_obj->on_funlock(obj->lock_obj);
		FQ_TRACE_EXIT(obj->l);
		return( FILE_READ_FATAL );
	}
	obj->lock_obj->on_funlock(obj->lock_obj);

	FQ_TRACE_EXIT(obj->l);
	return(TRUE);
}

int open_file_4_read_safe_multi( fq_logger_t *l, char *filename, char *process_unique_id,  bool binary_flag, safe_read_file_multi_obj_t **obj)
{
	safe_read_file_multi_obj_t *rc= NULL;
	char lock_filename[256];
	char offset_filename[256];
	int ret;

	rc = (safe_read_file_multi_obj_t *)calloc( 1, sizeof(safe_read_file_multi_obj_t));

	if( !rc ) {
		fq_log(l, FQ_LOG_ERROR, "Fatal: calloc() error. reason=[%s].", strerror(errno));
		return(FALSE);
	}
	rc->filename = NULL;
	rc->offset_filename = NULL;
	rc->offset_fp = NULL;
	rc->l = l;
	rc->fp = NULL;
	rc->open_flag = 0;

	rc->filename = strdup(filename);

	if( binary_flag == false ) {
		rc->fp = fopen(rc->filename, "r");
		if(rc->fp)  rc->open_flag = 1;
	}
	else {
		rc->fp = fopen(rc->filename, "rb");
		if(rc->fp)  rc->open_flag = 1;
	}
	rc->binary_flag = binary_flag;

	char *ts1 = strdup(filename);
	char *ts2 = strdup(filename);

	char *dir=dirname(ts1);
	char *fname=basename(ts2);

    ret = open_flock_obj(rc->l, dir, fname, DE_FLOCK, &rc->lock_obj);
    if( ret != TRUE ) {
        fq_log(l, FQ_LOG_ERROR, "Fatal: open_flock_obj('%s') error.", rc->filename);

        SAFE_FREE(rc->filename);
        if(rc->fp) fclose(rc->fp);
        SAFE_FREE(rc);

        FQ_TRACE_EXIT(l);
        return(FALSE);
    }
	ret = open_flock_obj(l, dir, fname, EN_FLOCK, &rc->en_lock_obj);
	if( ret != TRUE ) {
		fq_log(l, FQ_LOG_ERROR, "Fatal: open_flock_obj('%s') error.", rc->filename);

		SAFE_FREE(rc->filename);
		if(rc->fp) fclose(rc->fp);
		SAFE_FREE(rc);

		FQ_TRACE_EXIT(l);
        return(FALSE);
	}

	sprintf(offset_filename, "%s/%s.offset", dir, process_unique_id);
	rc->offset_filename =strdup(offset_filename);

	if( can_access_file(NULL, rc->offset_filename) == TRUE ) {
		char buf[128+1];
		rc->offset_fp = fopen(offset_filename, "r+"); /* read/write open: don't delete */
		if( rc->offset_fp == NULL ) {
			fq_log(l, FQ_LOG_ERROR, "Fatal: fopen('%s') error.", rc->offset_filename);
			if(rc->fp) fclose(rc->fp);
			FQ_TRACE_EXIT(l);
			return(FALSE);
		}
		fgets(buf, 128, rc->offset_fp);
		str_lrtrim(buf);
		rc->offset = atol(buf);
		rc->offset_open_flag = 1;
	}
	else {
		rc->offset_open_flag = 0;
		rc->offset = 0L;
	}

	rc->on_read = on_read;
	rc->on_read_b = on_read_b;
	rc->on_get_offset = on_get_offset;
	rc->on_progress = on_progress;
	
	*obj = rc;

	if(ts1) free(ts1);
	if(ts2) free(ts2);

	FQ_TRACE_EXIT(l);
	return(TRUE);
}

int close_file_4_read_safe_multi( fq_logger_t *l, safe_read_file_multi_obj_t **obj)
{
	SAFE_FREE((*obj)->filename);
	SAFE_FREE((*obj)->offset_filename);
	if( (*obj)->fp ) fclose((*obj)->fp);
	if( (*obj)->offset_fp ) fclose((*obj)->offset_fp);

	if( (*obj)->lock_obj )  close_flock_obj(l, &(*obj)->lock_obj);
	if( (*obj)->en_lock_obj )  close_flock_obj(l, &(*obj)->en_lock_obj);

	SAFE_FREE( (*obj) );
}

