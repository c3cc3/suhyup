/*
** netqt.c
** Network Queue Test
** 
** Description: This program is a example code to teach how to use an agent to communicate with the FQ_server.
** Performance: TCS is 10.
** updated: 2014/02/13 bug fix : add line 241~244.
*/
#define NETQT_C_VERSION "1.0.1"

#include <stdio.h>
#include <sys/types.h>

#include "fq_tcp_client_obj.h"
#include "fq_common.h"
#include "fq_protocol.h"
#include "fq_timer.h"

/* Global variables */
char *ip1=NULL;
char *ip2=NULL;
char *port=NULL;
char *logname = NULL;
char *cmd=NULL;
char *path=NULL;
char *qname=NULL;
char *loglevel=NULL;
int	 i_loglevel = 0;
fq_logger_t *l=NULL;
int     g_break_flag = 0;
char    *g_data_view_flag = "off";
int     g_data_view_length = 50;
char    *g_raw_data_filename = NULL;
FILE    *g_raw_data_fp=NULL;

pthread_mutex_t g_mutex;
tcp_client_obj_t *tcpobj=NULL;
char    *skey=NULL;
char	result;
double	e;


static void *data_thread(void *arg)
{
	int		buffer_size = 65530;
	unsigned char *en_buf=NULL;
	char	result;
	int		count = 0;

	FQ_TRACE_ENTER(l);

	en_buf = calloc(buffer_size, sizeof(char));

	if(strcmp(cmd, "get") == 0 ) { /* enQ through the network */
		fq_log(l, FQ_LOG_DEBUG, "get() start.");
		count++;
		start_timer();
		while(1) {
			int rc;
			unsigned char *qdata=NULL;
			int qdata_len=0;

			rc = FQ_get(l, tcpobj, skey, path, qname, &qdata, &qdata_len, &result);
			if( rc < 0 ) {
				fq_log(l, FQ_LOG_ERROR, "error. rc=[%d].", rc);
				break;
			}
			else if( result == 'E' ) {
				printf("%s\n", "empty");
				SAFE_FREE(qdata);
				usleep(1000000); /* 1 sec  */
			}
			else {
				count++;
				e = end_timer();
				if( e > 1.0 ) {
					printf("[%u] TPS. \n",(unsigned)(count/e));
					start_timer();
					count = 0;
				}
				if( strcmp(g_data_view_flag, "on") == 0 ) {
					char	fmt[40];
					char    tail[5];

					memset(tail, 0x00, sizeof(tail));
					memcpy(tail, &qdata[rc-4], sizeof(tail)-1);

					sprintf(fmt, "len=[%d] DATA=[%c-%d.%ds...%s]\n", (rc-(FQP_SESSION_ID_LEN+1)), '%', g_data_view_length, g_data_view_length, tail);
					printf( fmt, qdata+FQP_SESSION_ID_LEN+1);
				}
				SAFE_FREE(qdata);
			}
		} /* while end */
	} /* get end */
	else if( strcmp(cmd, "put") == 0 ){ 
		count = 0;
		start_timer();
		while(1) {
			int	rc;
			int	try_count=0;
			int	len;
			int send_len;

			fq_log(l, FQ_LOG_DEBUG, "put stated");

			if( fgets((char *)en_buf, 65000, g_raw_data_fp) == NULL ) {
					printf( "finished !!!\n");
					SAFE_FREE(en_buf);
					break;
			}
retry_en:
			if( strcmp(g_data_view_flag, "on") == 0 ) {
				char    tail[5];
				char	fmt[512];

				memset(tail, 0x00, sizeof(tail));
				len = strlen((char *)en_buf);
				send_len = len -1;
				en_buf[len-1] = 0x00;
				memcpy(tail, &en_buf[len-4], sizeof(tail)-1);
				
				sprintf(fmt, "LEN=[%d] DATA=[%c%d.%ds ...%s] \n", send_len,'%', g_data_view_length, g_data_view_length, tail);
				fprintf(stdout, fmt, en_buf);
			}
			else {
				len = strlen((char *)en_buf);
				send_len = len -1;
				en_buf[len-1] = 0x00;
			}

			fq_log(l, FQ_LOG_DEBUG, "FQ_put() begin.");

			rc =  FQ_put(l, tcpobj, (unsigned char *)en_buf, send_len, skey, path, qname,  &result);
			if( rc < 0 ) {
				fprintf( stderr, "en queue error. rc=[%d]. Processing will be stop. See log for detail.\n", rc);
				SAFE_FREE(en_buf);
				break;
			}
			else if( result == 'F') { /* full */
				try_count++;
				fprintf(stderr, "queue is full. ..retry [%d]-th...\\n", try_count);
				usleep(1000000);
				goto retry_en;
			}
			else { /* success */
				count++;
				e = end_timer();
				if( e > 1.0 ) {
					fprintf(stdout, "[%u] TPS. \n",(unsigned)(count/e));
					start_timer();
					count = 0;
				}
				memset(en_buf, 0x00, buffer_size);
			}
		}
	}

	FQ_TRACE_EXIT(l);
	pthread_exit((void *)0);
}

