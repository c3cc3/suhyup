/*
 * fq_flock.c
 */
#define FQ_FLOCK_C_VERSION "1.0.0"

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <strings.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h> /* for chmod() */

#include "fq_common.h"
#include "fq_logger.h"
#include "fq_flock.h"


static int on_flock( flock_obj_t *obj);
static int on_funlock( flock_obj_t *obj);
static int on_flock2( flock_obj_t *obj);
static int on_funlock2( flock_obj_t *obj);

int 
open_flock_obj( fq_logger_t *l, char *path, char *qname, int en_de_flag, flock_obj_t **obj)
{
	flock_obj_t *rc=NULL;

	FQ_TRACE_ENTER(l);
	
	if( !HASVALUE(path) || !HASVALUE(qname) )  {
		fq_log(l, FQ_LOG_ERROR, "illgal function call.");
        FQ_TRACE_EXIT(l);
        return(FALSE);
    }

	rc  = (flock_obj_t *)calloc(1, sizeof(flock_obj_t));
	if( rc ) {
		rc->path = strdup(path);
		if( en_de_flag == EN_FLOCK )
			sprintf(rc->pathfile, "%s/.%s.en_flock", path, qname);
		else if( en_de_flag == DE_FLOCK )
			sprintf(rc->pathfile, "%s/.%s.de_flock", path, qname);
		else
			sprintf(rc->pathfile, "%s/.%s.flock", path, qname);
		
		if( !is_file(rc->pathfile) ) {
			void *p;
			int n;

			if( (rc->fd = open(rc->pathfile, O_CREAT|O_EXCL|O_RDWR, 0666)) < 0){
				fq_log(l, FQ_LOG_ERROR, "[%s] open(CREATE) error. reason=[%s]", rc->pathfile, strerror(errno));
				goto return_FALSE;
			}
			chmod(rc->pathfile, 0666);

		
			p = malloc(1);
			memset(p, 0x00, 1);

			n = write(rc->fd, p, 1);
			if( n != 1 ) {
				fq_log(l, FQ_LOG_ERROR, "[%s] file write error. reason=[%s]", rc->pathfile, strerror(errno));
				goto return_FALSE;
			}

			close(rc->fd);
		}
		if( (rc->fd=open(rc->pathfile, O_RDWR)) < 0 ) {
			fq_log(l, FQ_LOG_ERROR,  "lock file open error.[%s] reason=[%s].", rc->pathfile, strerror(errno));
			goto return_FALSE;
		}

		pthread_mutexattr_init( &rc->mtx_attr);
        pthread_mutex_init(&rc->mtx, &rc->mtx_attr);

		rc->on_flock = on_flock;
		rc->on_funlock = on_funlock;
		rc->on_flock2 = on_flock2;
		rc->on_funlock2 = on_funlock2;

		rc->l = l;

		*obj = rc;

        FQ_TRACE_EXIT(l);
		return(TRUE);
	}

return_FALSE:
	SAFE_FREE(rc->path);
	SAFE_FREE(rc->qname);
	SAFE_FREE(*obj);

	FQ_TRACE_EXIT(l);
	return(FALSE);
}

int 
close_flock_obj( fq_logger_t *l, flock_obj_t **obj)
{
	FQ_TRACE_ENTER(l);
	
	SAFE_FREE( (*obj)->path );
	SAFE_FREE( (*obj)->qname );

	if( (*obj)->fd > 0 ) close( (*obj)->fd);

	SAFE_FREE( (*obj) );

	FQ_TRACE_EXIT(l);
	return(TRUE);
}

int 
unlink_flock( fq_logger_t *l, char *pathfile) 
{
	if (!is_file(pathfile) ) {
		fq_log(l, FQ_LOG_ERROR,  "[%s] does not exist.", pathfile);
		return(-1);
	}

	unlink(pathfile);
	return(TRUE);
}


