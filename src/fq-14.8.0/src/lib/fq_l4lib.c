/*
 * fq_l4lib.c
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <errno.h>
#include <pthread.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/uio.h>
#include <netdb.h>
#include <sys/resource.h>

#include "fq_l4lib.h" 
#include "fq_tlpi_hdr.h"
#include "fq_multi_config.h"
#include "fq_logger.h"
#include "fq_common.h"
#include "fq_unix_sockets.h"
#include "fq_rlock.h"

static void *touch_thread(void *arg);
static int check_L4_alive( Heartbeat_Table_t *p);

int L4_get_connection_count( Heartbeat_Table_t *p, int index_port)
{
	if( !p ) {
		fprintf(stderr, "p is null.\n");
		return(-1);
	}
	if( (index_port < 0) || (index_port >= MAX_PORTS) ) {
		fprintf(stderr, "index_port is invalid.\n");
		return(-1);
	}
	return(  p->L4[index_port].connections );
}

void L4_connection_update(Heartbeat_Table_t *p, int index_port, int method)
{
	if( !p ) {
		fprintf(stderr, "p is null.\n");
		return;
	}
	if( (index_port < 0) || (index_port >= MAX_PORTS) ) {
		fprintf(stderr, "index_port is invalid.\n");
		return;
	}
	if( method == INCREASE_COUNT) { 
		pthread_mutex_lock(&p->mtx);
		p->L4[index_port].connections++;
		pthread_mutex_unlock(&p->mtx);
	}
	else if( method == DECREASE_COUNT) {
		pthread_mutex_lock(&p->mtx);
		p->L4[index_port].connections--;
		pthread_mutex_unlock(&p->mtx);
	}
	else {
		fprintf(stderr, "method is invalid.\n");
		return;
	}
	return;
}

static int find_available_port( fq_logger_t *l,  Heartbeat_Table_t *p, char *sock_path)
{
	int i;
    time_t  current_time, diff_sec;

	FQ_TRACE_ENTER(l);

	if( !p ) {
		fq_log(l, FQ_LOG_ERROR, "p is null.");
		FQ_TRACE_EXIT(l);
		return(-1);
	}
	if( !sock_path ) {
		fq_log(l, FQ_LOG_ERROR, "ock_path is null.");
		FQ_TRACE_EXIT(l);
		return(-1);
	}

	pthread_mutex_lock(&p->mtx);
	/* 우선적으로 같은이름이 등록되어 있는지 확인한다. 같은이름으로 available 한것이 있으면 오류리턴  */
    for(i=0; i< MAX_PORTS; i++) {
		if( strcmp(p->L4[i].sock_path, sock_path) == 0) {
			if( p->L4[i].touch_time == 0 || p->L4[i].touch_time < 0 ) return(i);
			if( is_exceeded_sec(p->L4[i].touch_time, 3) ) { 
				pthread_mutex_unlock(&p->mtx);
				FQ_TRACE_EXIT(l);
				return(i);
			}
			else {
				fq_log(l, FQ_LOG_ERROR, "Already registed name. Use another name.");
				pthread_mutex_unlock(&p->mtx);
				FQ_TRACE_EXIT(l);
				return(-1);
			}
		}
	}

	/* 여기로 내려오면 , 같은이름으로 살아있는 것은 없다 */
    for(i=0; i< MAX_PORTS; i++) {
		/* 초기상태 */
		if( p->L4[i].touch_time == 0 || p->L4[i].touch_time < 0 ) {
				fq_log(l, FQ_LOG_DEBUG, "Found available port [%d]-th.", i);
				pthread_mutex_unlock(&p->mtx);
				FQ_TRACE_EXIT(l);
                return(i);
		}

		/* 이름 상관없이 죽어있는 포트를 찾아 쓴다 */
        if( is_exceeded_sec(p->L4[i].touch_time, 3) ) { 
				fq_log(l, FQ_LOG_DEBUG, "Found available port [%d]-th.", i);
				pthread_mutex_unlock(&p->mtx);
				FQ_TRACE_EXIT(l);
                return(i);
        }
    }
	
	pthread_mutex_unlock(&p->mtx);
	/* 모두사용중이다 */
	fq_log(l, FQ_LOG_ERROR, "There is no available port(full).");

	FQ_TRACE_EXIT(l);
    return(-1); /* not found */
}

