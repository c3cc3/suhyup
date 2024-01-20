/* vi: set sw=4 ts=4: */
/*
 * fq_singleton.c
 */
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>

#include "config.h"
#include "fq_common.h"
#include "fq_singleton.h"

#define FQ_SINGLETON_C_VERSION "1.0.0"

/**************************************************************************************
** pthread mutex  attribute initialize functions
*/
static int 
safe_pthread_attr_init_4_lock(fq_logger_t *l, pthread_mutexattr_t *mtx_attr)
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

#ifdef PTHREAD_MUTEX_ROBUST_NP
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

#if 0
/**************************************************************************************
** pthread mutex initialize functions
*/
static int
safe_pthread_mutex_init(fq_logger_t *l, pthread_mutexattr_t *mutex_attr, pthread_mutex_t *mutex)
{
	int rc;

	FQ_TRACE_ENTER(l);
	rc = pthread_mutex_init( mutex, mutex_attr);

	FQ_TRACE_EXIT(l);
	return(rc);
}
#endif

/**************************************************************************************
 * timed locking
 */
static int
safe_pthread_mutex_timedlock_singleton(fq_logger_t *l, singleton_shmlock_t *m, int seconds)
{
	int      rc;
	struct timespec abs_time;
	time_t  current;


	FQ_TRACE_ENTER(l);

	current = time(0);

	if( (m->lock_time + SINGLETON_LOCK_LIMIT_TIME) < current ) {
		pthread_mutexattr_t mutex_attr;
		int		rc;

		fq_log(l, FQ_LOG_ERROR, "singleton lock auto recovery start.");

		rc = safe_pthread_attr_init_4_lock(l, &mutex_attr);
		if( rc < 0 ) {
			fq_log(l, FQ_LOG_DEBUG, "pthread_mutexattr_setpshared() error.");
            return(-2);
        }
		pthread_mutex_init(&m->lock, &mutex_attr);
		m->lock_time = current;

		fq_log(l, FQ_LOG_ERROR, "singleton lock auto recovery end.");
	}

	clock_gettime(CLOCK_REALTIME, &abs_time);
	abs_time.tv_sec += seconds;
	abs_time.tv_nsec = 0;

#ifndef OS_HPUX
	rc = pthread_mutex_timedlock(&m->lock, &abs_time);
#else
	rc = pthread_mutex_lock(&m->lock);
#endif

	if( rc == 0 ) {
		fq_log(l, FQ_LOG_DEBUG, "pthread mutex timed locking sucess.");
		return(1);
	}
	else {
	  switch(rc) {
		case EINVAL:
			fq_log(l, FQ_LOG_ERROR, "timedlock() EINVAL error.");
			break;
		case ETIMEDOUT:
			fq_log(l, FQ_LOG_DEBUG, "ETIMEDOUT. please retry. is not error.");
			return(-99);
		case EAGAIN:
			fq_log(l, FQ_LOG_ERROR, "timedlock() EAGAIN error.");
			break;
		case EDEADLK:
			fq_log(l, FQ_LOG_ERROR, "timedlock() EDEADLK error.");
			break;
		default:
			fq_log(l, FQ_LOG_ERROR, "timedlock() unknown error.");
			break;
	  }
	}
	return(-99);
}
/**************************************************************************************
 * try locking
 */
/* Terminate always in the main task, it can lock up with SIGSTOPped GDB otherwise.  */
#define FQ_LOCK_TIMEOUT 10 /* soconds */

/* Do not use alarm as it would create a ptrace event which would hang up us if
   we are being traced by GDB which we stopped ourselves.  */

