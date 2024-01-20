/*
** fq_seq.c
*/
#include "fq_defs.h"
#include "fq_logger.h"
#include "fq_common.h"
#include "fq_sequence.h"

static int on_get_sequence( sequence_obj_t *obj, char *buf, size_t buf_size);
static int on_reset_sequence( sequence_obj_t *obj);

int  open_sequence_obj( fq_logger_t *l, char *path, char *fname, char *prefix, int total_length, sequence_obj_t **obj)
{
	sequence_obj_t *rc = NULL;
	void *lp=NULL, *sp=NULL;
	long x;
	int status;

	FQ_TRACE_ENTER( l );

	if( !HASVALUE(path) || !HASVALUE(fname)) {
		fq_log(l, FQ_LOG_ERROR, "illegal request. There is no path or fname.");
		FQ_TRACE_EXIT( l );
		return(FALSE);
	}
	lp = calloc(1, sizeof(char));
	sp = calloc(1, sizeof(long));
		
	rc = (sequence_obj_t *)calloc(1, sizeof(sequence_obj_t));

	if( rc ) {
		rc->l = l;
		rc->path = strdup(path);
		rc->fname = strdup(fname);
		sprintf(rc->pathfile, "%s/.%s.seq", rc->path, rc->fname);
		sprintf(rc->lock_pathfile, "%s/.%s.lock", rc->path, rc->fname);

		if( HASVALUE(prefix) ) {
			rc->prefix = strdup(prefix);
		}
		else {
			rc->prefix = NULL;
		}
		rc->total_length = total_length;

        if( can_access_file( rc->l, rc->lock_pathfile ) == 0 ) {
				int n;
                if( (rc->lockfd = open(rc->lock_pathfile, O_CREAT|O_EXCL, 0666)) < 0){
                        fq_log(l, FQ_LOG_ERROR,  "lock_file create|open error. reason=[%s].", strerror(errno));
						goto return_FALSE;
                }
				SAFE_FD_CLOSE(rc->lockfd);

                if( (rc->lockfd=open( rc->lock_pathfile, O_RDWR)) < 0 ) {
                        fq_log(l, FQ_LOG_ERROR, "lock_file open error. reason=[%s].", strerror(errno));
						goto return_FALSE;
                }

                memset(lp, 0x00, 1);
                n = write(rc->lockfd, lp, 1);
				SAFE_FD_CLOSE(rc->lockfd);

				fq_log(l, FQ_LOG_INFO, "New lock file is created succefully.'%s'", rc->lock_pathfile);
        }

        if( (rc->lockfd=open( rc->lock_pathfile, O_RDWR)) < 0 ) {
                fq_log(l, FQ_LOG_ERROR, "lock_file('%s') open error. reason=[%s].", rc->lock_pathfile,  strerror(errno));
				SAFE_FREE(lp);
				SAFE_FREE(sp);
				goto return_FALSE;
        }

		if( can_access_file( l, rc->pathfile ) == 0 ) {
                if( (rc->fd = open(rc->pathfile, O_CREAT|O_EXCL, 0666)) < 0){
                        fq_log(l, FQ_LOG_ERROR, "seq file create|open error. reason=[%s].", strerror(errno));
						goto return_FALSE;
                }
				SAFE_FD_CLOSE(rc->fd);
                
                if( (rc->fd=open( rc->pathfile, O_RDWR)) < 0 ) {
                        fq_log(l, FQ_LOG_ERROR, "seq file open failed. reason=[%s].", strerror(errno));
						goto return_FALSE;
                }
                memset(sp, 0x00, sizeof(long));
                x = 0L;
                memcpy(sp, &x, sizeof(long));
                if( write(rc->fd, sp, sizeof(long)) < 0 ) {
                        fq_log(l, FQ_LOG_ERROR, "seq file write error. reason=[%s].", strerror(errno));
						goto return_FALSE;
                }
                SAFE_FD_CLOSE(rc->fd);
        }

        if( (rc->fd=open( rc->pathfile, O_RDWR)) < 0 ) {
                fq_log(l, FQ_LOG_ERROR, "seq file open failed. reason=[%s].", strerror(errno));
				goto return_FALSE;
        }

		rc->on_get_sequence = on_get_sequence;
		rc->on_reset_sequence = on_reset_sequence;

		*obj = rc;

		FQ_TRACE_EXIT( l );
		return(TRUE);
	}

return_FALSE:
	SAFE_FREE( lp );
	SAFE_FREE( sp );

	SAFE_FD_CLOSE(rc->lockfd);
	SAFE_FD_CLOSE(rc->fd);

	FQ_TRACE_EXIT( l );
	return(FALSE);
}

