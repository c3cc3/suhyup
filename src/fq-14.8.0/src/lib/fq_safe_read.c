/* How to read a log file safely */
/* If the process stops and runs, it reads contents from last offset */

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

#include "fq_safe_read.h"

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
static int on_delete_file( safe_read_file_obj_t *obj)
{
	FQ_TRACE_ENTER(obj->l);
	obj->en_lock_obj->on_flock(obj->en_lock_obj);

	fclose(obj->fp);
	obj->fp = NULL;
	obj->open_flag = 0;
	unlink(obj->filename);

	fclose(obj->offset_fp);
	obj->offset_fp = NULL;
	obj->offset_open_flag = 0;
	unlink(obj->offset_filename);

	obj->en_lock_obj->on_funlock(obj->en_lock_obj);
	FQ_TRACE_EXIT(obj->l);
	return(TRUE);
}
static off_t on_get_offset( safe_read_file_obj_t *obj)
{
	off_t offset;

	offset = obj->offset;
	return(offset);
}
static float on_progress( safe_read_file_obj_t *obj)
{
	off_t offset = obj->offset;
	size_t file_size = get_Filesize(obj->filename);
	float  progress_percent;

	progress_percent = (float)offset*100.0/(float)file_size;
	return(progress_percent);
}

static int on_read( safe_read_file_obj_t *obj, char **dst)
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

static int goto_next_start_position(FILE *fp)
{
	while(1) {
		char one[2];
	
		fread(one, sizeof(unsigned char), 1, fp);
		if(one[0] == 0x03) {
			return(1); /* success */
		}
		if(feof(fp)) {
			return 0; /* end of file */
		}
	}
	return(-1);
}

static bool is_valid_header(int header_value)
{
	if(header_value > 0 && header_value < 65536) 
		return true;
	else
		return false;
}
static bool is_valid_body(unsigned char *body, int body_len)
{
	if(  (body[0] != 0x02) || (body[body_len-1] != 0x03) ) 
		return false;
	else
		return true;
}