static int 
on_flock( flock_obj_t *obj )
{
 	int status;

	FQ_TRACE_ENTER(obj->l);

	pthread_mutex_lock(&obj->mtx);

	if( lseek(obj->fd, 0, SEEK_SET) < 0 ) {
		fq_log(obj->l,  FQ_LOG_ERROR, "lseek() failed. reason=[%s]", strerror(errno));
		FQ_TRACE_EXIT(obj->l);
		return(-1);
	}

	status = lockf(obj->fd, F_LOCK, 1);
	if(status < 0 ) {
		fq_log(obj->l,  FQ_LOG_ERROR, "lockf() failed. reason=[%s]", strerror(errno));
		FQ_TRACE_EXIT(obj->l);
		return(-1);
	}
	FQ_TRACE_EXIT(obj->l);
	return(0);
}

int 
on_funlock( flock_obj_t *obj )
{
	FQ_TRACE_ENTER(obj->l);

	pthread_mutex_unlock(&obj->mtx);

	if( lseek(obj->fd, 0, SEEK_SET) < 0 ) {
		fq_log(obj->l, FQ_LOG_ERROR, "lseek() failed. reason=[%s]", strerror(errno));
		FQ_TRACE_EXIT(obj->l);
		return(-1);
	}
	FQ_TRACE_EXIT(obj->l);
	return(lockf(obj->fd, F_ULOCK, 1));
} 
static int 
on_flock2( flock_obj_t *obj )
{
 	int status;

	FQ_TRACE_ENTER(obj->l);

	pthread_mutex_lock(&obj->mtx);

	obj->fl.l_type = F_WRLCK;
	obj->fl.l_whence = SEEK_SET;
	obj->fl.l_start = 0;
	obj->fl.l_len = 0;
	obj->fl.l_pid = getpid();

	if( fcntl(obj->fd, F_SETLKW, &obj->fl) == -1 ) {
		fq_log(obj->l,  FQ_LOG_ERROR, "fcntl(locking) failed. reason=[%s]", strerror(errno));
		FQ_TRACE_EXIT(obj->l);
		return(-1);
	}

	FQ_TRACE_EXIT(obj->l);
	return(0);
}

int 
on_funlock2( flock_obj_t *obj )
{
	FQ_TRACE_ENTER(obj->l);

	pthread_mutex_unlock(&obj->mtx);

	obj->fl.l_type = F_UNLCK;

	if( fcntl(obj->fd, F_SETLK, &obj->fl) == -1 ) {
		fq_log(obj->l, FQ_LOG_ERROR, "fcntl(unlocking) failed. reason=[%s]", strerror(errno));
		FQ_TRACE_EXIT(obj->l);
		return(-1);
	}
	FQ_TRACE_EXIT(obj->l);
	return(0);
} 

#if 0
int flock_with_timeout( fq_logger_t *l, int fd , int timeout ) 
{
	struct timespec wait_interval = { .tv_sec = 1, .tv_nsec = 0 };
	int wait_total = 0; // Total time spent in sleep

	while( flock(fd, LOCK_EX | LOCK_NB ) == -1) {
		if( errno == EWOULDBLOCK ) {
			// File is locked by another process
			if( wait_total > timeout ) {// If timeout is reached
				fq_log(l, FQ_LOG_ERROR, "Timeout occurred while waiting to lock file  timeout=[%d].");
				return -1;
			}
			sleep(wait_interval.tv_sec);
			wait_total += wait_interval.tv_sec;
		}
		else {
			fq_log(l, FQ_LOG_ERROR, "flock failed." );
			return -1;
		}
	}
	return 0;
}
#endif

#if 0
#include <stdbool.h>
#include <sys/time.h>
#include <signal.h>

void TimerStop(fq_logger_t *l, int signum)
{
	fq_log(l, FQ_LOG_INFO, "Timer ran out! Stopping timer.");
	exit(signum);
}

void TimerSet(fq_logger_t *l, int interval)
{
	fq_log(l, FQ_LOG_INFO, "Starting Timer.");
	struct itimerval it_val;

	// interval value
	it_val.it_value.tv_sec = interval;
	it_val.it_interval = it_val.it_value;

	// in SIGALRM, close window
	if( signal(SIGALRM, TimerStop) == SIG_ERR)
	{
		fq_log(l, FQ_LOG_ERROR, "Unable to catch SIGALRM.");
		exit(1);
	}
	if( setitimer( ITIMER_REAL, &it_val, NULL) == -1 ) 
	{
		fq_log(l, FQ_LOG_ERROR, "error calling setitimer()");
		exit(1);
	}
	return;
}
#endif
