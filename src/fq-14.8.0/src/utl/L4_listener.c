#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
// #include <arpa/types.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/uio.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/shm.h>

// #include "scm_rights.h"
#include "fq_common.h"
#include "fq_l4lib.h"
#include "fq_unix_sockets.h"
// #include "miscellaneous.h"

#define BUFSIZE 1024
#define HEADER_SIZE 4
#define TEST_COUNT  10000000
#define CURRENT_SERVERS 2

typedef struct _time_stamp_thread_args {
    int shmkey;
} time_stamp_thread_args_t;

void error_handling(char *message);
static void *time_stamp_thread( void *arg );

static int init_shm(Heartbeat_Table_t *p, char *L4_switch_name, int listen_port, load_balance_type_t type);
int	type=0;
static int  find_L4_port(Heartbeat_Table_t *p);

int main(int ac, char **av)
{
	void  *sptr = NULL;   /* SH MEM Addr  */
	Heartbeat_Table_t *p=NULL;
	int serv_sock;
	int clnt_sock;
	FILE *readFP;
	FILE *writeFP;
	time_stamp_thread_args_t	t;

	char message[BUFSIZE];

	struct	sockaddr_in serv_addr;
	struct  sockaddr_in clnt_addr;
	int		clnt_addr_size;
	int		count=0;

	/* Shared memory */
	// L4_SessionTable_t *p;
	key_t     key = 5678; /* key value is 0x0000162e */
	int     sid = -1;
	int		size;
	int	rc;

	struct timespec ts;
	struct timeval now;


	/* for setsockopt() */
	struct linger option;
	int one = 1;

	/* UDP send */
	int	udp_fd;
	int current_servers;
	int	connect_cnt = 0;
	int	server_index;

	char	L4_switch_name[32];

	/* time_stamp_thread */
	pthread_t	tid;

	if(ac != 4 ) {
		printf("Usage: %s <L4_switch_name> <listen port> <loadbalancing type>(RR or LC)\n", av[0]);
		printf("                       RR: Round Robin\n");
		printf("                       LC: least Connection\n");
		exit(1);
	}

	if( (strcmp(av[3], "RR") != 0) && (strcmp(av[3], "LC") != 0)) {
		printf("illegal type.\n");
		exit(0);
	}
	else {
		if( strcmp(av[3], "RR") == 0 ) {
			type = LOAD_BALANCE_RR;
		}
		else if ( strcmp(av[3], "LC") == 0 ) {
			type = LOAD_BALANCE_LC;
		}
		else { 
			printf("illegal type.\n");
			exit(0);
		}
	}

	SetLimit(2048);

	/* shared memory */
	size = sizeof(Heartbeat_Table_t);
	sid = shmget(key, size, 0666 | IPC_CREAT);
	CHECK(sid != -1);

	sptr = shmat(sid, 0, 0);
    CHECK(sptr != (void *)-1);

    p = (Heartbeat_Table_t *)sptr;

	/* init shm */
	sprintf(L4_switch_name, "%s", av[1]);
	init_shm(p, L4_switch_name, atoi(av[2]), type);

	t.shmkey = key;
	if( pthread_create(&tid, NULL, time_stamp_thread, &t))  {
		fprintf(stderr, "pthread_create() error. \n");
		return(-1);
	}

	// serv_sock = socket(FP_INET, SOCK_STREAM, 0);
	serv_sock = socket(AF_INET, SOCK_STREAM, 0);
	if( serv_sock == -1 ) 
		error_handling( "socket() error");

	
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family=AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(atoi(av[2]));

	if ( setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, (const void *)&one, sizeof(one)) < 0 ) {
		fprintf(stderr, "Can't set socket option(SO_REUSEADDR) for sockfd=%d, reason=%s.", serv_sock, strerror(errno));
		close(serv_sock);
		return(-1);
	}

	option.l_onoff = 1; /* on */
	option.l_linger = 0; /* When You close a socket, Closing is delay for 3 sec  until data is sent. */ 
	if ( setsockopt(serv_sock, SOL_SOCKET, SO_LINGER, (const void *)&option, sizeof(option)) < 0 ) {
		fprintf(stderr, "Can't set socket option(SO_LINGER) for sockfd=%d, reason=%s.", serv_sock, strerror(errno));
		close(serv_sock);
		return(-1);
	}
	
	if( bind(serv_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
		error_handling( "bind() error");

	if(listen(serv_sock, 5) == -1) 
		error_handling( "listen() error");

	clnt_addr_size = sizeof(clnt_addr);

	current_servers = CURRENT_SERVERS;

	while(1) {
		int rc;
		int useDatagramSocket = 1;
		int	index_port;

		clnt_sock = accept( serv_sock, (struct sockaddr *)&clnt_addr, &clnt_addr_size);
		if( clnt_sock == -1 ) {
			error_handling("accept() error.");
		}

		option.l_onoff = 1; /* on */
		option.l_linger = 0; /* When You close a socket, Closing is delay for 3 sec  until data is sent. */ 
		if ( setsockopt(clnt_sock, SOL_SOCKET, SO_LINGER, (const void *)&option, sizeof(option)) < 0 ) {
			fprintf(stderr, "Can't set socket option(SO_LINGER) for sockfd=%d, reason=%s.", clnt_sock, strerror(errno));
			// close(clnt_sock);
			// return(-1);
		}

		index_port = find_L4_port(p);
		if( index_port < 0 ) {
			fprintf(stderr, "Can't found available port.\n");
			close(clnt_sock);
			continue;
		}
		fprintf(stdout, "Found available port. index=[%d]\n", index_port);

		// udp_fd = unixConnect(SOCK_PATH, useDatagramSocket ? SOCK_DGRAM : SOCK_STREAM);
		udp_fd = unixConnect(p->L4[index_port].sock_path, useDatagramSocket ? SOCK_DGRAM : SOCK_STREAM);

		if( udp_fd == -1) {
			fprintf(stderr, "unixConnect() error. Can't connect to real server[%s]. \n", p->L4[index_port].sock_path);
			memset(p->L4[index_port].sock_path, 0x00, sizeof(p->L4[index_port].sock_path));
			p->L4[index_port].pid = -1;
			close(clnt_sock);
			continue;
		}
		fprintf(stdout, "I will share clnt_sock with a process[%d]\n", p->L4[index_port].pid);

		rc = sendfd(udp_fd, clnt_sock);

		if ( rc == 0) {
			fprintf(stderr, "sendfd() error.\n");
			fprintf(stdout, "sockfd sharing success.\n");
			//break;
			memset(p->L4[index_port].sock_path, 0x00, sizeof(p->L4[index_port].sock_path));
			p->L4[index_port].pid = -1;
			close(clnt_sock);
			continue;
		}
		fprintf(stdout, "sockfd sharing success.\n");

		close(clnt_sock);
		close(udp_fd);
	}
	close(serv_sock);
	return(0);
}

void error_handling(char *msg)
{
	fputs(msg, stderr);
	fputc('\n', stderr);
	exit(1);
}

static int init_shm(Heartbeat_Table_t *p, char *L4_switch_name, int listen_port, load_balance_type_t type)
{
	int i;
	
	p->listen_port = listen_port;
	sprintf(p->L4_switch_name, "%s", L4_switch_name);
	p->listener_pid = getpid();
	p->type = type;
	p->next_port = 0;
	p->touch_time = time(0);

	for(i=0; i< MAX_PORTS; i++) {
		memset(p->L4[i].sock_path, 0x00, sizeof(p->L4[i].sock_path));
		p->L4[i].pid = -1;
		p->L4[i].last_balance_time = time(0);
		p->L4[i].touch_time = 0L;
		p->L4[i].connections = 0;
	}

	// pthread_mutexattr_init( &p->mtx_attr );
	// pthread_mutex_init(&p->mtx, &p->mtx_attr);
	pthread_mutex_init(&p->mtx, NULL);

	return 0;
}

static int oldest_port(Heartbeat_Table_t *p)
{
	int i;
	int oldest_index = -1;

	if( p->next_port >= MAX_PORTS ) p->next_port = 0;

	for(i=p->next_port; i<MAX_PORTS; i++) {
		if( !p->L4[i].sock_path[0] ) continue;	
		if( is_exceeded_sec(p->L4[i].touch_time, 3) ) continue;
		p->next_port = i+1;
		p->L4[i].last_balance_time = time(0);
		return(i);
	}
	for(i=0; i< MAX_PORTS; i++) {
		if( !p->L4[i].sock_path[0] ) continue;	
		if( is_exceeded_sec(p->L4[i].touch_time, 3) ) continue;
		p->next_port = i+1;
		p->L4[i].last_balance_time = time(0);
		return(i);
	}
	return(oldest_index);
}

static int  find_L4_port(Heartbeat_Table_t *p )
{
	int i;
	time_t	current_time, diff_sec;

	pthread_mutex_lock(&p->mtx);
	if( p->type == LOAD_BALANCE_RR ) {
		pthread_mutex_unlock(&p->mtx);
		return(oldest_port(p));	
	}
	else {
		fprintf(stderr, "not support balancing type.\n");
		pthread_mutex_unlock(&p->mtx);
		return(-1);
	}
}

static void *time_stamp_thread( void *arg )
{
	time_stamp_thread_args_t *t = (time_stamp_thread_args_t *)arg;
	int size;
	int	sid;
	Heartbeat_Table_t *p=NULL;

	size = sizeof(Heartbeat_Table_t);
	sid = shmget(t->shmkey, size, 0666 | IPC_CREAT);
	if( sid == -1 ) {
		fprintf(stderr, "shmget() error.\n");
		pthread_exit((void *)0);
	}

	p = shmat(sid, 0, 0);
	if( p == (void *)-1 ) {
		fprintf(stderr, "shmget() error.\n");
		pthread_exit((void *)0);
	}

	while(1) {
		pthread_mutex_lock(&p->mtx);
		p->touch_time = time(0);
		pthread_mutex_unlock(&p->mtx);
		sleep(1);
	}
	pthread_exit((void *)0);
}
