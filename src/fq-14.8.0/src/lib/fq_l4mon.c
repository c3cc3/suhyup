/*
** Program name: fq_l4mon.c
**
** author: Gwisang.Choi
** e-mail: gwisang.choi@gmail.com
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/mman.h>
#include <sys/file.h>

#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <strings.h>
#include <pthread.h>

#include <stdlib.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdarg.h>

#include "fqueue.h"
#include "fq_l4mon.h"
#include "fq_common.h"
#include "fq_rlock.h"

#define ALERT_LEVEL "1"

void *open_db( char *path, char *fname, int *fd, int *lockfd )
{
	char 	filename[128], lock_filename[128];
	off_t	file_size=0;
	void	*p;
	struct stat statbuf;

	sprintf(filename, "%s/%s", path, fname);
	sprintf(lock_filename, "dblock_%s", fname);

	if( is_file( lock_filename ) == 0 ) {
		if ( create_lock_table(path, lock_filename, 1) < 0 ) {
			fprintf(stderr, "create_lock_table() error. \n");
			return(NULL);
		}
	}

	if( (*lockfd = open_lock_table(lock_filename) ) < 0 ) {
		fprintf(stderr, "open_lock_table() error.\n");
		return(NULL);
	}

	if( (*fd=open(filename, O_RDWR|O_SYNC, 0666)) < 0 ) {
		fprintf(stderr, "file[%s] can not open.\n", filename);
		r_unlock_table( *lockfd, 0 );
		return(NULL);
	}

	if(( fstat(*fd, &statbuf)) < 0 ) {
		fprintf(stderr, "fstat() error. can't get file size.\n");
		r_unlock_table( *lockfd, 0 );
		close(*fd);
		return(NULL);
    }

    file_size = statbuf.st_size;

	if( ( p = (void *)fq_mmapping(NULL, *fd, file_size, (off_t)0)) == NULL ) {
		fprintf(stderr, "fq_mmapping() error.");
		r_unlock_table( *lockfd, 0 );
		close(*fd);
		return(NULL);
	}
	// fprintf(stdout, "open_db() OK.\n");
	return(p);
}
void close_db(void *p, off_t mmap_len, int fd, int lockfd )
{
	munmap(p, mmap_len);
	
	if(fd > 0 ) close(fd);
	if(lockfd > 0 ) close(lockfd);
}

int get_node_no()
{
	char	hostname[64];
	int		i;
	char	*host_list[] = { "aa-pipel-cn01", "aa-pipel-cn02", "aa-pipel-cn03", "aa-pipel-cn04", "aa-pipel-cn05", "aa-pipel-cn06", 0x00 };

	gethostname(hostname, sizeof(hostname));

	for(i=0; host_list[i]; i++) {
		if( strcmp(hostname, host_list[i]) == 0 )  return(i);
	}
	printf("get_node_no() error.\n");
	return(-1);
}

int get_domain_no(int node_no)
{
	if( node_no >= 0 && node_no <= 2) return(0);
	else if( node_no >= 3 && node_no <= 5) return(1);
	else return(-1);
}

static int  Connect(int sockfd, const struct sockaddr* servaddr, socklen_t addrlen)
{
	 int result = 0;
	 result = connect(sockfd, servaddr, addrlen);

	 if (result <0)
	 {
		printf("Connecting Error\n");
		return(-1);
	 }
	 else
	 {
		printf("Success Connecting\n");
		return(0);
	 }
} 

static int Socket(int family, int type, int protocol)
{
	 int result = 0;
	 result = socket(family, type, protocol);
	 if (result == -1)
	 {
	  /*  printf("Socket Contructing Error\n");*/
	  exit(1);
	 }
	 return result;
}

