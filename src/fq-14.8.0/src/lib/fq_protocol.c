/*
** fq_protocol.c
*/
#define FQ_SOCKET_C_VERSION "1.0.0"
/*
** ver_1.0.0:  2013/07/24 
*/

#include "config.h"
#include "fq_common.h"

#include <assert.h>

#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef NEED_SOCKLEN_T_DEFINED
#define _BSD_SOCKLEN_T_
#endif

#ifdef HAVE_SOL_IP
#define ICMP_SIZE sizeof(struct icmphdr)
#else
#define ICMP_SIZE sizeof(struct icmp)
#include <netinet/in_systm.h>
#endif

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif 

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif 

#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif

#ifdef HAVE_NET_IF_H
#include <net/if.h>
#endif

#ifdef HAVE_SYS_UN_H
#include <sys/un.h>
#endif

#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#ifdef HAVE_SYS_FILIO_H
#include <sys/filio.h>
#endif

#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

#ifdef HAVE_SYS_FILIO_H
#include <sys/filio.h>
#endif

#ifdef HAVE_NETINET_IP_H
#include <netinet/ip.h>
#endif

#ifndef OS_SOLARIS
#ifdef HAVE_NETINET_IP_ICMP_H
#include <netinet/ip_icmp.h>
#endif
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#if 0
#ifndef __dietlibc__
#ifdef HAVE_STROPTS_H
#include <stropts.h>
#endif
#endif
#endif

#include <sys/ioctl.h>

#include <math.h>

#include "fq_socket.h"
#include "fq_protocol.h"
#include "fq_logger.h"
#include "fq_tcp_client_obj.h"
#include "fq_common.h"
#include "parson.h"

#define FQP_VERSION "10"

/*
** This function makes a stream data with user data to use File Queue protocol.
*/
int make_stream_msg( fq_logger_t *l, FQ_protocol_t *fqp, unsigned char *data, size_t data_len, unsigned char **buf )
{
	size_t	total_msg_len;
	unsigned char *p=NULL;

	FQ_TRACE_ENTER(l);

	total_msg_len = FQ_PROTOCOL_LEN + data_len + 1;

	p = calloc( total_msg_len, sizeof(char *));
	memset(p, 0x20, total_msg_len - 1);

	memcpy(p, fqp->version, FQP_VERSION_LEN);

	memcpy(p+FQP_VERSION_LEN, fqp->skey, FQP_SESSION_ID_LEN);
	memcpy(p+FQP_VERSION_LEN+FQP_SESSION_ID_LEN, fqp->path, strlen(fqp->path));
	memcpy(p+FQP_VERSION_LEN+FQP_SESSION_ID_LEN+FQP_PATH_LEN, fqp->qname, strlen(fqp->qname));
	*(p+FQP_VERSION_LEN+FQP_SESSION_ID_LEN+FQP_PATH_LEN+FQP_QNAME_LEN) = fqp->ack_mode;
	memcpy(p+FQP_VERSION_LEN+FQP_SESSION_ID_LEN+FQP_PATH_LEN+FQP_QNAME_LEN+1, fqp->action_code, FQP_ACTION_CODE_LEN);
	memcpy(p+512, data, data_len);

	*buf = p;

	FQ_TRACE_EXIT(l);

	return(total_msg_len-1);
}

