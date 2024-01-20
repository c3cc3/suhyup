/*
 * Warning.
 * This source is licensed by Entrust.Co
 * Auther: gwisang.choi@gmail.com
 *
 * fq_pthread_mutex_lock.c
 *
 */
#include"config.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <math.h>

#include "fq_common.h"
#include "fq_logger.h"
#include "fq_pthread_mutex_lock.h"

#define FQ_PTHREAD_MUTEX_LOCK_C_VERSION "1.0.0"

static int 
safe_pthread_attr_init(fq_logger_t *l, pthread_mutexattr_t *mtx_attr)
{
	int ret;

	FQ_TRACE_ENTER(l);

	pthread_mutexattr_init( mtx_attr );
	ret = pthread_mutexattr_setpshared(mtx_attr, PTHREAD_PROCESS_SHARED);
	if (ret) {
		fq_log(l, FQ_LOG_ERROR, "pthread_mutexattr_setpshared() failed.(errno=%d).", errno);
		FQ_TRACE_EXIT(l);
		return ret;
	}

#ifdef HAVE_PTHREAD_MUTEX_CONSISTENT_NP
	ret = pthread_mutexattr_setrobust_np(mtx_attr, PTHREAD_MUTEX_ROBUST_NP);
	if( ret) {
		fq_log(l, FQ_LOG_ERROR, "pthread_mutexattr_setrobust_np() failed.(errno=%d)", errno);
		FQ_TRACE_EXIT(l);
		return ret;
	}
#endif

	FQ_TRACE_EXIT(l);
	return(0);
}

#ifndef HAVE_PTHREAD_MUTEX_CONSISTENT_NP 

#define FQ_FORCE_UNLOCK_TIME 10
static int 
on_mutex_lock(fq_logger_t *l, shmlock_obj_t *obj)
{
	int i;
	int recovery_flag = 0;
	time_t current;
	stopwatch_micro_t p;
	long sec=0L, usec=0L, total_micro_sec=0L;

	FQ_TRACE_ENTER(l);

	on_stopwatch_micro(&p);

retry:
	for(i=0; i<5; i++) {
		int rc;
		rc = pthread_mutex_trylock (&obj->p->mtx);
		if(rc == 0) { /* success */
			obj->p->lock_time = time(0);

			off_stopwatch_micro(&p);
			get_stopwatch_micro(&p, &sec, &usec);
			total_micro_sec = sec * 1000000;
			total_micro_sec =  total_micro_sec + usec;
			if( total_micro_sec > 4500000) {
				fq_log(l, FQ_LOG_EMERG, "safe_pthread_mutex_lock() delay time : %ld micro second.", total_micro_sec);
			}
			FQ_TRACE_EXIT(l);
			return(TRUE);
		}
		else {
			usleep(i*10000);
			continue;
		}
	}

	current = time(0);

    if( (obj->p->lock_time + FQ_FORCE_UNLOCK_TIME) < current ) {
        pthread_mutexattr_t mutex_attr;
        int     rc;

        fq_log(l, FQ_LOG_ERROR, "singleton lock auto recovery start. new");

		/* 2012-12-15 */
		rc = pthread_mutex_destroy(&obj->p->mtx);
		if( rc < 0 ) {
            fq_log(l, FQ_LOG_ERROR, "pthread_mutex_destroy() error.");
            FQ_TRACE_EXIT(l);
            return(FALSE);
        }

		rc = pthread_mutexattr_init( &mutex_attr );
        if( rc < 0 ) {
            fq_log(l, FQ_LOG_ERROR, "pthread_mutexattr_init() error.");
			FQ_TRACE_EXIT(l);
            return(FALSE);
        }

		rc = pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
        if( rc < 0 ) {
            fq_log(l, FQ_LOG_ERROR, "pthread_mutexattr_setpshared() error.");
			FQ_TRACE_EXIT(l);
            return(FALSE);
        }
        rc = pthread_mutex_init(&obj->p->mtx, &mutex_attr);

        if( rc < 0 ) {
            fq_log(l, FQ_LOG_ERROR, "pthread_mutexattr_setpshared() error.");
			FQ_TRACE_EXIT(l);
            return(FALSE);
		}
        fq_log(l, FQ_LOG_ERROR, "singleton lock auto recovery end. new. recovery_flag=[%d]", recovery_flag);

		rc = pthread_mutex_trylock (&obj->p->mtx);

		if(rc == 0) {/* success */
			obj->p->lock_time = time(0);

			off_stopwatch_micro(&p);
			get_stopwatch_micro(&p, &sec, &usec);
			total_micro_sec = sec * 1000000;
			total_micro_sec =  total_micro_sec + usec;
			if( total_micro_sec > 1000000) {
				fq_log(l, FQ_LOG_EMERG, "safe_pthread_mutex_lock() delay time : %ld micro second.", total_micro_sec);
			}
			FQ_TRACE_EXIT(l);
			return(TRUE);
		}
    }

	goto retry;
}

