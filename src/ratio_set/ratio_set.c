/*
** virtual_was_main.c
** Network library Test
** Performance Test: in localhost with TCP_sync_server, 
** result: 23000 TPS 
*/
#include <stdio.h>
#include <sys/types.h>

#include "fq_tcp_client_obj.h"
#include "fq_common.h"
#include "fq_timer.h"
#include "parson.h"
#include "ums_common_conf.h"
#include "my_conf.h"

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

int make_json_request_msg( unsigned char **dst , char *channel, char *ratio_string);

int main(int ac, char **av)  
{  
	/* ums common config */
	ums_common_conf_t *cm_conf = NULL;

	/* my_config */
	my_conf_t	*my_conf=NULL;
	char *errmsg=NULL;

	/* logging */
	fq_logger_t *l=NULL;
	char log_pathfile[256];

	/* common */
	int rc;
	bool tf;

	if( ac != 2 ) {
		printf("Usage: $ %s [your config file] <enter>\n", av[0]);
		return 0;
	}

	if(Load_ums_common_conf(&cm_conf, &errmsg) == false) {
		printf("Load_ums_common_conf() error. reason='%s'\n", errmsg);
		return(-1);
	}	
	printf("Load_ums_common_conf() ok.\n");

	if(LoadConf(av[1], &my_conf, &errmsg) == false) {
		printf("LoadMyConf('%s') error. reason='%s'\n", av[1], errmsg);
		return(-1);
	}
	printf("LoadConf() ok.\n");

	sprintf(log_pathfile, "%s/%s", cm_conf->ums_common_log_path, my_conf->log_filename);
	rc = fq_open_file_logger(&l, log_pathfile, my_conf->i_log_level);
	CHECK(rc==TRUE);

	printf("log file: '%s'\n", log_pathfile);

	rc = fq_open_file_logger(&l, log_pathfile, my_conf->i_log_level);
	CHECK( rc > 0 );
	printf("log open OK.\n");

	/*
	** Connect and Open queue.
	*/
	fprintf(stdout, "Connecting to FQ_server.....(%s:%s)\n",  my_conf->ip, my_conf->port);

	rc = open_tcp_client_obj(l, my_conf->ip, my_conf->port, BINARY_HEADER, SOCKET_HEADER_LEN, &tcpobj);
	if( rc != TRUE ) {
		fq_log(l, FQ_LOG_ERROR, "failed to connect to ip:%s, port:%s.\n", ip1, port);
		goto L0;
	}
	fq_log(l, FQ_LOG_INFO, "Connected to webgate(%s:%s)", my_conf->ip, my_conf->port);
	start_timer();

	int len;
	unsigned char *json_msg_buf = NULL;

	len = make_json_request_msg(&json_msg_buf, my_conf->channel, my_conf->ratio_string);
	CHECK(rc > 0 );

	int ret;
	ret = send_header_and_body_socket(l, tcpobj, json_msg_buf, len);
	if( ret < 0 ) {
		fq_log(l, FQ_LOG_ERROR, " send_header_and_body_socket() error. ret=[%d]", ret);
		goto L0;
	}
	
	unsigned char *databuf=NULL;
	int count = 0;

	ret = recv_header_and_body_socket( l, tcpobj, &databuf );
	if( ret < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "recv_header_and_body_socket() error. ret=[%d]", ret);
		goto L0;
	}
	else { /* receiving success */
		fq_log(l, FQ_LOG_DEBUG, "response=[%s]", databuf);
		fprintf(stdout, "response=[%s] ret=[%d].\n", databuf, ret);

		SAFE_FREE(json_msg_buf);
		SAFE_FREE(databuf);
		printf("count = %d\n", test_count);
		// `usleep(100000);
		// goto loop;
	}

L0:
	SAFE_FREE(json_msg_buf);
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

int make_json_request_msg( unsigned char **dst , char *channel, char *ratio_string)
{
	JSON_Value *rootValue = NULL;
	JSON_Object *rootObject = NULL;

	rootValue = json_value_init_object();
	rootObject = json_value_get_object(rootValue);

	json_object_set_string(rootObject, "svc_code", "0003");

	json_object_set_string(rootObject, "Channel", channel); // SMS
	json_object_set_string(rootObject, "NewRatio", ratio_string); // SMS

	char *buf=NULL;
	size_t buf_size;

	buf_size = json_serialization_size(rootValue) + 1;
	buf = calloc(sizeof(char), buf_size);

	json_serialize_to_buffer( rootValue, buf, buf_size);

	printf("request json message =[%s], len=%ld.\n", buf, buf_size);

	*dst = (unsigned char *) strdup( buf );

	SAFE_FREE(buf);
	json_value_free(rootValue);

	return strlen(*dst);
}