int FQ_link( fq_logger_t *l, 
				int	 max_try_count,
				char *primary_server_ip, 
				char *secondary_server_ip,
				char *port,
				tcp_client_obj_t **ptr_tcpobj,
				char	*path,
				char	*qname,
				char *result_code,
				char **session_key)
{
	int ret = -1;
	int	rc = -1;
	int	try_count=0;
	FQ_protocol_t fqp;
	unsigned char   *buf=NULL;
	unsigned char 	*databuf=NULL;
	int	len;
	char    skey[FQP_SESSION_ID_LEN+1];
    char    result;

	FQ_TRACE_ENTER(l);

retry_connect:
	/* Connection to FQ_server */
	try_count++;
	if( try_count > max_try_count ) {
		goto L0;
	}

	rc = open_tcp_client_obj(l, primary_server_ip, port, BINARY_HEADER, SOCKET_HEADER_LEN, ptr_tcpobj);
	if( rc != TRUE ) {
		fq_log(l, FQ_LOG_ERROR, "failed to connect to log_collect_svr. will restart to 2nd ip[%s] after 3 second.\n", secondary_server_ip);
		sleep(3);
	
		rc = open_tcp_client_obj(l, secondary_server_ip, port, BINARY_HEADER, SOCKET_HEADER_LEN, ptr_tcpobj);
		if( rc !=TRUE ) {
			fq_log(l, FQ_LOG_ERROR, "failed to connect to log_collect_svr. will restart to 1st ip[%s] after 3 second.\n", primary_server_ip);
			sleep(3);
			goto retry_connect;
		}
	}
	fq_log(l, FQ_LOG_DEBUG, "success connecting to FQ_server.");

	sprintf(fqp.version, "%s", FQP_VERSION );
	memset(fqp.skey, '0', sizeof(fqp.skey));
	sprintf(fqp.path, "%s", path);
	sprintf(fqp.qname, "%s", qname);
	fqp.ack_mode = 'Y';
	sprintf(fqp.action_code, "%s", "LINK");

	len = make_stream_msg(l,  &fqp, (unsigned char *)"I want to open a file queue", 27,  &buf);
	
	ret = send_header_and_body_socket(l, *ptr_tcpobj, buf, len);
	if( ret < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "LINK protocol error. ret=[%d]", ret);
		goto L0;
	}
	
	ret = recv_header_and_body_socket( l, *ptr_tcpobj, &databuf );
	if( ret < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "LINK response error. ret=[%d]", ret);
		goto L0;
	}
	fq_log(l, FQ_LOG_DEBUG, "response=[%s]", databuf);

	memset( skey, 0x00, sizeof(skey) );
	result = databuf[0];
	memcpy( skey , databuf+1, FQP_SESSION_ID_LEN);

	if( result == 'Y' && (strlen(skey)==32) ) {
		fq_log(l, FQ_LOG_DEBUG, "LINK OK. result=[%c] skey=[%s]", result, skey);
	}
	else {
		fq_log(l, FQ_LOG_ERROR, "LINK error. result=[%c] skey=[%s]", result, skey);
		goto L0;
	}

	*result_code = result;
	*session_key = strdup(skey);

	SAFE_FREE(buf);
	SAFE_FREE(databuf);

