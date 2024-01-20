/*
 * fq_tcp_client_obj.c
 */
#define FQ_TCP_CLIENT_OBJ_C_HEADER "1.0.2"

/* update */
/*
1.0.2: line358 VALUE(p) errror. on Solaris 
*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>

#include <sys/ipc.h>
#include <sys/msg.h>

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#include "fq_common.h"
#include "fq_logger.h"
#include "fq_tcp_client_obj.h"

#if 0
#define TRUE  1
#define FALSE 0
#endif

#include <sys/mman.h>

static ssize_t on_writen(fq_logger_t *l, tcp_client_obj_t *obj, const void *vptr, size_t n);
static ssize_t on_readn(fq_logger_t *l, tcp_client_obj_t *obj, void *vptr, size_t n);
static int on_set_bodysize(unsigned char* header, int size, int value, int header_type);
static int on_get_bodysize(unsigned char* header, int size, int header_type);

int 
open_tcp_client_obj( fq_logger_t *l, char *ip, char *port, socket_header_type_t type, int header_len, tcp_client_obj_t **obj)
{
	int ret=-1;
	tcp_client_obj_t *rc=NULL;
	struct addrinfo hints;
	struct addrinfo *servinfo=NULL;
	struct addrinfo *p=NULL;

	FQ_TRACE_ENTER(l);

	if( !HASVALUE(ip) || !HASVALUE(port)  )  {
        fq_log(l, FQ_LOG_ERROR, "illgal function call.");
		FQ_TRACE_EXIT(l);
		return(FALSE);
    }

	rc = (tcp_client_obj_t *)calloc(1, sizeof(tcp_client_obj_t));
	if(rc) {

		rc->ip = strdup(ip); // ip or hostname
		rc->port = strdup(port);
		rc->header_type = type;
		rc->header_len = header_len;
		rc->sockfd = -1;

		memset(&hints, 0, sizeof(struct addrinfo));

		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
	    // hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
	    hints.ai_flags = AI_PASSIVE;    /* For wildcard IP address */
	    hints.ai_protocol = 0;          /* Any protocol */
	    hints.ai_canonname = NULL;
	    hints.ai_addr = NULL;
	    hints.ai_next = NULL;

		// You must free servinfo after using.
		// void freeaddrinfo(struct addrinfo *res);
		//
		if ((ret = getaddrinfo(rc->ip, rc->port, &hints, &servinfo)) != 0) {
			fq_log(l, FQ_LOG_ERROR, "getaddrinfo() error. reason=[%s]", gai_strerror(ret));
			goto return_FALSE;
		}

		for(p = servinfo; p != NULL; p = p->ai_next) {
			if ((rc->sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
				fq_log(l, FQ_LOG_ERROR, "socket() error. reason=[%s]", strerror(errno));
				continue;
			}
			if (connect(rc->sockfd, p->ai_addr, p->ai_addrlen) == -1) {
				fq_log(l, FQ_LOG_ERROR, "connect() error. There is no listening server waiting a client at the [%s]-port. reason=[%s]", rc->port, strerror(errno));
				continue;
			}
			else {
				fq_log(l, FQ_LOG_DEBUG, "connected to %s %s.", rc->ip, rc->port);
				break;
			}
		}
		if( p == NULL ) {
			fq_log(l, FQ_LOG_ERROR, "connection failed.");
			goto return_FALSE;
		}

		fq_log(l, FQ_LOG_DEBUG, "client: connected to %s.", rc->ip);

		if(servinfo) freeaddrinfo(servinfo); // all done with this structure

		rc->l = l;
		rc->on_writen = on_writen;
		rc->on_readn = on_readn;
		rc->on_get_bodysize = on_get_bodysize;
		rc->on_set_bodysize = on_set_bodysize;

		SAFE_FREE(rc->ip);
		SAFE_FREE(rc->port);

		*obj = rc;
		FQ_TRACE_EXIT(l);
		return(TRUE);
	}

return_FALSE:
	if(rc->sockfd) {
		shutdown(rc->sockfd, SHUT_RDWR);
		close(rc->sockfd);
		rc->sockfd = -1;
	}
	if(servinfo) freeaddrinfo(servinfo);

	SAFE_FREE(rc);

	SAFE_FREE(*obj);

	FQ_TRACE_EXIT(l);
	return(FALSE);
}