#else
static int 
on_mutex_lock(fq_logger_t *l,  shmlock_obj_t *obj)
{
	int ret;

	FQ_TRACE_ENTER(l);

	if( (ret=pthread_mutex_lock(&obj->p->mtx)) == EOWNERDEAD ) {
		fq_log(l, FQ_LOG_DEBUG, "Owner of lock was deaded.");
		if( pthread_mutex_consistent_np(&obj->p->mtx) < 0 ) {
			fq_log(l, FQ_LOG_ERROR,"pthread_mutex_consistent_np() error.");
			FQ_TRACE_EXIT(l);
			return(FALSE);
		}
		fq_log(l, FQ_LOG_DEBUG, "An Owner of lock was changed. Now You are an owner of lock.");
	}

	obj->p->lock_time = time(0);

	fq_log(l, FQ_LOG_DEBUG, "safe lock OK.");
	FQ_TRACE_EXIT(l);
	return(TRUE);
}
#endif

static int 
on_mutex_unlock(fq_logger_t *l, shmlock_obj_t *obj)
{
	FQ_TRACE_ENTER(l);
	
	pthread_mutex_unlock(&obj->p->mtx);
	fq_log(l, FQ_LOG_DEBUG, "safe unlock OK.");

	FQ_TRACE_EXIT(l);
	return(TRUE);
}

int close_shmlock_obj(fq_logger_t *l, shmlock_obj_t **obj)
{
	FQ_TRACE_ENTER(l);

    SAFE_FREE( (*obj)->name );

	/* munlock((*obj)->p, sizeof(shm_lock_struct_t)); */
	munmap((void *)(*obj)->p, sizeof(shm_lock_struct_t));
	SAFE_FD_CLOSE( (*obj)->fd );

    SAFE_FREE( (*obj) );

    FQ_TRACE_EXIT(l);
    return(TRUE);
}

int unlink_shmlock_obj(fq_logger_t *l, char *name)
{
	FQ_TRACE_ENTER(l);

	shm_unlink(name);

	FQ_TRACE_EXIT(l);
	return(TRUE);
}

#ifndef PSHMNAMELENGTH
#define PSHMNAMELENGTH 64
#endif

