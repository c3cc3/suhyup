/*
** fq_socket.c
*/
#define FQ_SOCKET_C_VERSION "1.0.2"
/*
** ver1.0.1: line 830 timeout.sec is zero.
** ver1.0.2: bug patch: writen(): add signal(SIGPIPE, SIG_IGN);
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

/* in Solaris , It is not supported */
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

#include <signal.h>
#include <math.h>

#include "fq_socket.h"
#include "fq_logger.h"
#include "fq_common.h"

/* get sockaddr, IPv4 or IPv6: */
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int set_bodysize(unsigned char* header, int size, int value, int header_type)
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

int get_bodysize(unsigned char* header, int size, int header_type)
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

static long get_header_max(int HeaderSize)
{
	return( (long)(pow(10, HeaderSize)-1) );
}

unsigned char *recv_header( int HeaderSize, int HeaderType, int sockfd)
{
	int	rc=-1, n, recv_body_size=0;
	long 	max_header_size; 
	unsigned char *Header=NULL;

	if( HeaderSize == 0 ) { /* 헤더 사용하지 않음 */
		fprintf(stderr, "\t<< 헤더 사용하지 않음.\n");
		rc = 0;
		goto L0;
	}

	if( HeaderSize < 4 || HeaderSize > 8 ) {
		fprintf(stderr, "\t<< 헤더크기 범위[4-8] 오류. HeaderSize=[%d]\n", HeaderSize);
		goto L0;
	}

	Header = calloc(HeaderSize+1, sizeof(char));

	/* Header를 받을때 timed_recv()를 사용하지 말 것 */
	n = recv(sockfd, (char *)Header, HeaderSize, 0);

	if( n<=0 ) {
		fprintf(stderr, "\t>> header receiving failed.[%s][%d].\n", __FILE__, __LINE__);
		goto L0;
	}
	
	max_header_size = get_header_max(HeaderSize);

	recv_body_size = get_bodysize(Header, HeaderSize, HeaderType);

	if( recv_body_size < 1 || recv_body_size > max_header_size ) {
		fprintf(stderr, "\t>> Header range error:  Didn't receive proper header or Range is not right. Right header range is (0<x<%ld). your sending header is %d.\n", (long)max_header_size, recv_body_size);
		fprintf(stderr, "\t>> Header size was defined with %d in the config file. but client sent %d.\n", HeaderSize, recv_body_size);
		rc = -1;
		goto L0;
	}

	rc = recv_body_size;

L0:
	if( rc < 0 ) {
		SAFE_FREE(Header);
	}

	return(Header);
}

int send_header( int HeaderSize, int nSend, int HeaderType, int sock)
{
	int rc = -1;
	unsigned char header[128];

	if( HeaderSize > 0 ) {
        int     r;
		set_bodysize( header, HeaderSize, nSend, HeaderType );
		r = send(sock,(const char *)header, HeaderSize, 0);
		if( r == 0 ) {
				fprintf(stderr, "\t>> [%s][%d] reason=[%s].\n", __FILE__, __LINE__, strerror(errno));
				goto L0;
		}
		if( r < 0 ) {
				fprintf(stderr, "\t>> [%s][%d] reason=[%s].\n",  __FILE__, __LINE__, strerror(errno));
				goto L0;
		}
	}
	/* 
	fprintf(stdout, "\t>> send_header: HeaderType=[%d] HeaderSize=[%d] will=[%d]\n", HeaderType, HeaderSize, nSend);
	*/
	rc = 0;
L0:
	return(rc);
}

/**********************************************************************
** create_tcp_socket
** Operation : Create, bind, and listen a TCP socket.
** Argument  : int port;  - TCP port number
** Return    : sockfd if successful, -1 o/w
*/
int 
create_tcp_socket(fq_logger_t *l, int port, const char *ip)
{
	int sockfd=-1;
	int one = 1;
	struct sockaddr_in sin;
	struct linger option;

	FQ_TRACE_ENTER(l);


	if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "Can't get AF_INET sockfd, reason=%s.", strerror(errno));
		FQ_TRACE_EXIT(l);
		return(-1);
	}

	/*
	** 2013/01/20 in ShinHanBank.
	** this is a method to fork a process without inherit.
	*/
    fcntl(sockfd, F_SETFD, fcntl(sockfd, F_GETFD)|FD_CLOEXEC);

	if ( setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&one, sizeof(one)) < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "Can't set socket option(SO_REUSEADDR) for sockfd=%d, reason=%s.", sockfd, strerror(errno));
		SAFE_FD_CLOSE(sockfd);
		FQ_TRACE_EXIT(l);
		return(-1);
	}

	option.l_onoff = 1; /* on */
	option.l_linger = 0; /* When You close a socket, Closing is delay for 3 sec  until data is sent. */ 
	if ( setsockopt(sockfd, SOL_SOCKET, SO_LINGER, (const void *)&option, sizeof(option)) < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "Can't set socket option(SO_LINGER) for sockfd=%d, reason=%s.", sockfd, strerror(errno));
		SAFE_FD_CLOSE(sockfd);
		FQ_TRACE_EXIT(l);
		return(-1);
	}

#if 0
	/* We only increase SNDBUF for sending */
	if( setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, (const void *)&sndbuffsize, sizeof(option)) < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "Can't set socket option (SO_SNDBUF) for sockfd=%d, reason=%s.", sockfd, strerror(errno));
		SAFE_FD_CLOSE(sockfd);
		FQ_TRACE_EXIT(l);
		return(-1);
	}

	if( setsockopt(sockfd, SOL_SOCKET, SO_OOBINLINE, (const void *)&oobflag, sizeof(option)) < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "Can't set socket option (SO_OOBINLINE) for sockfd=%d, reason=%s.", sockfd, strerror(errno));
		SAFE_FD_CLOSE(sockfd);
		FQ_TRACE_EXIT(l);
		return(-1);
	}

	if( setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, (const void *)&aliveflag, sizeof(option)) < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "Can't set socket option (SO_KEEPALIVE) for sockfd=%d, reason=%s.", sockfd, strerror(errno));
		SAFE_FD_CLOSE(sockfd);
		FQ_TRACE_EXIT(l);
		return(-1);
	}
#endif

	bzero((void*)&sin, sizeof(sin));
	sin.sin_family = AF_INET;

	if( !HASVALUE(ip) ) 
		sin.sin_addr.s_addr = htonl(INADDR_ANY);
	else 
		sin.sin_addr.s_addr = inet_addr(ip);

	sin.sin_port = htons((u_short)port);

	if ( bind(sockfd, (struct sockaddr*)&sin, sizeof(sin)) < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "Can't bind TCP socket port=%d, reason=%s.", port, strerror(errno));
		SAFE_FD_CLOSE(sockfd);
		FQ_TRACE_EXIT(l);
		return(-1);
	}

	if ( listen(sockfd, 5) ) {
		fq_log(l, FQ_LOG_ERROR, "Can't listen TCP socket port=%d, reason=%s.", port, strerror(errno));
		SAFE_FD_CLOSE(sockfd);
		FQ_TRACE_EXIT(l);
		return(-1);
	}

	fq_log(l, FQ_LOG_DEBUG, "Server is listening to connect."); 
	FQ_TRACE_EXIT(l);
	return(sockfd);
}

