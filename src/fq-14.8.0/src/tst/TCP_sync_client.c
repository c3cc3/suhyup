/*
** TCP_sync_client.c
** Network library Test
** 
*/
#include <stdio.h>
#include <sys/types.h>

#include "fq_tcp_client_obj.h"
#include "fq_common.h"
#include "fq_timer.h"

/* Global variables */
char *ip1=NULL;
char *port=NULL;
char *logname = NULL;
char *cmd=NULL;
char *path=NULL;
char *qname=NULL;
char *loglevel=NULL;
int	 i_loglevel = 0;
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
	printf("\n\nUsage  : $ %s [-V] [-h] [-i 1st_ip] [-p port] [-l logname] [-o loglevel]  <enter>\n", p);
	printf("\t	-h: help \n");
	printf("\t	-i: ip[1st] Primary  FQ_server ip \n");
	printf("\t	-p: port : Port number that FQ_server is listening\n");
	printf("\t	-l: logfilename \n");
	printf("\t	-o: loglevel(trace|debug|info|error|emerg ) \n");
	printf("example: $ %s -i 172.30.1.11 -p 6666 -l %s.log -o error <enter>\n",p,p);
	return;
}


int main(int ac, char **av)  
{  
	
	int  rc, ret, len;
	int	ch;
	// int count=0;
	unsigned char   *buf=NULL;
	unsigned char   *databuf=NULL;

	while(( ch = getopt(ac, av, "Hh:p:l:i:o:r:")) != -1) {
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
			default:
				print_help(av[0]);
				return(0);
		}
	}

	if( !ip1 || !port || !logname || !loglevel ) {
		printf("ip1=[%s] port=[%s] logname=[%s] loglevel=[%s]\n", 
			VALUE(ip1), VALUE(port), VALUE(logname), VALUE(loglevel));
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

loop:
	buf = calloc(1, sizeof(char)*1024);
	memset(buf, 'D', 1023);

	len = 1024;

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
		test_count++;
		fq_log(l, FQ_LOG_DEBUG, "response=[%s]", databuf);
		SAFE_FREE(buf);
		SAFE_FREE(databuf);
		printf("count = %d\n", test_count);
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
