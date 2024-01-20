/*
 * $(HOME)/src/tst/var_multi_open.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include "fqueue.h"
#include "fq_common.h"
#include "fq_multi_queue.h"
#include "fq_heartbeat.h"

#define READ_Q1_ID  0
#define READ_Q2_ID  1
#define WRITE_Q_ID  2
#define READ_XA_Q1_ID 3
#define READ_XA_Q2_ID 4

char **ARGV;
char g_HOSTNAME[HOST_NAME_LEN+1];

/* for signal */
#define handle_error_en(en, msg) \
	   do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0)

typedef struct _thread_param {
	fq_logger_t		*l;
	Queues_obj_t	*Qs; /* QueueS */
	fqueue_obj_t	*qobj;
	int				Qindex;
} thread_param_t;

int g_stop_sign = 0;


static void *
sig_thread(void *arg)
{
   sigset_t *set = (sigset_t *) arg;
   int s, sig;

   for (;;) {
	   s = sigwait(set, &sig);
	   if (s != 0)
		   handle_error_en(s, "sigwait");
	   printf("Signal handling thread got signal %d\n", sig);
	   g_stop_sign = 1;
   }
}

void  *read_thread(void *arg)
{
	thread_param_t *tp = (thread_param_t *)arg;
	int buffer_size = 8192;
	int rc;

	while(1) {
		long l_seq=0L;
		unsigned char *buf=NULL;
		long run_time=0L;

		buf = malloc(buffer_size);
		memset(buf, 0x00, sizeof(buffer_size));
	

		rc = tp->qobj->on_de(tp->l, tp->qobj, buf, buffer_size, &l_seq, &run_time);

		if( rc == DQ_EMPTY_RETURN_VALUE ) {
			if( g_stop_sign ) break;

			printf("empty..\n");
			SAFE_FREE(buf);
			usleep(100000);
		}
		else if( rc < 0 ) {
			printf("error..\n");
			break;
		}
		else {
			printf("[%d]-thread: read:rc=[%d] seq=[%ld]-th run_time=[%ld]\n", tp->Qindex, rc, l_seq, run_time);
			SAFE_FREE(buf);
		}
	}
	g_stop_sign = 1;

	pthread_exit(0);
}

void  *read_xa_thread(void *arg)
{
	thread_param_t *tp = (thread_param_t *)arg;
	int buffer_size = 8192;
	int rc;

	while(1) {
		long l_seq=0L;
		unsigned char *buf=NULL;
		long run_time=0L;
		char unlink_fname[256];

		buf = malloc(buffer_size);
		memset(buf, 0x00, sizeof(buffer_size));

		if( g_stop_sign ) break;

		rc = tp->qobj->on_de_XA(tp->l, tp->qobj, buf, buffer_size, &l_seq, &run_time, unlink_fname);

		if( rc == DQ_EMPTY_RETURN_VALUE ) {
			if( g_stop_sign ) break;

			printf("empty..\n");
			SAFE_FREE(buf);
			usleep(100000);
		}
		else if( rc < 0 ) {
			printf("error..\n");
			break;
		}
		else {
			printf("[%d]-thread: read XA :rc=[%d] seq=[%ld]-th run_time=[%ld]\n", tp->Qindex,  rc, l_seq, run_time);
			rc = tp->qobj->on_commit_XA(tp->l, tp->qobj, l_seq, &run_time, unlink_fname);
			if( rc < 0 ) {
				printf("XA commit error.\n");
				break;
			}
			SAFE_FREE(buf);
		}
	}
	g_stop_sign = 1;
	pthread_exit(0);
}

void *write_thread(void *arg)
{
	int rc;
	thread_param_t *tp = (thread_param_t *)arg;
	unsigned char data[4000];
	int datalen;
	long l_seq, run_time;

	memset(data, 0x00, sizeof(data));
	memset(data, 'D', sizeof(data)-1);
	datalen = strlen(data);

	while(1) {
		if( g_stop_sign ) break;

		rc =  tp->qobj->on_en(tp->l, tp->qobj, EN_NORMAL_MODE, data, sizeof(data), datalen, &l_seq, &run_time );
		if( rc == EQ_FULL_RETURN_VALUE )  {
			printf("full.\n");
		}
		else if( rc < 0 ) { 
			if( rc == MANAGER_STOP_RETURN_VALUE ) {
				printf("Manager asked to stop a processing.\n");
			}
			printf("error.\n");
		}
		else {
			printf("enFQ OK. run_time=[%ld]\n", run_time);
			// getc(stdin);
			usleep(1000);
		}
	}
	g_stop_sign = 1;
	pthread_exit(0);
}

