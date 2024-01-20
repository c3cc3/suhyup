/*
** FQ_server.c
** Performance Test: in localhost with TCP_sync_client, 
** 4096 bytes , result: 23000 TPS
*/
#define FQ_SERVER_C_VERSION	"1.0.1"

#include <stdio.h>
#include <getopt.h>
#include "fq_param.h"
#include "fqueue.h"
#include "fq_socket.h"
#include "fq_logger.h"
#include "fq_common.h"
#include "fq_monitor.h"
#include "fq_protocol.h"
#include "fq_config.h"


fq_logger_t *l=NULL;
char g_Progname[64];

void sig_exit_handler( int signal )
{
    printf("sig_pipe_handler() \n");
	exit(EXIT_FAILURE);
}

int CB_function( unsigned char *data, int len, unsigned char *resp, int *resp_len)
{
	fq_log(l, FQ_LOG_INFO, "CB called.");

	/*
	** Warning : Variable data has value of header.so If you print data to sceeen, you can't see real data.
	** so you have to add 4bytes(header length) for saving and printing.
	*/
	fq_log(l, FQ_LOG_DEBUG, "len=[%d] [%-s]\n", len, data);

	memset(resp, 'Y', 1);
	*resp_len = 1;	

	fq_log(l, FQ_LOG_DEBUG, "response =[%s] resp_len=[%d]", resp, *resp_len);

	return(*resp_len);
}

void print_help(char *p)
{
	printf("\n\nUsage  : $ %s [-V] [-h] [-f config_file] [-i ip] [-p port] [-l logname] [-o loglevel] <enter>\n", p);
	printf("\t	-V: version \n");
	printf("\t	-h: help \n");
	printf("\t	-f: config_file \n");
	printf("\t	-p: port \n");
	printf("\t	-l: logfilename \n");
	printf("\t  -o: log level ( trace|debug|info|error|emerg )\n");
	printf("example: $ %s -i 127.0.0.1 -p 6666 -l /tmp/%s.log -o debug <enter>\n", p, p);
	printf("example: $ %s -f FQ_server_solaris.conf <enter>\n", p);
	return;
}

int main(int ac, char **av)
{
	TCP_server_t x;

	/* server environment */
	char *bin_dir = NULL;
	/* parameters */
	char *logname = NULL;
	char *ip = NULL;
	char *port = NULL;
	char *loglevel = NULL;
	char *conf=NULL;
	int	i_loglevel = 0;
	int	ack_mode = 1; /* ACK 모드: FQ_server는 항상 1로 설정한다. */

	int	rc = -1;
	int	ch;

	printf("Compiled on %s %s source program version=[%s]\n\n\n", __TIME__, __DATE__, FQ_SERVER_C_VERSION);

	getProgName(av[0], g_Progname);

	while(( ch = getopt(ac, av, "Hh:p:Q:l:i:P:o:f:")) != -1) {
		switch(ch) {
			case 'H':
			case 'h':
				print_help(av[0]);
				return(0);
			case 'i':
				ip = strdup(optarg);
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
			case 'f':
				conf = strdup(optarg);
				break;
			default:
				print_help(av[0]);
				return(0);
		}
	}

	if(HASVALUE(conf) ) { /* If there is a config file ... */
		config_t *t=NULL;
		char	buffer[128];

		t = new_config(conf);

		if( load_param(t, conf) < 0 ) {
			fprintf(stderr, "load_param() error.\n");
			return(EXIT_FAILURE);
		}
		/* server environment */
		get_config(t, "BIN_DIR", buffer);
		bin_dir = strdup(buffer);

		/* 사용자가 정한 항목을 읽을 경우 */
		get_config(t, "FQ_SERVER_IP", buffer);
		ip = strdup(buffer);

		get_config(t, "FQ_SERVER_PORT", buffer);
		port = strdup(buffer);

		get_config(t, "LOG_NAME", buffer);
		logname = strdup(buffer);

		get_config(t, "LOG_LEVEL", buffer);
		loglevel = strdup(buffer);

		free_config(&t);
	}

	/* Checking mandatory parameters */
	if( !ip || !port || !logname || !loglevel) {
		print_help(av[0]);
		printf("User input: ip=[%s] port=[%s] logname=[%s] loglevel=[%s]\n", 
			VALUE(ip), VALUE(port), VALUE(logname), VALUE(loglevel));
		return(0);
	}

	/*
	** make background daemon process.
	*/
	// daemon_init();

	signal(SIGPIPE, SIG_IGN);

	
	/********************************************************************************************
	** Open a file for logging
	*/
	i_loglevel = get_log_level(loglevel);
	rc = fq_open_file_logger(&l, logname, i_loglevel);
	CHECK( rc > 0 );
	/************************************************************************************************/

	if( bin_dir && conf ) {
		/**********************************************************************************************
		*/
		signal(SIGPIPE, sig_exit_handler); /* retry 시에는 반드시 헨들러를 재 구동하여야 한다 */
		signal(SIGTERM, sig_exit_handler); /* retry 시에는 반드시 헨들러를 재 구동하여야 한다 */
		signal(SIGQUIT, sig_exit_handler); /* retry 시에는 반드시 헨들러를 재 구동하여야 한다 */
		/************************************************************************************************/
	}

	make_thread_sync_CB_server( l, &x, ip, atoi(port), BINARY_HEADER_TYPE, 4, ack_mode, CB_function);

	fq_log(l, FQ_LOG_EMERG, "FQ server (entrust Co.version %s) start!!!.(listening ip=[%s] port=[%s]", 
		FQ_SERVER_C_VERSION, ip, port);

	pause();

    fq_close_file_logger(&l);
	exit(EXIT_FAILURE);
}
