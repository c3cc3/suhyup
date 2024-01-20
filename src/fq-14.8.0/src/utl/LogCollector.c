/* LogColletor.c
 *  This program is Linux-specific.
 *  L4 version
 *	2019/02/14
 *  by Clang Co.
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

#include "fq_common.h"
#include "fq_tlpi_hdr.h"
#include "fq_multi_config.h"
#include "fq_logger.h"
#include "fq_l4lib.h"
#include "fqueue.h"
#include "fq_singleton.h"
#include "fq_unix_sockets.h"
#include "fq_socket.h"

#define MAX_FQ_MESSAGE_SIZE 65535
#define MAX_THREADS 48

pthread_t tid[MAX_THREADS];
pthread_t	touch_tid;
fq_logger_t *l=NULL;

singleton_shmlock_t *m=NULL;
unsigned char key='k';

typedef struct thread_arg thread_arg_t;
struct thread_arg {
	fq_logger_t *l;
	fqueue_obj_t *obj;
	int		id;
    int     fd;
	int		status;
	Heartbeat_Table_t *p;
	int		index;
};

void *thread_server(void *arg);
int view_sock_receive_buffer(int r_sock);

void print_help(char *p)
{
	printf("\n\nUsage  : $ %s [-h] [-f config_file] <enter>\n", p);
	printf("example: $ %s -f /path/LogCollector.conf <enter>\n", p);
	return;
}

int
main(int argc, char *argv[])
{
    struct msghdr msgh;
    struct iovec iov;
    int data, sfd, opt;
	int i;

	int		index_port;
	thread_arg_t    x[MAX_THREADS];
	int	thread_num = 0;
	Heartbeat_Table_t *p = NULL;

	/* shared memory */
	int		rc;
	int		size;
    struct timespec ts;
    struct timeval now;

	/* for multi configuration */
	SZ_toDefine_t conf;
	char	*config_filename=NULL;
	int	ch;
	char	*connection_path=NULL;
	key_t     shmkey=5678;

	/* for logging */
	int		i_log_level;
	char	*log_filename=NULL;
	char	*log_level=NULL;
	char	*mq_path=NULL;
	char	*mq_name=NULL;

	/* for FileQueue */
	fqueue_obj_t *obj=NULL;
	
	if( argc != 3 ) {
		fprintf(stderr, "Usage: $ %s -f [conf_filename]\n", argv[0]);
		exit(0);
	}
	
	while(( ch = getopt(argc, argv, "Hh:f:")) != -1) {
		switch(ch) {
			case 'H':
			case 'h':
				print_help(argv[0]);
				return(0);
			case 'f':
				config_filename = strdup(optarg);
				break;
			default:
				print_help(argv[0]);
                return(0);
		}
	}

	m = READY_singleton(l, key);
	CHECK( m );


	/* init memory for configuration */
	SZ_toInit(&conf);

	if(SZ_fromFile( &conf, config_filename) <= 0) 
		errExit("Confirm your configuration file name and path.\n");

	if( (connection_path = SZ_toString( &conf, "server.connection_path")) == NULL) 
        errExit("not found.(server.connection_path) in the %s. Check configuration items.\n", config_filename);

	printf("connection_path=[%s]\n", connection_path);

	size = sizeof(Heartbeat_Table_t);

	if( (shmkey = SZ_toInteger( &conf, "server.shmkey") ) == 0 ) 
		errExit("not found.(server.shmkey) in the %s.\n", config_filename);

	if( (log_filename = SZ_toString( &conf, "server.log.filename") ) == 0 ) 
		errExit("not found.(server.log.filename) in the %s.\n", config_filename);

	if( (log_level = SZ_toString( &conf, "server.log.level") ) == 0 ) 
		errExit("not found.(server.log.level) in the %s.\n", config_filename);

	if( (mq_path = SZ_toString( &conf, "server.mq.path") ) == 0 ) 
		errExit("not found.(server.mq.path) in the %s.\n", config_filename);

	if( (mq_name = SZ_toString( &conf, "server.mq.name") ) == 0 ) 
		errExit("not found.(server.mq.name) in the %s.\n", config_filename);

	i_log_level = get_log_level(log_level);
	rc = fq_open_file_logger(&l, log_filename, i_log_level);
	if( !(rc > 0) ) 
		errExit("fq_open_file_logger() error.\n");

	fprintf(stdout, "Log file opened successfully. From now, See logfile(%s).\n", log_filename);

	/*
	** Open File Queue 
	*/
	rc =  OpenFileQueue(l, NULL, NULL, NULL, NULL, mq_path, mq_name, &obj);
	if( rc != TRUE ) 
		errExit("OpenFileQueue() error.\n");

	fq_log(l, FQ_LOG_DEBUG, "MQ open success. [%s/%s]", mq_path, mq_name);


	sfd =  L4_adaptor(l, shmkey, connection_path, &p, &index_port);
	if( sfd < 0 ) 
		errExit("L4_adaptor() error.\n");

	fq_log(l, FQ_LOG_DEBUG, "L4_adaptor() success! sfd=[%d] index=[%d].", sfd, index_port);
 
	for(i=0; i<MAX_THREADS; i++) {
		x[i].l = NULL;
		x[i].obj = NULL;
		x[i].id = i;
		x[i].fd = -1;
		x[i].status = STATUS_IDLE;
	}

	thread_num = 0;

	while(1) {
		int sess_id;
		int	i;
		int fd;
		struct linger option;
		int one = 1;

		fq_log(l, FQ_LOG_DEBUG, "I'm waiting to receive new fd from L4-switch...");
		fd = recvfd(sfd); /* recvfd() is block mode. */
		if( fd < 0 ) {
			fq_log(l, FQ_LOG_ERROR, "recvfd() error.");
			errExit("recvfd");
		}

		option.l_onoff = 1; /* on */
		option.l_linger = 0; /* When You close a socket, Closing is delay for 3 sec  until data is sent. */
		setsockopt(fd, SOL_SOCKET, SO_LINGER, (const void *)&option, sizeof(option));

		socket_buffers_control(fd,  1024*1024*10);
		
		for(i=0; i<MAX_THREADS; i++) {
			if( x[i].status == STATUS_BUSY ) continue;
			thread_num = i;
			break;
		}

		if( thread_num >= MAX_THREADS ) {
			fq_log(l, FQ_LOG_ERROR, "ession is full. MAX_THREADS=[%d]", MAX_THREADS);
			close(fd);
			continue;
		}

		x[thread_num].l = l; 
		x[thread_num].obj = obj; 
		x[thread_num].fd = fd; 
		x[thread_num].id = thread_num;
		x[thread_num].status = STATUS_BUSY;
		if( x[thread_num].fd == 0 ) 
			errExit("recvfd");

		x[thread_num].p = p;
		x[thread_num].index = index_port;
		
		if(pthread_create(&tid[thread_num], NULL, thread_server, &x[thread_num])) {
			fq_log(l, FQ_LOG_ERROR, "pthread_create failed.");
			// errExit("pthread_create failed.");
			errMsg("pthread_create failed.");
			x[thread_num].l = NULL;
			x[thread_num].obj = NULL;
			x[thread_num].fd = -1;
			x[thread_num].id = -1;
			x[thread_num].status =  STATUS_IDLE;
			x[thread_num].p = NULL;
			x[thread_num].index = - -1;
			usleep(1000);
			continue;
		}
		thread_num++;
		L4_connection_update(p, index_port, INCREASE_COUNT);
		// p->L4[index_port].connections++;
		fq_log(l, FQ_LOG_DEBUG, "Current connections is [%d] in this server.\n", L4_get_connection_count(p, index_port));
	}

	rc = CloseFileQueue(l, &obj);
    CHECK(rc==TRUE);

    fq_close_file_logger(&l);

    exit(EXIT_SUCCESS);
}