int close_sequence_obj( sequence_obj_t **obj)
{
	SAFE_FREE( (*obj)->path );
	SAFE_FREE( (*obj)->fname );
	SAFE_FREE( (*obj)->prefix );

	SAFE_FD_CLOSE((*obj)->lockfd);
	SAFE_FD_CLOSE((*obj)->fd);

	SAFE_FREE( (*obj));

	return(TRUE);
}

static long get_seq(sequence_obj_t *obj)
{
        long ret=-1;
        void *lp=NULL, *sp=NULL;
        long x;
        int status;

        lp = calloc(1, sizeof(char));
        sp = calloc(1, sizeof(long));

        if( lseek(obj->lockfd, 0, SEEK_SET) < 0 ) {
                fq_log(obj->l, FQ_LOG_ERROR, "lseek() error. reason=[%s].", strerror(errno));
                goto L0;
        }
        status = lockf(obj->lockfd, F_LOCK, 1);
        if(status < 0 ) {
                fq_log(obj->l, FQ_LOG_ERROR, "lockf() failed. reason=[%s]", strerror(errno));
				SAFE_FREE(lp);
				SAFE_FREE(sp);
                return(-1);
        }

        if( lseek(obj->fd, 0, SEEK_SET) < 0 ) {
                fq_log(obj->l, FQ_LOG_ERROR, "lseek() error. reason=[%s].", strerror(errno));
                goto L0;
        }
        memset(sp, 0x00, sizeof(long));
        if( read( obj->fd, sp, sizeof(long)) < 0 ) {
                fq_log(obj->l, FQ_LOG_ERROR, "read() error. reason=[%s].", strerror(errno));
                goto L0;
        }

        memcpy(&x, sp, sizeof(long));
        x++;

        if( lseek(obj->fd, 0, SEEK_SET) < 0 ) {
                fq_log(obj->l, FQ_LOG_ERROR,  "lseek() error. reason=[%s].", strerror(errno));
                goto L0;
        }

        memcpy(sp, &x, sizeof(long));
        if( write(obj->fd, sp, sizeof(long)) < 0 ) {
                fq_log(obj->l, FQ_LOG_ERROR, "write() error.reason=[%s].", strerror(errno));
                goto L0;
        }
        ret = x;
L0:
		SAFE_FREE(lp);
		SAFE_FREE(sp);

        lockf(obj->lockfd, F_ULOCK, 1);

        return(ret);    
}

/*
** return: TRUE -> success
**         FALSE-> failure
*/
static int on_get_sequence( sequence_obj_t *obj, char *buf, size_t buf_size)
{
	long l_seq;
	int no_len;
	char	fmt[40];
	int prefix_len;

	if( buf_size < (obj->total_length + 1 ) ) {
		fq_log(obj->l, FQ_LOG_ERROR, "buffer size(%d) is less than total_length(%d)+1. ", buf_size,  obj->total_length);
		return(FALSE);
	}
	
	prefix_len = strlen(obj->prefix);
	if( obj->prefix ) {
		l_seq = get_seq(obj);
		no_len = obj->total_length - prefix_len;
		sprintf(fmt, "%s%c0%d%s", obj->prefix, '%', no_len, "ld");
	}
	else {
		sprintf(fmt, "%c0%d%s", '%', obj->total_length, "ld");
	}
	sprintf( buf, fmt, l_seq);

	fq_log(obj->l, FQ_LOG_DEBUG, "SEQ = '%s'", buf);

	return(TRUE);
}

int on_reset_sequence( sequence_obj_t *obj) 
{
        void *lp=NULL, *sp=NULL; /* lock file pointer */
        int status;
		long x;

        lp = calloc(1, sizeof(char));
        sp = calloc(1, sizeof(long));

        status = lockf(obj->lockfd, F_LOCK, 1);
        if(status < 0 ) {
                fq_log(obj->l, FQ_LOG_ERROR, "lockf() failed. reason=[%s].", strerror(errno));
				SAFE_FREE(lp);
                return(-1);
        }

        if( lseek(obj->fd, 0, SEEK_SET) < 0 ) {
                fq_log(obj->l, FQ_LOG_ERROR, "lseek() error. reason=[%s].", strerror(errno));
                goto L0;
        }

		memset(sp, 0x00, sizeof(long));

		x = 0L;
		memcpy(sp, &x, sizeof(long));
		if( write(obj->fd, sp, sizeof(long)) < 0 ) {
				fq_log(obj->l, FQ_LOG_ERROR, "seq file write error. reason=[%s].", strerror(errno));
				goto L0;
		}
L0:
		SAFE_FREE(lp);

        lockf(obj->lockfd, F_ULOCK, 1);

		return(TRUE);
}