static void print_help(char *p)
{
	char	*data_home=NULL;

	data_home = getenv("FQ_DATA_HOME");

	printf("Description: This program connects to FQ_server and get or put message.\n");
	printf("             For testing easily, set FQ_DATA_HOME to your .profile or .bashrc or .bash_profile.\n");
	printf("			 FQ_DATA_HOME=/home/data \n");
	printf("			 export FQ_DATA_HOME \n");
	printf("\n\nUsage  : $ %s [-V] [-h] [-i 1st_ip] [-I 2nd_ip] [-p port] [-l logname] [-c put|get] [-o loglevel] [-r /home/sg/data/4092.dat] <enter>\n", p);
	printf("\t	-V: version \n");
	printf("\t	-h: help \n");
	printf("\t	-i: ip[1st] Primary  FQ_server ip \n");
	printf("\t	-I: ip[2nd] Secondary FQ_server ip\n");
	printf("\t	-p: port : Port number that FQ_server is listening\n");
	printf("\t	-P: path : File Queue Path\n");
	printf("\t	-Q: qname : File Queue Name\n");
	printf("\t	-l: logfilename \n");
	printf("\t	-c: cmd(put or get) \n");
	printf("\t	-o: loglevel(trace|debug|info|error|emerg ) \n");
	printf("\t	-r: raw_data_file for en_queue \n");
	printf("example: $ %s -i 127.0.0.1 -I 127.0.0.1 -p 6666 -l /tmp/%s.log -P %s -Q TST -c put -o error -r %s/BIG.dat <enter>\n", p, p, data_home?data_home:"/data", data_home?data_home:"/data");
	printf("example: $ %s -i 127.0.0.1 -I 127.0.0.1 -p 6666 -l /tmp/%s.log -P %s -Q TST -c get -o error <enter>\n", p, p, data_home?data_home:"/data");
	return;
}

static void print_version(char *p)
{
	printf("\nversion: %s.\n\n", NETQT_C_VERSION);
	return;
}

int main(int ac, char **av)  
{  
	
	int  rc;
	int	ch;
	// int count=0;

    pthread_t thread_data;

	printf("Compiled on %s %s source program version [%s]\n", __TIME__, __DATE__, NETQT_C_VERSION);
	while(( ch = getopt(ac, av, "VvHh:p:l:i:I:c:P:Q:o:r:")) != -1) {
		switch(ch) {
			case 'H':
			case 'h':
				print_help(av[0]);
				return(0);
			case 'V':
			case 'v':
				print_version(av[0]);
				return(0);
			case 'i':
				ip1 = strdup(optarg);
				break;
			case 'I':
				ip2 = strdup(optarg);
				break;
			case 'p':
				port = strdup(optarg);
				break;
			case 'l':
				logname = strdup(optarg);
				break;
			case 'c':
				cmd = strdup(optarg);
				break;
			case 'P':
				path = strdup(optarg);
				break;
			case 'Q':
				qname = strdup(optarg);
				break;
			case 'o':
				loglevel = strdup(optarg);
				break;
			case 'r':
				g_raw_data_filename = strdup(optarg);
				break;
			default:
				print_help(av[0]);
				return(0);
		}
	}

	if( !ip1 || !ip2 || !port || !logname || !cmd || !path || !qname || !loglevel ) {
		printf("ip1=[%s] ip2=[%s] port=[%s] logname=[%s] cmd=[%s] path=[%s] qname=[%s] loglevel=[%s]\n", 
			VALUE(ip1), VALUE(ip2), VALUE(port), VALUE(logname), VALUE(cmd), VALUE(path), VALUE(qname), VALUE(loglevel));
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
	rc = FQ_link(l, 100, ip1, ip2, port, &tcpobj, path, qname, &result, &skey );
	if( rc < 0 || result != 'Y') {
		fq_log(l, FQ_LOG_ERROR, "failed to connect at the FQ_server.");
		goto STOP;
	}
	fq_log(l, FQ_LOG_DEBUG, "FQ_link OK. skey=[%s]", skey);

	if( g_raw_data_filename ) {
		/* open raw data file */
		g_raw_data_fp = fopen(g_raw_data_filename, "r");
		if( g_raw_data_fp == NULL ) {
			printf("raw data file[%s] open error.\n", g_raw_data_filename);
			goto STOP;
		}
		fq_log(l, FQ_LOG_DEBUG, "raw data open  OK.");
	}

	rc = pthread_mutex_init (&g_mutex, NULL);
    if( rc !=0 )
        perror("pthread_mutex_init"), exit(0);

	fq_log(l, FQ_LOG_DEBUG, "pthread_mutex_init() OK.");

    if( pthread_create(&thread_data, NULL, data_thread, NULL) != 0 ) { 
		fq_log(l, FQ_LOG_ERROR, "pthread_create() error."); 
		exit(0);
	}
	fq_log(l, FQ_LOG_DEBUG, "Successfully, thread started.");

    if( pthread_join(thread_data, NULL)) 
		perror("pthread_join"); exit(0);

STOP:
	fq_log(l, FQ_LOG_INFO, "Stopped!! rc=[%d] bye..\n", rc);

	SAFE_FREE(skey);

	rc = close_tcp_client_obj(l, &tcpobj);
	CHECK(rc==TRUE);

	fq_close_file_logger(&l);

	fq_log(l, FQ_LOG_INFO, "Process end.");

	fprintf(stdout, "Bye...\n");

	exit(EXIT_FAILURE);
}