ssize_t						/* Write "n" bytes to a descriptor. */
writen(int fd, const void *vptr, size_t n)
{
	size_t		nleft;
	ssize_t		nwritten;
	const char	*ptr;

	signal(SIGPIPE, SIG_IGN);

	ptr = vptr;
	nleft = n;
	while (nleft > 0) {
		if ( (nwritten = write(fd, ptr, nleft)) <= 0) {
			if (errno == EINTR) 
				nwritten = 0;		/* and call write() again */
			else
				return(-1);			/* error */
		}

		nleft -= nwritten;
		ptr   += nwritten;
	}
	return(n);
}
/* end writen */

ssize_t						/* Write "n" bytes to a descriptor. */
debug_writen(fq_logger_t *l, int fd, const void *vptr, size_t n)
{
	size_t		nleft;
	ssize_t		nwritten;
	const char	*ptr=NULL;

    FQ_TRACE_ENTER(l);
	
	signal(SIGPIPE, SIG_IGN);

	ptr = vptr;
	nleft = n;
	while (nleft > 0) {
		if ( (nwritten = write(fd, ptr, nleft)) <= 0) {
			if (errno == EINTR) 
				nwritten = 0;		/* and call write() again */
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
ssize_t						/* Write "n" bytes to a descriptor. */
debug_writen_timeout(fq_logger_t *l, int fd, const void *vptr, size_t n, struct timeval *timeout)
{
	size_t		nleft;
	ssize_t		nwritten;
	const char	*ptr=NULL;
	fd_set set;
	int	nset, cnt;

    FQ_TRACE_ENTER(l);

	signal(SIGPIPE, SIG_IGN);

	FD_ZERO(&set);
	nset = fd + 1;
	FD_ZERO(&set);
	FD_SET(fd, &set);
	if ((cnt = select(nset, 0, &set, 0, timeout)) == 0) {
		fq_log(l, FQ_LOG_ERROR, "write timeout");
		return(-1);
	}
	if( cnt < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "select() error.");
		return(-2);
	}
	if (FD_ISSET(fd, &set))  {
		ptr = vptr;
		nleft = n;
		while (nleft > 0) {
			if ( (nwritten = write(fd, ptr, nleft)) <= 0) {
				if (errno == EINTR) 
					nwritten = 0;		/* and call write() again */
				else {
					fq_log(l, FQ_LOG_ERROR, "write() error.");
					FQ_TRACE_EXIT(l);
					return(-1);			/* error */
				}
			}

			nleft -= nwritten;
			ptr   += nwritten;
		}
	}

    FQ_TRACE_EXIT(l);
	return(n);
}

ssize_t						/* Read "n" bytes from a descriptor. */
readn(int fd, void *vptr, size_t n)
{
	size_t	nleft;
	ssize_t	nread;
	char	*ptr;

	signal(SIGPIPE, SIG_IGN);

	ptr = vptr;
	nleft = n;
	while (nleft > 0) {
		if ( (nread = read(fd, ptr, nleft)) < 0) {
			if (errno == EINTR)
				nread = 0;		/* and call read() again */
			else
				return(-1);
		} else if (nread == 0)
			break;				/* EOF */

		nleft -= nread;
		ptr   += nread;
	}
	return(n - nleft);		/* return >= 0 */
}
/* end readn */
ssize_t						/* Read "n" bytes from a descriptor. */
debug_readn(fq_logger_t *l, int fd, void *vptr, size_t n)
{
	size_t	nleft;
	ssize_t	nread;
	char	*ptr;

    FQ_TRACE_ENTER(l);

	ptr = vptr;
	nleft = n;
	while (nleft > 0) {
		if ( (nread = read(fd, ptr, nleft)) < 0) {
			if (errno == EINTR)
				nread = 0;		/* and call read() again */
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
ssize_t						/* Read "n" bytes from a descriptor. */
debug_readn_timeout(fq_logger_t *l, int fd, void *vptr, size_t n, struct timeval *timeout)
{
	size_t	nleft;
	ssize_t	nread;
	char	*ptr;
	fd_set set;
    int nset, cnt;

    FQ_TRACE_ENTER(l);

	nset = fd + 1;
	FD_ZERO(&set);
	FD_SET(fd, &set);
	if ((cnt = select(nset, &set, 0, 0, timeout)) == 0) {
		fq_log(l, FQ_LOG_ERROR, "write() timeout");
		FQ_TRACE_EXIT(l);
		return(-1);
	} 
	if( cnt < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "select() error.");
		FQ_TRACE_EXIT(l);
		return(-2);
	}
	if (FD_ISSET(fd, &set)) {
		ptr = vptr;
		nleft = n;
		while (nleft > 0) {
			if ( (nread = read(fd, ptr, nleft)) < 0) {
				if (errno == EINTR)
					nread = 0;		/* and call read() again */
				else {
					fq_log(l, FQ_LOG_ERROR, "read() error.");
					FQ_TRACE_EXIT(l);
					return(-1);
				}
			} else if (nread == 0)
				break;				/* EOF */

			nleft -= nread;
			ptr   += nread;
		}
	}

	FQ_TRACE_EXIT(l);

	return(n - nleft);		/* return >= 0 */
}
/* end readn */

int make_thread_server( fq_logger_t *l, TCP_server_t *x, char *ip, int port, CB1 fp)
{
	pthread_t tid;
	int	error;
	pthread_attr_t attr;

	FQ_TRACE_ENTER(l);

	if(!check_host(ip)) {
		fq_log(l, FQ_LOG_ERROR, "invalid hostname.");
		FQ_TRACE_EXIT(l);
		return(-1);
	}

	if( port < 0 || port > 99999 ) {
		fq_log(l, FQ_LOG_ERROR, "invalid port number");
		FQ_TRACE_EXIT(l);
		return(-2);
	}

	fq_log(l, FQ_LOG_DEBUG, "valid hostname. ip=[%s] listen port=[%d]", ip, port);
	
	x->l = l;
	x->ip = strdup(ip);
	x->port = port;
	x->fp = fp;
	x->header_type = BINARY;
	x->header_size = 4;

	pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	if( pthread_create(&tid, &attr, TCP_callback_server, x) != 0 ) {
        fq_log(l, FQ_LOG_ERROR, "pthread_create() error!");
		FQ_TRACE_EXIT(l);
        exit(0);
    }
	fq_log(l, FQ_LOG_DEBUG, "callback server is started.");
	FQ_TRACE_EXIT(l);
	return(0);
}

void *TCP_callback_server(void *argument)
{
	TCP_server_t *x=NULL;
	fd_set  rmask, mask;
	int request_sock, addrlen;
	static struct timeval timeout = {30, 0};
	struct sockaddr_in remote;
	struct linger option;

#if 0
	signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
#endif

#if 0
    signal(SIGPIPE, SIG_IGN);
#endif

	x = (TCP_server_t *)argument;
		
	/* printf("x->port = [%d]\n", x->port); */

	FQ_TRACE_ENTER(x->l);

	if( (request_sock = create_tcp_socket(x->l, x->port, x->ip)) == -1) {
		fq_log(x->l, FQ_LOG_ERROR,  "create_tcp_socket() error. Good bye!...\n");
		FQ_TRACE_EXIT(x->l);
		exit(0);
	}
	fq_log(x->l, FQ_LOG_DEBUG, "create socket OK.");

	FD_ZERO(&mask);

	for(;;) {
		int nfound;
		rmask = mask;
		FD_SET(request_sock, &rmask);

		timeout.tv_sec = (long)60;
		timeout.tv_usec = 0;

		nfound = select(request_sock+1, &rmask, (fd_set *)0, (fd_set *)0, &timeout);
		if( nfound < 0) {
			if(errno == EINTR ) {
				continue;
			}
			fq_log(x->l, FQ_LOG_ERROR,  "fatal error.[%s][%d]!\n", __FILE__, __LINE__);
			FQ_TRACE_EXIT(x->l);
			exit(0);
		}

		if( nfound == 0 ) {
			fq_log(x->l,  FQ_LOG_INFO, "New connection is not during %ld sec.\n", 60 );
			continue;
		}

		/* Maximum masking count is 1024 */
		if(FD_ISSET(request_sock, &rmask)) {
			int new_sock;

			addrlen = sizeof(remote);

#ifdef OS_HPUX
			new_sock = accept(request_sock, (struct sockaddr *)&remote, &addrlen);
#else
			new_sock = accept(request_sock, (struct sockaddr *)&remote, (socklen_t *)&addrlen);
#endif

			if( new_sock < 0 ) {
				/* Maximum masking count is 1024 */
				fq_log(x->l, FQ_LOG_ERROR, "too many clients (sleep mode = 30 sec)!\n");
				FQ_TRACE_EXIT(x->l);
				exit(0);
			}
			option.l_onoff = 1;
			option.l_linger = 0;
			if ( setsockopt(new_sock, SOL_SOCKET, SO_LINGER, (const void *)&option, sizeof(option)) < 0 ) {
				fq_log(x->l, FQ_LOG_ERROR, "Can't set socket option(SO_LINGER) for sockfd=%d, reason=%s.", new_sock, strerror(errno));
			}
			fcntl(new_sock, F_SETFD, fcntl(new_sock, F_GETFD)|FD_CLOEXEC);
		
			while(1) {
				int nbytes=0;
				fd_set mask;
				int	cnt=0;
				unsigned char	*ptr=NULL;
				int	data_len=0;
				unsigned char   body[SOCK_MAXDATALEN+1];
				int		rc=-1;

				timeout.tv_sec = (long)60;
				timeout.tv_usec = 0;
				
				FD_ZERO(&mask);
				FD_SET(new_sock, &mask);

				memset(body, 0x00, sizeof(body));
				cnt = select(new_sock+1, &mask, (fd_set *)0, (fd_set *)0, &timeout);
				if (cnt < 0) {
					fq_log(x->l, FQ_LOG_ERROR,  "Something is bad.\n");
					break;
				}

				if(cnt == 0) {
					fq_log(x->l,  FQ_LOG_DEBUG, "no data for 30 sec.\n");
					continue;
				}

				if (FD_ISSET(new_sock, &mask)) {
					int header_size=x->header_size;
					unsigned char *header = NULL;

					nbytes = 0;

					header = calloc( x->header_size+1, sizeof(unsigned char *));

					fq_log(x->l, FQ_LOG_DEBUG, "select OK.");

					if(check_socket(new_sock) < 0 ) {
						fq_log(x->l, FQ_LOG_ERROR, "disabled socket.");
						exit(0);
					}

					if( (header_size=readn(new_sock, &body[0], x->header_size)) != x->header_size ) {
						fq_log(x->l, FQ_LOG_ERROR, "header receive error!!");
						exit(0);
					}
					memcpy(header, body, SOCK_CLIENT_HEADER_SIZE);

					data_len = get_bodysize( header, x->header_size, x->header_type);

					if( data_len > SOCK_MAX_USER_DATA_SIZE ) {
						fq_log(x->l, FQ_LOG_ERROR, "too big size data., Check header_type and header_size.");
						fq_log(x->l,FQ_LOG_ERROR, "------HEADER DUMP------");
						if( x->header_type == BINARY ) {
							fq_log(x->l,FQ_LOG_ERROR, "header = [%02x][%02x][%02x][%02x]", 
								header[0], header[1], header[2], header[3]);
						}
						else {
							fq_log(x->l, FQ_LOG_ERROR, "header = [%s]", header);
						}
						exit(0);
					}

					if( data_len < 0 ) {
						fq_log(x->l, FQ_LOG_ERROR, "header value errror(minus): Check header_type and header_size. ");
						exit(0);
					}

					fq_log(x->l, FQ_LOG_DEBUG, "Header received: data_len=[%d].", data_len);

					if( (nbytes=readn( new_sock, &body[SOCK_CLIENT_HEADER_SIZE], data_len-SOCK_CLIENT_HEADER_SIZE)) != (data_len-SOCK_CLIENT_HEADER_SIZE) ) {
						fq_log(x->l, FQ_LOG_ERROR, "socket read error!! nbytes=[%d].", nbytes);
						SAFE_FREE(ptr);
						exit(0);
					}
					fq_log(x->l, FQ_LOG_DEBUG, "Body received: [%s]", body);
					/* callback */
					if( (rc = x->fp(body, data_len)) < 0) {
						fq_log( x->l, FQ_LOG_ERROR, "callback user function error. rc=[%d]", rc);
						exit(0);
					}
					fq_log(x->l, FQ_LOG_DEBUG, "ready for next data......");
				}
			} /* while end */
		}
	}
}

/* Usage */
#if 0
int my_function(unsigned char *data, int len)
{
	printf("called CB.\n");
    printf("len=[%d] [%s]\n", len, data);
    return(1);
}

int main(int ac, char **av)
{
	TCP_server_t x;

	make_thread_server( &x,  "127.0.0.1", 33333, my_function);

	printf("server start!!!\n");

	pause();
	
	return(0);
}
#endif

int make_thread_sync_CB_server( fq_logger_t *l, TCP_server_t *x, char *ip, int port, header_type_t header_type, int header_size, int ack_mode, CB2 sync_fp)
{
	pthread_t tid;
	pthread_attr_t attr;

	FQ_TRACE_ENTER(l);

	x->l = l;
	x->ip = strdup(ip);
	x->port = port;
	x->ack_mode = ack_mode;
	x->sync_fp = sync_fp;
	x->header_type = header_type;
	x->header_size = header_size;

	fq_log(l, FQ_LOG_DEBUG, "ip=[%s], port=[%d], header_type=[%d], header_size==[%d]",
		x->ip, x->port, x->header_type, x->header_size);

	pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	if( pthread_create(&tid, &attr, TCP_callback_sync_server, x) != 0) {
        fq_log(l, FQ_LOG_ERROR, "pthread_create() error!\n");
        exit(0);
    }
	FQ_TRACE_EXIT(l);
	return(0);
}

typedef struct _cleanup_args {
	int sockfd; /* opened socket descriptor */
	char	*ptr;
} cleanup_args_t;
	

void cleanup_manager_thread(void *arg)
{
	cleanup_args_t *t = (cleanup_args_t *)arg;
	
	SAFE_FD_CLOSE( t->sockfd);
	SAFE_FREE(t->ptr);

	return;
}

void *manager_thread( void *arg )
{
	int new_sock;
	manager_server_t *x=NULL;
	x = (manager_server_t *)arg;
	struct timeval timeout;

#if 0
	signal(SIGPIPE, SIG_IGN);
#endif

	new_sock = x->new_sock;
	fq_log(x->l, FQ_LOG_INFO, "Accepted: TCP managing Server thread started.");

	while(1) {
		int nbytes=0;
		fd_set mask;
		int	cnt=0;
		int	data_len=0;
		int	resp_len=0;
		unsigned char   body[SOCK_MAXDATALEN+1];
		unsigned char   resp[SOCK_MAXDATALEN+1];
		int		rc=-1;

		timeout.tv_sec = (long)60;
		timeout.tv_usec = 0;
		
		FD_ZERO(&mask);
		FD_SET(new_sock, &mask);

		cnt = select(new_sock+1, &mask, (fd_set *)0, (fd_set *)0, &timeout);
		if (cnt < 0) {
			fq_log(x->l, FQ_LOG_ERROR, "Something is bad.\n");
			break;
		}

		if(cnt == 0) {
			fq_log(x->l, FQ_LOG_INFO, "no data for 30 sec.\n");
			continue;
		}

		if (FD_ISSET(new_sock, &mask)) {
			int n;
			int header_size=x->header_size;
			unsigned char *header=NULL;

			memset(body, 0x00, sizeof(body));
			memset(resp, 0x00, sizeof(resp));
			nbytes = 0;

			fq_log(x->l, FQ_LOG_DEBUG, "\n\n---------<new request>----------");
			fq_log(x->l, FQ_LOG_DEBUG, "header reading start.(defined header type[%d] size [%d])", x->header_type, header_size);
			
			if( (n=readn(new_sock, &body[0], header_size)) != header_size ) {
				/* In non-permanant socket, It is not error.
				fq_log(x->l, FQ_LOG_ERROR, "Error receiving header! Socket may be stopped by peer.\n");
				*/
				fq_log(x->l, FQ_LOG_INFO, "Header receiving failed! In case of non-permant socket, It is not error. Socket may be stopped by peer.\n");
				break;
			}

			fq_log(x->l, FQ_LOG_DEBUG, "header readn() OK. n=[%d]",  header_size);

			header = calloc(header_size+1, sizeof(unsigned char));
			memcpy(header, body, header_size);

			/* packet_dump(header, header_size, 4, "receive header contents"); */

			data_len = get_bodysize( header, header_size, x->header_type);

			if( data_len > SOCK_MAX_USER_DATA_SIZE ) {
				fq_log(x->l,FQ_LOG_ERROR, "Too big size data. Check header_type and header_size. data_len=[%d] SOCK_MAX_USER_DATA_SIZE=[%d]", data_len, SOCK_MAX_USER_DATA_SIZE);

				fq_log(x->l,FQ_LOG_ERROR, "------HEADER DUMP------");
				if( x->header_type == BINARY ) {
					fq_log(x->l,FQ_LOG_ERROR, "header = [%02x][%02x][%02x][%02x]", 
						header[0], header[1], header[2], header[3]);
				}
				else {
					fq_log(x->l,FQ_LOG_ERROR, "header = [%s]", header);
				}
				SAFE_FREE(header);
				// break;
				goto STOP_Thread;
			}

			if( data_len < 0 ) {
				fq_log(x->l,FQ_LOG_ERROR, "Error: Check header protocol type and size");
				break;
			}

			if( data_len == 0 ) {
				fq_log(x->l,FQ_LOG_ERROR, "Error: header value is 0. client is close or header data error.");
				break;
			}

			fq_log(x->l, FQ_LOG_DEBUG, "header parsing OK len=[%d]\n", data_len);

			if( (nbytes=readn(new_sock, &body[x->header_size], data_len)) != (data_len) ) {
				fq_log(x->l,FQ_LOG_ERROR,  "Error reading body!! nbytes=[%d]. header was [%d].\n", 
					nbytes, data_len);
				break;
			}

			fq_log(x->l, FQ_LOG_DEBUG, "body receiving OK len=[%d]\n", nbytes);

			/* callback(transaction) */
			if( (rc = x->sync_fp(body, data_len, resp, &resp_len)) < 0) {
				fq_log( x->l, FQ_LOG_ERROR, "callback user function error. check to regist func. rc=[%d]\n", rc);
				break;
			}

			if( rc == 99 ) { /* logout */
				fq_log( x->l, FQ_LOG_ERROR, "logout. rc=[%d]\n", rc);
				break;
			}

			if( resp_len > SOCK_MAXDATALEN ) {
				fq_log(x->l, FQ_LOG_ERROR, "Response data(%d) length is too long. max is [%d]\n", resp_len, SOCK_MAXDATALEN);
				break;
			}

			memset(header, 0x00, sizeof(header));
			set_bodysize(header, x->header_size, resp_len, x->header_type);

			fq_log(x->l, FQ_LOG_DEBUG, "Start Sending a response.");

			if(x->ack_mode == 1) {
				unsigned char *packet=NULL;
				int packet_len;

				packet_len = x->header_size+resp_len;
				packet = calloc(packet_len, sizeof(unsigned char));
				memcpy(packet, header, x->header_size);
				memcpy(packet+x->header_size, resp, resp_len);

#if 0
				/* send ACK header */
				if( (nbytes=writen(new_sock, header, x->header_size)) != x->header_size ) {
					fq_log(x->l, FQ_LOG_ERROR, "Error writing header!! nbytes=[%d].\n", nbytes);
					break;
				}

				/* send ACK body */
				if( (nbytes=writen(new_sock, resp, resp_len)) != resp_len ) {
					fq_log(x->l, FQ_LOG_ERROR, "Error writing resp !! nbytes=[%d].\n", nbytes);
					break;
				}
#else
				if( (nbytes=writen(new_sock, packet, packet_len)) != packet_len ) {
					fq_log(x->l, FQ_LOG_ERROR, "Error writing packet!! nbytes=[%d].\n", nbytes);
					break;
				}
#endif

				fq_log(x->l, FQ_LOG_DEBUG, "Response to client:[%s] len=[%d].\n", resp, resp_len);
				if(packet) free(packet);
			}

			if(header) free(header);
		}
	} /* while end */

STOP_Thread:
	shutdown(new_sock, SHUT_RDWR);
	SAFE_FD_CLOSE(new_sock);

	fq_log(x->l, FQ_LOG_INFO, "Server Thread exited.\n");
    pthread_exit((void *)0);
	return((void *)0);
}
	

void *TCP_callback_sync_server( void *argument)
{
	TCP_server_t *x=NULL;
	fd_set  rmask, mask;
	int request_sock, addrlen;
	static struct timeval timeout = {60, 0};
	struct sockaddr_in remote;
	pthread_attr_t attr;

	/* x = (struct data_to_pass_via_pthread_create *)argument; */
	x = (TCP_server_t *)argument;
	
	/* printf("x->port = [%d]\n", x->port); */

	if( (request_sock = create_tcp_socket( x->l, x->port, x->ip)) == -1) {
		fq_log(x->l, FQ_LOG_ERROR, "create_tcp_socket() error. Good bye!...");
		exit(0);
		/* pthread_exit((void *)0); */
	}
	
	pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	FD_ZERO(&mask);

	for(;;) {
		int nfound;
		rmask = mask;
		FD_SET(request_sock, &rmask);

		timeout.tv_sec = 60L;
		timeout.tv_usec = 0;

		nfound = select(request_sock+1, &rmask, (fd_set *)0, (fd_set *)0, &timeout);
		 if( nfound < 0) {
			if(errno == EINTR ) {
				continue;
			}
			fq_log(x->l, FQ_LOG_ERROR, "fatal error.[%s][%d]!", __FILE__, __LINE__);
			pthread_exit((void *)0);
		}

		if( nfound == 0 ) {
			/* fq_log(x->l, FQ_LOG_INFO, "New connection is not during [%ld] sec.", timeout.tv_sec ); */
			fq_log(x->l, FQ_LOG_INFO, "There is no new connection from client for [%ld] sec.", 60L);
			continue;
		}

		if(FD_ISSET(request_sock, &rmask)) {
			pthread_t tid;
			manager_server_t y;
			int error;
			char	peer_ip_buf[16];

			y.l = x->l;
			y.header_type = x->header_type;
			y.header_size = x->header_size;
			y.ack_mode = x->ack_mode;

			addrlen = sizeof(remote);

			fq_log(x->l, FQ_LOG_DEBUG, "accept start.");
#ifdef OS_HPUX
			y.new_sock = accept(request_sock, (struct sockaddr *)&remote, &addrlen);
#else
			y.new_sock = accept(request_sock, (struct sockaddr *)&remote, (socklen_t *)&addrlen);
#endif
			if( y.new_sock < 0 ) {
				fq_log(x->l, FQ_LOG_ERROR, "too many clients (sleep mode = 30 sec)!");
				continue;
			}
			else {
				fq_log(x->l, FQ_LOG_DEBUG, "accept OK.");
			}

			memset(peer_ip_buf, 0x00, sizeof(peer_ip_buf));

			sprintf(y.peer_ip, "%s", get_peer_ip(y.new_sock, peer_ip_buf));

			y.sync_fp = x->sync_fp;

			fq_log(x->l, FQ_LOG_DEBUG, "pthread_create() start. peer_ip=[%s]", peer_ip_buf);
			if( pthread_create(&tid, &attr, manager_thread, &y)  != 0) {
                fq_log(x->l,  FQ_LOG_ERROR,"pthread_create() error!");
                exit(0);
            }
			fq_log(x->l, FQ_LOG_DEBUG, "pthread_create() OK.");
		}
	}
}

char *get_peer_ip(int fd, char buff[])
{
    struct sockaddr_in name;
#ifdef OS_HPUX
	int len;
#else
	socklen_t	len;
#endif

    if (getpeername(fd, (struct sockaddr *)&name, &len) == -1) {
		return NULL;
    }

    strcpy(buff, inet_ntoa(name.sin_addr));
    return buff;
}
/**
 * dump a sockaddr_in in A.B.C.D:P in ASCII buffer
 *
 */
char *jk_dump_hinfo(struct sockaddr_in *saddr, char *buf)
{
    unsigned long laddr = (unsigned long)htonl(saddr->sin_addr.s_addr);
    unsigned short lport = (unsigned short)htons(saddr->sin_port);

    sprintf(buf, "%d.%d.%d.%d:%d",
            (int)(laddr >> 24), (int)((laddr >> 16) & 0xff),
            (int)((laddr >> 8) & 0xff), (int)(laddr & 0xff), (int)lport);

    return buf;
}

char *jk_dump_sinfo(int sd, char *buf)
{
    struct sockaddr_in rsaddr;
    struct sockaddr_in lsaddr;
#ifdef OS_HPUX
	int salen;
#else
    socklen_t          salen;
#endif

    salen = sizeof(struct sockaddr);
    if (getsockname(sd, (struct sockaddr *)&lsaddr, &salen) == 0) {
        salen = sizeof(struct sockaddr);
        if (getpeername(sd, (struct sockaddr *)&rsaddr, &salen) == 0) {
            unsigned long  laddr = (unsigned  long)htonl(lsaddr.sin_addr.s_addr);
            unsigned short lport = (unsigned short)htons(lsaddr.sin_port);
            unsigned long  raddr = (unsigned  long)htonl(rsaddr.sin_addr.s_addr);
            unsigned short rport = (unsigned short)htons(rsaddr.sin_port);
            sprintf(buf, "%d.%d.%d.%d:%d -> %d.%d.%d.%d:%d",
                    (int)(laddr >> 24), (int)((laddr >> 16) & 0xff),
                    (int)((laddr >> 8) & 0xff), (int)(laddr & 0xff), (int)lport,
                    (int)(raddr >> 24), (int)((raddr >> 16) & 0xff),
                    (int)((raddr >> 8) & 0xff), (int)(raddr & 0xff), (int)rport);
            return buf;
        }
    }
    sprintf(buf, "error=%d", errno);
    return buf;
}

#define JK_IS_SOCKET_ERROR(x) ((x) == -1)
#define JK_GET_SOCKET_ERRNO() ((void)0)
#define JK_SOCKET_EOF      (-2)

#ifndef SHUT_WR
#ifdef SD_SEND
#define SHUT_WR SD_SEND
#else
#define SHUT_WR 0x01
#endif
#endif

#ifndef SHUT_RD
#ifdef SD_RECEIVE
#define SHUT_RD SD_RECEIVE
#else
#define SHUT_RD 0x00
#endif
#endif


#if 0
int is_socket_connected(int sd)
{
    fd_set fd;
    struct timeval tv;
    int rc;

    errno = 0;
    FD_ZERO(&fd);
    FD_SET(sd, &fd);

    /* Initially test the socket without any blocking.
     */
    tv.tv_sec  = 0;
    tv.tv_usec = 0;

    do {
        rc = select((int)sd + 1, &fd, NULL, NULL, &tv);
        JK_GET_SOCKET_ERRNO();
        /* Wait one microsecond on next select, if EINTR */
        tv.tv_sec  = 0;
        tv.tv_usec = 1;
    } while (JK_IS_SOCKET_ERROR(rc) && errno == EINTR);

    errno = 0;
    if (rc == 0) {
        /* If we get a timeout, then we are still connected */
        return JK_TRUE;
    }
    else if (rc == 1) {
        int nr;

#if 0
#ifndef __INTERIX
(void) ioctl(fd, FIOCLEX, NULL)
#else
(void) fcntl(fd, F_SETFL, fcntl(fd, F_GETFD) | FD_CLOEXEC));
#endif
#endif
        rc = ioctl(sd, FIONREAD, (void*)&nr);

        if (rc == 0 && nr != 0) {
            return JK_TRUE;
        }
        JK_GET_SOCKET_ERRNO();
    }
	shutdown(sd, SHUT_WR);
    /* jk_shutdown_socket(sd, l); */

    return JK_FALSE;
}
#endif

char *get_socket_fd_info(int sd, char *buf, char *src_ip, char *src_port, char *dst_ip, char *dst_port)
{
    struct sockaddr_in rsaddr;
    struct sockaddr_in lsaddr;
#ifdef OS_HPUX
	int		salen;
#else
    socklen_t          salen;
#endif

    salen = sizeof(struct sockaddr);
    if (getsockname(sd, (struct sockaddr *)&lsaddr, &salen) == 0) {
        salen = sizeof(struct sockaddr);
        if (getpeername(sd, (struct sockaddr *)&rsaddr, &salen) == 0) {
            unsigned long  laddr = (unsigned  long)htonl(lsaddr.sin_addr.s_addr);
            unsigned short lport = (unsigned short)htons(lsaddr.sin_port);
            unsigned long  raddr = (unsigned  long)htonl(rsaddr.sin_addr.s_addr);
            unsigned short rport = (unsigned short)htons(rsaddr.sin_port);

            sprintf(src_ip, "%d.%d.%d.%d",  (int)(laddr >> 24),
                                            (int)((laddr >> 16) & 0xff),
                                            (int)((laddr >> 8) & 0xff),
                                            (int)(laddr & 0xff));
            sprintf(src_port, "%d", (int)lport);
            sprintf(dst_ip, "%d.%d.%d.%d",  (int)(raddr >> 24),
                                            (int)((raddr >> 16) & 0xff),
                                            (int)((raddr >> 8) & 0xff),
                                            (int)(raddr & 0xff));
            sprintf(dst_port, "%d", (int)rport);

            sprintf(buf, "%d.%d.%d.%d:%d -> %d.%d.%d.%d:%d",
                    (int)(laddr >> 24), (int)((laddr >> 16) & 0xff),
                    (int)((laddr >> 8) & 0xff), (int)(laddr & 0xff), (int)lport,
                    (int)(raddr >> 24), (int)((raddr >> 16) & 0xff),
                    (int)((raddr >> 8) & 0xff), (int)(raddr & 0xff), (int)rport);

            return buf;
        }
    }
    sprintf(buf, "error=%d", errno);
    return buf;
}

#if 0
int looping_read(int sd, char *dst, int until_len)
{
    int rd=0, rdlen=0;
    unsigned char   *buf=NULL;

    buf = calloc(until_len+1, sizeof(char));

    while(rdlen < until_len) {
        do {
            rd = read(sd, (char *)buf + rdlen, until_len - rdlen);
        } while (JK_IS_SOCKET_ERROR(rd) && errno == EINTR);

        if (JK_IS_SOCKET_ERROR(rd)) {
            int err = (errno > 0) ? -errno : errno;
            /* jk_shutdown_socket(sd, l); */
			shutdown(sd, 1);
			SAFE_FREE(buf);
            return (err == 0) ? JK_SOCKET_EOF : err;
        }
        else if (rd == 0) {
            /* jk_shutdown_socket(sd, l); */
			shutdown(sd,1);
			SAFE_FREE(buf);
            return JK_SOCKET_EOF;
        }
        rdlen += rd;
    }
    if( rdlen > 0 ) memcpy(dst, buf, rdlen);
	SAFE_FREE(buf);
    return(rdlen);
}
#endif

static int set_nonblock(int sock)
{
        int val;

        val = fcntl(sock, F_GETFL, 0);
        if( val == -1 ) return(-1);
        val |= O_NONBLOCK;
        val = fcntl(sock, F_SETFL, val);
        if( val == -1 ) return(-1);
		return(0);
}


static int set_block(int sock)
{
        int val;

        val = fcntl(sock, F_GETFL, 0);
        if( val == -1 ) return(-1);

        val &= ~O_NONBLOCK;
        val = fcntl(sock, F_SETFL, val);
        if( val == -1 ) return(-1);

		return(0);
}

int timed_nonblock_connect( fq_logger_t *l, int sock, struct sockaddr *sa, int nsa, int tv_sec )
{
        int r, ret = -1;
        struct timeval timeval;
        fd_set fds;

#if OS_HPUX
		int	optlen; 
#else
		/* socklen_t optlen; */ /* Option length */  
#endif

        int soerr, nsoerr;

        if( set_nonblock(sock) < 0 ) {
                goto L1;
        }

        if (connect(sock, sa, nsa) == 0) {
                goto L1; /* success */
        }

        if (errno != EINPROGRESS) {
                fq_log(l, FQ_LOG_ERROR,  "timed_nonblock_connect:reason='%s'.", strerror(errno));
                goto L0;
        }

        errno = 0;
        FD_ZERO(&fds);
        FD_SET(sock, &fds);
        timeval.tv_sec = tv_sec;
        timeval.tv_usec = 0;

        if ((r = select(sock + 1, NULL, &fds, NULL, &timeval)) == 0) {
                errno = ETIMEDOUT;
                fq_log(l, FQ_LOG_ERROR, "timed_nonblock_connect:select::reason='%s':timeout=%d",
                       strerror(errno), tv_sec);
                goto L0;
        }

        if (r < 0) {
				fq_log(l, FQ_LOG_ERROR, "select() failed.");
                goto L0;
        }

        nsoerr = sizeof soerr;

#ifdef OS_HPUX
		getsockopt(sock, SOL_SOCKET, SO_ERROR, &soerr, &optlen);
#else
        getsockopt(sock, SOL_SOCKET, SO_ERROR, &soerr, (socklen_t * __restrict__)&nsoerr);

        if (soerr != 0) {
                errno = soerr;
                goto L0;
        }
#endif

L1:
        ret = 0;
		
L0:
        set_block(sock);

        return ret;
}

#include <poll.h>

int is_valid_socket(fq_logger_t *l, int sd)
{
    struct pollfd fds;
    int rc;

    FQ_TRACE_ENTER(l);

    errno = 0;
    fds.fd = sd;
    fds.events = POLLIN;

    do {
        rc = poll(&fds, 1, 0);
    } while (rc < 0 && errno == EINTR);

	fq_log(l, FQ_LOG_DEBUG, "socket fd=[%d].", sd);
    if (rc == 0) {
        /* If we get a timeout, then we are still connected */
		fq_log(l, FQ_LOG_DEBUG, "valid socket.");
        FQ_TRACE_EXIT(l);
        return 1;
    }
    else if (rc == 1) {
		if( fds.revents == POLLIN) {
			char buf;

			fq_log(l, FQ_LOG_DEBUG, "check to be valid socket.");
			do {
				rc = (int)recvfrom(sd, &buf, 1, MSG_PEEK, NULL, NULL);
			} while (rc < 0 && errno == EINTR);
			if (rc == 1) {
				/* There is at least one byte to read. */
				FQ_TRACE_EXIT(l);
				return 1;
			}
		}
		else {
			switch(fds.revents) {
				case POLLPRI:
					fq_log(l, FQ_LOG_DEBUG, "POLLPRI");
					break;
				case POLLOUT:
					fq_log(l, FQ_LOG_DEBUG, "POLLOUT");
					break;
#if 0
				case POLLRDHUP:
					fq_log(l, FQ_LOG_DEBUG, "c3cc3: POLLRDHUP");
					break;
#endif
				case POLLERR:
					fq_log(l, FQ_LOG_DEBUG, "POLLERR");
					break;
				case POLLHUP:
					fq_log(l, FQ_LOG_DEBUG, "POLLHUP");
					break;
				case POLLNVAL:
					fq_log(l, FQ_LOG_DEBUG, "POLLNVAL");
					break;
				default:
					fq_log(l, FQ_LOG_DEBUG, "default..other event");
					break;
			}
		}
    }
	fq_log(l, FQ_LOG_DEBUG, "c3cc3: expired socket.");
    /* jk_shutdown_socket(sd, l); */
	shutdown(sd, SHUT_RD);
	shutdown(sd, SHUT_WR);
    FQ_TRACE_EXIT(l);
    return(-1);
}


/**
 * Check if data is available, if not, wait timeout seconds for data
 * to be present.
 * @param socket A socket
 * @param timeout How long to wait before timeout (value in seconds)
 * @return Return TRUE if the event occured, otherwise FALSE.
 */
int can_read(int socket, int timeout) {

  int r= 0;
  fd_set rset;
  struct timeval tv;
  
  FD_ZERO(&rset);
  FD_SET(socket, &rset);
  tv.tv_sec= timeout;
  tv.tv_usec= 0;
  
  do {
    r= select(socket+1, &rset, NULL, NULL, &tv);
  } while(r == -1 && errno == EINTR);

  return (r > 0);

}

/**
 * Check if data can be sent to the socket, if not, wait timeout
 * seconds for the socket to be ready.
 * @param socket A socket
 * @param timeout How long to wait before timeout (value in seconds)
 * @return Return TRUE if the event occured, otherwise FALSE.
 */
int can_write(int socket, int timeout) {

  int r= 0;
  fd_set wset;
  struct timeval tv;

  FD_ZERO(&wset);
  FD_SET(socket, &wset);
  tv.tv_sec= timeout;
  tv.tv_usec= 0;

  do {
    r= select(socket+1, NULL, &wset, NULL, &tv);
  } while(r == -1 && errno == EINTR);

  return (r > 0);
  
}

/**
 * Write <code>size</code> bytes from the <code>buffer</code> to the
 * <code>socket</code> 
 * @param socket the socket to write to
 * @param buffer The buffer to write
 * @param size Number of bytes to send
 * @return The number of bytes sent or -1 if an error occured.
 */
int sock_write(int socket, const void *buffer, int size) {

  ssize_t n= 0;
  
  if(size<=0)
      return 0;
  
  errno= 0;
  do {
    n= write(socket, buffer, size);
  } while(n == -1 && errno == EINTR);
  
  if(n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
    if(! can_write(socket, NET_TIMEOUT)) {
      return -1;
    }
    do {
      n= write(socket, buffer, size);
    } while(n == -1 && errno == EINTR);
  }
  
  return n;

}


/**
 * Read up to size bytes from the <code>socket</code> into the
 * <code>buffer</code>. If data is not available wait for
 * <code>timeout</code> seconds.
 * @param socket the Socket to read data from
 * @param buffer The buffer to write the data to
 * @param size Number of bytes to read from the socket
 * @param timeout Seconds to wait for data to be available
 * @return The number of bytes read or -1 if an error occured. 
*/
int sock_read(int socket, void *buffer, int size, int timeout) {
  
  ssize_t n;

  if(size<=0)
      return 0;
  
  errno= 0;
  do {
    n= read(socket, buffer, size);
  } while(n == -1 && errno == EINTR);
  
  if(n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
    if(! can_read(socket, timeout)) {
      return -1;
    }
    do {
      n= read(socket, buffer, size);
    } while(n == -1 && errno == EINTR);
  }

  return n;

}

/**
 * Check if the hostname resolves
 * @param hostname The host to check
 * @return TRUE if hostname resolves, otherwise FALSE
 */
int check_host(const char *hostname) {
  
  struct hostent *hp;

  /* ASSERT(hostname); */

  if((hp = gethostbyname(hostname)) == NULL) {
    return FQ_FALSE;
  }
  
  return FQ_TRUE;
}


/**
 * Verify that the socket is ready for i|o
 * @param socket A socket
 * @return TRUE if the socket is ready, otherwise FALSE.
 */
int check_socket(int socket) {

  return (can_read(socket, 0) || can_write(socket, 0));
  
}


/**
 * Verify that the udp socket is ready for i|o. The given socket must
 * be a connected udp socket if we should be able to test the udp
 * server. The test is conducted by sending one byte to the server and
 * check for a returned ICMP error when reading from the socket. A
 * better test would be to send an empty SYN udp package to avoid
 * possibly raising an error from the server we are testing but
 * assembling an udp by hand requires SOCKET_RAW and running the
 * program as root.
 * @param socket A socket
 * @return TRUE if the socket is ready, otherwise FALSE.
 */
int check_udp_socket(int socket) {

  int w, r;
  char buf[1]= {0};

  /*
   * R/W is asynchronous and we should probably loop and wait longer
   * for a possible ICMP error.
   */
  w = write(socket, buf, 1);
  CHECK(w >=0 );

  sleep(2);
  r= read(socket, buf, 1);
  if(0>r) {
    switch(errno) {
    case ECONNREFUSED: return FQ_FALSE;
    default:           break;
    }
  }
  
  return FQ_TRUE;

}

int view_sock_receive_buffer(int r_sock)
{
	char	buf[4096];
	int	n;

#ifdef MSG_DONTWAIT
	n = recv(r_sock, buf, sizeof(buf)-1, MSG_PEEK|MSG_DONTWAIT);
#else
	n = recv(r_sock, buf, sizeof(buf)-1, MSG_PEEK);
#endif
	buf[n]=0x00;
	return(n);
}

void socket_buffers_control(int sockfd, int buffer_size_bytes)
{
	int	rcvbuflen=buffer_size_bytes; 
	int sndbuflen=buffer_size_bytes;

	if (rcvbuflen > 0) {
		if (setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &rcvbuflen, sizeof(rcvbuflen)) < 0)
			fprintf(stderr, "SO_RCVBUF setsockopt error\n");
	}

	if (sndbuflen > 0) {
		if (setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &sndbuflen, sizeof(sndbuflen)) < 0)
			fprintf(stderr, "SO_SNDBUF setsockopt error\n");
	
	}
	return;
}

int _make_snd_data(char *f, int l, unsigned char *buf, int bufsize, int header_size, int *datalen, int seqno, int seq_size)
{
	char    fmt[10], szsequence[32];
	unsigned char   header[32];

	if( !buf || !bufsize || seqno<0 ) {
		fprintf(stderr, "ERR: [%s][%d]\n", __FILE__, __LINE__);
		return(-1);
	}

	sprintf(fmt, "%c0%dd", '%', seq_size);
	sprintf(szsequence, fmt, seqno);

	memset(header, 0x00, sizeof(header));
	set_bodysize(header, header_size, (bufsize-header_size), 1);

	memcpy(buf, header, header_size);
	memcpy(buf+header_size, szsequence, seq_size);
	memset(buf+header_size+seq_size, '-', bufsize-header_size-seq_size);
	buf[bufsize-1]='Z';
	buf[bufsize-2]='Z';

	*datalen = bufsize;

	return(0);
}

/*
 * Socket Libraries using socket object, fq_socket_t
 */
static int 
fill_snd_header( fq_logger_t *l, fq_socket_t *t, int datalen)
{
	FQ_TRACE_ENTER(l);

	if( t->header_length > 0 ) {
		memset(t->snd_header, 0x00, sizeof(t->snd_header));
		set_bodysize( t->snd_header, t->header_length, datalen, t->header_type);
	}

	FQ_TRACE_EXIT(l);
	return(0);
}

int 
on_send( fq_logger_t *l, fq_socket_t *t, unsigned char *data, int datalen)
{
    FQ_TRACE_ENTER(l);

	fill_snd_header(l, t, datalen);

	/* packet_dump(t->snd_header, 4, 8, "header contents"); */

	if( (t->snd_rtn=writen(t->sockfd, t->snd_header, t->header_length)) < 0 ) {
		fq_log(l, FQ_LOG_ERROR,  "header writen() error. n=[%d].", t->snd_rtn);
		goto L0;
	}
	/* printf("Sending header is OK. n=[%d]. will send [%d]bytes.\n", t->snd_rtn, datalen); */
	fq_log(l, FQ_LOG_DEBUG, "Sending header is OK. n=[%d]. will send [%d]bytes.", t->snd_rtn, datalen);

	if( (t->snd_rtn=writen(t->sockfd, data, datalen)) < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "data writen() error. n=[%d].", t->snd_rtn);
		goto L0;
	}
	fq_log(l, FQ_LOG_DEBUG, "Sending body is OK.  sent [%d]bytes.", t->snd_rtn);

L0:
    FQ_TRACE_EXIT(l);
	return(t->snd_rtn);
}