static void *touch_thread(void *arg)
{
	touch_thread_args_t *t = (touch_thread_args_t *)arg;

	int     sid = -1;
	int		size;
	void	*sptr = NULL;
	Heartbeat_Table_t *p=NULL;
	int		index;

	// printf("In a thread, index=[%d]\n", t->index);
	index = t->index;

	size = sizeof(Heartbeat_Table_t);

	sid = shmget(t->shmkey, size, 0666);
	if( sid == -1) {
        fprintf(stderr, "shmget() error.\n");
		pthread_exit((void *)0);
		return((void *)0);
    }

	sptr = shmat(sid, 0, 0);
    if(sptr == (void *)-1) {
        fprintf(stderr, "shmat() error.\n");
		pthread_exit((void *)0);
		return((void *)0);
    }

	p = (Heartbeat_Table_t *)sptr;

	while(1) {
		// printf("touch thread index = [%d]\n", index);

		pthread_mutex_lock(&p->mtx);
		p->L4[index].touch_time = time(0);
		pthread_mutex_unlock(&p->mtx);
		// printf("touch. [%ld]\n", p->touch_time);
		sleep(1);
		if( p->L4[index].touch_time <= 0 ) break;
	}

	shmdt(p);
	pthread_exit((void *)0);
    return((void *)0);
}
/*
 ** return: sockfd
*/
int L4_adaptor( fq_logger_t *l, key_t shmkey, char *connection_path, Heartbeat_Table_t **ptr_p, int *index)
{
	touch_thread_args_t t;
	int     sid = -1;
	int		size;
	void	*sptr = NULL;
	pthread_t   touch_tid;
	Heartbeat_Table_t *p;
	int		index_port;
	Boolean useDatagramSocket;
	union {
        struct cmsghdr cmh;
        char   control[CMSG_SPACE(sizeof(int))];
                        /* Space large enough to hold an 'int' */
    } control_un;
    struct cmsghdr *cmhp;
	int lfd, sfd;

	FQ_TRACE_ENTER(l);

	size = sizeof(Heartbeat_Table_t);

	sid = shmget(shmkey, size, 0666);
	if( sid == -1) {
		fq_log(l, FQ_LOG_ERROR, "shmget() error.");
		FQ_TRACE_EXIT(l);
        return(-1);
    }

	sptr = shmat(sid, 0, 0);
    if(sptr == (void *)-1) {
		fq_log(l, FQ_LOG_ERROR, "shmat() error.");
		FQ_TRACE_EXIT(l);
        return(-2);
    }

	p = (Heartbeat_Table_t *)sptr;

	if( check_L4_alive(p) < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "There is no alive L4. Check L4 status.");
		shmdt(p);
		FQ_TRACE_EXIT(l);
		return(-1);
	}

	index_port = find_available_port(l, p, connection_path);
	
	if( index_port < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "Can't found available port in Shared table.");
		shmdt(p);
		FQ_TRACE_EXIT(l);
        return(-4);
    }
	pthread_mutex_lock(&p->mtx);

	sprintf(p->L4[index_port].sock_path, "%s", connection_path);
	p->L4[index_port].connections = 0;
	p->L4[index_port].pid = getpid();
	pthread_mutex_unlock(&p->mtx);
	// p->L4[index_port].last_balance_time = time(0);


	// t.p = p;
	t.index = index_port;
	t.shmkey = shmkey;

	if(pthread_create(&touch_tid, NULL, touch_thread, &t) != 0) {
		fq_log(l, FQ_LOG_ERROR, "pthread_create(touch_thread) error.");
		shmdt(p);
		FQ_TRACE_EXIT(l);
		return(-5);
	}

	fq_log(l, FQ_LOG_DEBUG, "touch_thread create success!\n");

	/* Parse command-line arguments */
    useDatagramSocket = TRUE;

    /* Create socket bound to well-known address */
    if (remove(connection_path) == -1 && errno != ENOENT) {
		fq_log(l, FQ_LOG_ERROR, "connection path remove(%s) error. ", connection_path);
		shmdt(p);
		FQ_TRACE_EXIT(l);
		return(-6);
    }

    if (useDatagramSocket) {
		fq_log(l, FQ_LOG_ERROR, "Receiving via datagram socket.");
        sfd = unixBind(connection_path, SOCK_DGRAM);
        if (sfd == -1) {
			fq_log(l, FQ_LOG_ERROR, "unixBind() error.");
			shmdt(p);
			FQ_TRACE_EXIT(l);
            return(-7);
        }
        else {
            fq_log(l, FQ_LOG_DEBUG, "Binded. connection_path=[%s].", connection_path);
        }
    } else {
        fq_log(l, FQ_LOG_DEBUG, "Receiving via stream socket.");
        lfd = unixListen(connection_path, 5);
        if (lfd == -1) {
			fq_log(l, FQ_LOG_ERROR, "unixListen() error.");
			shmdt(p);
			FQ_TRACE_EXIT(l);
			return(-8);
        }
        else {
			fq_log(l, FQ_LOG_DEBUG, "Listened. connection_path=[%s].", connection_path);
        }

        sfd = accept(lfd, NULL, NULL);
        if (sfd == -1) {
			fq_log(l, FQ_LOG_ERROR, "accept() error.");
			shmdt(p);
			FQ_TRACE_EXIT(l);
			return(-9);
		}
    }
	*ptr_p = p;
	*index = index_port;

	FQ_TRACE_EXIT(l);
	return(sfd);
}

#if 0
void SetLimit(int value)
{
	struct rlimit rlb;
	int	current;

	if ((current = getrlimit(RLIMIT_NOFILE, &rlb)) < 0) {
		printf("warning: getrlimit\n");
		rlb.rlim_cur = 64;
	}
	else {
		if( current < value ) {
			rlb.rlim_cur = value;
		}
		if (setrlimit(RLIMIT_NOFILE, &rlb) < 0) {
			printf("warning: setrlimit\n");
		}
	}
}
#endif

static int check_L4_alive( Heartbeat_Table_t *p)
{
	if( is_alive_pid_in_general(p->listener_pid) ) { 
		return(p->listener_pid);
	}
	else {
		return(-1);
	}
}