void analysis_agent_send_status( char *agent_sendtime, char *agent_hostname, char *agent_ip)
{
	return;
}

#define MAX_AGENT_SENDTIME_LEN 14
#define MAX_AGENT_HOSTNAME_LEN 32
#define MAX_AGENT_IP_ADDR_LEN 15

void *thread_server(void *arg)
{
	thread_arg_t    *x=arg;


	FQ_TRACE_ENTER(x->l);

    for (;;) {
		unsigned char header[FQ_HEADER_SIZE+1];
        unsigned char 	*buf=NULL;
		ssize_t numHeader=0;
        ssize_t numRead=0;
        ssize_t numWrite=0;
        ssize_t response_len;
		ssize_t data_len;
		int n;
		long    l_seq=0L;
		long run_time=0L;
		int rc;

		char	agent_sendtime[MAX_AGENT_SENDTIME_LEN+1];
		char	agent_hostname[128];
		char	agent_ip[128];

		/*
		** read 4 bytes length-header of binary type from LogAgent.
		*/
		numHeader = readn(x->fd, header, FQ_HEADER_SIZE);
		if( numHeader != 4) {
			fq_log(x->l, FQ_LOG_ERROR, "header read error! thread_num=[%d] fd=[%d]", x->id, x->fd);
			break;
		}
		fq_log(x->l, FQ_LOG_DEBUG,"[%d]-th thread: received length-header %d bytees: numRead=[%d]", x->id, FQ_HEADER_SIZE,  numRead);

		/*
		** conversion and check length-header.
		*/
		data_len = get_bodysize( header, FQ_HEADER_SIZE, 0);
		if( data_len > MAX_FQ_MESSAGE_SIZE ) {
			fq_log(x->l, FQ_LOG_ERROR, "Data of length-header(%d) is too big. Max is %d.",
				data_len, MAX_FQ_MESSAGE_SIZE);
			break;
		} 
		fq_log(x->l, FQ_LOG_DEBUG, "[%d]-th thread: will receive body of %d bytes.", x->id, data_len);


		/*
		** read n-bytes of body
		*/
		buf = calloc( data_len+1 , sizeof(unsigned char));
		if( !buf ) {
			fq_log(x->l, FQ_LOG_ERROR, "callec(%d) error.", data_len+1);
			break;
		}

        numRead = readn(x->fd, buf, data_len);
        if (numRead != data_len) {
			fq_log(x->l, FQ_LOG_ERROR, "readn() error. thread_num=[%d] fd=[%d]", x->id, x->fd);
			SAFE_FREE(buf);
			break;
		}
		if( numRead == 0 ) {
			fq_log(x->l, FQ_LOG_ERROR, "numRead is 0.");
			SAFE_FREE(buf);
			break;
		}
		fq_log(x->l, FQ_LOG_DEBUG, "[%d]-th thread: received body: numRead=[%d]", x->id, numRead);


		/*
		** parsing protocol part of body.
		** Protocol part has sendtime(14), hostname(32), ip-address(15) of Server be running LogAgent.
		*/
		memcpy( agent_sendtime, buf, MAX_AGENT_SENDTIME_LEN);
		agent_sendtime[MAX_AGENT_SENDTIME_LEN] = 0x00;
		memcpy( agent_hostname, buf+MAX_AGENT_SENDTIME_LEN, MAX_AGENT_HOSTNAME_LEN);
		agent_hostname[MAX_AGENT_SENDTIME_LEN+MAX_AGENT_HOSTNAME_LEN] = 0x00;
		memcpy( agent_ip, buf+MAX_AGENT_SENDTIME_LEN+MAX_AGENT_HOSTNAME_LEN, MAX_AGENT_IP_ADDR_LEN);
		agent_ip[MAX_AGENT_SENDTIME_LEN+MAX_AGENT_HOSTNAME_LEN+MAX_AGENT_IP_ADDR_LEN] = 0x00;

		analysis_agent_send_status( agent_sendtime, agent_hostname, agent_ip);

		rc =  SAFE_lock_singleton(l, FQ_ROBUST_NP_LOCK, m);
		CHECK( rc >= 0 );

		/*
		** put a data to file-queue until success
		*/
		while(1) {
			int rc;
			int data_start_offset=0;
			int	en_data_len=0;

			data_start_offset = MAX_AGENT_SENDTIME_LEN + MAX_AGENT_HOSTNAME_LEN + MAX_AGENT_IP_ADDR_LEN; /* Protocol Header */
			en_data_len = data_len - data_start_offset;

			rc =  x->obj->on_en(x->l, x->obj, EN_NORMAL_MODE, buf+data_start_offset, data_len+1, en_data_len, &l_seq, &run_time );
			if( rc == EQ_FULL_RETURN_VALUE )  {
				fq_log(x->l, FQ_LOG_ERROR, "MQ is full. We send a full signal(ACK) to LogAgent and after 1 sec, retry enQ().");
				/*
				** Send a Full message to LogAgent.
				** ACK O:succes X:failed
				*/
				numWrite = writen(x->fd, (unsigned char *)"F", 1);
				if (numWrite != 1) {
					fq_log(x->l, FQ_LOG_ERROR, "writen() error. thread_num=[%d] fd=[%d] numWrite=[%d]", x->id, x->fd, numWrite);
					break; /* exit thread */
				}
				sleep(1);
				continue; /* retry on_en() */
			}
			else if( rc < 0 ) {

				if( rc == MANAGER_STOP_RETURN_VALUE ) {
					fq_log(x->l, FQ_LOG_ERROR, "Manager asked to stop a processing. Good bye.\n");
					/*
					** Send a Full message to LogAgent.
					** ACK O:succes X:failed
					*/
					numWrite = writen(x->fd, (unsigned char *)"X", 1);
					if (numWrite != 1) {
						fq_log(x->l, FQ_LOG_ERROR, "writen() error. thread_num=[%d] fd=[%d] numWrite=[%d]", x->id, x->fd, numWrite);
						break;
					}

					SAFE_FREE(buf);
					goto stop;
				}
				else {
					fq_log(x->l, FQ_LOG_ERROR, "MQ error. We send a fail signal(ACK) to LogAgent and Stop Process. Emergency Status.");
					/*
					** Send a Full message to LogAgent.
					** ACK O:succes X:failed
					*/
					numWrite = writen(x->fd, (unsigned char *)"X", 1);
					if (numWrite != 1) {
						fq_log(x->l, FQ_LOG_ERROR, "writen() error. thread_num=[%d] fd=[%d] numWrite=[%d]", x->id, x->fd, numWrite);
						break;
					}
					SAFE_FREE(buf);
					goto stop;
				}
			}
			else {
				fq_log(x->l, FQ_LOG_DEBUG, "en queue success! rc=[%d] en_data_len=[%d]", rc, en_data_len);
				/*
				** Send a Full message to LogAgent.
				** ACK O:succes 
				*/
				numWrite = writen(x->fd, (unsigned char *)"O", 1);
				if (numWrite != 1) {
					fq_log(x->l, FQ_LOG_ERROR, "writen() error. thread_num=[%d] fd=[%d] numWrite=[%d]", x->id, x->fd, numWrite);
					SAFE_FREE(buf);
					goto stop;
				}
				break; /* enQ ending */
			}
		}

		rc = SAFE_unlock_singleton(l, m);
		CHECK( rc >= 0 );

		SAFE_FREE(buf);

		/*
		** ACK O:succes X:failed
		*/
		numWrite = writen(x->fd, (unsigned char *)"O", 1);
        if (numWrite != 1) {
			fq_log(x->l, FQ_LOG_ERROR, "writen() error. thread_num=[%d] fd=[%d] numWrite=[%d]", x->id, x->fd, numWrite);
			break;
		}

		fq_log(x->l, FQ_LOG_DEBUG, "[%d]-th thread: Responsed: numWrite=[%d]", x->id,  numWrite);
    }

stop:

	L4_connection_update(x->p, x->index, DECREASE_COUNT);
	// x->p->L4[x->index].connections--;

	fq_log(x->l, FQ_LOG_ERROR, "thread exit. thread_num=[%d] fd=[%d].", x->id, x->fd);

	close(x->fd);
	x->l = NULL;
	x->fd = -1;
	x->status = STATUS_IDLE;

	FQ_TRACE_EXIT(x->l);
	pthread_exit((void *)0);
    return((void *)0);
}
int view_sock_receive_buffer(int r_sock)
{
    char    buf[4096];
    int n;

#ifdef MSG_DONTWAIT
    n = recv(r_sock, buf, sizeof(buf)-1, MSG_PEEK|MSG_DONTWAIT);
#else
    n = recv(r_sock, buf, sizeof(buf)-1, MSG_PEEK);
#endif
    buf[n]=0x00;
    return(n);
}