static int safe_pthread_mutex_trylock_singleton(fq_logger_t *l, singleton_shmlock_t *m)
{
	int i;
	struct timespec start, now;
	time_t  current;

	FQ_TRACE_ENTER(l);

	current = time(0);

	if( (m->lock_time + SINGLETON_LOCK_LIMIT_TIME) < current ) {
		pthread_mutexattr_t mutex_attr;
		int		rc;

		fq_log(l, FQ_LOG_ERROR, "singleton lock auto recovery start.");

		rc = safe_pthread_attr_init_4_lock(l, &mutex_attr);
		if( rc < 0 ) {
			fq_log(l, FQ_LOG_DEBUG, "pthread_mutexattr_setpshared() error.");
            return(-2);
        }
		pthread_mutex_init(&m->lock, &mutex_attr);

		m->lock_time = current;

		fq_log(l, FQ_LOG_ERROR, "singleton lock auto recovery end.");
	}
	
#ifdef CLOCK_MONOTONIC
	i = clock_gettime (CLOCK_MONOTONIC, &start);
#else
	i = clock_gettime (CLOCK_REALTIME, &start);
#endif
	if(i!=0) {
		fq_log(l, FQ_LOG_ERROR, "clock_gettime()  error.");
		FQ_TRACE_EXIT(l);
		return(-1);
	}

	do
	{
      i = pthread_mutex_trylock (&m->lock);

      if (i == 0) {
		  m->lock_time = time(0);
		  fq_log(l, FQ_LOG_DEBUG, "mutexlocking success.");
		  FQ_TRACE_EXIT(l);
		  return(0); /* success */
	  }

      if( i == EBUSY ) { /* 16 */
		  /* fprintf(stdout, "mutex is currently locked by another thread.\n"); */
	  }

#if 0
	  else {
		fq_log(l, FQ_LOG_ERROR, "mutexlock fatal error. Please check if your application program makes overflow or underflow. obj was crashed.");
		FQ_TRACE_EXIT(l);
		return(-1);
	  }
#endif

#ifdef CLOCK_MONOTONIC
      i = clock_gettime (CLOCK_MONOTONIC, &now);
#else
      i = clock_gettime (CLOCK_REALTIME, &now);
#endif
      if(i != 0) {
		fq_log(l, FQ_LOG_ERROR, "clock_gettime()  error.");
		FQ_TRACE_EXIT(l);
		return(-1);
      }
      if( now.tv_sec <  start.tv_sec) {
		fq_log(l, FQ_LOG_ERROR, "clock_gettime()  invalid value error.");
		FQ_TRACE_EXIT(l);
		return(-2);
	  }
	  usleep(10000);
	}
	while (now.tv_sec - start.tv_sec < FQ_LOCK_TIMEOUT);

	fq_log(l, FQ_LOG_ERROR, "mutex locking timeout (%d) second.", FQ_LOCK_TIMEOUT);

	FQ_TRACE_EXIT(l);
	return(-99); /* timeout */
}

/**************************************************************************************
 * normal robust_np locking
 */
static int 
safe_pthread_mutex_lock_singleton(fq_logger_t *l, singleton_shmlock_t *m)
{
	int ret;
#ifndef EOWNERDEAD
	int rc;
#endif
	time_t  current;

	FQ_TRACE_ENTER(l);

#ifndef EOWNERDEAD 
	rc = safe_pthread_mutex_trylock(l, &m->lock);
	if( rc < 0 ) {
		FQ_TRACE_EXIT(l);
		return(rc);
	}
#else
	current = time(0);

	if( (m->lock_time + SINGLETON_LOCK_LIMIT_TIME) < current ) {
		pthread_mutexattr_t mutex_attr;
		int		rc;

		fq_log(l, FQ_LOG_ERROR, "singleton lock auto recovery start.");

		rc = safe_pthread_attr_init_4_lock(l, &mutex_attr);
		if( rc < 0 ) {
			fq_log(l, FQ_LOG_DEBUG, "pthread_mutexattr_setpshared() error.");
            return(-2);
        }
		pthread_mutex_init(&m->lock, &mutex_attr);
		m->lock_time = current;

		fq_log(l, FQ_LOG_ERROR, "singleton lock auto recovery end.");
	}

	if( (ret=pthread_mutex_lock(&m->lock)) == EOWNERDEAD ) {
		fq_log(l, FQ_LOG_DEBUG, "Owner of lock was deaded.");
		// if( pthread_mutex_consistent_np(&m->lock) < 0 ) {
		if( pthread_mutex_consistent(&m->lock) < 0 ) {
			fq_log(l, FQ_LOG_ERROR,"pthread_mutex_consistent_np() error.");
			FQ_TRACE_EXIT(l);
			return(-1);
		}
		fq_log(l, FQ_LOG_DEBUG, "An Owner of lock was changed. Now You are an owner of lock.");
	}
#endif

	m->lock_time = time(0);
	fq_log(l, FQ_LOG_DEBUG, "safe lock OK.");
	FQ_TRACE_EXIT(l);
	return(0);
}

/**************************************************************************************
 *  un-locking
 */
static int 
safe_pthread_mutex_unlock(fq_logger_t *l, pthread_mutex_t *mutex)
{
	int ret;

	FQ_TRACE_ENTER(l);
	
	ret = pthread_mutex_unlock(mutex);
	fq_log(l, FQ_LOG_DEBUG, "safe unlock OK.");

	FQ_TRACE_EXIT(l);
	return(ret);
}

/**************************************************************************************
 * UNIX Shared Memory Libraries for monotoring
 */
