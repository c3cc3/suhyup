/* vi: set sw=4 ts=4: */
/* 
 * fq_multi_queue.c
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
#include "fqueue.h"
#include "fq_multi_queue.h"

#define FQ_MULTI_QUEUE_C_VERSION "1.0.0"

static fqueue_obj_t *on_open_queue( Queues_obj_t *Qs, char **argv, char *hostname, char *distname,  char *monitoring_ID, char *path, char *qname, int Qindex);
static int on_close_queue( Queues_obj_t *Qs, int Qindex);
static fqueue_obj_t *on_get_fqueue_obj_with_index( Queues_obj_t *Qs, int Qindex  );
static fqueue_obj_t *on_get_fqueue_obj_with_qname( Queues_obj_t *Qs, char *path, char *qnaeme  );

static fqueue_obj_t *on_open_queue( Queues_obj_t *Qs, char **argv, char *hostname, char *distname,  char *monitoring_ID, char *path, char *qname, int Qindex)
{
	int rc;

	FQ_TRACE_ENTER(Qs->l);

	pthread_mutex_lock( &Qs->mutex);

	if( Qindex > Qs->alloced_queue_count ) {
		fq_log(Qs->l, FQ_LOG_ERROR, "illegal requeust. Qindex=[%d]", Qindex);
		FQ_TRACE_EXIT(Qs->l);
		pthread_mutex_unlock( &Qs->mutex);
		return(NULL);
	}

	if( Qs-> opened_queue_count >= Qs->alloced_queue_count) {
		fq_log(Qs->l, FQ_LOG_ERROR, "Can't open any more...");
		FQ_TRACE_EXIT(Qs->l);
		pthread_mutex_unlock( &Qs->mutex);
		return(NULL);
	}

	/* check where duplicated open */
	if( Qs->mobj[Qindex] ) {
		fq_log(Qs->l, FQ_LOG_ERROR, "Already [%d]-index is opened.", Qindex);
        pthread_mutex_unlock( &Qs->mutex);
		FQ_TRACE_EXIT(Qs->l);
        return(NULL);
    }

	rc = OpenFileQueue(Qs->l, argv,  hostname, distname,  monitoring_ID, path, qname, &Qs->mobj[Qindex]);
	if(rc == FALSE) {
		fq_log( Qs->l, FQ_LOG_ERROR, "OpenFileQueue('%s', '%s') error.", path, qname);
		FQ_TRACE_EXIT(Qs->l);
		pthread_mutex_unlock( &Qs->mutex);
		return(NULL);
	}
	Qs->opened_queue_count++;

	FQ_TRACE_EXIT(Qs->l);
	pthread_mutex_unlock( &Qs->mutex);
	return(Qs->mobj[Qindex]);
}

static fqueue_obj_t *on_get_fqueue_obj_with_index( Queues_obj_t *Qs, int Qindex  )
{
	
	if( (Qindex > Qs->alloced_queue_count) ||  (Qindex < 0 ) ) {
		fq_log( Qs->l, FQ_LOG_ERROR, "illegal request. There is no queue pointer in [%d]-th index.", Qindex);
		return( NULL);
	}
	return( Qs->mobj[Qindex]);
}

static fqueue_obj_t *on_get_fqueue_obj_with_qname( Queues_obj_t *Qs, char *path, char *qname  )
{
	int i;

	for(i=0; i<Qs->opened_queue_count;i++) {
		if( strcmp( Qs->mobj[i]->path, path) == 0  &&  strcmp(Qs->mobj[i]->qname, qname) == 0 ) {
			return( Qs->mobj[i]); /* found */
		}
	}
	return(NULL); /* not found */
}

static int on_close_queue( Queues_obj_t *Qs, int Qindex)
{

	FQ_TRACE_ENTER(Qs->l);

	pthread_mutex_lock( &Qs->mutex);

	CloseFileQueue(Qs->l, &Qs->mobj[Qindex]);

	Qs->opened_queue_count--;

	pthread_mutex_unlock( &Qs->mutex);

	FQ_TRACE_EXIT(Qs->l);

	return(TRUE);
}

int new_Queues_obj(fq_logger_t *l, Queues_obj_t **Qs, int alloc_queue_count)
{
	Queues_obj_t *rc=NULL;

	FQ_TRACE_ENTER(l);

	rc = (Queues_obj_t *)malloc(sizeof(Queues_obj_t));
	if( rc ) {
		int i;
		pthread_mutex_init(&rc->mutex, NULL);

		for(i=0; i<alloc_queue_count; i++) {
			rc->mobj = calloc(1, sizeof(fqueue_obj_t));
			rc->mobj++;
		}
		rc->alloced_queue_count = alloc_queue_count;
		rc->opened_queue_count=0;
		rc->on_open_queue = on_open_queue;
		rc->on_get_fqueue_obj_with_index = on_get_fqueue_obj_with_index;
		rc->on_get_fqueue_obj_with_qname = on_get_fqueue_obj_with_qname;
		rc->on_close_queue = on_close_queue;
		
		rc->l = l;
		*Qs = rc;

		FQ_TRACE_EXIT(l);
		return(TRUE);
	}

	FREE(*Qs);
	FQ_TRACE_EXIT(l);
	return(FALSE);
}
void free_Queues_obj(fq_logger_t *l, Queues_obj_t **Qs)
{
	FQ_TRACE_ENTER(l);

	FREE(*Qs);

	FQ_TRACE_EXIT(l);
	return;
}