int 
on_recv( fq_logger_t *l, fq_socket_t *t )
{
    FQ_TRACE_ENTER(l);

	t->rcv_rtn=-1;

	if( (t->rcv_rtn=readn(t->sockfd, t->rcv_header, t->header_length)) < 0 ) {
		fq_log(l, FQ_LOG_ERROR,  "Receiving header was failed.");
		FQ_TRACE_EXIT(l);
		return(t->rcv_rtn);
	}
	fq_log(l, FQ_LOG_DEBUG, "Receiving header is OK.");

	t->rcv_rtn = get_bodysize(t->rcv_header, t->header_length, t->header_type);

	t->rcv_data = calloc(t->rcv_rtn+1, sizeof(char));
	memset(t->rcv_data, 0x00, t->rcv_rtn+1);

	if( (t->rcv_rtn=readn(t->sockfd, t->rcv_data, t->rcv_rtn)) < 0 ) {
		fq_log(l, FQ_LOG_ERROR,  "Receiving body was failed.");
		goto L0;
	}
	t->rcv_datalen = t->rcv_rtn;
	fq_log(l, FQ_LOG_DEBUG, "Received  body: [%d] '%s'.", t->rcv_rtn, t->rcv_data);
	
L0:
	FQ_TRACE_EXIT(l);

	return(t->rcv_rtn);
}