/* ES^201312111212^WSMS^^WSMS^^TESTMSG^aa-pipel-cn01 Ping Error ^^ */
int SendToVAS(char *VAS_MessageID, char *error_msg)
{
	struct	sockaddr_in connect_addr;
	int		connect_fd;
	int		msgsize;
	int		count=0;
	int		forCount=0;	
	int		IsSeverity = 0;
	char	msg[1024];
	char   *pchBlankSeparate = "^";
	char   *pchBlank1 = " ";
	char   *pchBlankUnder = "_";
	char   *pchBlankNA = "NA";
	char	date[9];
	char	time[7];
	char	hostname[64];
	int		rc=-1;
	int		n;

	memset(msg, '\0', sizeof(msg));
	
	strcat( msg, "ES");
	strcat( msg, pchBlankSeparate);
	
	get_time(date, time);
	strcat(msg, date);
	strcat(msg, time);
	strcat( msg,pchBlankSeparate);  /*  ^  */

	strcat( msg, "PIP");		/* 4 senderID */
	strcat( msg,pchBlankSeparate);  /*  ^  */

	strcat( msg,pchBlankSeparate);  /*  ^  */

	strcat( msg, "PIP");
	strcat( msg,pchBlankSeparate);  /*  ^  */
	strcat( msg,pchBlankSeparate);  /*  ^  */

	strcat( msg, VAS_MessageID); /* MessageID */
	strcat( msg,pchBlankSeparate);  /*  ^  */
	

	gethostname(hostname, sizeof(hostname));
	strcat( msg, hostname);
	strcat( msg, ":");

	strcat( msg, error_msg);
	strcat( msg,pchBlankSeparate);  /*  ^  */

	strcat( msg, ALERT_LEVEL); /* Level */
	strcat( msg,pchBlankSeparate);  /*  ^  */

	msgsize = strlen(msg);

	/*TCP ¼ÒÄÏ »ý¼º*/
	connect_fd = Socket(AF_INET, SOCK_STREAM, 0);

	connect_addr.sin_family = AF_INET;	

	connect_addr.sin_addr.s_addr = inet_addr(VAS_IP);/* count 1 addr*/
	connect_addr.sin_port = htons(VAS_PORT);
	memset(&(connect_addr.sin_zero), 0, 8);

	if( (rc=Connect(connect_fd, (struct sockaddr*)&connect_addr, sizeof(connect_addr)) ) < 0 ) {
		goto L0;
	}
	mon_eventlog("Connected to VAS.");

	n = write(connect_fd, msg, msgsize);
	if( n != msgsize ) {
		mon_eventlog("%s", "VAS write error n=[%d]\n", n);
		goto L0;
	}

	mon_eventlog("Message Sent>>%s", msg);

	memset(msg, '\0', sizeof(msg));

	msgsize = read(connect_fd, msg, sizeof(msg));
	if( msgsize <= 0 ) {
		mon_eventlog( "read error n=[%d]", n);
		goto L0;
	}

	mon_eventlog("Message Received<<%s", msg);

#if 0
	if( !strcmp(msg,"OK"))
		printf("*** Success ***\n");
	else	
		printf("*** Error   ***\n");
#endif

	close(connect_fd);
	return(0);

L0:
	close(connect_fd);
	return(-1);
}

pthread_mutex_t Log_Lock = PTHREAD_MUTEX_INITIALIZER;

void mon_eventlog(char *fmt, ...)
{
	char filename[128];
	char datestr[40], timestr[40];
	va_list ap;
	FILE *fp;
	char buf[1024];

	pthread_mutex_lock(&Log_Lock);

	get_time(datestr, timestr);

    sprintf(filename, "./../logs/VAS_event_%s.log", datestr);

    if ( (fp = fopen(filename, "a")) == NULL ) {
        fprintf(stderr, "Unable to open event log file %s, %s--->fatal.\n", filename, strerror(errno));
		pthread_mutex_unlock(&Log_Lock);
		return;
    }

	va_start(ap, fmt);
	vsprintf(buf, fmt, ap);
	fprintf(fp, "%s:%s %s\n", datestr, timestr, buf);

	fflush(fp);
    fclose(fp);
	va_end(ap);

	pthread_mutex_unlock(&Log_Lock);
}