int 
create_unixShm_4_lock( fq_logger_t *l, size_t size, char key_char)
{
	int shm_id;
	key_t	shmkey;

	FQ_TRACE_ENTER(l);

	/* #define FQ_SINGLETON_KEY_PATH "/tmp" */
	shmkey = ftok(FQ_SINGLETON_KEY_PATH, key_char);

	if ((shm_id = shmget(shmkey, size, IPC_CREAT|IPC_EXCL|0666 )) == -1) {
		if( errno == EEXIST ) {
			fq_log(l, FQ_LOG_DEBUG, "Shared memory key already exist.\n");
			FQ_TRACE_EXIT(l);
			return(0);
		}
		else {
			fq_log(l, FQ_LOG_ERROR, "shmget() failed. reason=[%s]", strerror(errno));
			FQ_TRACE_EXIT(l);
			return(-1);
		}
	}
	else { /* new creating success */
		pthread_mutexattr_t mutex_attr;
		singleton_shmlock_t *m=NULL;
		void *shm_p=NULL;
		int		rc;

		fq_log(l, FQ_LOG_DEBUG, "new shm_id is [%d]\n", shm_id);
		shm_p = shmat(shm_id, NULL, 0);
		if(!shm_p) {
			fq_log(l, FQ_LOG_DEBUG, "shmat() error.");
			FQ_TRACE_EXIT(l);
			return(-1);
		}
		m = (singleton_shmlock_t *)shm_p;

#if 0
		pthread_mutexattr_init(&mutex_attr);
		
		if (pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED) != 0) {
			fq_log(l, FQ_LOG_DEBUG, "pthread_mutexattr_setpshared() error.");
			return(-2);
		}
#else
		rc = safe_pthread_attr_init_4_lock(l, &mutex_attr);
		if( rc < 0 ) {
			fq_log(l, FQ_LOG_DEBUG, "pthread_mutexattr_setpshared() error.");
			FQ_TRACE_EXIT(l);
            return(-2);
        }
#endif
		pthread_mutex_init(&m->lock, &mutex_attr);

		shmdt(shm_p);

		FQ_TRACE_EXIT(l);
		return(0);
	}
}

int 
SAFE_recovery_singleton( fq_logger_t *l, unsigned char key)
{
	int shm_id;
	key_t	shmkey;
	void	*shm_p=NULL;

	FQ_TRACE_ENTER(l);

	shmkey = ftok(FQ_SINGLETON_KEY_PATH, key);

	if ((shm_id = shmget(shmkey, sizeof(singleton_shmlock_t),  0)) == -1) {
		fq_log(l, FQ_LOG_ERROR, "shmget() error. reason=[%s]", strerror(errno));
        FQ_TRACE_EXIT(l);
		return(-1);
	}

	if ((shm_p = shmat( shm_id, (void *)0, 0)) == (void *)-1 ) {
		fq_log(l, FQ_LOG_ERROR, "shmat() error. reason=[%s]", strerror(errno));
		FQ_TRACE_EXIT(l);
		return(-2);
	}
	else { /* open success */
		pthread_mutexattr_t mutex_attr;
		singleton_shmlock_t *m=NULL;
		int		rc;

		m = (singleton_shmlock_t *)shm_p;

		rc = safe_pthread_attr_init_4_lock(l, &mutex_attr);
		if( rc < 0 ) {
			fq_log(l, FQ_LOG_DEBUG, "pthread_mutexattr_setpshared() error.");
            return(-3);
        }
		pthread_mutex_init(&m->lock, &mutex_attr);

		m->lock_time = time(0);

		shmdt(shm_p);

		FQ_TRACE_EXIT(l);
		return(0);
	}
}

int get_stat_unixShm_4_lock( fq_logger_t *l, int shm_id, struct shmid_ds *buf)
{
	int rc;

	FQ_TRACE_ENTER(l);
	rc =  shmctl(shm_id, IPC_STAT, buf);
	if( rc < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "shmctl(IPC_STAT) error. reason=[%s]", strerror(errno));
	}

	FQ_TRACE_EXIT(l);
	return(rc);
}

int unlink_unixShm_4_lock( fq_logger_t *l, size_t size, char key_char )
{
	key_t shmkey;
	int	shm_id;

	FQ_TRACE_ENTER(l);

	shmkey=ftok(FQ_SINGLETON_KEY_PATH, key_char);
	if ((shm_id = shmget(shmkey, size, 0)) == -1) {
		fq_log(l, FQ_LOG_ERROR, "shmget() error. reason=[%s]", strerror(errno));
		FQ_TRACE_EXIT(l);
		return(-1);
	}

	if (shmctl(shm_id, IPC_RMID, NULL) == -1) {
		fq_log(l, FQ_LOG_ERROR, "shmctl( IPC_RMID ) error. reason=[%s]", strerror(errno));
		FQ_TRACE_EXIT(l);
		return(-2);
	}

	FQ_TRACE_EXIT(l);
	return(0);
}

#if 0
SYNOPSIS
       #include <sys/types.h>
       #include <sys/shm.h>

       void *shmat(int shmid, const void *shmaddr, int shmflg);

       int shmdt(const void *shmaddr);

#endif

