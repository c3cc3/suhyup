/*
** TCP_sync_test.c
** 2019/03/07 tested by Gwisang.
** Testing result:
**	ack_mode 1: 8000 TPS
**  ack_mode 0: 38000 TPS
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "fq_logger.h"
#include "fq_common.h"
#include "sema_queue.h"
#include "fq_sequence.h"
#include "fq_socket.h"
#include "fq_timer.h"
#include "fq_tcp_client_obj.h"

#define ACK_MODE 1

#define D_LOGFILE "/tmp/TCP_sync_test.log"
#define D_USER_SEND_MSG_LEN (1020)
#define IP_ADDR_STR "172.30.1.59"
#define LISTEN_PORT (4444)
#define LISTEN_PORT_STR "4444"

void print_sleep(int n);


void usage(char *progname)
{
	printf("$ %s -[options] <enter>\n", progname);
	printf("\t options.\n");
	printf("\t -s : listening server.\n");
	printf("\t -c : connect client.\n");
}

/*
** We receiving packet that the packet includes 4 bytes binary header.
** Data staring position is 4.
*/
int CB_function( unsigned char *packet, int data_len, unsigned char *resp, int *resp_len)
{
	// fprintf(stdout, "received: len=[%d] [%s].\n", data_len, packet+SOCKET_HEADER_LEN);

	memset(resp, 'Y', 1);
	*resp_len = 1;	

	// fprintf(stdout, "response: resp =[%s] resp_len=[%d].\n", resp, *resp_len);

	return(*resp_len);
}

int main(int ac, char **av)
{
	int my_job_count = 0;
	fq_logger_t *l = NULL;
	int rc;
	double e;

	if(ac != 2 ) {
		usage(av[0]);
		return(0);
	}

	rc = fq_open_file_logger(&l, D_LOGFILE, FQ_LOG_ERROR_LEVEL);
	CHECK(rc>0);

	if(av[1][1] == 's') { // listen server
		TCP_server_t x;
#ifdef ACK_MODE
		int ack_mode = 1;
#else
		int ack_mode = 0;
#endif

		make_thread_sync_CB_server( l, &x, IP_ADDR_STR , LISTEN_PORT, BINARY_HEADER_TYPE, SOCKET_HEADER_LEN, ack_mode, CB_function);

		pause();
		return(0);
	} else if( av[1][1] == 'c') {
		tcp_client_obj_t *tcpobj=NULL;
		sequence_obj_t *sobj = NULL;
		int rc1;
		int count = 0;
		
		rc = open_tcp_client_obj(l, IP_ADDR_STR, LISTEN_PORT_STR, BINARY_HEADER, SOCKET_HEADER_LEN, &tcpobj);
		if( rc == TRUE ) {
			fq_log(l, FQ_LOG_ERROR, "failed to connect to ip:%s, port:%s.\n", IP_ADDR_STR, LISTEN_PORT_STR);
			goto L0;
		}

		rc1 = open_sequence_obj(l, ".", "sema_queue_seq", "A",  12, &sobj);
		CHECK( rc1==TRUE );

		start_timer();
		while(1) {
			unsigned char *buf = NULL;
			unsigned char *databuf = NULL;
			int ret;
			int len;
			char str_seq[21];
			int rc2;

			rc2 = sobj->on_get_sequence(sobj, str_seq, sizeof(str_seq));
			CHECK( rc2 == TRUE );

			buf = calloc(1, sizeof(char) * D_USER_SEND_MSG_LEN);
			memset(buf, 'D',  D_USER_SEND_MSG_LEN -1);
			memcpy(buf, str_seq, strlen(str_seq));

			len = D_USER_SEND_MSG_LEN ;

			ret = send_header_and_body_socket(l, tcpobj, buf, len);
			if( ret < 0 ) {
				fq_log(l, FQ_LOG_ERROR, " send_header_and_body_socket() error. ret=[%d]", ret);
				goto L0;
			}
			fq_log(l, FQ_LOG_DEBUG, "send: ret=[%d]. data=[%-80.80s]", ret, buf);
			
#ifdef ACK_MODE 
			ret = recv_header_and_body_socket( l, tcpobj, &databuf );
			if( ret < 0 ) { // failed
				fq_log(l, FQ_LOG_ERROR, "recv_header_and_body_socket() error. ret=[%d]", ret);
				break;
			} else { // success
				fq_log(l, FQ_LOG_DEBUG, "response=[%-80.80s].\n", databuf);
				SAFE_FREE(buf);
				SAFE_FREE(databuf);

				count++;
				e = end_timer();
				if( e > 1.0 ) {
					printf("[%u] TPS.\n", (unsigned)(count/e));
					start_timer();
					count = 0;
				}
			}
#else
			// success
			count++;
			e = end_timer();
			if( e > 1.0 ) {
				printf("[%u] TPS.\n", (unsigned)(count/e));
				start_timer();
				count = 0;
			}
#endif
		}
		if( sobj ) {
			close_sequence_obj( &sobj );
		}
		
	}
	else {
		usage(av[0]);
	}
L0:

	fq_close_file_logger(&l);

	return(0);
}