int 
close_tcp_client_obj(fq_logger_t *l,  tcp_client_obj_t **obj)
{
	FQ_TRACE_ENTER(l);

	if( !*obj ) {
		FQ_TRACE_EXIT(l);
		return(TRUE);
	}

	SAFE_FREE((*obj)->ip);
	SAFE_FREE((*obj)->port);

	if( (*obj)->sockfd > 0 ) {
		shutdown((*obj)->sockfd, SHUT_RDWR);
		close( (*obj)->sockfd );
		(*obj)->sockfd = -1;
	}

	SAFE_FREE(*obj);

	FQ_TRACE_EXIT(l);
	return(TRUE);
}

static int 
on_set_bodysize(unsigned char* header, int size, int value, int header_type)
{
	int i;
	unsigned char* ptr = header;
	char fmt[40];

	if( header_type != 0 ) { /* ASCII */
		sprintf(fmt, "%c0%dd", '%', size);
		sprintf((char*)header, fmt, value);
		return(0);
	}

	else { /* Binary */
		for (i=0; i < size; i++) {
			*ptr = (value >> 8*(size-i-1)) & 0xff;
			ptr++;
		}
	}
	return(0);
}

static int 
on_get_bodysize(unsigned char* header, int size, int header_type)
{
	int i, k, value=0;
	unsigned char* ptr = header;

	if ( header_type !=0 ) { /* ASCII */
			header[size] = '\0';
			return(atoi((char*)header));
	}
	else {
		for (i=0; i < size; i++) {
			k = (int)(*ptr);
			value |= ( (k & 0xff) << 8*(size-i-1) );
			ptr++;
		}
		return(value);
	}
}
static ssize_t						/* Write "n" bytes to a descriptor. */
on_writen(fq_logger_t *l, tcp_client_obj_t *obj, const void *vptr, size_t n)
{
	size_t		nleft;
	ssize_t		nwritten;
	const char	*ptr=NULL;

	FQ_TRACE_ENTER(l);

	ptr = vptr;

	// packet_dump(ptr, n, 8, "writen ontents");

	nleft = n;
	while (nleft > 0) {
		if ( (nwritten = write(obj->sockfd, ptr, nleft)) <= 0) {
			if (errno == EINTR) {
				nwritten = 0;		/* and call write() again */
			}
			else {
				FQ_TRACE_EXIT(l);
				return(-1);			/* error */
			}
		}

		nleft -= nwritten;
		ptr   += nwritten;
	}

	FQ_TRACE_EXIT(l);
	return(n);
}
/* end writen */


static ssize_t						/* Read "n" bytes from a descriptor. */
on_readn(fq_logger_t *l, tcp_client_obj_t *obj, void *vptr, size_t n)
{
	size_t	nleft;
	ssize_t	nread;
	char	*ptr=NULL;

	FQ_TRACE_ENTER(l);

	ptr = vptr;
	nleft = n;
	while (nleft > 0) {
		if ( (nread = read(obj->sockfd, ptr, nleft)) < 0) {
			if (errno == EINTR) {
				nread = 0;		/* and call read() again */
			}
			else {
				FQ_TRACE_EXIT(l);
				return(-1);
			}
		} else if (nread == 0)
			break;				/* EOF */

		nleft -= nread;
		ptr   += nread;
	}

	FQ_TRACE_EXIT(l);
	return(n - nleft);		/* return >= 0 */
}
/* end readn */

int 
daemon_init(void)
{
	pid_t pid;
	int rc;

	if ((pid = fork()) < 0)
		return -1;
	else if (pid != 0)
		exit(0); /* parent exit */

#if 0
	close(0);
	close(1);
	close(2);
#endif

	setsid();
	rc = chdir("/");
	CHECK(rc == 0);
	umask(0);

	return 0;
}
#if 0
/* It is 10 TPS */
int 
send_header_and_body_socket(fq_logger_t *l, tcp_client_obj_t *tcpobj, unsigned char *buf, int datalen)
{
	int	ret=-1;
	int	sent=0;
	unsigned char *header=NULL;

	FQ_TRACE_ENTER(l);

	header = calloc(SOCKET_HEADER_LEN+1, sizeof(char));

	/* µ¥ÀÌÅÍ¸¦ Àü¼ÛÇÑ´Ù */
	ret = tcpobj->on_set_bodysize(header, tcpobj->header_len, datalen, tcpobj->header_type);
	if( ret != 0 ) {
		fq_log(l, FQ_LOG_ERROR, "on_set_bodysize() error.");
		goto L0;
	}

	sent = tcpobj->on_writen(l, tcpobj, (const void *)header, SOCKET_HEADER_LEN);
	if(sent < 0) {
		fq_log(l, FQ_LOG_ERROR, "header: on_writen() error. sent=[%d].", sent);
		goto L0;
	}
	fq_log(l, FQ_LOG_DEBUG, "header: on_writen() OK. sent=[%d].", sent);

	sent = tcpobj->on_writen(l, tcpobj, (const void *)buf, datalen);
	if(sent < 0) {
		fq_log(l, FQ_LOG_ERROR, "body: on_writen() error. sent=[%d].", sent);
		goto L0;
	}
	fq_log(l, FQ_LOG_DEBUG, "body: on_writen() OK. sent=[%d].", sent);
	ret=sent;
L0:
	SAFE_FREE(header);

	FQ_TRACE_EXIT(l);
	return(ret);
}
#else 
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
#endif

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