L0:
	FQ_TRACE_EXIT(l);
    return(ret);
}
int FQ_link_json( fq_logger_t *l, 
				int	 max_try_count,
				char *primary_server_ip, 
				char *secondary_server_ip,
				char *port,
				tcp_client_obj_t **ptr_tcpobj,
				char	*path,
				char	*qname,
				char *result_code,
				char **session_key)
{
	int ret = -1;
	int	rc = -1;
	int	try_count=0;
	unsigned char   *buf=NULL;
	unsigned char 	*databuf=NULL;
	int	len;
	char    skey[FQP_SESSION_ID_LEN+1];
    char    result;

	FQ_TRACE_ENTER(l);

retry_connect:
	/* Connection to FQ_server */
	try_count++;
	if( try_count > max_try_count ) {
		goto L0;
	}

	rc = open_tcp_client_obj(l, primary_server_ip, port, BINARY_HEADER, SOCKET_HEADER_LEN, ptr_tcpobj);
	if( rc != TRUE ) {
		fq_log(l, FQ_LOG_ERROR, "failed to connect to log_collect_svr. will restart to 2nd ip[%s] after 3 second.\n", secondary_server_ip);
		sleep(3);
	
		rc = open_tcp_client_obj(l, secondary_server_ip, port, BINARY_HEADER, SOCKET_HEADER_LEN, ptr_tcpobj);
		if( rc !=TRUE ) {
			fq_log(l, FQ_LOG_ERROR, "failed to connect to log_collect_svr. will restart to 1st ip[%s] after 3 second.\n", primary_server_ip);
			sleep(3);
			goto retry_connect;
		}
	}
	fq_log(l, FQ_LOG_DEBUG, "success connecting to FQ_server.");


// here-1
	JSON_Value *rootValue = json_value_init_object();
	JSON_Object *rootObject = json_value_get_object(rootValue);

	json_object_set_string(rootObject, "FQP_VERSION", FQP_VERSION );
	json_object_set_string(rootObject, "SESSION_ID", "");
	json_object_set_string(rootObject, "QUEUE_PATH", path);
	json_object_set_string(rootObject, "QUEUE_NAME", qname);
	json_object_set_string(rootObject, "ACK_MODE", "Y");
	json_object_set_string(rootObject, "MSG_LENGTH", "Y");
	json_object_set_string(rootObject, "ACTION", "LINK");
	json_object_set_number(rootObject, "MSG_LENGTH", 0);
	json_object_set_string(rootObject, "MESSAGE", "");

	size_t buf_size;

	buf_size = json_serialization_size(rootValue) + 1;
	buf = calloc(sizeof(unsigned char), buf_size);
	json_serialize_to_buffer(rootValue, buf, buf_size);

	debug_json(l, rootValue);

	ret = send_header_and_body_socket(l, *ptr_tcpobj, buf, strlen(buf));
	if( ret < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "LINK protocol error. ret=[%d]", ret);
		goto L0;
	}
	
	ret = recv_header_and_body_socket( l, *ptr_tcpobj, &databuf );
	if( ret < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "LINK response error. ret=[%d]", ret);
		goto L0;
	}
	fq_log(l, FQ_LOG_DEBUG, "json response=[%s]", databuf);

	JSON_Value *jsonValue = json_parse_string(databuf);
	if( !jsonValue ) {
		fq_log(l, FQ_LOG_ERROR, "illegal json format. We throw it.\n");
		goto L0;
	}
	JSON_Object *jsonObject = json_value_get_object(jsonValue);
    if( !jsonObject ) {
        fq_log(l, FQ_LOG_ERROR, "illegal json format. We throw it.\n");
        goto L0;
    }
	fq_log(l, FQ_LOG_DEBUG, "json varify ok.");

	debug_json(l, jsonValue);

	char json_result[16];
	sprintf(json_result, "%s", json_object_get_string(jsonObject, "RESULT"));
	sprintf(skey, "%s", json_object_get_string(jsonObject, "SESSION_ID"));


	fq_log(l, FQ_LOG_DEBUG, "LINK: json_result=[%s]\n", json_result);
	fq_log(l, FQ_LOG_DEBUG, "LINK: skey=[%s]\n", skey);

	if( strcmp(json_result, "OK") == 0 ) {
		result = 'Y';
	}
	else {
		result = 'N';
	}

	if( result == 'Y' && (strlen(skey)==32) ) {
		fq_log(l, FQ_LOG_DEBUG, "LINK OK. result=[%c] skey=[%s]", result, skey);
	}
	else {
		fq_log(l, FQ_LOG_ERROR, "LINK error. result=[%c] skey=[%s]", result, skey);
		goto L0;
	}

	*result_code = result;
	*session_key = strdup(skey);

	SAFE_FREE(buf);
	SAFE_FREE(databuf);
L0:
	json_value_free(rootValue);
	FQ_TRACE_EXIT(l);
    return(ret);
}

int FQ_put( fq_logger_t *l, 
	tcp_client_obj_t *tcpobj, 
	unsigned char *putdata, 
	int putdata_len, 
	char *skey, 
	char *path,
	char *qname,
	char *result_code)
{
	FQ_protocol_t fqp;
	unsigned char *buf=NULL;
	unsigned char *recvbuf=NULL;
	int	stream_len = 0;
	int send_len = 0;
	int	recv_len = 0;
	char	result='N';
	int	rc = -1;
	sigset_t    sigset;

	FQ_TRACE_ENTER(l);

	sigemptyset(&sigset);
    sigaddset(&sigset, SIGINT);
    sigaddset(&sigset, SIGQUIT);
    sigaddset(&sigset, SIGPIPE);
    sigaddset(&sigset, SIGSTOP);

    sigprocmask(SIG_BLOCK, &sigset, NULL);

	sprintf(fqp.version, "%s", FQP_VERSION);
	sprintf(fqp.skey, "%s", skey);
	sprintf(fqp.path, "%s", path);
	sprintf(fqp.qname, "%s", qname);
	fqp.ack_mode = 'Y';   
	sprintf(fqp.action_code, "%s", "ENQU");

	stream_len = make_stream_msg(l, &fqp, putdata, putdata_len, &buf);
	if( stream_len < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "make_stream_msg() error.");
		goto L0;
	}

	send_len = send_header_and_body_socket(l, tcpobj, buf, stream_len);  /* rc is length of data */
	if( send_len < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "send_header_and_body_socket() error.");
		goto L0;
	}
	fq_log(l, FQ_LOG_DEBUG, "send OK ENQU send_len=[%d]", send_len);
	SAFE_FREE(buf);

	recv_len = recv_header_and_body_socket( l, tcpobj, &recvbuf );
	if( recv_len < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "ENQU response error. recv_len=[%d]", recv_len);
		goto L0;
	}

	fq_log(l, FQ_LOG_DEBUG, "response=[%s]", recvbuf);

	result = recvbuf[0];

	if( result == 'Y' ) {
		fq_log(l, FQ_LOG_DEBUG, "ENQU result OK.\n");
		rc = recv_len; 
	}
	else if( result == 'F' ) { /* Full */
		fq_log(l, FQ_LOG_DEBUG, "ENQU result :full.\n");
		rc = EQ_FULL_RETURN_VALUE; 
	}
	else {
		fq_log(l, FQ_LOG_ERROR, "ENQU result error. result=[%c]", result);
		rc = -1;
	}