int 
on_release( fq_logger_t *l, fq_socket_t *t )
{
	FQ_TRACE_ENTER(l);

	SAFE_FREE(t->rcv_data);
	t->rcv_datalen = -1;
	SAFE_FREE(t->error_msg);

	FQ_TRACE_EXIT(l);
	return(0);
}

int prepare_socket_client(fq_logger_t *l, char *hostname, char *port)
{
    int sockfd = -1;  
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(hostname, port, &hints, &servinfo)) != 0) {
		fq_log(l, FQ_LOG_ERROR,"getaddrinfo: %s.", gai_strerror(rv));
		goto return_MINuS;
    }

    /* loop through all the results and connect to the first we can. */
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			fq_log(l, FQ_LOG_ERROR,"socket() error.\n");
            continue;
        }
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
			/* fq_log(l, FQ_LOG_ERROR, "connect() failed."); */
            continue;
        }
        break;
    }
    if (p == NULL) {
        fq_log(l, FQ_LOG_ERROR, "client: failed to connect to the '%s'.", hostname);
		goto return_MINuS;
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
    printf("client: connecting to %s\n", s);

    freeaddrinfo(servinfo); /* all done with this structure */
	
	return(sockfd); /* success */

return_MINuS:

	if(sockfd > 0) close(sockfd);

	return(-1);
}