static int on_read_b( safe_read_file_obj_t *obj, unsigned char **dst, int *ptr_msglen)
{
	int n, rc;
	unsigned char linebuf[MAX_MSG_LEN+1];
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

re_read:
			fread( header, 1, 4, obj->fp);
			msglen = get_bodysize( header, 4, BINARY_HEADER_TYPE );
			fq_log(obj->l, FQ_LOG_DEBUG, "msglen=[%d].", msglen);
			if( is_valid_header(msglen) == true) {
				int readn;
				
				readn = fread( linebuf, 1, msglen, obj->fp);
				if( is_valid_body( linebuf, readn) == true) {
					unsigned char *out = NULL;

					out = calloc(msglen+1, sizeof(unsigned char));
					obj->offset = ftell(obj->fp);
					fseek(obj->offset_fp, 0, SEEK_SET);
					fprintf(obj->offset_fp, "%013ld", obj->offset);  
					fflush(obj->offset_fp);
					memcpy(out, &linebuf[1], msglen-2);
					*ptr_msglen = msglen-2;
					*dst = out;
					fq_log(obj->l, FQ_LOG_DEBUG, "read success. changed offset=[%ld]", obj->offset);

					obj->lock_obj->on_funlock(obj->lock_obj);
					FQ_TRACE_EXIT(obj->l);
					return(FILE_READ_SUCCESS);
				}
				else {
					int r;
					r = goto_next_start_position(obj->fp);
					if( r == 1 ) {
						goto re_read;
					}
					else if( r == 0 ) {
						obj->offset = ftell(obj->fp);
						fseek(obj->offset_fp, 0, SEEK_SET);
						fprintf(obj->offset_fp, "%013ld", obj->offset);  
						fflush(obj->offset_fp);
						obj->lock_obj->on_funlock(obj->lock_obj);
						FQ_TRACE_EXIT(obj->l);
						return(FILE_READ_RETRY);
					}
					else {
						fq_log(obj->l, FQ_LOG_ERROR, "goto_next_start_position() error.");
						obj->lock_obj->on_funlock(obj->lock_obj);
						FQ_TRACE_EXIT(obj->l);
						return(FILE_READ_FATAL);
					}
				}
			}
			else {
				fq_log(obj->l, FQ_LOG_ERROR, "invalid header. msglen=[%d]", msglen);
				int  r;
				r = goto_next_start_position(obj->fp);
				if( r == 1 ) {
					goto re_read;
				}
				else if( r == 0 ) {
					obj->offset = ftell(obj->fp);
					fseek(obj->offset_fp, 0, SEEK_SET);
					fprintf(obj->offset_fp, "%013ld", obj->offset);  
					fflush(obj->offset_fp);
					obj->lock_obj->on_funlock(obj->lock_obj);
					FQ_TRACE_EXIT(obj->l);
					return(FILE_READ_RETRY);
				}
				else {
					fq_log(obj->l, FQ_LOG_ERROR, "goto_next_start_position() error.");
					obj->lock_obj->on_funlock(obj->lock_obj);
					FQ_TRACE_EXIT(obj->l);
					return(FILE_READ_FATAL);
				}
			}
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

int open_file_4_read_safe( fq_logger_t *l, char *filename, bool binary_flag,  safe_read_file_obj_t **obj)
{
	safe_read_file_obj_t *rc= NULL;
	char lock_filename[256];
	char offset_filename[256];
	int ret;

	rc = (safe_read_file_obj_t *)calloc( 1, sizeof(safe_read_file_obj_t));
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

	sprintf(offset_filename, "%s.offset", rc->filename);
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
	rc->on_delete_file = on_delete_file;
	rc->on_get_offset = on_get_offset;
	rc->on_progress = on_progress;
	
	*obj = rc;

	if(ts1) free(ts1);
	if(ts2) free(ts2);

	FQ_TRACE_EXIT(l);
	return(TRUE);
}

int close_file_4_read_safe( fq_logger_t *l, safe_read_file_obj_t **obj)
{
	SAFE_FREE((*obj)->filename);
	SAFE_FREE((*obj)->offset_filename);
	if( (*obj)->fp ) fclose((*obj)->fp);
	if( (*obj)->offset_fp ) fclose((*obj)->offset_fp);

	if( (*obj)->lock_obj )  close_flock_obj(l, &(*obj)->lock_obj);
	if( (*obj)->en_lock_obj )  close_flock_obj(l, &(*obj)->en_lock_obj);

	SAFE_FREE( (*obj) );
}

#if 0
/*
** examples
*/
#include "config.h"
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include <time.h>
#include "fq_defs.h"
#include "fq_logger.h"
#include "fq_types.h"
#include "fq_common.h"
#include "fq_config.h"
#include "fq_param.h"
#include "fq_safe_read.h"
#include "fqueue.h"

#define MAX_MSG_LEN 65535

extern int errno;
char g_Progname[256];


void print_help(char *p)
{
	printf("\n\n Usage: $ %s [-f config filename] <enter>\n", p);
	return;
}

int main(int ac, char **av)
{
	int rc;
	char	buffer[256];
	char *data_filename=NULL;
	char *logname = NULL;
	char *loglevel=NULL;
	int i_loglevel=4;
	char *config_filename=NULL;
	char *linebuf=NULL;
	fq_logger_t *l=NULL;
	int ch;
	config_t *t=NULL;
	char *qpath=NULL;
	char *qname=NULL;
	fqueue_obj_t *q_obj=NULL;
	safe_read_file_obj_t *obj=NULL;
	char *starting_usage = NULL;
	float f_starting_usage;
	char *stdout_print=NULL;
	
	getProgName(av[0], g_Progname);

	if(ac < 3 ) {
		print_help(g_Progname);
		return(0);
	}

	while(( ch = getopt(ac, av, "f:")) != -1) {
		switch(ch) {
			case 'f':
				config_filename = strdup(optarg);
				break;
			default:
				print_help(av[0]);
				return(0);
		}
	}

	if(!HASVALUE(config_filename) ) { 
		print_help(g_Progname);
		return(0);
	}

	/* Loading server environment */

	t = new_config(config_filename);

	if( load_param(t, config_filename) < 0 ) {
		fprintf(stderr, "load_param() error.\n");
		return(EXIT_FAILURE);
	}

	get_config(t, "DATA_FILENAME", buffer);
	data_filename = strdup(buffer);

	get_config(t, "LOG_NAME", buffer);
	logname = strdup(buffer);

	get_config(t, "LOG_LEVEL", buffer);
	loglevel = strdup(buffer);

	get_config(t, "QPATH", buffer);
	qpath = strdup(buffer);

	get_config(t, "QNAME", buffer);
	qname = strdup(buffer);

	get_config(t, "STARTING_USAGE", buffer);
	starting_usage = strdup(buffer);
	f_starting_usage = atof(starting_usage);

	get_config(t, "STDOUT_PRINT", buffer);
	stdout_print = strdup(buffer);

	/* Checking mandatory parameters */
	if( !data_filename|| !logname || !loglevel || !qpath || !qname || !starting_usage) {
		printf("Check items(DATA_FILENAME/LOG_NAME/LOG_LEVEL/QPATH/QNAME/STARTING_USAGE) in config file.\n");
		return(0);
	}
	step_print("config file checking.", "OK");

	i_loglevel = get_log_level(loglevel);
	rc = fq_open_file_logger(&l, logname, i_loglevel);
	if( rc <= 0 ) {
		fprintf(stderr, "Please check log filename of your config.\n");
		return(0);
	}
	step_print("log file open.", "OK");

	if( strcmp(stdout_print,"yes")==0) {
		fq_file_logger_t *p = l->logger_private;
		fclose(p->logfile);
		p->logfile = stdout;
	}

	rc =  OpenFileQueue(l, NULL, NULL, NULL, NULL, qpath, qname, &q_obj);
	if(rc != TRUE) {
		fq_log(l, FQ_LOG_ERROR, "OpenFileQueue('%s', '%s') error.", qpath, qname);
		return(0);
	}
	step_print("file queue open.", "OK");

	rc = open_file_4_read_safe(l, data_filename, &obj);
	if(rc != TRUE) {
		fq_log(l, FQ_LOG_ERROR, "open_file_4_read_safe('%s') error.", data_filename);
		return(0);
	}
	step_print("safe read file open.", "OK");

	fq_log(l, FQ_LOG_EMERG, "Server started.(compiled:[%s][%s]", __DATE__, __TIME__);

	step_print("Server start.", "OK");
	while(1) { 
		long l_seq, run_time;
		int msglen;
		off_t offset;
		float q_current_usage =  q_obj->on_get_usage(l, q_obj);
		float progress_percent = 0.0;

		if( q_current_usage > f_starting_usage) {
			fq_log(l, FQ_LOG_INFO, "Current usage(%f) is bigger than setting value at config.(%f)\n",
				 q_current_usage, f_starting_usage);
			sleep(1);
			continue;
		}

		rc = obj->on_read(obj, &linebuf);
		switch(rc) {
			case FILE_NOT_EXIST:
				fq_log(l, FQ_LOG_INFO, "FILE_NOT_EXIST\n");
				sleep(5);
				break;
			case FILE_READ_SUCCESS:
				msglen= strlen(linebuf);
				offset = obj->on_get_offset(obj);
				progress_percent = obj->on_progress(obj);

				fq_log(l, FQ_LOG_INFO, "read OK. linebuf len=[%zu], offset=[%ld], [%5.2f]%%.", 
						msglen, obj->on_get_offset(obj), progress_percent);
				/* ToDo your job with linebuf in here.
				*/
				while(1) {
					rc =  q_obj->on_en(l, q_obj, EN_NORMAL_MODE, linebuf, msglen+1, msglen, &l_seq, &run_time );
					if( rc == EQ_FULL_RETURN_VALUE )  {
						fq_log(l, FQ_LOG_INFO, "Queue is full. I will retry.");
						usleep(1000);
						continue;
					}
					else if( rc < 0 ) { 
						if( rc == MANAGER_STOP_RETURN_VALUE ) {
							fq_log(l, FQ_LOG_ERROR, "Manager asked to stop a processing.");
						}
						goto stop;
					}
					else {
						fq_log(l, FQ_LOG_INFO, "enQ success rc=[%d].", rc);
						break;
					}
				}

				break;
			case FILE_READ_FATAL:
				fq_log(l, FQ_LOG_ERROR, "FILE_READ_FATAL. Good bye.\n");
				return(-1);
			case FILE_NO_CHANGE:
				fq_log(l, FQ_LOG_INFO, "FILE_NO_CHANGE.\n");
				obj->on_delete_file(obj);
				fq_log(l, FQ_LOG_INFO, "We wil delete files('%s', '%s').", obj->filename, obj->offset_filename);
				sleep(1);
				break;
			case FILE_READ_RETRY:
				fq_log(l, FQ_LOG_INFO, "FILE_READ_RETRY.\n");
				sleep(1);
				break;
			default:
				fq_log(l, FQ_LOG_ERROR, "Undefined return.\n");
				return(-1);
		}
		SAFE_FREE(linebuf);
	}
stop:
	if(obj) close_file_4_read_safe(l, &obj);
	if(t) free_config(&t);
	fq_close_file_logger(&l);
}

#endif
