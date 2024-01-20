/*
** fq_agent.c
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

int g_health_check_sec;

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


static void *deQ_and_send_thread(void *arg)
{
	thread_param_t *tp = (thread_param_t *)arg;
	fqueue_obj_t *obj=NULL;
	fq_logger_t *l = tp->l;
	int rc;

	/*
	** Open File Queue 
	*/
	rc =  OpenFileQueue(l, NULL, NULL, NULL, NULL, tp->local_qpath, tp->local_qname, &obj);
	if( rc != TRUE ) 
		errExit("OpenFileQueue( '%s', '%s') error.\n", tp->local_qpath, tp->local_qname);

	fq_log(l, FQ_LOG_DEBUG, "MQ open success. [%s/%s]", tp->local_qpath, tp->local_qname);

	/*
	** Connect and Open queue.
	*/
	tcp_client_obj_t *tcpobj=NULL;
	char    *skey=NULL;
	char    result;

	fprintf(stdout, "Connecting to fq_handler_json server.....\n");
	rc = FQ_link_json(l, 100, tp->handler_ip_1, tp->handler_ip_2, tp->handler_port, &tcpobj, tp->remote_qpath, tp->remote_qname, &result, &skey );
	if( rc < 0 || result != 'Y') {
		fq_log(l, FQ_LOG_ERROR, "failed to connect at the fq_handler_json server.");
		goto stop;
	}
	fq_log(l, FQ_LOG_DEBUG, "FQ_link OK. skey=[%s]", skey);

	print_current_time();

	double passed_time;

	start_timer();
	while(1) {
		long l_seq=0L;
		long run_time=0L;

		unsigned char *buf=NULL;
		int n;
		int body_size=0;
		char ack_buf[2];
		int rc;
		int buffer_size = MAX_MSG_LENGTH;
		char	unlink_filename[512];

		buf = calloc(buffer_size, sizeof(unsigned char));
		if(!buf) {
			fq_log(l, FQ_LOG_ERROR, "calloc() error.");
			break;
		}

		memset(unlink_filename, 0x00, sizeof(unlink_filename));
		rc = obj->on_de_XA(l, obj, buf, buffer_size, &l_seq, &run_time, unlink_filename);

		if( rc == DQ_EMPTY_RETURN_VALUE ) {
			passed_time = end_timer();
			if( passed_time > g_health_check_sec ) {
				int health_rc;
				health_rc = FQ_health_check_json(l, tcpobj, skey, tp->remote_qpath, tp->remote_qname, &result);
				if( health_rc < 0 ) {
					fq_log( l, FQ_LOG_ERROR, "health checking error. rc=[%d]", health_rc);
				 	break;
				}
				else {
					fq_log( l, FQ_LOG_DEBUG, "health checking OK,");
				}
				fq_log(l, FQ_LOG_DEBUG, "There is no data.(empty)");
				start_timer();
			}
			SAFE_FREE(buf);
			usleep(1000000);
			continue;
		}
		else if( rc < 0 ) {
			if( rc == MANAGER_STOP_RETURN_VALUE ) {
                fq_log(l, FQ_LOG_ERROR, "Manager asked to stop a processing.\n");
				SAFE_FREE(buf);
                break;
            }
            fq_log(l, FQ_LOG_ERROR, "on_de_XA() error. rc=[%d]", rc);
			SAFE_FREE(buf);
            break;
		}
		else {
			if( buf[0] == 0x00) {
				int commit_rc;
				fq_log(l, FQ_LOG_ERROR, "deQ_XA() data is null. We don't send it. and will throw it and commit.");
				commit_rc = obj->on_commit_XA(l, obj, l_seq, &run_time, unlink_filename);
				if(commit_rc < 0) {
					fq_log(l, FQ_LOG_ERROR, "fatal: XA_commit() failed.");
					goto stop;
				}
				fq_log(l, FQ_LOG_DEBUG, "XA commit success!,");
				SAFE_FREE(buf);
				start_timer();
				continue;
			}
			else {
				while(1) {
					int try_count = 0;
					rc =  FQ_put_json(l, tcpobj, (unsigned char *)buf, rc, skey, tp->remote_qpath, tp->remote_qname,  &result);
					if( rc < 0 ) {
						obj->on_cancel_XA(l, obj, l_seq, &run_time);
						fq_log( l, FQ_LOG_ERROR, "XA_canceled. en queue error. rc=[%d]. Processing will be stop.", rc);
						SAFE_FREE(buf);
						return 0;
					}
					else if( result == 'F' || rc == EQ_FULL_RETURN_VALUE ) { /* full */
						fq_log(l, FQ_LOG_DEBUG, "queue is full. ..retry [%d]-th..", ++try_count);
						usleep(1000000);
						continue;
					}
					else { /* success */
						int commit_rc ;

						passed_time = end_timer();
						fq_log(l, FQ_LOG_DEBUG, "Received ACK from LogCollector: Success.");
						commit_rc = obj->on_commit_XA(l, obj, l_seq, &run_time, unlink_filename);
						if(commit_rc < 0) {
							fq_log(l, FQ_LOG_ERROR, "fatal: XA_commit() failed.");
							goto stop;
						}
						fq_log(l, FQ_LOG_DEBUG, "XA commit success!,");
						SAFE_FREE(buf);
						start_timer();
						break;
					}
				}
			} 
		}
	}

