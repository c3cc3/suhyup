/*
 * $(HOME)/src/utl/enFQ_thrad.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include "fqueue.h"
#include "fq_common.h"

#define RUN_THREADS 10

#define SEND_MSG_SIZE 4000
#define QPATH "/home/pi/data"
#define QNAME "TST"
#define LOG_FILE_NAME "enFQ.log"

#ifndef LOOPING_TEST
#define LOOPING_TEST
#define USLEEP_TIME 1000 
#endif

typedef struct _thread_param {
    int             Qindex;
} thread_param_t;


void *enQ_thread(void *arg);

int	 databuflen;
int	datasize;
fq_logger_t *l=NULL;
fqueue_obj_t *obj=NULL;


int main(int ac, char **av)  
{  
	int rc;
	char *path=QPATH;
    char *qname=QNAME;
    char *logname = LOG_FILE_NAME;
	unsigned char	*buf=NULL;
	long	l_seq=0L;
	long run_time=0L;
	char *p= NULL;
	pthread_t tids[RUN_THREADS];
    thread_param_t t_param[RUN_THREADS];
    void *thread_rtn=NULL;
    int t_no;

    p = getenv("LD_LIBRARY_PATH");
    printf("Current LD_LIBRARY_PATH=[%s]\n\n", p);

    p = getenv("FQ_DATA_HOME");
    printf("Current FQ_DATA_HOME=[%s]\n\n", p);

	if( p ) {
		path = strdup(p);
	}

	printf("Current Program settings:\n");
	printf("\t- send message size: [%d]\n", SEND_MSG_SIZE);
	printf("\t- qpath            : [%s]\n", path);
	printf("\t- qname            : [%s]\n", QNAME);
	printf("\t- log file         : [%s]\n", LOG_FILE_NAME);

	printf("\n Are you sure? Press anykey for continue or Press Ctrl+'C' to stop\n");
	getc(stdin);

	rc = fq_open_file_logger(&l, logname, FQ_LOG_ERROR_LEVEL);
	CHECK( rc > 0 );

	rc =  OpenFileQueue(l, NULL, NULL, NULL, NULL, path, qname, &obj);
	CHECK(rc==TRUE);

	/* normal data : data size is less than msglen-1 */
	/* databuflen = obj->h_obj->h->msglen - 4 + 1; */
	/* big data : data size is greater than msglen-1 */

	for(t_no=0; t_no < RUN_THREADS ; t_no++) {
        t_param[t_no].Qindex = t_no;
        pthread_create( &tids[t_no], NULL, &enQ_thread, &t_param[t_no]);
    }

	for(t_no=0; t_no < RUN_THREADS ; t_no++) {
        pthread_join( tids[t_no] , &thread_rtn);
    }

	rc = CloseFileQueue(l, &obj);
	CHECK(rc==TRUE);

	fq_close_file_logger(&l);

	printf("Good bye !\n");
	return(rc);
}

void *enQ_thread(void *arg)
{
	thread_param_t *tp = (thread_param_t *)arg;

	while(1) {
		int rc;
		unsigned char *buf=NULL;
		long l_seq=0L;
		long run_time=0L;

		databuflen = SEND_MSG_SIZE+1;
		buf = malloc(databuflen);
		memset(buf, 0x00, databuflen);
		memset(buf, 'D', databuflen-1);
		
		datasize = strlen((char *)buf);

		rc =  obj->on_en(l, obj, EN_NORMAL_MODE, buf, databuflen, datasize, &l_seq, &run_time );
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
			printf("[%d]-thread: enFQ OK. rc=[%d]\n", tp->Qindex, rc);
			usleep(10000);
		}

		if(buf) free(buf);

	}

	pthread_exit( (void *)0);
}
