/*
** sema_queue_test.c
** 2019/03/01 tested by Gwisang.
**         safely
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "fq_logger.h"
#include "fq_common.h"
#include "sema_queue.h"
#include "fq_sequence.h"
#include "fq_timer.h"

#define SEMA_QUEUE_NAME "/tmp/c3cc3"
#define USER_MAX_MSG_LEN (4096)
#define USER_JOB_TIME (1000)

#define D_LOGFILE "/tmp/sema_queue_test.log"

void print_sleep(int n);

void usage(char *progname)
{
	printf("$ %s -[options] <enter>\n", progname);
	printf("\t options.\n");
	printf("\t -d : destroy queue.\n");
	printf("\t -p : post messages to queue.\n");
	printf("\t -w : wait messages from queue.\n");
}

int main(int ac, char **av)
{
	int my_job_count = 0;
	semaqueue_t	*semap = NULL;
	fq_logger_t *l = NULL;
	int rc;
	double e;

	if(ac != 2 ) {
		usage(av[0]);
		return(0);
	}

	rc = fq_open_file_logger(&l, D_LOGFILE, FQ_LOG_ERROR_LEVEL);
	CHECK(rc>0);

	if(av[1][1] == 'd') {
		fq_log(l, FQ_LOG_DEBUG, "SemaQueue Destroy start.[%s].", SEMA_QUEUE_NAME);
		semaqueue_destroy(l, SEMA_QUEUE_NAME );
		printf("semaqueue_destroy() OK.\n");
		fq_log(l, FQ_LOG_DEBUG, "SemaQueue Destroy end.[%s].", SEMA_QUEUE_NAME);
		fq_close_file_logger(&l);
		return(0);
	}

#if 0 // We move it to library	
	/* For testing, If it does not exist, We always create queue */
	if( !is_file(SEMA_QUEUE_NAME) ) {
		semaqueue_create(l, SEMA_QUEUE_NAME);
	}
#endif

	if( (semap = semaqueue_open(l,SEMA_QUEUE_NAME)) ) {
		int count = 0;

		/* post : enQ */
		if( av[1][1] == 'p' ) { // enQ
			int rc1;
			sequence_obj_t *sobj=NULL;

			rc1 = open_sequence_obj(l, ".", "sema_queue_seq", "A",  12, &sobj);
			CHECK( rc1==TRUE );

			start_timer();
			while(1) {
				int rc2;

				unsigned char buf[SEMA_QUEUE_MAX_MSG_LEN-1]; // test OK, but very slow
				// unsigned char buf[USER_MAX_MSG_LEN-1];
				char str_seq[21];

				memset(buf, 0x00, sizeof(buf));

				rc2 = sobj->on_get_sequence(sobj, str_seq, sizeof(str_seq));
				CHECK( rc2==TRUE );

				memset(buf, 'D', sizeof(buf)-1);
				memcpy(buf, str_seq, strlen(str_seq));

				// sprintf(buf, "%s", "Hi Server");
				my_job_count++;
					
				semaqueue_post( l, semap, buf, strlen(buf) );
				count++;
				e = end_timer();
				if( e > 1.0) {
					printf("[%u] TPS.\n", (unsigned)(count/e));
					start_timer();
					count = 0;
				}
			}

			if( sobj ) {
				close_sequence_obj(&sobj);
			}
		} else if(av[1][1] == 'w') { // deQ
			while(1) {
				unsigned char *buf=NULL;
				unsigned msglen;

				semaqueue_wait(l, semap, &buf, &msglen);
				printf("msg=[%-80.80s] len=[%d]\n", buf, msglen);
				if(buf)	free(buf);

				my_job_count++;
				if( USER_JOB_TIME > 0 ) {
					usleep( USER_JOB_TIME);
				}
				// print_sleep(my_job_count);
			}
		} else {
			printf("illegal option.\n");
		}	
	}
	else {
		printf("semaqueue_open() failed.\n");
	}

	if(semap) {
		semaqueue_close(l, semap);
	}


	return(0);
}

void print_sleep(int n)
{
	printf("[%d]-th.\n", n);
	usleep(1000);
}
