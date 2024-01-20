/*
** fq_mon_svr.c
*/
#include <stdio.h>
#include <unistd.h>
#include "fqueue.h"
#include "fq_socket.h"
#include "fq_logger.h"
#include "fq_common.h"
#include "fq_monitor.h"
#include "fq_conf.h"
#include "fq_info.h"
#include "fq_manage.h"
#include "fq_tci.h"
#include "mem_queue.h"

#define FQ_AGENT_C_VERSION "1.0"

char svr_id[36];
char local_svr_ip[16];
char g_Progname[128];
char log_directory[256];
char log_filename[128];
char log_full_name[1024];
char log_level[10];
int	 i_loglevel;
fq_logger_t *l=NULL;
conf_obj_t *conf_obj=NULL; /* configuration object */

static void print_help(char *p);

int main(int ac, char **av)
{
	int ch;
	char *conf_file=NULL;
	int	rc;
	
	/* UDP socket */
	int socketDescriptor; 
	unsigned short int listen_port; 
	unsigned int msgSize = 1024; // the max receivable size is msgSize. We should have this number larger than the max amount of data that we can receive. For now, this doesn't matter. 
	struct sockaddr_in serverAddress, clientAddress;  // we are going to store information for both the client and the server. 
	socklen_t clientAddressLength; 
	char msg[msgSize];  // msg received will be stored here. 

	/* FileQueue */
	fqueue_obj_t *qobj=NULL;
	char	qpath[128];
	char 	qname[16];
	long    l_seq=0L;
	long 	run_time=0L;


	printf("Compiled on %s %s source program version=[%s]\n\n", __TIME__, __DATE__, FQ_AGENT_C_VERSION);

	getProgName(av[0], g_Progname);
	
	while(( ch = getopt(ac, av, "f:")) != -1) {
		switch(ch) {
			case 'f':
				conf_file = strdup(optarg);
				break;
			default:
				print_help(g_Progname);
				return(-1);
		}
	}
	if( !HASVALUE(conf_file) ) {
		print_help(g_Progname);
		return(-2);
	}

	rc = open_conf_obj( conf_file, &conf_obj);
	if( rc == FALSE) {
		fprintf(stderr, "open_conf_obj() error.\n");
		return(-3);
	}

	/* get values from config */
	conf_obj->on_get_str(conf_obj, "SVR_ID", svr_id);
	conf_obj->on_get_str(conf_obj, "LOG_DIRECTORY", log_directory);
	conf_obj->on_get_str(conf_obj, "LOG_FILE_NAME", log_filename);
	conf_obj->on_get_str(conf_obj, "LOG_LEVEL", log_level);
	conf_obj->on_get_str(conf_obj, "LOCAL_SVR_IP", local_svr_ip);
    listen_port = conf_obj->on_get_int(conf_obj, "LISTEN_PORT");

	conf_obj->on_get_str(conf_obj, "QPATH", qpath);
	conf_obj->on_get_str(conf_obj, "QNAME", qname);

	// conf_obj->on_print_conf(conf_obj);

	i_loglevel = get_log_level(log_level);

	sprintf(log_full_name, "%s/%s", log_directory , log_filename);
	rc = fq_open_file_logger(&l, log_full_name, i_loglevel);
	if(rc <= 0) {
		fprintf(stderr, "fq_open_file_logger(%s) error.\n", log_full_name);
		return(-4);
	}

	fq_log(l, FQ_LOG_EMERG, "[%s] process is started.\n", g_Progname);

	/* Open File Queue */
	rc =  OpenFileQueue(l, NULL, NULL, NULL, NULL, qpath, qname, &qobj);
	CHECK(rc==TRUE);


	/* UDP Receiving and enQ */
	socketDescriptor = socket(AF_INET, SOCK_DGRAM, 0);

	serverAddress.sin_family = AF_INET;
	serverAddress.sin_addr.s_addr = inet_addr(local_svr_ip);
	serverAddress.sin_port = listen_port;

	if (bind(socketDescriptor, (struct sockaddr *) &serverAddress, sizeof(serverAddress)) < 0) {
		fq_log(l, FQ_LOG_ERROR, "Could not bind socket");
		return(-5);
	}

	listen(socketDescriptor, 2); // sets the queue size to two messages at a time. The number '2' was chosen arbitrarely. This line is optional.
	fq_log(l, FQ_LOG_EMERG, "Waiting for data on UDP port %i.", listen_port);
	
	while(1) {
		clientAddressLength = sizeof(clientAddress);
		memset(msg, 0, msgSize);

		if (recvfrom(socketDescriptor, msg, msgSize, 0, (struct sockaddr *)&clientAddress, &clientAddressLength) < 0) {
			fq_log(l, FQ_LOG_ERROR, "An error occured while receiving data... Program is terminating. ");
			return(-4);
		}
		fq_log(l, FQ_LOG_INFO, "Received from %s on UDP port %d (port at sender is %d): \n%s(len=%d)",
			inet_ntoa(clientAddress.sin_addr), serverAddress.sin_port, ntohs(clientAddress.sin_port), msg, strlen(msg));

		while(1) {
			rc =  qobj->on_en(l, qobj, EN_NORMAL_MODE, (unsigned char *)&msg, sizeof(msg), strlen(msg), &l_seq, &run_time );
			if( rc == EQ_FULL_RETURN_VALUE )  {
				fq_log(l, FQ_LOG_EMERG, "Queue is full(%s/%s).", qpath, qname);
				sleep(1);
				continue;
			}
			else if( rc < 0 ) {
				if( rc == MANAGER_STOP_RETURN_VALUE ) {
					fq_log(l, FQ_LOG_EMERG, "Manager asked to stop a processing.");
					goto L0;
				}
			}
			else {
				fq_log(l, FQ_LOG_INFO, "enFQ OK. rc=[%d]", rc);
				break;
			}
		}
	}

L0:
	fq_log(l, FQ_LOG_EMERG, "[%s] process is stopped.\n", g_Progname);

	rc = CloseFileQueue(l, &qobj);
	CHECK(rc==TRUE);

	fq_close_file_logger(&l);
	close_conf_obj(&conf_obj);

	return(0);
}

static void print_help(char *p)
{
	printf("\nUsage  : $ %s [-f config_file] <enter>\n", p);
	printf("example: $ %s -f agent.conf <enter>\n", p);
	return;
}
