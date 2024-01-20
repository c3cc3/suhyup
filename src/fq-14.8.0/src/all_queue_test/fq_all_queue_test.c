#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <curses.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

#include "fqueue.h"
#include "shm_queue.h"
#include "fq_socket.h"
#include "fq_file_list.h"
#include "fq_manage.h"
#include "parson.h"
#include "fq_external_env.h"
#include "fq_hashobj.h"
#include "my_conf.h"
#include "ums_common_conf.h"

static void *producer_thread( void *arg );
static void *consumer_thread( void *arg );
static int uchar_enQ(fq_logger_t *l, unsigned char *data, size_t data_len, fqueue_obj_t *obj);
static int takeout_uchar_msg_from_Q_XA(fq_logger_t *l, fqueue_obj_t *deq_obj, unsigned char **msg, char **uf, long *p_seq);
static int takeout_uchar_msg_from_Q(fq_logger_t *l, fqueue_obj_t *deq_obj, unsigned char **msg, char **uf, long *p_seq);
static void commit_Q( fq_logger_t *l, fqueue_obj_t *deq_obj, char *unlink_filename, long l_seq);

typedef struct thread_param{
	fqueue_list_t *node;
	fq_logger_t *l;
} thread_param_t;

int main(int ac, char **av)
{
	char *fq_data_home = NULL;

	fq_data_home = getenv("FQ_DATA_HOME");
	CHECK(fq_data_home);

	ums_common_conf_t *cm_conf = NULL;
	fq_mon_svr_conf_t   *my_conf=NULL;

	if( ac != 2 ) {
		printf("Usage: $ %s [your config file] <enter>\n", av[0]);
		printf("ex   : $ %s my.conf <enter>\n", av[0]);
		return 0;
	}

	char *errmsg = NULL;
	if(Load_ums_common_conf(&cm_conf, &errmsg) == false) {
		printf("Load_ums_common_conf() error. reason='%s'\n", errmsg);
		return(-1);
	}	

	if(LoadConf(av[1], &my_conf, &errmsg) == false) {
		printf("LoadMyConf('%s') error. reason='%s'\n", av[1], errmsg);
		return(-1);
	}

	char log_pathfile[256];
	fq_logger_t *l = NULL;
	sprintf(log_pathfile, "%s/%s", cm_conf->ums_common_log_path, my_conf->log_filename);

	int rc;
	rc = fq_open_file_logger(&l, log_pathfile, my_conf->i_log_level);
	CHECK(rc==TRUE);
	printf("log file: '%s'\n", log_pathfile);

	linkedlist_t *ll = linkedlist_new("file queue linkedlist.");
	bool tf = MakeLinkedList_filequeues(l, ll);
	CHECK(tf);
	printf("Total File queues: %d.\n", ll->length);


	ll_node_t *p;
	for(p=ll->head; p!=NULL; p=p->next) {
		thread_param_t *tp = NULL;
		pthread_t producer_id;
		pthread_t consumer_id;

		fqueue_list_t *tmp=NULL;
		tmp = (fqueue_list_t *) p->value;

		printf("path = [%s]\n", tmp->qpath);
		printf("name = [%s]\n", tmp->qname);

		tp = calloc(1, sizeof(thread_param_t));
		CHECK(tp);
		tp->node = tmp;
		tp->l = l;

		pthread_create( &producer_id, NULL, producer_thread, tp);
		pthread_join(producer_id, NULL);

		pthread_create( &consumer_id, NULL, consumer_thread, tp);
		pthread_join(consumer_id, NULL);
	
		free(tp);
		usleep(1000);
	}
	printf("Congraturation !!!!\n");
	printf("All testing end. Number of queues = [%d]\n", ll->length);
			
	if(ll) linkedlist_free(&ll);

	Free_ums_common_conf(cm_conf);
	FreeConf(my_conf);

	fq_close_file_logger(&l);

	return 0;
}

static void *producer_thread( void *arg )
{
	thread_param_t    *tp = arg;
	fqueue_obj_t	*fqobj = NULL;

	char *p=NULL;
	printf("produce thread : [%s][%s] .\n", tp->node->qpath, tp->node->qname);
	int rc;

	if( (p= strstr(tp->node->qname, "SHM_")) == NULL ) {
		rc =  OpenFileQueue(tp->l, NULL, NULL, NULL, NULL, tp->node->qpath, tp->node->qname, &fqobj);
	}
	else {
		rc =  OpenShmQueue(tp->l, tp->node->qpath, tp->node->qname, &fqobj);
	}
	CHECK(rc == TRUE);

	unsigned char msg[512];
	sprintf(msg, "-----------> Hey %s!", tp->node->qname);
	rc = uchar_enQ(tp->l, msg , sizeof(msg), fqobj);
	CHECK( rc > 0 );
	printf("enQ success.\n");

	CloseFileQueue(tp->l, &fqobj);
	printf("exit producer.\n");
	pthread_exit(0);
}

