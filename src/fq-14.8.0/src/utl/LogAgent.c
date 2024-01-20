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

#define MAX_FQ_MESSAGE_SIZE 65535

#define MAX_AGENT_SENDTIME_LEN 	(14)
#define MAX_AGENT_HOSTNAME_LEN 	(32)
#define MAX_AGENT_IP_ADDR_LEN 	(15)

#define FULL_RESEND_WAIT_TIME (1) // second

static void print_help(char *p);

int main(int argc, char **argv)
{
	int sock;
	struct sockaddr_in serv_addr;
	unsigned char header[FQ_HEADER_SIZE+1];

	int ch;
	SZ_toDefine_t conf;
	char    *config_filename=NULL;
	fqueue_obj_t *obj=NULL;
	char	*mq_path=NULL;
	char	*mq_name=NULL;
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
	**  sock = socket(PF_INET, SOCK_STREAM, 0);
	**  sock = socket(AF_INET, SOCK_STREAM, 0);
	*/
	sock = socket(PF_INET, SOCK_STREAM, 0);

	if( sock == -1 ) 
		errExit( "socket() error");

	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family=AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(collector_ip);
	serv_addr.sin_port = htons(atoi(collector_port));

	if( connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
		errExit( "connect() error");

	print_current_time();
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

		unsigned char header[FQ_HEADER_SIZE+1];

		buf = calloc(buffer_size, sizeof(unsigned char));
		if(!buf) {
			fq_log(l, FQ_LOG_ERROR, "calloc() error.");
			break;
		}

		// rc = obj->on_de(l, obj, buf, buffer_size, &l_seq, &run_time);
		memset(unlink_filename, 0x00, sizeof(unlink_filename));
		rc = obj->on_de_XA(l, obj, buf, buffer_size, &l_seq, &run_time, unlink_filename);

		if( rc == DQ_EMPTY_RETURN_VALUE ) {
			SAFE_FREE(buf);
			usleep(100000);
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
			char date[9], time[7];
			char hostname[MAX_AGENT_HOSTNAME_LEN+1];
			int  data_start_offset=0;

			fq_log(l, FQ_LOG_DEBUG, "on_de() success. rc=[%d]", rc);

resend:
			get_time(date, time);
			gethostname(hostname, sizeof(hostname));


			/* shift 61 bytes() to rigth */
			data_start_offset = MAX_AGENT_SENDTIME_LEN + MAX_AGENT_HOSTNAME_LEN + MAX_AGENT_IP_ADDR_LEN;
			memcpy( buf+FQ_HEADER_SIZE+data_start_offset, buf, rc);
			memset( buf, 0x20, FQ_HEADER_SIZE+data_start_offset); /* clear protocol arean with space */
			memcpy( buf+FQ_HEADER_SIZE, date, 8);
			memcpy( buf+FQ_HEADER_SIZE+8, time, 6);
			memcpy( buf+FQ_HEADER_SIZE+MAX_AGENT_SENDTIME_LEN, hostname, strlen(hostname));
			memcpy( buf+FQ_HEADER_SIZE+MAX_AGENT_SENDTIME_LEN+MAX_AGENT_HOSTNAME_LEN, my_ip, strlen(my_ip));

			body_size = rc+data_start_offset;
			set_bodysize(header, FQ_HEADER_SIZE, body_size, 0);
			memcpy(buf, header, FQ_HEADER_SIZE);

			fq_log(l, FQ_LOG_DEBUG, "part of protocol->[%-61.*s]", data_start_offset, buf+FQ_HEADER_SIZE);

			n = writen(sock, buf, FQ_HEADER_SIZE+body_size);
			if( n != (FQ_HEADER_SIZE+body_size) ) {
				fq_log(l, FQ_LOG_ERROR, "socket body writen() error. n=[%d]. Check LogCollector Process.", n);
				SAFE_FREE(buf);
				break;
			}

			/* receive ACK of only a charactor */
			memset(ack_buf, 0x00, sizeof(ack_buf));
			n = readn(sock, ack_buf, 1);
			if( n < 0 ) {
				fq_log(l, FQ_LOG_ERROR, "readn() error. n=[%d]. Check LogCollector Process.", n);
				SAFE_FREE(buf);
				break;
			}
			if( ack_buf[0] != 'O') {
				fq_log(l, FQ_LOG_ERROR, "ACK error. is not O.\n");
				SAFE_FREE(buf);
				break;
			}
			switch(ack_buf[0]) {
				int commit_rc;

				case 'O':
					fq_log(l, FQ_LOG_DEBUG, "Received ACK from LogCollector: Success.");
					commit_rc = obj->on_commit_XA(l, obj, l_seq, &run_time, unlink_filename);
					if(commit_rc < 0) {
						fq_log(l, FQ_LOG_ERROR, "fatal: XA_commit() failed.");
						goto stop;
					}
					fq_log(l, FQ_LOG_DEBUG, "XA commit success!,");
					break;
				case 'F':
					fq_log(l, FQ_LOG_ERROR, "Received ACK from LogCollector: Full. We re-send this message after [%s] sec.", FULL_RESEND_WAIT_TIME);
					sleep(FULL_RESEND_WAIT_TIME);
					goto resend;
				case 'X':
					fq_log(l, FQ_LOG_ERROR, "Received ACK from LogCollector: Collector Failure. We stop process. Good-bye.");
					obj->on_cancel_XA(l, obj, l_seq, &run_time);
					goto stop;
				default:
					fq_log(l, FQ_LOG_ERROR, "Received ACK from LogCollector: Unknown ACK[%c]. We stop process. Good-bye.", ack_buf[0]);
					obj->on_cancel_XA(l, obj, l_seq, &run_time);
					goto stop;
			}
			continue;
		}
	}

stop:

	print_current_time();
	close(sock);

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
