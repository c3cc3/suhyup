/*
** fq_disaster.c
** It communicates with fq_handler_json server and can de-queue or en-queue to remote server.
** 
** Author: gwisang.choi@gmail.
** All right reserved by CLANG Co.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
// #include <arpa/types.h>

#include "fqueue.h"
#include "fq_common.h"
#include "fq_multi_config.h"
#include "fq_logger.h"
#include "fq_error_functions.h"
#include "fq_socket.h"
#include "fq_tcp_client_obj.h"
#include "fq_common.h"
#include "fq_protocol.h"
#include "fq_timer.h"
#include "parson.h"

#include "fq_file_list.h"
#include "fq_scanf.h"

#define MAX_MSG_LENGTH (65535*10)

#define MAX_AGENT_SENDTIME_LEN 	(14)
#define MAX_AGENT_HOSTNAME_LEN 	(32)
#define MAX_AGENT_IP_ADDR_LEN 	(15)

#define FULL_RESEND_WAIT_TIME (1) // second

hashmap_obj_t *heartbeat_hash_obj=NULL;

bool check_remote_server_status(fq_logger_t *l, char *remote_ip, char *remote_port);

typedef struct _thread_param {
	fq_logger_t	*l;
	char		*action;
	char		*local_qpath;
	char		*local_qname;
	char		*remote_qpath;
	char		*remote_qname;
	char		*handler_ip_1;
	char		*handler_ip_2;
	char		*handler_port;
} thread_param_t;

typedef struct _threads_actions_t threads_actions_t;
struct _threads_actions_t {
	char 	action[5];
	char	local_qpath[256]; // deQ
	char	local_qname[256];
	char	remote_qpath[256]; // enQ
	char	remote_qname[256];
};

static void print_help(char *p);

static bool MakeLinkedList_threads_mapfile( fq_logger_t *l, char *mapfile, char delimiter, linkedlist_t *ll);
static int uchar_enQ(fq_logger_t *l, unsigned char *data, size_t data_len, fqueue_obj_t *obj);

bool append_mypid_fq_pid_listfile( fq_logger_t *l, char *file );
bool heartbeat_ums(fq_logger_t *l, hashmap_obj_t *hash_obj, char *progname);

static void *moveQ_thread(void *arg)
{
	thread_param_t *tp = (thread_param_t *)arg;
	fq_logger_t *l = tp->l;
	int moved_count=0;
	int move_req_count=10000000;

	while(1) {
		// moved_count = MoveFileQueue_XA( l, from_path, from_qname, to_path, to_qname, move_req_count);
		moved_count = MoveFileQueue( l, tp->local_qpath, tp->local_qname, tp->remote_qpath, tp->remote_qname, move_req_count);
		if( moved_count < 0 ) {
			fq_log(l, FQ_LOG_ERROR, "MoveFileQueue() error. current_moved_count=[%d].", moved_count);
			break;
		}
		else {
			fq_log(l, FQ_LOG_INFO, "moved count = [%d].", moved_count);
			if(moved_count == 0) {
				fq_log(l, FQ_LOG_INFO, "Finished.( from: %s,%s -> to: %s,%s", tp->local_qpath, tp->local_qname, tp->remote_qpath, tp->remote_qname);
				break;
			}
			sleep(1);
			continue;
		}
	}
	fq_log(l, FQ_LOG_ERROR, "move thread('%s','%s' -> '%s','%s') was stopped. pid=[%d]\n", tp->local_qpath, tp->local_qname, tp->remote_qpath, tp->remote_qname,  getpid());

	pthread_exit( (void *)0);

}

int main(int argc, char **argv)
{

	int ch;
	SZ_toDefine_t conf;
	char    *config_filename=NULL;
	fqueue_obj_t *obj=NULL;

	char	*fq_threads_mapfile=NULL;

	char 	*collector_ip_1=NULL;
	char 	*collector_ip_2=NULL;
	char 	*collector_port=NULL;
	char	*log_filename=NULL;
	char	*log_level=NULL;
	char	*my_ip=NULL;
	char	*heartbeat_hash_map_path=NULL;
	char	*heartbeat_hash_map_name=NULL;
	char	*fq_pid_list_file=NULL;
	int		i_log_level;
	char	*remote_handler_ip=NULL;
	char	*remote_handler_port=NULL;
	char 	*remote_handler_check_count=0;
	fq_logger_t	*l=NULL;
	int		rc;

	if(argc != 3 ) 
		errExit("Usage: %s -f [config_filename]\n", argv[0]);

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

	SZ_toInit(&conf);

	if(SZ_fromFile( &conf, config_filename) <= 0)
        errExit("Confirm your configuration file name and path.\n");

	if( (log_filename = SZ_toString( &conf, "agent.log.filename") ) == 0 )
        errExit("not found.(agent.log.filename) in the %s.\n", config_filename);

    if( (log_level = SZ_toString( &conf, "agent.log.level") ) == 0 )
        errExit("not found.(agent.log.level) in the %s.\n", config_filename);

	if(SZ_fromFile( &conf, config_filename) <= 0)
		errExit("Confirm your configuration file name and path.\n");

	if( (fq_threads_mapfile = SZ_toString( &conf, "agent.mq.fq_threads_mapfile") ) == 0 )
        errExit("not found.(agent.mq.fq_threads_mapfile) in the %s.\n", config_filename);

	if( (heartbeat_hash_map_path = SZ_toString( &conf, "agent.hash.heartbeat_hash_map_path") ) == 0 )
        errExit("not found.(agent.hash.heartbeat_hash_map_path) in the %s.\n", config_filename);
	if( (heartbeat_hash_map_name = SZ_toString( &conf, "agent.hash.heartbeat_hash_map_name") ) == 0 )
        errExit("not found.(agent.hash.heartbeat_hash_map_name) in the %s.\n", config_filename);
	if( (fq_pid_list_file = SZ_toString( &conf, "agent.hash.fq_pid_list_file") ) == 0 )
        errExit("not found.(agent.hash.fq_pid_list_file) in the %s.\n", config_filename);

    if( (my_ip = SZ_toString( &conf, "agent.ip") ) == 0 )
        errExit("not found.(agent.ip) in the %s.\n", config_filename);

    if( (collector_ip_1 = SZ_toString( &conf, "collector.ip_1") ) == 0 )
        errExit("not found.(collector.ip_1) in the %s.\n", config_filename);

    if( (collector_ip_2 = SZ_toString( &conf, "collector.ip_2") ) == 0 )
        errExit("not found.(collector.ip_2) in the %s.\n", config_filename);

    if( (collector_port = SZ_toString( &conf, "collector.port") ) == 0 )
        errExit("not found.(collector.port) in the %s.\n", config_filename);

    if( (remote_handler_ip = SZ_toString( &conf, "remote_handler.ip") ) == 0 )
        errExit("not found.(remote_handler.ip) in the %s.\n", config_filename);
    if( (remote_handler_port = SZ_toString( &conf, "remote_handler.port") ) == 0 )
        errExit("not found.(remote_handler.port) in the %s.\n", config_filename);

    if( (remote_handler_check_count = SZ_toString( &conf, "remote_handler.check_count") ) == 0 )
        errExit("not found.(remote_handler.check_oount) in the %s.\n", config_filename);

	int i_remote_handler_check_count;
	i_remote_handler_check_count = atoi(remote_handler_check_count);

	i_log_level = get_log_level(log_level);
    rc = fq_open_file_logger(&l, log_filename, i_log_level);
    if( !(rc > 0) )
        errExit("fq_open_file_logger() error.\n");

    fprintf(stdout, "Log file opened successfully. From now, See logfile(%s).\n", log_filename);

	/*
	** Make a linkedlist for threads
	*/
	while(1) {
		bool tf;


		int remote_handler_error_count = 0;
		int n;
		for( n=0; n<i_remote_handler_check_count; n++) {
			tf = check_remote_server_status(l, remote_handler_ip, remote_handler_port);
			if( tf == false ) {
				remote_handler_error_count++;
				fq_log(l, FQ_LOG_EMERG, "We cannot connect to Remote server. [%d]-th error.", remote_handler_error_count);
				sleep(5);
			}
			else {
				fq_log(l, FQ_LOG_INFO, "Remote server(%s) is normal. We reset error count.", remote_handler_ip);
				remote_handler_error_count = 0; // reset count
				sleep(5);
			}
			sleep(5);
		}
		if( remote_handler_error_count >= i_remote_handler_check_count ) {
			fq_log(l, FQ_LOG_EMERG, "Remote server dead, [%s] conf=[%s] Start. [version]: %s %s.", argv[0], config_filename, __DATE__, __TIME__);
			break;
		}
		else {
			sleep(5);
			continue;
		}
	}
	
	bool tf;

	linkedlist_t *ll;
	ll = linkedlist_new("thread_list");
	
	tf = MakeLinkedList_threads_mapfile( l, fq_threads_mapfile, 0x20, ll);
	CHECK(tf);


	thread_param_t *t_params=NULL;
	t_params = calloc( ll->length, sizeof(thread_param_t));
    CHECK(t_params);

	rc = OpenHashMapFiles(l, heartbeat_hash_map_path, heartbeat_hash_map_name, &heartbeat_hash_obj);
	if(tf == false) {
		fq_log(l, FQ_LOG_ERROR, "Can't open ums hashmap(path='%s', name='%s'). Please check hashmap.", 
				heartbeat_hash_map_path, heartbeat_hash_map_name);
		return(0);
	}

	pthread_t *tids=NULL;
	void *thread_rtn=NULL;
	int t_no;
	ll_node_t *node_p = NULL;

	tids = calloc ( ll->length, sizeof(pthread_t));
    CHECK(tids);

	for( node_p=ll->head, t_no=0; (node_p != NULL); (node_p=node_p->next, t_no++) ) {

		threads_actions_t *tmp;
		tmp = (threads_actions_t *) node_p->value;
		printf("create thread: [%s] [%s] [%s] [%s] [%s]\n", 
			tmp->action,
			tmp->local_qpath,
			tmp->local_qname,
			tmp->remote_qpath,
			tmp->remote_qname);

		thread_param_t *p;
		p = t_params+t_no;
		p->l = l;
		p->action = strdup(tmp->action);
		p->local_qpath = strdup(tmp->local_qpath);
		p->local_qname = strdup(tmp->local_qname);
		p->remote_qpath = strdup(tmp->remote_qpath);
		p->remote_qname = strdup(tmp->remote_qname);

		p->handler_ip_1 = strdup(collector_ip_1);
		p->handler_ip_2 = strdup(collector_ip_2);
		p->handler_port = strdup(collector_port);

		if( strcmp(p->action, "SEND") == 0 ) {
			if( pthread_create( tids+t_no, NULL, &moveQ_thread, p) ) {
				fq_log(l, FQ_LOG_ERROR, "pthread_create(moveQ_thread) error. [%d]-th.",  t_no);
				exit(0);
			}
		}
		else {
			printf("action code error. [%s]\n", p->action);
			continue;
		}
		sleep(1);

	}
	append_mypid_fq_pid_listfile(l, fq_pid_list_file);

	for(t_no=0; t_no < ll->length ; t_no++) {
        if( pthread_join( *(tids+t_no) , &thread_rtn)) {
            printf("pthread_join() error.");
            return(-1);
        }
    }
	fq_log(l, FQ_LOG_INFO, "All thread stopped. Finished.");

	fq_close_file_logger(&l);

	return(0);
}

