/*
 * relay_FQ.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include "fqueue.h"
#include "fq_common.h"

#define MAX_THREADS 1000

#define BUFFER_SIZE 65000
#define QPATH "/home/pi/data"
#define QNAME "TST"
#define LOG_FILE_NAME "/tmp/relay_FQ.log"

int thread_count=0;
void *write_thread(void *arg);

typedef struct _thread_param {
	fq_logger_t		*l;
	fqueue_obj_t	*qobj;
	char			w_msg[65536];;
	int				w_msg_buf_size;
	int				w_msg_len;
} thread_param_t;

pthread_mutex_t my_lock;

#define MAX_TRHEADS 100

int main(int ac, char **av)  
{  
	int rc;
	fq_logger_t *l=NULL;
	fqueue_obj_t *obj_r=NULL, *obj_w=NULL;
	char *progname=NULL;
	char *path_r=QPATH, *path_w=QPATH;
    char *qname_r, *qname_w;
    char *logname = LOG_FILE_NAME;
	int	 buffer_size;
	char *p=NULL;


	progname = av[0];

	if(ac == 2) {
		qname_r = av[1];
	}
	else {
		qname_r = QNAME;
	}
	qname_w = "TST1";

	p = getenv("LD_LIBRARY_PATH");
	printf("Current LD_LIBRARY_PATH=[%s]\n\n", p);
	p = NULL;

	p = getenv("FQ_DATA_HOME");
	printf("Current FQ_DATA_HOME=[%s]\n\n", p);

	if( p ) {
		path_r = strdup(p);
	}
	path_w = strdup("/home/pi/data");

    printf("Current Program settings:\n");
    printf("\t- recv buffer  size: [%d]\n", BUFFER_SIZE);
    printf("\t- read path        : [%s]\n", path_r);
    printf("\t- read qname       : [%s]\n", qname_r);
    printf("\t- write path       : [%s]\n", path_w);
    printf("\t- write qname      : [%s]\n", qname_w);
    printf("\t- log file         : [%s]\n", LOG_FILE_NAME);

	printf("\n Are you sure? Press anykey for continue or Press Ctrl+'C' to stop\n");
    getc(stdin);

	pthread_mutex_init(&my_lock, NULL);

	rc = fq_open_file_logger(&l, logname, FQ_LOG_TRACE_LEVEL);
	CHECK( rc > 0 );

	rc =  OpenFileQueue(l, NULL,NULL, NULL, NULL, path_r, qname_r, &obj_r);
	CHECK(rc==TRUE);

	rc =  OpenFileQueue(l, NULL,NULL, NULL, NULL, path_w, qname_w, &obj_w);
	CHECK(rc==TRUE);



	/* buffer_size = obj_r->h_obj->h->msglen-4+1; */ /* queue message size */
	buffer_size = BUFFER_SIZE;

	printf("File Queue msglen is [%ld].\n", obj_r->h_obj->h->msglen);

	while(1) {
		long l_seq=0L;
		unsigned char *buf=NULL;
		long run_time=0L;
		int read_msg_len = 0;
		thread_param_t write_thread_param;

		buf = malloc(buffer_size);
		memset(buf, 0x00, sizeof(buffer_size));

		read_msg_len = obj_r->on_de(l, obj_r, buf, buffer_size, &l_seq, &run_time);

		if( read_msg_len == DQ_EMPTY_RETURN_VALUE ) {
			printf("empty..\n");
			SAFE_FREE(buf);
			usleep(100000);
		}
		else if( read_msg_len < 0 ) {
			if( rc == MANAGER_STOP_RETURN_VALUE ) {
				printf("Manager asked to stop a processing.\n");
				break;
			}
			printf("error..\n");
			break;
		}
		else {
			if( thread_count < 100 ) {
				pthread_attr_t attr;
				pthread_t write_thread_id;

				pthread_mutex_lock(&my_lock);
				write_thread_param.l = l;
				write_thread_param.qobj = obj_w;
				memcpy(write_thread_param.w_msg, buf, read_msg_len);
				write_thread_param.w_msg[read_msg_len] = 0x00;
				write_thread_param.w_msg_buf_size = buffer_size;
				write_thread_param.w_msg_len = read_msg_len;
				pthread_mutex_unlock(&my_lock);

				pthread_attr_init(&attr);
				pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
				rc = pthread_create( &write_thread_id, &attr, write_thread, &write_thread_param);
				if( rc != 0 ) {
					fprintf(stderr, "pthread_create() error.\n");
					exit(0);
				}
				pthread_mutex_lock( &my_lock );
				thread_count++;
				pthread_mutex_unlock( &my_lock );

				pthread_attr_destroy(&attr);

				printf("current threads=[%d]-th: create_thread() ok.\n", thread_count);
				SAFE_FREE(buf);
			}
			else {
				while(1) {
					long l_seq, run_time;

					rc =  obj_w->on_en(l, obj_w, EN_NORMAL_MODE, buf, buffer_size, read_msg_len, &l_seq, &run_time );
					if( rc == EQ_FULL_RETURN_VALUE )  {
						printf("full.\n");
						sleep(1);
						continue;
					}
					else if( rc < 0 ) { 
						if( rc == MANAGER_STOP_RETURN_VALUE ) {
							printf("Manager asked to stop a processing.\n");
						}
						printf("main: on_en() error.\n");
						exit(0);
					}
					else {
						printf("main(): enFQ OK(rc=%d]. l_seq=[%ld], run_time=[%ld]\n", rc, l_seq, run_time);
						break;
					}
				}
				SAFE_FREE(buf);
			}
		}
	} // while end

	rc = CloseFileQueue(l, &obj_r);
	rc = CloseFileQueue(l, &obj_w);
	CHECK(rc==TRUE);

	fq_close_file_logger(&l);

	return(rc);
}