L0:
	*result_code = result;

	SAFE_FREE(buf);
	SAFE_FREE(recvbuf);

	sigprocmask(SIG_UNBLOCK, &sigset, NULL);

	FQ_TRACE_EXIT(l);
    return(rc);
}
int FQ_put_json( fq_logger_t *l, 
	tcp_client_obj_t *tcpobj, 
	unsigned char *putdata, 
	int putdata_len, 
	char *skey, 
	char *path,
	char *qname,
	char *result_code)
{
	unsigned char *buf=NULL;
	unsigned char *recvbuf=NULL;
	int	stream_len = 0;
	int send_len = 0;
	int	recv_len = 0;
	char	result='N';
	int	rc = -1;
	sigset_t    sigset;

	FQ_TRACE_ENTER(l);

	sigemptyset(&sigset);
    sigaddset(&sigset, SIGINT);
    sigaddset(&sigset, SIGQUIT);
    sigaddset(&sigset, SIGPIPE);
    sigaddset(&sigset, SIGSTOP);

    sigprocmask(SIG_BLOCK, &sigset, NULL);

	JSON_Value *rootValue = json_value_init_object();
	JSON_Object *rootObject = json_value_get_object(rootValue);

	json_object_set_string(rootObject, "FQP_VERSION", FQP_VERSION );
	json_object_set_string(rootObject, "SESSION_ID", skey);
	json_object_set_string(rootObject, "QUEUE_PATH", path);
	json_object_set_string(rootObject, "QUEUE_NAME", qname);
	json_object_set_string(rootObject, "ACK_MODE", "Y");
	json_object_set_string(rootObject, "MSG_LENGTH", "Y");
	json_object_set_string(rootObject, "ACTION", "ENQU");
	json_object_set_number(rootObject, "MSG_LENGTH", putdata_len);
	json_object_set_string(rootObject, "MESSAGE", putdata);

	size_t buf_size;

	buf_size = json_serialization_size(rootValue) + 1;
	buf = calloc(sizeof(unsigned char), buf_size);
	json_serialize_to_buffer(rootValue, buf, buf_size);

	fq_log(l, FQ_LOG_DEBUG, "json=[%s].", buf);

	if( FQ_IS_DEBUG_LEVEL(l) ) {
		char *tmp = NULL;
		tmp = json_serialize_to_string_pretty(rootValue);
		fq_log(l, FQ_LOG_DEBUG, "JSON=[%s]\n", tmp);
		SAFE_FREE(tmp);
	}

	send_len = send_header_and_body_socket(l, tcpobj, buf, strlen(buf));  /* rc is length of data */
	if( send_len < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "send_header_and_body_socket() error.");
		goto L0;
	}
	fq_log(l, FQ_LOG_DEBUG, "send OK ENQU send_len=[%d]", send_len);
	SAFE_FREE(buf);

	recv_len = recv_header_and_body_socket( l, tcpobj, &recvbuf );
	if( recv_len < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "ENQU response error. recv_len=[%d]", recv_len);
		goto L0;
	}

	fq_log(l, FQ_LOG_DEBUG, "response=[%s]", recvbuf);

	JSON_Value *jsonValue = json_parse_string(recvbuf);
	if( !jsonValue ) {
		fq_log(l, FQ_LOG_ERROR, "illegal json format. We throw it.\n");
		goto L0;
	}
	JSON_Object *jsonObject = json_value_get_object(jsonValue);
    if( !jsonObject ) {
        fq_log(l, FQ_LOG_ERROR, "illegal json format. We throw it.\n");
        goto L0;
    }
	fq_log(l, FQ_LOG_DEBUG, "JSON verify ok.");

	debug_json(l, jsonValue);

	char json_result[16];
	sprintf(json_result, "%s", json_object_get_string(jsonObject, "RESULT"));
	sprintf(skey, "%s", json_object_get_string(jsonObject, "SESSION_ID"));


	if( strcmp( json_result, "OK") == 0 ) {
		result = 'Y';
	}
	else if( strcmp( json_result, "FULL") == 0 ) {
		result = 'F';
	}
	else {
		result = 'E';
	}

	if( result == 'Y' ) {
		fq_log(l, FQ_LOG_DEBUG, "ENQU result OK.\n");
		rc = recv_len; 
	}
	else if( result == 'F' ) { /* Full */
		fq_log(l, FQ_LOG_DEBUG, "ENQU result :full.\n");
		rc = EQ_FULL_RETURN_VALUE; 
	}
	else {
		fq_log(l, FQ_LOG_ERROR, "ENQU result error. result=[%c]", result);
		rc = -1;
	}
