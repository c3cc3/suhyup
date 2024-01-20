/*
 * deFQ_XA_thread.c
 * sample
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include "fqueue.h"
#include "fq_common.h"
#include "fq_monitor.h"

#define VAR_THREADS 1

#ifndef VAR_THREADS

#define RUN_THREADS 128
#define BUFFER_SIZE 65000
#define QPATH "/home/pi/data"
#define QNAME "TST"
#define LOG_FILE_NAME "./deFQ_XA_thread.log"

typedef struct _thread_param {
    int             Qindex;
} thread_param_t;

void *deQ_XA_thread( void *arg );

/*
** Global variables
*/
fq_logger_t *l=NULL;
fqueue_obj_t *obj=NULL;
monitor_obj_t   *mon_obj=NULL;
char *progname = NULL;

int	 buffer_size;


int main(int ac, char **av)  
{  
	int rc;
	int t_no;
	char *path=QPATH;
    char *qname=QNAME;
    char *logname = LOG_FILE_NAME;
	char *p=NULL;
	thread_param_t t_param[RUN_THREADS];
	pthread_t tids[RUN_THREADS];
	void *thread_rtn=NULL;

	p = getenv("LD_LIBRARY_PATH");
    printf("Current LD_LIBRARY_PATH=[%s]\n\n", p);

	p = getenv("FQ_DATA_HOME");
    printf("Current FQ_DATA_HOME=[%s]\n\n", p);

	if( p ) {
		path = strdup(p);
	}

	printf("Current Program settings:\n");
    printf("\t- recv buffer  size: [%d]\n", BUFFER_SIZE);
    printf("\t- qpath            : [%s]\n", path);
    printf("\t- qname            : [%s]\n", QNAME);
    printf("\t- log file         : [%s]\n", LOG_FILE_NAME);
    printf("\t- run threads      : [%d]\n", RUN_THREADS);

	printf("\n Are you sure? Press anykey for continue or Press Ctrl+'C' to stop\n");
    getc(stdin);

	progname = av[0];

	rc = fq_open_file_logger(&l, logname, FQ_LOG_ERROR_LEVEL);
	CHECK( rc > 0 );

	rc =  OpenFileQueue(l, NULL, NULL, NULL, NULL, path, qname, &obj);
	CHECK(rc==TRUE);

	rc =  open_monitor_obj(l,  &mon_obj);
	CHECK(rc==TRUE);

	/* buffer_size = obj->h_obj->h->msglen-4+1; */ /* queue message size */
	buffer_size = BUFFER_SIZE;
	printf("File Queue msglen is [%d].\n", obj->h_obj->h->msglen);

	for(t_no=0; t_no<RUN_THREADS; t_no++) {
		t_param[t_no].Qindex = t_no;
		pthread_create( &tids[t_no], NULL, &deQ_XA_thread, &t_param[t_no]);
	}

	for(t_no=0; t_no<RUN_THREADS; t_no++) {
		pthread_join( tids[t_no] , &thread_rtn);
	}

	rc = close_monitor_obj(l, &mon_obj);
	CHECK(rc==TRUE);

	rc = CloseFileQueue(l, &obj);
	CHECK(rc==TRUE);

	fq_close_file_logger(&l);

	return(rc);
}
#else
#define BUFFER_SIZE 65000
#define QPATH "/home/pi/data"
#define QNAME "TST"
#define LOG_FILE_NAME "./deFQ_XA_thread.log"

typedef struct _thread_param {
    int             Qindex;
} thread_param_t;

void *deQ_XA_thread( void *arg );

/*
** Global variables
*/
fq_logger_t *l=NULL;
fqueue_obj_t *obj=NULL;
monitor_obj_t   *mon_obj=NULL;
char *progname = NULL;

int	 buffer_size;