int open_shmlock_obj( fq_logger_t *l, char *name, shmlock_obj_t **obj)
{
	char	shm_name[PSHMNAMELENGTH+1];
	shmlock_obj_t *rc=NULL;
	FQ_TRACE_ENTER(l);


	if( !HASVALUE(name) )  {
        fq_log(l, FQ_LOG_ERROR, "illgal function call.");
        FQ_TRACE_EXIT(l);
        return(FALSE);
    }

	if( strlen(name) > PSHMNAMELENGTH ) {
        fq_log(l, FQ_LOG_ERROR, "illgal function call.(name='%s')len=%d, The maximum length for a filename currently is [%d] chars", name, strlen(name), PSHMNAMELENGTH);
        FQ_TRACE_EXIT(l);
        return(FALSE);
    }

	if (name[0] == '\0' || strchr (name, '/') != NULL) {
        fq_log(l, FQ_LOG_ERROR, "illgal shared memory name rule. Shared memory name can't have a '/ ' character.");
        FQ_TRACE_EXIT(l);
        return(FALSE);
    }

	sprintf(shm_name, "/%s", name);

	rc  = (shmlock_obj_t *)malloc(sizeof(shmlock_obj_t));

	if(rc) {
		rc->name = strdup(shm_name);
	
		rc->l = l;
        rc->on_mutex_lock = on_mutex_lock;
        rc->on_mutex_unlock = on_mutex_unlock;

		/*
		** Notice!
		** It makes a file under /dev/shm/name and /var/run/shm
		** mout | grep shm -> tmpfs ( rw,nosuid,nodev,relatime ) 
		*/
		if((rc->fd = shm_open(rc->name, O_RDWR, 0666 )) < 0 ) { /* not exist */
			int ret;

			if((rc->fd = shm_open(rc->name, O_CREAT|O_EXCL|O_TRUNC|O_RDWR, 0777 )) < 0 ) {
				fq_log(l, FQ_LOG_ERROR, "shm_open(CREATE) error. (reason=%s)",strerror(errno));
				goto return_FALSE;
			}

			if( ftruncate(rc->fd, sizeof(shm_lock_struct_t) ) < 0 ) {
				fq_log(l, FQ_LOG_ERROR, "ftruncate() error. (reason=%s)", strerror(errno));
				goto return_FALSE;
			}

			if( lseek(rc->fd, 0, SEEK_SET) < 0 ) {
				fq_log(l, FQ_LOG_ERROR, "lseek() error. reason=[%s]", strerror(errno));
				goto return_FALSE;
			}

			rc->p = (void *)mmap(NULL, sizeof(shm_lock_struct_t) , PROT_READ|PROT_WRITE, MAP_SHARED, rc->fd, 0);
			if( rc->p == MAP_FAILED) {
				fq_log(l, FQ_LOG_ERROR, "Shared lock file mmap error! reason=[%s].", strerror(errno));
				goto return_FALSE;
			}
	
			/*
			if(mlock(rc->p, sizeof(shm_lock_struct_t)) != 0){
				fq_log(l, FQ_LOG_ERROR, "mlock()! reason=[%s].", strerror(errno));
				goto return_FALSE;
			}
			*/
			
			if( safe_pthread_attr_init(l, &rc->mtx_attr) != 0 ) {
				fq_log(l, FQ_LOG_ERROR, "safe_pthread_attr_init() error. reason=[%s]\n", strerror(errno));
				goto return_FALSE;
			}

			pthread_mutex_init(&rc->p->mtx, &rc->mtx_attr);

			pthread_condattr_init(&rc->cond_attr);
			ret = pthread_condattr_setpshared(&rc->cond_attr, PTHREAD_PROCESS_SHARED);
			if (ret) {
				fq_log(l, FQ_LOG_ERROR, "pthread_mutexattr_setpshared() failed. (errno=%d)\n", errno);
				goto return_FALSE;
			}

			if( pthread_cond_init(&rc->p->cond, &rc->cond_attr) < 0 ) {
				fq_log(l, FQ_LOG_ERROR, "pthread_cond_init() error. reason=[%s]", strerror(errno));
				goto return_FALSE;
			}

			rc->p->lock_time = time(0);

			*obj = rc;

			FQ_TRACE_EXIT(l);
			return(TRUE);
		}
		else { /* open success */
			rc->p = NULL;
			rc->p = (void *)mmap(NULL, sizeof(shm_lock_struct_t) , PROT_READ|PROT_WRITE, MAP_SHARED, rc->fd, 0);
			if( rc->p == MAP_FAILED) {
				fq_log(l, FQ_LOG_ERROR, "Shared lock file mmap error! reason=[%s].", strerror(errno));
				goto return_FALSE;
			}
			*obj = rc;

			FQ_TRACE_EXIT(l);
			return(TRUE);
		}

	}
return_FALSE:

	if(rc->p) {
		munmap((void *)rc->p, sizeof(shm_lock_struct_t));
		rc->p = (void *)0;
	}

	SAFE_FREE(rc->name);
	SAFE_FREE(rc);

	FQ_TRACE_EXIT(l);
    return(FALSE);
}