int 
init_socket_obj( fq_socket_t *obj, 
					int sockfd, 
					header_type_t header_type, 
					int	header_length, fq_logger_t *l )
{
	FQ_TRACE_ENTER(l);

	/* This function can call on available socket. */
	if( check_socket(sockfd ) != FQ_TRUE ) {
		fq_log(l, FQ_LOG_ERROR, "check socket error."); 
		FQ_TRACE_EXIT(l);
		return(-1);
	}
	else 
		obj->sockfd = sockfd;

	/* socket_buffers_control(sockfd); */

	obj->header_flag = FLAG_TRUE;
	obj->header_type = header_type;
	obj->header_length = header_length;
	obj->error_msg = NULL;

	memset(obj->snd_header, 0x00, sizeof(obj->snd_header));
	obj->snd_rtn = -1;

	memset(obj->rcv_header, 0x00, sizeof(obj->rcv_header));
	obj->rcv_data = NULL;
	obj->rcv_datalen = -1;
	obj->rcv_rtn = -1;

	obj->on_send = on_send;
	obj->on_recv = on_recv;
	obj->on_release = on_release;

	FQ_TRACE_EXIT(l);
	
	return(0);
}

int 
new_connector(fq_logger_t *l, const char *sz_ip, const char *sz_port, int *sockfd)
{
	int	rc=-1;
	struct sockaddr_in      saddr;
	uint16_t	port = (uint16_t) atoi(sz_port);

	FQ_TRACE_ENTER(l);

	if ( (*sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "socket() error, reason=%s.", strerror(errno));
		goto L0;
	}

	bzero((void*)&saddr, sizeof(saddr));
	/* memset(&saddr, 0, sizeof(saddr)); */

	saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = inet_addr(sz_ip);
    saddr.sin_port = htons((uint16_t)port);

	if (connect(*sockfd, (struct sockaddr *)&saddr, sizeof(saddr)) == -1) {
		fq_log(l, FQ_LOG_ERROR, "connect() error, reason=%s.", strerror(errno));
		goto L0;
	}

	rc = *sockfd;
	fq_log(l, FQ_LOG_DEBUG, "connecting OK. rc(sockfd)=[%d]", rc);
	FQ_TRACE_EXIT(l);
	return(rc);
L0:
	if(rc < 0) {
		close(*sockfd);
		*sockfd = -1;
	}

	FQ_TRACE_EXIT(l);
	return(rc);
}

