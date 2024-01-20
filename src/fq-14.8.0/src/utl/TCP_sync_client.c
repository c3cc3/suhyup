/*
** TCP_sync_client.c
** Network library Test
** Performance Test: in localhost with TCP_sync_server, 
** result: 23000 TPS 
*/
#include <stdio.h>
#include <sys/types.h>

#include "fq_tcp_client_obj.h"
#include "fq_common.h"
#include "fq_timer.h"

#define SEND_MSG_LEN 4096

/* Global variables */
char *ip1=NULL;
char *port=NULL;
char *logname = NULL;
char *cmd=NULL;
char *path=NULL;
char *qname=NULL;
char *loglevel=NULL;
int	 i_loglevel = 0;
int	i_send_msg_len = 4096;
fq_logger_t *l=NULL;
tcp_client_obj_t *tcpobj=NULL;
char	result;
int test_count = 0;

static void print_help(char *p)
{
	printf("Description: This program connects to FQ_server and get or put message.\n");
	printf("             For testing easily, set FQ_DATA_HOME to your .profile or .bashrc or .bash_profile.\n");
	printf("			 FQ_DATA_HOME=/home/data \n");
	printf("			 export FQ_DATA_HOME \n");
	printf("\n\nUsage  : $ %s [-V] [-h] [-i 1st_ip] [-p port] [-l logname] [-o loglevel] [-m send_msg_len]  <enter>\n", p);
	printf("\t	-h: help \n");
	printf("\t	-i: ip[1st] Primary  FQ_server ip \n");
	printf("\t	-p: port : Port number that FQ_server is listening\n");
	printf("\t	-l: logfilename \n");
	printf("\t	-o: loglevel(trace|debug|info|error|emerg ) \n");
	printf("\t	-m: length of sending message. \n");

	printf("example: $ %s -i 14.58.127.137 -p 6666 -l %s.log -o error -m 4096 <enter>\n",p,p);
	return;
}

int main(int ac, char **av)  
{  
	
	double e;
	int count = 0;
	int  rc, ret, len;
	int	ch;
	// int count=0;
	unsigned char   *buf=NULL;
	unsigned char   *databuf=NULL;

	while(( ch = getopt(ac, av, "Hh:i:p:l:o:m:")) != -1) {
		switch(ch) {
			case 'H':
			case 'h':
				print_help(av[0]);
				return(0);
			case 'i':
				ip1 = strdup(optarg);
				break;
			case 'p':
				port = strdup(optarg);
				break;
			case 'l':
				logname = strdup(optarg);
				break;
			case 'o':
				loglevel = strdup(optarg);
				break;
			case 'm':
				i_send_msg_len = atoi(optarg);
				break;
			default:
				print_help(av[0]);
				return(0);
		}
	}

	if( !ip1 || !port || !logname || !loglevel || (i_send_msg_len < 1)) {
		printf("ip1=[%s] port=[%s] logname=[%s] loglevel=[%s] send_msg_len=[%d]\n", 
			VALUE(ip1), VALUE(port), VALUE(logname), VALUE(loglevel), i_send_msg_len);
		print_help(av[0]);
		return(0);
	}

	i_loglevel = get_log_level(loglevel);
	rc = fq_open_file_logger(&l, logname, i_loglevel);
	CHECK( rc > 0 );
	fq_log(l, FQ_LOG_DEBUG, "log open OK.");

	/*
	** Connect and Open queue.
	*/
	fprintf(stdout, "Connecting to FQ_server.....\n");

	rc = open_tcp_client_obj(l, ip1, port, BINARY_HEADER, SOCKET_HEADER_LEN, &tcpobj);
	if( rc != TRUE ) {
		fq_log(l, FQ_LOG_ERROR, "failed to connect to ip:%s, port:%s.\n", ip1, port);
		goto L0;
	}

	start_timer();

loop:
	buf = calloc(1, sizeof(char)*i_send_msg_len);
	memset(buf, 'D', i_send_msg_len-1);

	len = i_send_msg_len;

	ret = send_header_and_body_socket(l, tcpobj, buf, len);
	if( ret < 0 ) {
		fq_log(l, FQ_LOG_ERROR, " send_header_and_body_socket() error. ret=[%d]", ret);
		goto L0;
	}
	
	ret = recv_header_and_body_socket( l, tcpobj, &databuf );
	if( ret < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "recv_header_and_body_socket() error. ret=[%d]", ret);
		goto L0;
	}
	else {
		count++;
		e = end_timer();
		if( e > 1.0 ) {
			printf("[%u] TPS. \n",(unsigned)(count/e));
			start_timer();
			count = 0;
		}
		test_count++;
		// fq_log(l, FQ_LOG_DEBUG, "response=[%s]", databuf);
		SAFE_FREE(buf);
		SAFE_FREE(databuf);
		// printf("count = %d\n", test_count);
		goto loop;
	}

L0:
	SAFE_FREE(buf);
	SAFE_FREE(databuf);

	rc = close_tcp_client_obj(l, &tcpobj);
	CHECK(rc==TRUE);

	fq_close_file_logger(&l);

	fq_log(l, FQ_LOG_INFO, "Process end.");

	fprintf(stdout, "Bye...\n");

	exit(EXIT_FAILURE);
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