static void print_help(char *p)
{
	printf("\n\nUsage  : $ %s -f [config_file] <enter>\n", p);
	printf("example: $ %s -f /path/fq_agent.conf <enter>\n", p);
	return;
}

static bool MakeLinkedList_threads_mapfile( fq_logger_t *l, char *mapfile, char delimiter, linkedlist_t	*ll)
{
	file_list_obj_t *filelist_obj=NULL;
	FILELIST *this_entry=NULL;
	int rc;
	int t_no;

	FQ_TRACE_ENTER(l);


	fq_log(l, FQ_LOG_DEBUG, "mapfile='%s'" , mapfile);

	rc = open_file_list(l, &filelist_obj, mapfile);
	CHECK( rc == TRUE );

	printf("count = [%d]\n", filelist_obj->count);

	// filelist_obj->on_print(filelist_obj);

	this_entry = filelist_obj->head;
	for( t_no=0;  (this_entry->next && this_entry->value); t_no++ ) {
		char *errmsg = NULL;
		printf("this_entry->value=[%s]\n", this_entry->value);

		char	action[5]; // SEND, RECV
		char	local_qpath[256]; // deQ
		char	local_qname[256];
		char	remote_qpath[256]; // enQ
		char	remote_qname[256];

		int cnt;

		cnt = fq_sscanf(delimiter, this_entry->value, "%s%s%s%s%s", 
			action, local_qpath, local_qname, remote_qpath, remote_qname);

		CHECK(cnt == 5);

		fq_log(l, FQ_LOG_INFO, "action='%s', local_qpath='%s', local_qname='%s' remote_qpath='%s' remote_qname='%s'.", 
			action, local_qpath, local_qname, remote_qpath, remote_qname);

		threads_actions_t *tmp=NULL;
		tmp = (threads_actions_t *)calloc(1, sizeof(threads_actions_t));

		char	key[11];
		memset(key, 0x00, sizeof(key));
		sprintf(key, "%010d", t_no);

		sprintf(tmp->action, "%s", action);
		sprintf(tmp->local_qpath, "%s", local_qpath);
		sprintf(tmp->local_qname, "%s", local_qname);
		sprintf(tmp->remote_qpath, "%s", remote_qpath);
		sprintf(tmp->remote_qname, "%s", remote_qname);

		ll_node_t *ll_node=NULL;
		ll_node = linkedlist_put(ll, key, (void *)tmp, sizeof(char)+sizeof(threads_actions_t));
		CHECK(ll_node);

		this_entry = this_entry->next;
	}

	if( filelist_obj ) close_file_list(l, &filelist_obj);

	fq_log(l, FQ_LOG_INFO, "ll->length is [%d]", ll->length);

	FQ_TRACE_EXIT(l);
	return true;
}

