/* pthread_cancel.c */
/*
** 이 프로그램은 deQ_XA를 thread 버전으로 구현한 예제 프로그램이다.
** 이 프로그램의 특징은 환경파일에 설정된 expired time 동안 enQ 되지 않으면 main 에서 thread 를 cancel 시키고
** thread 에서 생성한 handler가 thread가 죽기전에 사용하던 자원을 모두 close 하고 
** smooth 하게 종료하는 프로그램이다.
** deQ_XA 하는 서버 데몬의 전형이라고 할 수 있다.
*/
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include "fqueue.h"
#include "fq_logger.h"
#include "fq_conf.h"
#include "fq_common.h"

static int is_expired( time_t stamp, int limit);

time_t g_thread_touch_time;

typedef struct _thread_param {
	fq_logger_t     *l;
	conf_obj_t		*c;
} thread_param_t;

typedef struct _handler_param {
	fq_logger_t     *l;
	fqueue_obj_t	*Q;
	conf_obj_t		*c;
} handler_param_t;

void cleanup_handler(void *arg)
{
	handler_param_t *h = arg;
	fq_logger_t *l = h->l;
	fqueue_obj_t *Q = h->Q;
	conf_obj_t *c = h->c;
	
	fq_log(l, FQ_LOG_ERROR, "cleanup handler start.");

	if( Q ) {
		long l_seq=0L;
		long run_time=0L;
		Q->on_cancel_XA(l, Q, l_seq, &run_time);
	}
	if( Q ) CloseFileQueue( l, &Q);

	fq_log(l, FQ_LOG_ERROR, "cleanup handler end.");
	return;
}

void* deQ_thread(void *arg)
{
	int rc;
	thread_param_t *t = arg;
	fq_logger_t *l = t->l;
	conf_obj_t *c = t->c;
	handler_param_t h; 
	fqueue_obj_t *Q = NULL;

	char qpath[256], qname[256];
	
	g_thread_touch_time = time(0);

	c->on_get_str(c, "QPATH", qpath);
    c->on_get_str(c, "QNAME", qname);

	rc =  OpenFileQueue(l, NULL, NULL, NULL, NULL, qpath, qname, &Q);
	CHECK(rc==TRUE);

	h.Q = Q;
	h.l = l;
	h.c = c;

	pthread_cleanup_push(cleanup_handler, &h);

	while(1) {
		long l_seq=0L;
		long run_time=0L;
		unsigned char *buf=NULL;
		int buffer_size = Q->h_obj->h->msglen;
		char	unlink_filename[256];


		buf = calloc(buffer_size, sizeof(unsigned char));
		rc = Q->on_de_XA(l, Q, buf, buffer_size, &l_seq, &run_time, unlink_filename);
		if( rc == DQ_EMPTY_RETURN_VALUE ) {
			fq_log(l, FQ_LOG_DEBUG, "empty...");
			sleep(1);
			continue;
		}
		else if( rc < 0 ) {
			fq_log(l, FQ_LOG_ERROR, "file queue error.");
			SAFE_FREE(buf);
			break;
		}
		else {
			int rc;
			int speed_level;

			g_thread_touch_time = time(0);

			fq_log(l, FQ_LOG_DEBUG, "buf=[%s]\n", buf);

			speed_level = c->on_get_int(c, "USER_JOB_SPEED_LEVEL");
			rc = user_job(speed_level);
			if( rc < 0 ) {
				// rc = Q->on_commit_XA(l, Q, l_seq, &run_time, unlink_filename);
				rc = Q->on_cancel_XA(l, Q, l_seq, &run_time);
				if( rc < 0 ) {
					fq_log(l, FQ_LOG_ERROR, "XA cancel error.");
					SAFE_FREE(buf);
					break;
				}
			}
			else {
				rc = Q->on_commit_XA(l, Q, l_seq, &run_time, unlink_filename);
				if( rc < 0 ) {
					fq_log(l, FQ_LOG_ERROR, "XA commit error.");
					SAFE_FREE(buf);
					break;
				}
			}
			SAFE_FREE(buf);
			continue;
		}
	}

	printf("end deQ_thread...\n");

	pthread_cleanup_pop(0);

	return NULL;
}

int main(int ac, char **av)
{
	int rc;
	pthread_t thread;
	int ret;
	fqueue_obj_t *Q=NULL;
	fq_logger_t *l=NULL;
	conf_obj_t	*c;
	char logpath[256], logname[256], loglevel[32], logpathfile[512];
	int i;
	thread_param_t t;
	int expired_time_sec;

	if( ac != 2 ) {
		printf("Usage: $ %s [config_file] <enter>\n", av[0]);
		printf("Usage: $ %s thread_cancel_test.cfg <enter>\n", av[0]);
		return(0);
	}

	
	rc = open_conf_obj( av[1], &c);
	CHECK(rc == TRUE);

	/* logging */
    c->on_get_str(c, "LOG_PATH", logpath);
    c->on_get_str(c, "LOG_NAME", logname);
    c->on_get_str(c, "LOG_LEVEL", loglevel);
    sprintf(logpathfile, "%s/%s", logpath, logname);
    rc = fq_open_file_logger(&l, logpathfile, get_log_level(loglevel));
    CHECK( rc > 0 );

	expired_time_sec = c->on_get_int(c, "EXPIRED_TIME_SEC");

	printf("start main thread...If It does not working for %d seconds, It will be stop.\n", expired_time_sec);

	t.l = l;
	t.c = c;

	if( pthread_create(&thread, NULL, deQ_thread, &t ) != 0 )
		return 1;

	while(1) { /* monitoring time_stamp of thread */
		sleep(1);
		if( is_expired( g_thread_touch_time, expired_time_sec) == TRUE ) {
			/* cancel!! */
			fq_log(l, FQ_LOG_EMERG, "I found that thread didn't work for [%d] sec. I'll cancel thread.", expired_time_sec);
			pthread_cancel(thread);
			break;
		}
	}

	pthread_join(thread, (void**)&ret);

	if( l ) fq_close_file_logger( &l );

	if( c ) close_conf_obj( &c );

	printf("main end.\n");
	return 0;
}
static int 
is_expired( time_t stamp, int limit_sec)
{
	time_t current_bintime;

	current_bintime = time(0);
	if( (stamp + limit_sec) < current_bintime) {
		return(TRUE);
	}
	else {
		return(FALSE);
	}
}