struct connector {
    pthread_cond_t cond;
    int sock;
    struct sockaddr *sin;
    int nsin;
    int result;
};

static void* tryconnect(void *arg)
{
	struct connector *c = (struct connector *)arg;

	if (connect(c->sock, c->sin, c->nsin) < 0) {
		c->result = errno;
	} else
		c->result = 0;
	pthread_cond_signal(&c->cond);

	pthread_exit((void*)0);
}

int timed_connect( fq_logger_t *l, int sock, struct sockaddr *sin, int nsin, int timeout)
{
	pthread_cond_t cond_initializer = PTHREAD_COND_INITIALIZER;
	struct connector c;
	pthread_t threadID;
	pthread_attr_t attr;
	pthread_mutex_t mutex;
	struct timespec timespec;
	int error;

	FQ_TRACE_ENTER(l);

	pthread_mutex_init(&mutex, NULL);

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	c.cond = cond_initializer;
	c.sock = sock;
	c.sin = sin;
	c.nsin = nsin;

	if ( (errno = pthread_create(&threadID, &attr, tryconnect, (void*)&c)) != 0 ) {
		fq_log(l, FQ_LOG_ERROR, "timed_connect: pthread_create: %s.", strerror(errno));
		FQ_TRACE_EXIT(l);
		return(-1);
	}
	if ( (error = pthread_mutex_lock(&mutex)) != 0 ) {
		fq_log(l, FQ_LOG_ERROR, "timed_connect: pthread_mutex_lock: %s.",strerror(error));
		pthread_cancel(threadID);
		errno = error;
		FQ_TRACE_EXIT(l);
		return(-1);
	}
	timespec.tv_sec = time(0) + timeout;
	timespec.tv_nsec = 0;
	if ( (error = pthread_cond_timedwait(&c.cond, &mutex, &timespec)) != 0) {
		fq_log(l, FQ_LOG_ERROR, "timed_connect: pthread_cond_timedwait: %s", strerror(error));
		pthread_cancel(threadID);
		errno = error;
		FQ_TRACE_EXIT(l);
		return(-1);
	}

	if (c.result == 0) {
		FQ_TRACE_EXIT(l);
		return(0);
	}
	else {
		FQ_TRACE_EXIT(l);
		errno = c.result;
		return(-1);
	}
}