int main(int ac, char **av)  
{  
	int rc;
	int t_no;
	char *path=QPATH;
    char *qname=QNAME;
    char *logname = LOG_FILE_NAME;
	char *p=NULL;
	thread_param_t *t_params;
	pthread_t *tids;
	void *thread_rtn=NULL;
	int t_cnt;

	if( ac != 2 ) {
        printf("Usage: $ %s [thread_nums] <enter> \n", av[0]);
        return(0);
    }

    t_cnt = atoi(av[1]);

	p = getenv("LD_LIBRARY_PATH");
    printf("Current LD_LIBRARY_PATH=[%s]\n\n", p);

	p = getenv("FQ_DATA_HOME");
    printf("Current FQ_DATA_HOME=[%s]\n\n", p);

	if( p ) {
		path = strdup(p);
	}

	t_cnt = atoi(av[1]);

	printf("Current Program settings:\n");
    printf("\t- recv buffer  size: [%d]\n", BUFFER_SIZE);
    printf("\t- qpath            : [%s]\n", path);
    printf("\t- qname            : [%s]\n", QNAME);
    printf("\t- log file         : [%s]\n", LOG_FILE_NAME);
    printf("\t- run threads      : [%d]\n", t_cnt);

	printf("\n Are you sure? Press anykey for continue or Press Ctrl+'C' to stop\n");
    getc(stdin);

	progname = av[0];

	rc = fq_open_file_logger(&l, logname, FQ_LOG_ERROR_LEVEL);
	CHECK( rc > 0 );

	rc =  OpenFileQueue(l, NULL, NULL, NULL, NULL, path, qname, &obj);
	CHECK(rc==TRUE);

	rc =  open_monitor_obj(l,  &mon_obj);
	CHECK(rc==TRUE);

	/* buffer_size = obj->h_obj->h->msglen-4+1; */ /* queue message size */
	buffer_size = BUFFER_SIZE;
	printf("File Queue msglen is [%ld].\n", obj->h_obj->h->msglen);

	t_params = calloc( t_cnt, sizeof(thread_param_t));
    CHECK(t_params);

    tids = calloc ( t_cnt, sizeof(pthread_t));
    CHECK(tids);

	for(t_no=0; t_no < t_cnt; t_no++) {
		thread_param_t *p;

		p = t_params+t_no;
		p->Qindex = t_no;
		pthread_create( tids+t_no, NULL, &deQ_XA_thread, p);
	}

	for(t_no=0; t_no < t_cnt; t_no++) {
		pthread_join( *(tids+t_no) , &thread_rtn);
	}

	if(t_params) free( t_params );
    if(tids) free( tids );

	rc = close_monitor_obj(l, &mon_obj);
	CHECK(rc==TRUE);

	rc = CloseFileQueue(l, &obj);
	CHECK(rc==TRUE);

	fq_close_file_logger(&l);

	return(rc);
}


#endif

void *deQ_XA_thread( void *arg )
{
	thread_param_t *tp = (thread_param_t *)arg;

	while(1) {
		int rc;
		long l_seq=0L;
		unsigned char *buf=NULL;
		long run_time=0L;
		char    unlink_filename[256];

		buf = malloc(buffer_size);
		memset(buf, 0x00, sizeof(buffer_size));

		rc = obj->on_de_XA(l, obj, buf, buffer_size, &l_seq, &run_time, unlink_filename);

		mon_obj->on_send_action_status(l, progname, obj->path, obj->qname, obj->h_obj->h->desc, FQ_DE_ACTION, mon_obj);

		if( rc == DQ_EMPTY_RETURN_VALUE ) {
			printf("empty..\n");
			SAFE_FREE(buf);
			usleep(100000);
		}
		else if( rc < 0 ) {
			if( rc == MANAGER_STOP_RETURN_VALUE ) {
				printf("Manager asked to stop a processing.\n");
				break;
			}
			printf("error..\n");
			break;
		}
		else {
			printf("[%d]-thread: rc = [%d] seq=[%ld]-th\n", tp->Qindex, rc, l_seq );
			rc = obj->on_commit_XA(l, obj, l_seq, &run_time, unlink_filename);
			if( rc < 0 ) {
				printf("XA commit error.\n");
				SAFE_FREE(buf);
				exit(0);
			}
			/* success */
			SAFE_FREE(buf);
		}
	}
	pthread_exit(0);
}