int main(int ac, char **av)  
{  
	int rc;
	fq_logger_t *l=NULL;
	Queues_obj_t	*Qs=NULL; /* QueueS */

	char *path="/home/pi/data";
    char *qname="TST";
    char *logname = "var_multi_open.log";
	int cnt;
	fqueue_obj_t *readQ1=NULL;
	fqueue_obj_t *readQ2=NULL;
	fqueue_obj_t *readXAQ1=NULL;
	fqueue_obj_t *readXAQ2=NULL;
	fqueue_obj_t *writeQ=NULL;
	char *monitoring_ID = "var_multi_open";

	pthread_t	read_thread_1_id, read_thread_2_id, write_thread_id, read_xa_thread_1_id, read_xa_thread_2_id;
	thread_param_t read_thread_1_param, read_thread_2_param, write_thread_param, read_xa_thread_1_param, read_xa_thread_2_param;
	void *thread_rtn=NULL;

	pthread_t sig_handle_thread_id;
	sigset_t	set;
	int			s;

	if( ac != 2 ) {
		printf("Usage: $ %s [cnt] <enter>\n", av[0]);
		return(0);
	}

	ARGV = av;
	gethostname(g_HOSTNAME, sizeof(g_HOSTNAME));

	printf("Current setting:\n");
	printf("\t-path : '%s'\n", path);
	printf("\t-qname: '%s'\n", qname);
	printf("Sure?\n"); getc(stdin);

	cnt = atoi(av[1]);
	if( cnt > MAX_FQ_OPENS ) {
		printf("Too many file_queue open. max=[%d]\n", MAX_FQ_OPENS);
		return(0);
	}

	rc = fq_open_file_logger(&l, logname, FQ_LOG_TRACE_LEVEL);
	CHECK( rc > 0 );

	rc = new_Queues_obj(l, &Qs, cnt);
	CHECK( rc == TRUE );


	sigemptyset(&set);
	sigaddset(&set, SIGQUIT);
	sigaddset(&set, SIGINT);
	sigaddset(&set, SIGUSR1);
	
	s = pthread_sigmask( SIG_BLOCK, &set, NULL);
	if(s != 0 ) {
		handle_error_en(s, "pthread_sigmask");
	}

	s = pthread_create(&sig_handle_thread_id, NULL, &sig_thread, (void *) &set); 
	if( s != 0 ) {
		handle_error_en(s, "pthread_create");
	}

	RegistMon(l, monitoring_ID, "TEST", path, qname, FQ_EN_ACTION);

	if( (readQ1=Qs->on_open_queue(Qs, NULL, NULL, NULL, NULL, path, qname, READ_Q1_ID)) == NULL ) {
		printf("on_open_queues() error.\n");
		return(-1);
	}
	if( (readQ2=Qs->on_open_queue(Qs, NULL, NULL, NULL, NULL, path, qname, READ_Q2_ID)) == NULL ) {
		printf("on_open_queues() error.\n");
		return(-1);
	}
	if( (readXAQ1=Qs->on_open_queue(Qs, NULL, NULL, NULL, NULL, path, qname, READ_XA_Q1_ID)) == NULL ) {
		printf("on_open_queues() error.\n");
		return(-1);
	}
	if( (readXAQ2=Qs->on_open_queue(Qs, NULL, NULL, NULL, NULL, path, qname, READ_XA_Q2_ID)) == NULL ) {
		printf("on_open_queues() error.\n");
		return(-1);
	}

	if( (writeQ=Qs->on_open_queue(Qs, NULL, NULL, NULL, NULL, path, qname, WRITE_Q_ID)) == NULL ) {
		printf("on_open_queues() error.\n");
		return(-1);
	}

	read_thread_1_param.l = l;
	read_thread_1_param.Qs = Qs;
	read_thread_1_param.qobj = readQ1;
	read_thread_1_param.Qindex = READ_Q1_ID;

	read_thread_2_param.l = l;
	read_thread_2_param.Qs = Qs;
	read_thread_2_param.qobj = readQ2;
	read_thread_2_param.Qindex = READ_Q2_ID;

	read_xa_thread_1_param.l = l;
	read_xa_thread_1_param.Qs = Qs;
	read_xa_thread_1_param.qobj = readXAQ1;
	read_xa_thread_1_param.Qindex = READ_XA_Q1_ID;

	read_xa_thread_2_param.l = l;
	read_xa_thread_2_param.Qs = Qs;
	read_xa_thread_2_param.qobj = readXAQ2;
	read_xa_thread_2_param.Qindex = READ_XA_Q2_ID;

	write_thread_param.l = l;
	write_thread_param.Qs = Qs;
	write_thread_param.qobj = writeQ;
	write_thread_param.Qindex = WRITE_Q_ID;

	// pthread_create( &read_thread_1_id, NULL, &read_thread,  &read_thread_1_param);
	// pthread_create( &read_thread_2_id, NULL, &read_thread,  &read_thread_2_param);

	pthread_create( &read_xa_thread_1_id, NULL, &read_xa_thread, &read_xa_thread_1_param);
	pthread_create( &read_xa_thread_2_id, NULL, &read_xa_thread, &read_xa_thread_2_param);
	sleep(1);

	pthread_create( &write_thread_id, NULL, &write_thread, &write_thread_param);


	//  pthread_join( read_thread_1_id , &thread_rtn);
	// pthread_join( read_thread_2_id , &thread_rtn);

	pthread_join( read_xa_thread_1_id , &thread_rtn);
	pthread_join( read_xa_thread_2_id , &thread_rtn);

	pthread_join( write_thread_id , &thread_rtn);

	Qs->on_close_queue(Qs,READ_Q1_ID);
	Qs->on_close_queue(Qs,READ_Q2_ID);
	Qs->on_close_queue(Qs,READ_XA_Q1_ID);
	Qs->on_close_queue(Qs,READ_XA_Q2_ID);
	Qs->on_close_queue(Qs,WRITE_Q_ID);

	free_Queues_obj(l, &Qs);

	fq_close_file_logger(&l);

	printf("Good bye !\n");
	return(rc);
}