L0:
	*result_code = result;

	SAFE_FREE(buf);
	SAFE_FREE(recvbuf);

	json_value_free(jsonValue); 
	json_value_free(rootValue); 

	sigprocmask(SIG_UNBLOCK, &sigset, NULL);

	FQ_TRACE_EXIT(l);
    return(rc);
}

int FQ_get( fq_logger_t *l, 
	tcp_client_obj_t *tcpobj, 
	char *skey, 
	char *path,
	char *qname,
	unsigned char **qdata,
	int	*qdata_len,
	char *result_code)
{
	int rc = -1;
	FQ_protocol_t fqp;
	int	stream_len = 0;
	int send_len = 0;
	int	recv_len = 0;
	char	result='N';
	unsigned char *buf=NULL;
	unsigned char *recvbuf=NULL;
	char 	*user_msg = "I want to get a message";
	sigset_t    sigset;

	FQ_TRACE_ENTER(l);

	sigemptyset(&sigset);
    sigaddset(&sigset, SIGINT);
    sigaddset(&sigset, SIGQUIT);
    sigaddset(&sigset, SIGPIPE);
    sigaddset(&sigset, SIGSTOP);

    sigprocmask(SIG_BLOCK, &sigset, NULL);

	sprintf(fqp.version, "%s", FQP_VERSION);
	sprintf(fqp.skey, "%s", skey);
	sprintf(fqp.path, "%s", path);
	sprintf(fqp.qname, "%s", qname);
	fqp.ack_mode = 'Y';   
	sprintf(fqp.action_code, "%s", "DEQU");

	stream_len = make_stream_msg(l, &fqp, (unsigned char *)user_msg, strlen(user_msg) , &buf);
	if( stream_len < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "make_stream_msg() error.");
		goto L0;
	}

	send_len = send_header_and_body_socket(l, tcpobj, buf, stream_len);  /* rc is length of data */
	if( send_len < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "send_header_and_body_socket() error.");
		goto L0;
	}
	fq_log(l, FQ_LOG_DEBUG, "send OK ENQU send_len=[%d]", send_len);
	SAFE_FREE(buf);

	recv_len = recv_header_and_body_socket( l, tcpobj, &recvbuf );
	if( recv_len < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "ENQU response error. recv_len=[%d]", recv_len);
		goto L0;
	}
	fq_log(l, FQ_LOG_DEBUG, "recv_len=[%d] response=[%s]", recv_len, recvbuf);

	result = recvbuf[0];

	if( result == 'Y' ) {
		fq_log(l, FQ_LOG_DEBUG, "DEQU result OK.\n");
		rc = recv_len; 
	}
	else if( result == 'E' ) { /* empty */
		fq_log(l, FQ_LOG_DEBUG, "DEQU result : empty.\n");
		rc = EQ_EMPTY_RETURN_VALUE; 
	}
	else {
		fq_log(l, FQ_LOG_ERROR, "DEQU result error. result=[%c]", result);
		rc = -1;
	}
