/*
** fq_socket_no_header.c
*/
#define FQ_SOCKET_NO_HEADER_C_VERSION "1.0.0"

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

#include <math.h>

#include "fq_socket.h"
#include "fq_logger.h"

static void *manager_thread_no_header( void *arg )
{
	int new_sock;
	manager_server_t *x=NULL;
	x = (manager_server_t *)arg;
	struct timeval timeout;

	new_sock = x->new_sock;
	fprintf(stdout, "TCP managing Server thread(no_header) started.\n");
	fq_log(x->l, FQ_LOG_DEBUG, "TCP managing Server thread(no_header) started.");

	while(1) {
		int nbytes=0;
		fd_set mask;
		int	cnt=0;
		int	data_len=0;
		int	resp_len=0;
		unsigned char   body[SOCK_MAXDATALEN+1];
		unsigned char   resp[SOCK_MAXDATALEN+1];
		int		rc=-1;

		timeout.tv_sec = (long)30;
		timeout.tv_usec = 0;
		
		fprintf(stdout, "Wait...\n");

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
			memset(body, 0x00, sizeof(body));
			memset(resp, 0x00, sizeof(resp));
			nbytes = 0;

			if( (nbytes=recv( new_sock, body, sizeof(body), 0)) < 0) {
				fq_log(x->l,FQ_LOG_ERROR,  "socket header read error!! nbytes=[%d]. header was [%s].\n", 
					nbytes, data_len);
				break;
			}
			fq_log(x->l, FQ_LOG_DEBUG, "body receiving OK len=[%d]\n", nbytes);

			if( nbytes == 0 ) {
				fq_log(x->l, FQ_LOG_DEBUG, "I reveived zero return. I suppose that client exit.\n");
				break;
			}

			/* callback */
			if( (rc = x->sync_fp(body, nbytes, resp, &resp_len)) < 0) {
				fq_log( x->l, FQ_LOG_ERROR, "callback user function error. check to regist func. rc=[%d]\n", rc);
				break;
			}

			if( rc == 99 ) { /* logout */
				fq_log( x->l, FQ_LOG_DEBUG, "Callback returned -99(exit signal)."); 
				break;
			}

			if( resp_len > 0 ) {
				if( (nbytes=writen( new_sock, resp, resp_len)) != resp_len ) {
					fq_log(x->l, FQ_LOG_ERROR, "socket write error!! nbytes=[%d].\n", nbytes);
					break;
				}
				fq_log(x->l, FQ_LOG_DEBUG, "Response to client:[%s] len=[%d] thread will be exit.\n", resp, resp_len);
			}
			/* break; */
		}
	} /* while end */

	shutdown(new_sock, SHUT_RDWR);
	SAFE_FD_CLOSE(new_sock);
	fq_log(x->l, FQ_LOG_DEBUG, "Thread exit.\n");
    pthread_exit((void *)0);
	return((void *)0);
}

static void *TCP_callback_sync_server_no_header( void *argument)
{
	TCP_server_t *x=NULL;
	fd_set  rmask, mask;
	int request_sock, addrlen;
	static struct timeval timeout = {30, 0};
	struct sockaddr_in remote;
	pthread_attr_t attr;

	/* x = (struct data_to_pass_via_pthread_create *)argument; */
	x = (TCP_server_t *)argument;
	
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

		timeout.tv_sec = 30;
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
			fq_log(x->l, FQ_LOG_ERROR, "New connection is not during [%d] sec.", timeout.tv_sec );
			continue;
		}

		if(FD_ISSET(request_sock, &rmask)) {
			pthread_t tid;
			manager_server_t y;
			int error;

			y.l = x->l;
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

			y.sync_fp = x->sync_fp;

			fq_log(x->l, FQ_LOG_DEBUG, "pthread_create() start.");
			error = pthread_create(&tid, &attr, manager_thread_no_header, &y);
			if( error != 0 ) {
                fq_log(x->l,  FQ_LOG_ERROR,"pthread_create() error!");
                exit(0);
            }
			fq_log(x->l, FQ_LOG_DEBUG, "pthread_create() OK.");
		}
	}
}

int make_thread_sync_CB_server_no_header( fq_logger_t *l, TCP_server_t *x, char *ip, int port, int (*sync_fp)(unsigned char *, int, unsigned char *, int *))
{
	pthread_t tid;
	int	error;
	pthread_attr_t attr;

	x->l = l;
	x->ip = strdup(ip);
	x->port = port;
	x->sync_fp = sync_fp;

	pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	error = pthread_create(&tid, &attr, TCP_callback_sync_server_no_header, x);
	if( error != 0 ) {
        fq_log(l, FQ_LOG_ERROR, "pthread_create() error!\n");
        exit(0);
    }
	return(0);
}