static void *consumer_thread( void *arg )
{
	thread_param_t    *tp = arg;
	fqueue_obj_t	*fqobj = NULL;

	printf("consumer thread : [%s][%s] .\n", tp->node->qpath, tp->node->qname);

	int rc;
	char *p=NULL;
	unsigned char *msg = NULL;
	char    *unlink_filename = NULL;
	long    l_seq;

	if( (p= strstr(tp->node->qname, "SHM_")) == NULL ) {
		rc =  OpenFileQueue(tp->l, NULL, NULL, NULL, NULL, tp->node->qpath, tp->node->qname, &fqobj);
		CHECK(rc == TRUE);
		printf("File Queue Open success.\n");

		rc = takeout_uchar_msg_from_Q_XA(tp->l, fqobj, &msg, &unlink_filename, &l_seq);
		CHECK(rc > 0 );

		printf("File Queue takeout success.\n");
		commit_Q(tp->l, fqobj, unlink_filename, l_seq);
		printf("File Queue commit success.\n");
			
	}
	else {
		printf("This is a shared memory queue.\n");
		rc =  OpenShmQueue(tp->l, tp->node->qpath, tp->node->qname, &fqobj);
		CHECK(rc == TRUE);
		printf("Shared Memory Queue Open success.\n");

		rc = takeout_uchar_msg_from_Q(tp->l, fqobj, &msg, &unlink_filename, &l_seq);
		CHECK(rc > 0 );
		printf("Shared Memory Queue takeout success.\n");
	}
	if(msg) printf("msg = [%s]\n", msg);

	SAFE_FREE(msg);

	CloseFileQueue(tp->l, &fqobj);

	printf("exit consumer.\n");
	pthread_exit(0);
}

static int uchar_enQ(fq_logger_t *l, unsigned char *data, size_t data_len, fqueue_obj_t *obj)
{
	int rc;
	long l_seq=0L;
    long run_time=0L;

	while(1) {
		rc =  obj->on_en(l, obj, EN_NORMAL_MODE, (unsigned char *)data, data_len+1, data_len, &l_seq, &run_time );
		// rc =  obj->on_en(l, obj, EN_CIRCULAR_MODE, (unsigned char *)data, data_len+1, data_len, &l_seq, &run_time );
		if( rc == EQ_FULL_RETURN_VALUE )  {
			fq_log(l, FQ_LOG_EMERG, "Queue('%s', '%s') is full.\n", obj->path, obj->qname);
			usleep(100000);
			continue;
		}
		else if( rc < 0 ) { 
			if( rc == MANAGER_STOP_RETURN_VALUE ) { // -99
				printf("Manager stop!!! rc=[%d]\n", rc);
				fq_log(l, FQ_LOG_EMERG, "[%s] queue is Manager stop.", obj->qname);
				sleep(1);
				continue;
			}
			fq_log(l, FQ_LOG_ERROR, "Queue('%s', '%s'): on_en() error.\n", obj->path, obj->qname);
			return(rc);
		}
		else {
			fq_log(l, FQ_LOG_INFO, "enqueued ums_msg len(rc) = [%d].", rc);
			break;
		}
	}
	return(rc);
}
static int takeout_uchar_msg_from_Q_XA(fq_logger_t *l, fqueue_obj_t *deq_obj, unsigned char **msg, char **uf, long *p_seq)
{
	long l_seq=0L;
	unsigned char *buf=NULL;
	long run_time=0L;
	int cnt_rc;
	char unlink_filename[256];
	size_t buffer_size = (65536*10);
	
	// buffer_size = deq_obj->h_obj->h->msglen;
	// buffer_size = sizeof(ums_msg_t)+1;
	buf = calloc(buffer_size, sizeof(unsigned char));

	int rc;
	rc = deq_obj->on_de_XA(l, deq_obj, buf, buffer_size, &l_seq, &run_time, unlink_filename);
	if( rc == DQ_EMPTY_RETURN_VALUE || rc < 0 ) {
		SAFE_FREE(buf);
	}
	else {
		fq_log(l, FQ_LOG_DEBUG, "deQ msg:'%s'.", buf);
		fq_log(l, FQ_LOG_DEBUG, "deQ msglen:'%d'.", rc);
		*msg = (unsigned char *)buf;
		*p_seq = l_seq;

		*uf = strdup(unlink_filename);
		// SAFE_FREE(buf);
	}

	return rc;
}
static void commit_Q( fq_logger_t *l, fqueue_obj_t *deq_obj, char *unlink_filename, long l_seq)
{
	long run_time=0L;
	deq_obj->on_commit_XA(l, deq_obj, l_seq, &run_time, unlink_filename);
	
	SAFE_FREE(unlink_filename);
	return;
}

static int takeout_uchar_msg_from_Q(fq_logger_t *l, fqueue_obj_t *deq_obj, unsigned char **msg, char **uf, long *p_seq)
{
	long l_seq=0L;
	unsigned char *buf=NULL;
	long run_time=0L;
	int cnt_rc;
	char unlink_filename[256];
	size_t buffer_size;
	
	buffer_size = deq_obj->h_obj->h->msglen;
	// buffer_size = sizeof(ums_msg_t)+1;
	buf = calloc(buffer_size, sizeof(unsigned char));

	int rc;

	rc = deq_obj->on_de(l, deq_obj, buf, buffer_size, &l_seq, &run_time);
	if( rc == DQ_EMPTY_RETURN_VALUE || rc < 0 ) {
		SAFE_FREE(buf);
	}
	else {
		fq_log(l, FQ_LOG_DEBUG, "deQ msg:'%s'.", buf);
		fq_log(l, FQ_LOG_DEBUG, "deQ msglen:'%d'.", rc);
		*msg = (unsigned char *)buf;
		*p_seq = l_seq;

		// SAFE_FREE(buf);
	}

	return rc;
}