L0:
	*result_code = result;

	if( result == 'Y' ) {
		*qdata = recvbuf;
		*qdata_len = recv_len;
	}

	SAFE_FREE(buf);

	sigprocmask(SIG_UNBLOCK, &sigset, NULL);

	FQ_TRACE_EXIT(l);
    return(rc);
}
int FQ_get_json( fq_logger_t *l, 
	tcp_client_obj_t *tcpobj, 
	char *skey, 
	char *path,
	char *qname,
	unsigned char **qdata,
	int	*qdata_len,
	char *result_code)
{
	int rc = -1;
	FQ_protocol_t fqp;
	int	stream_len = 0;
	int send_len = 0;
	int	recv_len = 0;
	char	result='N';
	unsigned char *buf=NULL;
	unsigned char *recvbuf=NULL;
	char 	*user_msg = "I want to get a message";
	sigset_t    sigset;

	FQ_TRACE_ENTER(l);

	sigemptyset(&sigset);
    sigaddset(&sigset, SIGINT);
    sigaddset(&sigset, SIGQUIT);
    sigaddset(&sigset, SIGPIPE);
    sigaddset(&sigset, SIGSTOP);

    sigprocmask(SIG_BLOCK, &sigset, NULL);

// here-2
	JSON_Value *rootValue = json_value_init_object();
	JSON_Object *rootObject = json_value_get_object(rootValue);

	json_object_set_string(rootObject, "FQP_VERSION", FQP_VERSION );
	json_object_set_string(rootObject, "SESSION_ID", skey);
	json_object_set_string(rootObject, "QUEUE_PATH", path);
	json_object_set_string(rootObject, "QUEUE_NAME", qname);
	json_object_set_string(rootObject, "ACK_MODE", "Y");
	json_object_set_string(rootObject, "ACTION", "DEQU");
	json_object_set_number(rootObject, "MSG_LENGTH", 0);
	json_object_set_string(rootObject, "MESSAGE", "");

	size_t buf_size;

	buf_size = json_serialization_size(rootValue) + 1;
	buf = calloc(sizeof(unsigned char), buf_size);
	json_serialize_to_buffer(rootValue, buf, buf_size);

	debug_json(l, rootValue);

	send_len = send_header_and_body_socket(l, tcpobj, buf, strlen(buf));  /* rc is length of data */
	if( send_len < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "send_header_and_body_socket() error.");
		goto L0;
	}
	fq_log(l, FQ_LOG_DEBUG, "send OK DEQU send_len=[%d]", send_len);
	SAFE_FREE(buf);

	recv_len = recv_header_and_body_socket( l, tcpobj, &recvbuf );
	if( recv_len < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "ENQU response error. recv_len=[%d]", recv_len);
		goto L0;
	}
	fq_log(l, FQ_LOG_DEBUG, "recv_len=[%d] response=[%s]", recv_len, recvbuf);

	JSON_Value *jsonValue = json_parse_string(recvbuf);
	if( !jsonValue ) {
		fq_log(l, FQ_LOG_ERROR, "illegal json format. We throw it.\n");
		goto L0;
	}
	JSON_Object *jsonObject = json_value_get_object(jsonValue);
    if( !jsonObject ) {
        fq_log(l, FQ_LOG_ERROR, "illegal json format. We throw it.\n");
        goto L0;
    }
	fq_log(l, FQ_LOG_DEBUG, "JSON varify OK.");

	debug_json(l, jsonValue);

	char json_result[16];
	sprintf(json_result, "%s", json_object_get_string(jsonObject, "RESULT"));
	sprintf(skey, "%s", json_object_get_string(jsonObject, "SESSION_ID"));

	if( strcmp( json_result, "OK") == 0 ) {
		result = 'Y';
	}
	else if(strcmp( json_result, "EMPTY") == 0){
		result = 'E';
	}
	else {
		result = 'N'; // error
	}

	if( result == 'Y' ) {
		fq_log(l, FQ_LOG_DEBUG, "DEQU result OK.\n");
		rc = recv_len; 
	}
	else if( result == 'E' ) { /* empty */
		fq_log(l, FQ_LOG_DEBUG, "DEQU result : empty.\n");
		rc = EQ_EMPTY_RETURN_VALUE; 
	}
	else {
		fq_log(l, FQ_LOG_ERROR, "DEQU result error. result=[%c]", result);
		rc = -1;
	}
