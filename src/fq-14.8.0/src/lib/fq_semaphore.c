/* vi: set sw=4 ts=4: */
/*
 * fq_semaphore.c
 */
#include <stdio.h>
#include <string.h> 
#include <time.h>
#include <errno.h> 
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>

#include "config.h"
#include "fq_common.h"
#include "fq_semaphore.h"

#define FQ_SEMAPHORE_C_VERSION "1.0.0"

#if 0
static int on_sem_lock( fq_logger_t *l, semaphore_obj_t *obj);
static int on_sem_unlock( fq_logger_t *l, semaphore_obj_t *obj);
#else
static int on_sem_lock( semaphore_obj_t *obj);
static int on_sem_unlock( semaphore_obj_t *obj);
#endif
/*
 *
 * return
 * 		TRUE: 1
 * 		FALSE: 0
 */

int open_semaphore_obj( fq_logger_t *l, char *path, char key_char, semaphore_obj_t **obj)
{
	union fq_semun arg;
	struct semid_ds buf;
	struct sembuf sb;
	int nsems=1;
	semaphore_obj_t *rc=NULL;

	FQ_TRACE_ENTER(l);

	if( !HASVALUE(path) )  {
		fq_log(l, FQ_LOG_ERROR, "illgal function call.");
		FQ_TRACE_EXIT(l);
		return(FALSE);
	}

	rc  = (semaphore_obj_t *)calloc(1, sizeof(semaphore_obj_t));
	if(rc) {
		int i;

		rc->key = ftok(path, key_char);
		rc->path = strdup(path);
		rc->key_char = key_char;

		rc->l = l;
		rc->on_sem_lock = on_sem_lock;
		rc->on_sem_unlock = on_sem_unlock;

		rc->semid = semget(rc->key, nsems, IPC_CREAT | IPC_EXCL | 0666);
		fq_log(l, FQ_LOG_DEBUG, "key=[0x%08x] semid=[%d]\n", rc->key, rc->semid);

		if(rc->semid >= 0) {
			sb.sem_op = 1; sb.sem_flg = 0;
			arg.val = 1;

			for(sb.sem_num = 0; sb.sem_num < nsems; sb.sem_num++) {
				if( semop(rc->semid, &sb, 1) == -1) {
					int e=errno;
		
					semctl(rc->semid, 0, IPC_RMID); /* clean up */
					errno = e;
					fq_log(l, FQ_LOG_ERROR, "semop() error.");
					goto return_FALSE;
				}
			} /* for end. */
		}
		else if(errno == EEXIST || errno==0 || errno==2) { /* someone else got it first */
			int ready = 0;

			rc->semid = semget(rc->key, nsems, 0); /* get the id */
			if (rc->semid < 0) {
				fq_log(l, FQ_LOG_ERROR, "semget() error.");
				goto return_FALSE;
			}
			/* wait for other process to initialize the semaphore: */
			arg.buf = &buf;
			for(i = 0; i < MAX_RETRIES && !ready; i++) {
				semctl(rc->semid, nsems-1, IPC_STAT, arg);
				if (arg.buf->sem_otime != 0) {
					ready = 1;
				} else {
					sleep(1);
				}
			}
			if (!ready) {
				errno = ETIME;
				fq_log(l, FQ_LOG_ERROR, "semget() timeout error.");
				goto return_FALSE;
			}

		} else {
			/* in Linux, if errno is 28, it means lack of semaphore array.  remove using ipcrm -s or increase kernel parameters. */
			fq_log(l, FQ_LOG_ERROR, "semget() error. errno=[%d] reason=[%s]", errno, strerror(errno) );
			goto return_FALSE;

			/*
			*obj = rc;

			fq_log(l, FQ_LOG_DEBUG, "New Creation OK.");
			FQ_TRACE_EXIT(l);
			return(TRUE);
			*/
		}

		*obj = rc;

		fq_log(l, FQ_LOG_DEBUG, "Already exist. reuse. semid=[%d]", rc->semid);
		FQ_TRACE_EXIT(l);
		return(TRUE); 
	}
return_FALSE:

	SAFE_FREE(*obj);
	
	return(FALSE);
}

int close_semaphore_obj( fq_logger_t *l, semaphore_obj_t **obj)
{
	FQ_TRACE_ENTER(l);

	SAFE_FREE( (*obj)->path );

	SAFE_FREE( (*obj) );

	FQ_TRACE_EXIT(l);
	return(TRUE);
}

int unlink_semaphore( fq_logger_t *l, char *path, char key_char)
{
	key_t key;
	int semid;
	int	nsems=1;
	int rc;

	FQ_TRACE_ENTER(l);
	key = ftok(path, key_char);
	semid = semget(key, nsems, 0);
	if (semid < 0) {
		fq_log(l, FQ_LOG_ERROR, "semget() error.");
		goto return_FALSE;
	}

	rc = semctl(semid, 1, IPC_RMID); /* clean up */
	if( rc < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "semget() error.");
		goto return_FALSE;
	}

	fq_log(l, FQ_LOG_DEBUG, "unlink samephore OK.");
	FQ_TRACE_EXIT(l);
	return(TRUE);

return_FALSE:
	FQ_TRACE_EXIT(l);
	return(FALSE);
}


static int on_sem_lock( semaphore_obj_t *obj )
{
	obj->sb.sem_num = 0;
    obj->sb.sem_op = -1;  /* set to allocate resource */
    obj->sb.sem_flg = SEM_UNDO;

	FQ_TRACE_ENTER(obj->l);
	if (semop(obj->semid, &obj->sb, 1) == -1) {
		fq_log(obj->l, FQ_LOG_ERROR, "semop() locking error");
		FQ_TRACE_EXIT(obj->l);
		return(-1);
	}
	fq_log(obj->l, FQ_LOG_DEBUG, "locking OK.");
	FQ_TRACE_EXIT(obj->l);
	return(0);
}

static int on_sem_unlock(semaphore_obj_t *obj)
{
	FQ_TRACE_ENTER(obj->l);

    obj->sb.sem_op = 1;  /* set to allocate resource */
	if (semop(obj->semid, &obj->sb, 1) == -1) {
		fq_log(obj->l, FQ_LOG_ERROR, "semop() un-locking error");
		FQ_TRACE_EXIT(obj->l);
		return(-1);
	}
	fq_log(obj->l, FQ_LOG_DEBUG, "un-locking OK.");
	FQ_TRACE_EXIT(obj->l);
	return(0);
}