void print_sleep(int n)
{
	printf("[%d]-th.\n", n);
	usleep(1000);
}

int 
send_header_and_body_socket(fq_logger_t *l, tcp_client_obj_t *tcpobj, unsigned char *buf, int datalen)
{
	int	ret=-1;
	int	sent=0;
	unsigned char *header=NULL;
	unsigned char *packet=NULL;
	int packet_len=0;

	FQ_TRACE_ENTER(l);

	header = calloc(SOCKET_HEADER_LEN+1, sizeof(char));

	/* µ¥ÀÌÅÍ¸¦ Àü¼ÛÇÑ´Ù */
	ret = tcpobj->on_set_bodysize(header, tcpobj->header_len, datalen, tcpobj->header_type);
	if( ret != 0 ) {
		fq_log(l, FQ_LOG_ERROR, "on_set_bodysize() error.");
		goto L0;
	}

	packet = calloc( SOCKET_HEADER_LEN+datalen+1, sizeof(unsigned char));
	packet_len = SOCKET_HEADER_LEN+datalen;

	memcpy(packet, header, SOCKET_HEADER_LEN);
	memcpy(packet+SOCKET_HEADER_LEN, buf, datalen);

	sent = tcpobj->on_writen(l, tcpobj, (const void *)packet, packet_len);
	if(sent <= 0) {
		fq_log(l, FQ_LOG_ERROR, "packet: on_writen() error. sent=[%d].", sent);
		goto L0;
	}

	fq_log(l, FQ_LOG_DEBUG, "packet: on_writen() OK. sent=[%d].", sent);

	ret=sent;
L0:
	SAFE_FREE(header);
	SAFE_FREE(packet);

	FQ_TRACE_EXIT(l);
	return(ret);
}

int 
recv_header_and_body_socket( fq_logger_t *l, tcp_client_obj_t *tcpobj, unsigned char **databuf )
{
	unsigned char *header=NULL;
	int	bodylen=0;
	int	n;
	int	ret=-1;

	FQ_TRACE_ENTER(l);

	header = calloc(SOCKET_HEADER_LEN+1, sizeof(char));
	n = tcpobj->on_readn(l, tcpobj, header, SOCKET_HEADER_LEN);
	if( n != SOCKET_HEADER_LEN ) {
		fq_log(l, FQ_LOG_ERROR,  "header receiveing failed.");
		goto L0;
	}
	fq_log(l, FQ_LOG_DEBUG, "header: on_readn() OK. readn=[%d].", n);

	bodylen = tcpobj->on_get_bodysize(header, tcpobj->header_len, tcpobj->header_type);

	if( bodylen <= 0 || bodylen > 65536  ) {
		fq_log(l, FQ_LOG_ERROR,  "header contents is not available. bodylen=[%d].", bodylen);
		goto L0;
	}
	fq_log(l, FQ_LOG_DEBUG, "length of header contents(ACK) is [%d]-character.", bodylen);

	*databuf = calloc(bodylen+1, sizeof(char));

	n = tcpobj->on_readn(l, tcpobj, *databuf, bodylen);
	if( n != bodylen ) {
		fq_log(l, FQ_LOG_ERROR,  "body receiveing error.");
		goto L0;
	}

	fq_log(l, FQ_LOG_DEBUG, "on_readn() OK. n=[%d].\n", n);

	ret = n;
L0:
	SAFE_FREE(header);

	FQ_TRACE_EXIT(l);
	return(ret);
}
