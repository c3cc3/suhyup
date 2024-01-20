/*
** sema_queue.c
*/
#include <sys/ipc.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <pthread.h>
#include <errno.h>

#include "sema_queue.h"
#include "fq_common.h"
#include "fq_logger.h"

extern int errno;

semaqueue_t	*
semaqueue_create(fq_logger_t *l, char *sema_qname)
{
	int i;
	int fd;
	semaqueue_t	*semap;
	pthread_mutexattr_t psharedm;
	pthread_condattr_t psharedc;
	int rc;

	FQ_TRACE_ENTER(l);

	fd = open(sema_qname, O_RDWR | O_CREAT | O_EXCL, 0666);
	if(fd<0) {
		fq_log(l, FQ_LOG_ERROR, "open('%s') error. reason='%s'.", sema_qname, strerror(errno));
		return(NULL);
	}

	rc =  ftruncate(fd, sizeof(semaqueue_t));
	if( rc == -1) {
		fq_log(l, FQ_LOG_ERROR, "ftruncate('%s') error. reason='%s'", sema_qname, strerror(errno));
		close(fd);
		FQ_TRACE_EXIT(l);
		return(NULL);
	}

	(void) pthread_mutexattr_init(&psharedm);
	(void) pthread_mutexattr_setpshared(&psharedm, PTHREAD_PROCESS_SHARED);
	/* deprecated */
	// (void) pthread_mutexattr_setrobust_np(&psharedm, PTHREAD_MUTEX_ROBUST_NP);
	(void) pthread_mutexattr_setrobust(&psharedm, PTHREAD_MUTEX_ROBUST_NP);

	(void) pthread_condattr_init(&psharedc);
	(void) pthread_condattr_setpshared(&psharedc, PTHREAD_PROCESS_SHARED);
	semap = (semaqueue_t *)mmap(NULL, sizeof(semaqueue_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if( semap == MAP_FAILED ) {
		fq_log(l, FQ_LOG_ERROR, "mmap() error. reason='%s'", strerror(errno));
		close(fd);
		FQ_TRACE_EXIT(l);
        return(NULL);
    }
	close(fd);

	(void) pthread_mutex_init( &semap->lock, &psharedm);
	(void) pthread_cond_init( &semap->nonzero_put, &psharedc);
	(void) pthread_cond_init( &semap->nonzero_get, &psharedc);
	semap->count = 0;
	semap->en_total = 0;
	semap->de_total = 0;

	for(i=0; i<MAX_QUEUE_RECORDS; i++)
		semap->Q[i][0] = 0x00;

	FQ_TRACE_EXIT(l);
	return(semap);
}

semaqueue_t *
semaqueue_open(fq_logger_t *l, char *sema_qname)
{
	int fd;
	semaqueue_t	*semap;

	FQ_TRACE_ENTER(l);

	/* For testing, If it does not exist, We always create queue */
	if( !is_file(sema_qname) ) {
		semaqueue_create(l, sema_qname);
	}

	fd = open(sema_qname, O_RDWR, 0666);
	if(fd<0) {
		fq_log(l, FQ_LOG_ERROR, "open('%s') error. reason='%s'", sema_qname, strerror(errno));
		FQ_TRACE_EXIT(l);
		return(NULL);
	}

	semap = (semaqueue_t *)mmap(NULL, sizeof(semaqueue_t), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if( semap == MAP_FAILED ) {
		fq_log(l, FQ_LOG_ERROR, "mmap() error. reason='%s'", strerror(errno));
		close(fd);
		FQ_TRACE_EXIT(l);
        return(NULL);
    }

	close(fd);
	FQ_TRACE_EXIT(l);
	return(semap);
}

void
semaqueue_post(fq_logger_t *l, semaqueue_t *semap, unsigned char *src, unsigned srclen)
{
	int ret;
	struct timespec ts;
	unsigned record_no;
	sigset_t	sigset;

	FQ_TRACE_ENTER(l);

	if( srclen > SEMA_QUEUE_MAX_MSG_LEN) {
		fq_log(l, FQ_LOG_ERROR, "too long message lenth. max is %d. We will skip it.", SEMA_QUEUE_MAX_MSG_LEN);
		FQ_TRACE_EXIT(l);
		return;
	}

	sigemptyset(&sigset);
	sigaddset(&sigset, SIGINT);
	sigaddset(&sigset, SIGQUIT);
	sigaddset(&sigset, SIGPIPE);
	sigaddset(&sigset, SIGSTOP);

re_wait:
	sigprocmask(SIG_BLOCK, &sigset, NULL);

	clock_gettime(CLOCK_REALTIME, &ts);
	ts.tv_sec += 1;

	if( (ret = pthread_mutex_lock(&semap->lock)) == EOWNERDEAD ) {
		// deprecated
		// pthread_mutex_consistent_np( &semap->lock);
		pthread_mutex_consistent( &semap->lock);
	}

	if( semap->count > MAX_QUEUE_RECORDS ) {
		semap->count = MAX_QUEUE_RECORDS;
		fq_log(l, FQ_LOG_ERROR, "Fatal error. semap->count = '%d'", semap->count);
		pthread_mutex_unlock(&semap->lock);
		sigprocmask(SIG_UNBLOCK, &sigset, NULL);
		FQ_TRACE_EXIT(l);
		exit(0);
	}

	if(semap->count == MAX_QUEUE_RECORDS ) {
		int rc;

		rc = pthread_cond_timedwait(&semap->nonzero_put, &semap->lock, &ts);
		if( rc != 0 ) {
			fq_log(l, FQ_LOG_DEBUG, "sema_queue() is normal FULL. There is no empty record in queue.");
			pthread_mutex_unlock(&semap->lock);
			sigprocmask(SIG_UNBLOCK, &sigset, NULL);
			goto re_wait;
		}
		else {
			if( semap->count == MAX_QUEUE_RECORDS ) {
				fq_log(l, FQ_LOG_DEBUG, "FULL(2)..");	
				pthread_mutex_unlock(&semap->lock);
				sigprocmask(SIG_UNBLOCK, &sigset, NULL);
				goto re_wait;
			}
		}
	}
	
	/*
	** enQ
	*/
	record_no = semap->en_total % MAX_QUEUE_RECORDS;
	// sprintf(semap->Q[record_no], "%15d:%s", semap->en_total, src);
	// sprintf(semap->Q[record_no], "%s", src);
	memcpy(semap->Q[record_no], src, srclen);
	
	if(semap->count == 0 ) {
		pthread_cond_signal(&semap->nonzero_get);
	}

	semap->en_total++;
	semap->count++;
	pthread_mutex_unlock(&semap->lock);

	sigprocmask(SIG_UNBLOCK, &sigset, NULL);

	FQ_TRACE_EXIT(l);
	return;
}

void
semaqueue_wait( fq_logger_t *l, semaqueue_t *semap, unsigned char **buf, unsigned *msglen)
{
	int ret;
	struct timespec ts;
	unsigned record_no;
	unsigned char *p=NULL;
	sigset_t	sigset;

	FQ_TRACE_ENTER(l);


	sigemptyset(&sigset);
    sigaddset(&sigset, SIGINT);
    sigaddset(&sigset, SIGQUIT);
    sigaddset(&sigset, SIGPIPE);
    sigaddset(&sigset, SIGSTOP);


re_wait:
	sigprocmask(SIG_BLOCK, &sigset, NULL);

	clock_gettime(CLOCK_REALTIME, &ts);
	ts.tv_sec += 1;

	if( (ret = pthread_mutex_lock(&semap->lock)) == EOWNERDEAD ) {
		// deprecated
		// pthread_mutex_consistent_np(&semap->lock);
		pthread_mutex_consistent(&semap->lock);
	}

	if( semap->count == 0 ) {
		int rc;

		rc = pthread_cond_timedwait( &semap->nonzero_get, &semap->lock, &ts);
		if( rc != 0 ) {
			// printf("Normal TIMEOUT(1). There is no data in queue.\n");
			fq_log(l, FQ_LOG_DEBUG, "Normal TIMEOUT(1). There is no data in queue.\n");
			pthread_mutex_unlock(&semap->lock);
			sigprocmask(SIG_UNBLOCK, &sigset, NULL);
			goto re_wait;
		}
		else {
			if( semap->count == 0 ) {
				// printf("TIMEOUT(2)..\n");
				fq_log(l, FQ_LOG_DEBUG, "TIMEOUT(2). There is no data in queue.\n");
				pthread_mutex_unlock(&semap->lock);
				sigprocmask(SIG_UNBLOCK, &sigset, NULL);
				goto re_wait;
			}
		}
	}

	/*
	** deQ
	*/
	record_no = semap->de_total % MAX_QUEUE_RECORDS;
	*msglen = strlen(semap->Q[record_no]);
	p = calloc( *msglen+1, sizeof(unsigned char));
	memcpy(p, semap->Q[record_no], *msglen);
	*buf = p;

	semap->de_total++;
	semap->count--;
	pthread_cond_signal( &semap->nonzero_put);
	pthread_mutex_unlock( &semap->lock);

	sigprocmask(SIG_UNBLOCK, &sigset, NULL);

	FQ_TRACE_EXIT(l);
	return;
}

void
semaqueue_close(fq_logger_t *l, semaqueue_t *semap)
{
	FQ_TRACE_ENTER(l);
	munmap((void *)semap, sizeof(semaqueue_t));
	FQ_TRACE_EXIT(l);
	return;
}

void 
semaqueue_destroy(fq_logger_t *l, char *sema_qname)
{
	FQ_TRACE_ENTER(l);
	unlink(sema_qname);
	FQ_TRACE_EXIT(l);
}
