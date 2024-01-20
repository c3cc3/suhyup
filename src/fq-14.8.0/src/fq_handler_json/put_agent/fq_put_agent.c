/*
** LogAgent.c
** It reads data from local_file_queue and If there are data, Send it to LogCollector(L4 switch). 
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

#define MAX_FQ_MESSAGE_SIZE 65535

#define MAX_AGENT_SENDTIME_LEN 	(14)
#define MAX_AGENT_HOSTNAME_LEN 	(32)
#define MAX_AGENT_IP_ADDR_LEN 	(15)

#define FULL_RESEND_WAIT_TIME (1) // second

static void print_help(char *p);

int main(int argc, char **argv)
{

	int ch;
	SZ_toDefine_t conf;
	char    *config_filename=NULL;
	fqueue_obj_t *obj=NULL;

	char	*mq_path=NULL;
	char	*mq_name=NULL;

	char	*dst_mq_path=NULL;
	char	*dst_mq_name=NULL;

	char 	*collector_ip=NULL;
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

	if( (mq_path = SZ_toString( &conf, "agent.mq.path") ) == 0 )
        errExit("not found.(agent.mq.path) in the %s.\n", config_filename);

    if( (mq_name = SZ_toString( &conf, "agent.mq.name") ) == 0 )
        errExit("not found.(agent.mq.name) in the %s.\n", config_filename);

    if( (my_ip = SZ_toString( &conf, "agent.ip") ) == 0 )
        errExit("not found.(agent.ip) in the %s.\n", config_filename);

    if( (collector_ip = SZ_toString( &conf, "collector.ip") ) == 0 )
        errExit("not found.(collector.ip) in the %s.\n", config_filename);

    if( (collector_port = SZ_toString( &conf, "collector.port") ) == 0 )
        errExit("not found.(collector.port) in the %s.\n", config_filename);

	if( (dst_mq_path = SZ_toString( &conf, "collector.mq.path") ) == 0 )
        errExit("not found.(collector.mq.path) in the %s.\n", config_filename);

    if( (dst_mq_name = SZ_toString( &conf, "collector.mq.name") ) == 0 )
        errExit("not found.(collector.mq.name) in the %s.\n", config_filename);


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
		errExit("OpenFileQueue( '%s', '%s') error.\n", mq_path, mq_name);

	fq_log(l, FQ_LOG_DEBUG, "MQ open success. [%s/%s]", mq_path, mq_name);


	/*
	** Connect and Open queue.
	*/
	tcp_client_obj_t *tcpobj=NULL;
	char    *skey=NULL;
	char    result;

re_connect:

	fprintf(stdout, "Connecting to FQ_server.....\n");
	rc = FQ_link_json(l, 100, collector_ip, collector_ip, collector_port, &tcpobj, dst_mq_path, dst_mq_name, &result, &skey );
	if( rc < 0 || result != 'Y') {
		fq_log(l, FQ_LOG_ERROR, "failed to connect at the FQ_server.");
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
		int buffer_size = 65000;
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
			if( passed_time > 10.0 ) {
				fq_log(l, FQ_LOG_DEBUG, "There is no data.(empty). heath checking start");

				rc =  FQ_health_check_json(l, tcpobj, skey, dst_mq_path, dst_mq_name,  &result);
				if( rc < 0 ) {
					fq_log(l, FQ_LOG_ERROR, "heath checking errr. We will re-connect.");
					goto re_connect;
				}
				fq_log(l, FQ_LOG_DEBUG, "heath checking OK.");
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
            fq_log(l, FQ_LOG_ERROR, "on_de() error. rc=[%d]", rc);
			SAFE_FREE(buf);
            break;
		}
		else {
			while(1) {
				int try_count = 0;
				rc =  FQ_put_json(l, tcpobj, (unsigned char *)buf, rc, skey, dst_mq_path, dst_mq_name,  &result);
				if( rc < 0 ) {
					obj->on_cancel_XA(l, obj, l_seq, &run_time);
					fprintf( stderr, "en queue error. rc=[%d]. Processing will be stop. See log for detail.\n", rc);
					SAFE_FREE(buf);
					return 0;
				}
				else if( result == 'F') { /* full */
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

stop:
	print_current_time();

	rc = CloseFileQueue(l, &obj);
	CHECK(rc==TRUE);

	fq_log(l, FQ_LOG_ERROR, "Process was stopped. pid=[%d]\n", getpid());

	fq_close_file_logger(&l);

	return(0);
}

static void print_help(char *p)
{
	printf("\n\nUsage  : $ %s [-h] [-f config_file] <enter>\n", p);
	printf("example: $ %s -f /path/LogAgent.conf <enter>\n", p);
	return;
}