L0:
	*result_code = result;

	if( result == 'Y' ) {
		*qdata = recvbuf;
		*qdata_len = recv_len;
	}

	json_value_free(jsonValue);
	json_value_free(rootValue);
	SAFE_FREE(buf);

	sigprocmask(SIG_UNBLOCK, &sigset, NULL);

	FQ_TRACE_EXIT(l);
    return(rc);
}

int FQ_health_check_json( fq_logger_t *l, 
	tcp_client_obj_t *tcpobj, 
	char *skey, 
	char *path,
	char *qname,
	char *result_code)
{
	int rc = -1;
	FQ_protocol_t fqp;
	int	stream_len = 0;
	int send_len = 0;
	int	recv_len = 0;
	char	result='N';
	unsigned char *buf=NULL;
	unsigned char *recvbuf=NULL;
	char 	*user_msg = "I want to get a message";
	sigset_t    sigset;

	FQ_TRACE_ENTER(l);

	sigemptyset(&sigset);
    sigaddset(&sigset, SIGINT);
    sigaddset(&sigset, SIGQUIT);
    sigaddset(&sigset, SIGPIPE);
    sigaddset(&sigset, SIGSTOP);

    sigprocmask(SIG_BLOCK, &sigset, NULL);

	JSON_Value *rootValue = json_value_init_object();
	JSON_Object *rootObject = json_value_get_object(rootValue);

	json_object_set_string(rootObject, "FQP_VERSION", FQP_VERSION );
	json_object_set_string(rootObject, "SESSION_ID", skey);
	json_object_set_string(rootObject, "QUEUE_PATH", path);
	json_object_set_string(rootObject, "QUEUE_NAME", qname);
	json_object_set_string(rootObject, "ACK_MODE", "Y");
	json_object_set_string(rootObject, "ACTION", "HCHK");
	json_object_set_number(rootObject, "MSG_LENGTH", 0);
	json_object_set_string(rootObject, "MESSAGE", "");

	size_t buf_size;

	buf_size = json_serialization_size(rootValue) + 1;
	buf = calloc(sizeof(unsigned char), buf_size);
	json_serialize_to_buffer(rootValue, buf, buf_size);

	debug_json(l, rootValue);

	send_len = send_header_and_body_socket(l, tcpobj, buf, strlen(buf));  /* rc is length of data */
	if( send_len < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "send_header_and_body_socket() error.");
		goto L0;
	}
	fq_log(l, FQ_LOG_DEBUG, "send OK HCHK send_len=[%d]", send_len);
	SAFE_FREE(buf);

	recv_len = recv_header_and_body_socket( l, tcpobj, &recvbuf );
	if( recv_len < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "HCHK response error. recv_len=[%d]", recv_len);
		goto L0;
	}
	fq_log(l, FQ_LOG_DEBUG, "recv_len=[%d] response=[%s]", recv_len, recvbuf);

	JSON_Value *jsonValue = json_parse_string(recvbuf);
	if( !jsonValue ) {
		fq_log(l, FQ_LOG_ERROR, "illegal json format. We throw it.\n");
		goto L0;
	}
	JSON_Object *jsonObject = json_value_get_object(jsonValue);
    if( !jsonObject ) {
        fq_log(l, FQ_LOG_ERROR, "illegal json format. We throw it.\n");
        goto L0;
    }
	fq_log(l, FQ_LOG_DEBUG, "JSON varify OK.");

	debug_json(l, jsonValue);

	char json_result[16];
	sprintf(json_result, "%s", json_object_get_string(jsonObject, "RESULT"));
	sprintf(skey, "%s", json_object_get_string(jsonObject, "SESSION_ID"));

	if( strcmp( json_result, "OK") == 0 ) {
		result = 'Y';
	}
	else {
		result = 'N'; // error
	}

	if( result == 'Y' ) {
		fq_log(l, FQ_LOG_DEBUG, "HCHK result OK.\n");
		rc = recv_len; 
	}
	else {
		fq_log(l, FQ_LOG_ERROR, "HCHK result error. result=[%c]", result);
		rc = -1;
	}
L0:
	*result_code = result;

	json_value_free(jsonValue);
	json_value_free(rootValue);
	SAFE_FREE(buf);

	sigprocmask(SIG_UNBLOCK, &sigset, NULL);

	FQ_TRACE_EXIT(l);
    return(rc);
}