void *write_thread(void *arg)
{
	pthread_attr_t attr;
	int chk;
	int rc;
	thread_param_t *tp = (thread_param_t *)arg;
	long l_seq, run_time;

#if 1
	pthread_attr_getdetachstate(&attr, &chk);
	if(chk == PTHREAD_CREATE_DETACHED ) 
		printf("Detached\n");
	else if (chk == PTHREAD_CREATE_JOINABLE) 
		printf("Joinable\n");

	while(1) {
		// printf("w_msg_buf_size=[%d]\n", tp->w_msg_buf_size);
		// printf("w_msg_len=[%d]\n", tp->w_msg_len);
		if( !tp->w_msg ) {
			printf("fatal error.\n");
			exit(0);
		}
		// printf("w_msg_strlen=[%d]\n", strlen(tp->w_msg));

		rc =  tp->qobj->on_en(tp->l, tp->qobj, EN_NORMAL_MODE, tp->w_msg, tp->w_msg_buf_size, tp->w_msg_len, &l_seq, &run_time );

		if( rc == EQ_FULL_RETURN_VALUE )  {
			printf("full. retry...\n");
			sleep(1);
			continue;
		}
		else if( rc < 0 ) { 
			if( rc == MANAGER_STOP_RETURN_VALUE ) {
				printf("Manager asked to stop a processing.\n");
			}
			printf("thread on_en() error.\n");
			exit(0);
		}
		else {
			printf("thread: enFQ OK(rc=%d]. l_seq=[%ld], run_time=[%ld]\n", rc, l_seq, run_time);
			break;
		}
	}
#else
	pthread_attr_getdetachstate(&attr, &chk);
	if(chk == PTHREAD_CREATE_DETACHED ) 
		printf("Detached\n");
	else if (chk == PTHREAD_CREATE_JOINABLE) 
		printf("Joinable\n");

	printf("w_msg_buf_size=[%d]\n", tp->w_msg_buf_size);
	printf("w_msg_len=[%d]\n", tp->w_msg_len);
	if( !tp->w_msg ) {
		printf("fatal error.\n");
		exit(0);
	}
	printf("w_msg_strlen=[%d]\n", strlen(tp->w_msg));
#endif
	pthread_mutex_lock( &my_lock );
	thread_count--;
	pthread_mutex_unlock( &my_lock );
	return(NULL);
}