void *open_unixShm_4_lock( fq_logger_t *l, size_t size, char key_char)
{
	int shm_id;
	key_t	shmkey;
	void	*shm_p;

	FQ_TRACE_ENTER(l);
	shmkey = ftok(FQ_SINGLETON_KEY_PATH, key_char);

	if ((shm_id = shmget(shmkey, size,  0)) == -1) {
		fq_log(l, FQ_LOG_ERROR, "shmget() error. reason=[%s]", strerror(errno));
        FQ_TRACE_EXIT(l);
		return((void *)0);
	}
	if ((shm_p = shmat( shm_id, (void *)0, 0)) == (void *)-1 ) {
		fq_log(l, FQ_LOG_ERROR, "shmat() error. reason=[%s]", strerror(errno));
		FQ_TRACE_EXIT(l);
		return((void *)0);
	}

	FQ_TRACE_EXIT(l);
	return(shm_p);
}

int SAFE_close_singleton(fq_logger_t *l, void *p)
{
	int rc;

	FQ_TRACE_ENTER(l);

	if( (rc=shmdt(p)) < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "shmde() error. reason=[%s]", strerror(errno));
		FQ_TRACE_EXIT(l);
		return(rc);
	}
	p = NULL;
	FQ_TRACE_EXIT(l);
	return(rc);
}

int SAFE_lock_singleton( fq_logger_t *l, int lock_kind, singleton_shmlock_t *m)
{
	int rc=-1;

	FQ_TRACE_ENTER(l);

	switch(lock_kind) {
		case FQ_ROBUST_NP_LOCK:
			rc = safe_pthread_mutex_lock_singleton(l, m);
			break;
		case FQ_TIMED_LOCK:
			rc = safe_pthread_mutex_timedlock_singleton(l, m, 5);
			break;
		case FQ_TRY_LOCK:
			rc = safe_pthread_mutex_trylock_singleton(l, m);
			break;
		default:
			fq_log(l, FQ_LOG_ERROR, "kind of locking error.\n");
			break;
	}
	FQ_TRACE_EXIT(l);
	return(rc);
}
int 
SAFE_unlock_singleton(fq_logger_t *l, singleton_shmlock_t *m)
{
	int rc=-1;

	FQ_TRACE_ENTER(l);

	rc = safe_pthread_mutex_unlock(l, &m->lock);
	
	FQ_TRACE_EXIT(l);
    return(rc);
}

singleton_shmlock_t *
READY_singleton(fq_logger_t *l, unsigned char key)
{
	int rc;
	void *shm_p=NULL;
	singleton_shmlock_t *m=NULL;

	FQ_TRACE_ENTER(l);

	rc = create_unixShm_4_lock( l, sizeof(singleton_shmlock_t), key);
	if( rc < 0 ) {
		fq_log(l, FQ_LOG_ERROR,"create_unixShm_4_lock() error.");
		return(NULL);
	}

	shm_p = open_unixShm_4_lock(l, sizeof(singleton_shmlock_t), key);
	if( !shm_p ) {
		fq_log(l, FQ_LOG_ERROR,"open_unixShm_4_lock() error.");
		return(NULL);
	}
		
	m = (singleton_shmlock_t *)shm_p;

	FQ_TRACE_EXIT(l);

	return(m);
}

int DELETE_singleton(fq_logger_t *l, unsigned char key)
{
	int rc;

	FQ_TRACE_ENTER(l);
	
	rc = unlink_unixShm_4_lock( l, sizeof(singleton_shmlock_t),  key);
	if( rc < 0 ) {
		fq_log(l, FQ_LOG_ERROR,"unlink_unixShm_4_lock() error.");
	}	

	FQ_TRACE_EXIT(l);
	return(rc);
}

int getLockedTime_singletone(singleton_shmlock_t *m)
{
	return(m->lock_time);
}

#if 0
int main(int ac, char **av)
{
	int rc;
	fq_logger_t *l=NULL;
	singleton_shmlock_t *m=NULL;
	unsigned char key='k';

	m = READY_singleton(l, key);
	CHECK( m );

	rc =  SAFE_lock_singleton(l, FQ_ROBUST_NP_LOCK, m);
#if 0
	rc = SAFE_lock_singleton(l, FQ_TIMED_LOCK, m);
	rc = SAFE_lock_singleton(l, FQ_TRY_LOCK, m);
#endif
	CHECK( rc >= 0 );

	printf("locking OK. for unlocking, press any key...rc=[%d]\n", rc);
	getchar();

	rc = SAFE_unlock_singleton(l, m);
	CHECK( rc >= 0 );

	printf("OK\n");

	rc = SAFE_close_singleton(l, m);
	CHECK( rc >= 0 );

	rc = DELETE_singleton(l);
	CHECK( rc >= 0 );
	
	return(0);
}
#endif