static int uchar_enQ(fq_logger_t *l, unsigned char *data, size_t data_len, fqueue_obj_t *obj)
{
	int rc;
	long l_seq=0L;
    long run_time=0L;

	FQ_TRACE_ENTER(l);
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
			FQ_TRACE_EXIT(l);
			return(rc);
		}
		else {
			fq_log(l, FQ_LOG_INFO, "queue('%s/%s'): enqueued msg len(rc) = [%d].", obj->path, obj->qname, rc);
			break;
		}
	}
	FQ_TRACE_EXIT(l);
	return(rc);
}

bool append_mypid_fq_pid_listfile( fq_logger_t *l, char *file )
{
	FILE *fp=NULL;

	fp = fopen(file, "a");
	if( fp == NULL) {
		fq_log(l, FQ_LOG_ERROR, "append_mypid_fq_pid_listfile('%s'). error.", file);
		return false;
	}
	fprintf(fp, "%d\n", getpid() );
	fflush(fp);
	fclose(fp);
	fq_log(l, FQ_LOG_INFO, "append_mypid_fq_pid_listfile('%s'). OK.", file);
	return true;
}
bool heartbeat_ums(fq_logger_t *l, hashmap_obj_t *hash_obj, char *progname)
{
	FQ_TRACE_ENTER(l);

	char str_pid[10];
	char str_bintime[16];

	sprintf(str_pid, "%d", getpid());

	time_t now;
	now = time(0);
	sprintf(str_bintime,  "%ld", now);

	int rc;
	rc = PutHash(l, hash_obj, str_pid, str_bintime);
	if( rc != TRUE ) {
		fq_log(l, FQ_LOG_ERROR, "PutHash(key='%s', value='%s')", str_pid, str_bintime);
		return false;
		// obj->on_clean_table(l, seq_check_hash_obj);
	}
	FQ_TRACE_EXIT(l);

	return true;
}

bool check_remote_server_status(fq_logger_t *l, char *remote_ip, char *remote_port)
{
	tcp_client_obj_t *tcpobj = NULL;
	
	FQ_TRACE_ENTER(l);

	int rc;
	rc = open_tcp_client_obj(l, remote_ip, remote_port, BINARY_HEADER, SOCKET_HEADER_LEN, &tcpobj);
	if( rc != TRUE ) {
		fq_log(l, FQ_LOG_ERROR, "failed to connect to ip:%s, port:%s.\n", remote_ip, remote_port);
		
		FQ_TRACE_EXIT(l);
		return false;
	}
	fq_log(l, FQ_LOG_INFO, "Successfully, Connected to %s:%s", remote_ip, remote_port);

	close_tcp_client_obj(l, &tcpobj);

	FQ_TRACE_EXIT(l);
	return true;
}