stop:

	print_current_time();

	rc = CloseFileQueue(l, &obj);
	CHECK(rc==TRUE);

	fq_log(l, FQ_LOG_ERROR, "Process was stopped. pid=[%d]\n", getpid());

	pthread_exit( (void *)0);
}

static void *recv_and_enQ_thread(void *arg)
{
	thread_param_t *tp = (thread_param_t *)arg;
	fqueue_obj_t *obj=NULL;
	fq_logger_t *l = tp->l;
	int rc;

	/*
	** Open File Queue 
	*/
	rc =  OpenFileQueue(l, NULL, NULL, NULL, NULL, tp->local_qpath, tp->local_qname, &obj);
	if( rc != TRUE ) 
		errExit("OpenFileQueue( '%s', '%s') error.\n", tp->local_qpath, tp->local_qname);

	fq_log(l, FQ_LOG_DEBUG, "MQ open success. [%s/%s]", tp->local_qpath, tp->local_qname);


	/*
	** Connect and Open queue.
	*/
	tcp_client_obj_t *tcpobj=NULL;
	char    *skey=NULL;
	char    result;

	fprintf(stdout, "Connecting to fq_handler_json server.....\n");
	rc = FQ_link_json(l, 100, tp->handler_ip_1, tp->handler_ip_2, tp->handler_port, &tcpobj, tp->remote_qpath, tp->remote_qname, &result, &skey );
	if( rc < 0 || result != 'Y') {
		fq_log(l, FQ_LOG_ERROR, "failed to connect at the fq_handler_json server.");
		goto stop;
	}
	fq_log(l, FQ_LOG_DEBUG, "FQ_link OK. skey=[%s]", skey);

	print_current_time();

	double passed_time;

	start_timer();
	while(1) {
		int rc;
		unsigned char *qdata=NULL;
		int qdata_len=0;

		rc = FQ_get_json(l, tcpobj, skey, tp->remote_qpath, tp->remote_qname, &qdata, &qdata_len, &result);
		if( rc < 0 ) {
			fq_log(l, FQ_LOG_ERROR, "FQ_get_json() error. rc=[%d].", rc);
			break;
		}
		else if( result == 'E' ) {
			passed_time = end_timer();
			if( passed_time > 5.0 ) {
				fq_log(l, FQ_LOG_DEBUG, "empty.");
				start_timer();
			}
			SAFE_FREE(qdata);
			usleep(1000000); /* 1 sec  */
			continue;
		}
		else {
			int en_rc;	
			en_rc =  uchar_enQ(l, qdata, rc, obj);
			if( rc < 0 ) {
				fq_log(l, FQ_LOG_ERROR, "uchar_enQ() error.");
				break;
			}
			fq_log(l, FQ_LOG_DEBUG, "enQ success. rc=[%d]. qdata=[%s]", en_rc, qdata);
			SAFE_FREE(qdata);
			start_timer();
		}
	}

stop:
	print_current_time();

	rc = CloseFileQueue(l, &obj);
	CHECK(rc==TRUE);

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
	int		i_log_level;
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

    if( (my_ip = SZ_toString( &conf, "agent.ip") ) == 0 )
        errExit("not found.(agent.ip) in the %s.\n", config_filename);

    if( (g_health_check_sec = SZ_toInteger( &conf, "agent.health_check_sec") ) == 0 )
        errExit("not found.(agent.health_check_sec) in the %s.\n", config_filename);

    if( (collector_ip_1 = SZ_toString( &conf, "collector.ip_1") ) == 0 )
        errExit("not found.(collector.ip_1) in the %s.\n", config_filename);

    if( (collector_ip_2 = SZ_toString( &conf, "collector.ip_2") ) == 0 )
        errExit("not found.(collector.ip_2) in the %s.\n", config_filename);

    if( (collector_port = SZ_toString( &conf, "collector.port") ) == 0 )
        errExit("not found.(collector.port) in the %s.\n", config_filename);

	i_log_level = get_log_level(log_level);
    rc = fq_open_file_logger(&l, log_filename, i_log_level);
    if( !(rc > 0) )
        errExit("fq_open_file_logger() error.\n");

    fprintf(stdout, "Log file opened successfully. From now, See logfile(%s).\n", log_filename);

	/*
	** Make a linkedlist for threads
	*/
	
	bool tf;

	linkedlist_t *ll;
	ll = linkedlist_new("thread_list");
	
	tf = MakeLinkedList_threads_mapfile( l, fq_threads_mapfile, 0x20, ll);
	CHECK(tf);


	thread_param_t *t_params=NULL;
	t_params = calloc( ll->length, sizeof(thread_param_t));
    CHECK(t_params);


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
			if( pthread_create( tids+t_no, NULL, &deQ_and_send_thread, p) ) {
				fq_log(l, FQ_LOG_ERROR, "pthread_create() error. [%d]-th.",  t_no);
				exit(0);
			}
		}
		else if( strcmp(p->action, "RECV") == 0 ) {
			if( pthread_create( tids+t_no, NULL, &recv_and_enQ_thread, p) ) {
				fq_log(l, FQ_LOG_ERROR, "pthread_create() error. [%d]-th.",  t_no);
				exit(0);
			}
		}
		else {
			printf("action code error. [%s]\n", p->action);
			continue;
		}
		sleep(1);

	}

	for(t_no=0; t_no < ll->length ; t_no++) {
        if( pthread_join( *(tids+t_no) , &thread_rtn)) {
            printf("pthread_join() error.");
            return(-1);
        }
    }

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