/*
** getip.c
*/
#include <stdio.h>
#include <stdbool.h>
#include <netdb.h>

bool get_ip_from_hostname(fq_logger_t *l, char *hostname, char *dst)
{
	char srvr_addr[33];

	struct hostent *phostent;

	if(!HASVALUE(hostname) ) {
		fq_log(l, FQ_LOG_ERROR, "illegal fuction call. There is no hostname.(NULL)");
		return false;
	}

	phostent = gethostbyname(hostname);
	if(phostent == 0) {
		fq_log(l, FQ_LOG_ERROR, "gethostbyname() error.");
		return false;
	}

	inet_ntop(AF_INET, (void *)*phostent->h_addr_list, srvr_addr, sizeof(srvr_addr));
	sprintf(dst, "%s", srvr_addr);

	return true;
}

bool get_ip_from_ethernet_name( fq_logger_t *l, char *eth_name)
{
	int fd;
	struct ifreq ifr;
	char IPbuffer[32+1];

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if(fd<0) {
		fq_log(l, FQ_LOG_ERROR, "socket() error.");
		return false;
	}

	ifr.ifr_addr.sa_family = AF_INET;
	snprintf(ifr.ifr_name, IFNAMSIZ, "%s", (const char *)eth_name);
	ioctl(fd, SIOCGIFADDR, &ifr);

	sprintf(IPbuffer, "%s", inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));
	close(fd);
    return true;
}