int 
recv_ACK( fq_logger_t *l, tcp_client_obj_t *tcpobj )
{
	unsigned char *header=NULL;
	int	bodylen=0;
	unsigned char buf[4096];
	int	n;
	int	ret=-1;

	FQ_TRACE_ENTER(l);

	header = calloc(SOCKET_HEADER_LEN+1, sizeof(char));

	/* ACK ¸¦ ¼ö½ÅÇÑ´Ù */
	memset(header, 0x00, SOCKET_HEADER_LEN+1);
	n = tcpobj->on_readn(l, tcpobj, header, SOCKET_HEADER_LEN);
	if( n != SOCKET_HEADER_LEN ) {
		fq_log(l, FQ_LOG_ERROR,  "header receiveing failed.");
		goto L0;
	}
	fq_log(l, FQ_LOG_DEBUG, "header: on_readn() OK. readn=[%d].", n);

	bodylen = tcpobj->on_get_bodysize(header, tcpobj->header_len, tcpobj->header_type);
	if( bodylen <= 0 || bodylen > sizeof(buf)-1 ) {
		fq_log(l, FQ_LOG_ERROR,  "header contents is not available. bodylen=[%d].", bodylen);
		goto L0;
	}
	fq_log(l, FQ_LOG_DEBUG, "length of header contents(ACK) is [%d]-character.", bodylen);

	memset(buf, 0x00, sizeof(buf));
	n = tcpobj->on_readn(l, tcpobj, buf, bodylen);
	if( n != bodylen ) {
		fq_log(l, FQ_LOG_ERROR,  "body receiveing error.");
		goto L0;
	}
	fq_log(l, FQ_LOG_DEBUG, "n=[%d] ACK body=[%s].", n, buf);

	ret = n;
L0:
	SAFE_FREE(header);

	FQ_TRACE_EXIT(l);

	return(ret);
}

int 
request_restart_me( fq_logger_t *l, char *auto_fork_svr_listen_port, unsigned char *restart_cmd)
{
	int rc;
	tcp_client_obj_t *tcpobj=NULL;

	FQ_TRACE_ENTER(l);

	rc = open_tcp_client_obj(l, "localhost", auto_fork_svr_listen_port, BINARY_HEADER, SOCKET_HEADER_LEN, &tcpobj);
	if( rc != TRUE ) {
		fq_log(l, FQ_LOG_ERROR, "failed to connect to auto_fork_svr. auto_fork_svr_listen_port=[%s].", auto_fork_svr_listen_port);
		FQ_TRACE_EXIT(l);
		return(-1);
	}

	rc = send_header_and_body_socket(l, tcpobj, restart_cmd , strlen((char *)restart_cmd));
	if( rc <= 0 ) {
		fq_log(l, FQ_LOG_ERROR, "send_header_and_body_socket() error.");
		FQ_TRACE_EXIT(l);
		return(-2);
	}

	rc = close_tcp_client_obj(NULL, &tcpobj);
	if( rc != TRUE ) {
		fq_log(l, FQ_LOG_ERROR, "send_header_and_body_socket() error.");
		FQ_TRACE_EXIT(l);
		return(-2);
	}
	FQ_TRACE_EXIT(l);

	return(TRUE);
}

bool checkServerStatus(const char *server_ip, int server_port) 
{
    int sockfd;
    struct sockaddr_in server_addr;

    // ¿¿ ¿¿
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket creation failed");
        return false;  // ¿¿ ¿¿ ¿ -1 ¿¿
    }

    // ¿¿ ¿¿ ¿¿
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        close(sockfd);
        return false;  // ¿¿ ¿¿ ¿ -1 ¿¿
    }

    // ¿¿¿ ¿¿ ¿¿
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Connection failed");
        close(sockfd);
        return false;  // ¿¿ ¿¿ ¿ 0 ¿¿ (¿¿ ¿¿¿ ¿¿¿ ¿¿)
    }

    // ¿¿ ¿¿ ¿ ¿¿ ¿¿ 1 ¿¿ (¿¿ ¿¿ ¿¿¿ ¿¿)
    close(sockfd);
    return true;
}
