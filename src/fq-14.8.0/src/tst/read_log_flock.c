/* How to read a log file safely */
/* If the process stops and runs, it reads contents from last offset */

/*
** Check list
** 1. memory leak -> passed
** 2. LARGEFILE = 3.2 Gbytes -> passed
** 3. Restart -> passed
** 4. config file test -> passed
** 5. Network Connecting -> passed
** 6. Message verify -> passed
** 7. ip failover with TCP_sync_server(~/tst) -> passed
** 8. When msg.dat is changed, re-fopen -> yet
*/

#include "config.h"
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include <time.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "fq_defs.h"
#include "fq_logger.h"
#include "fq_types.h"
#include "fq_common.h"
#include "fq_flock.h"
#include "fq_config.h"
#include "fq_param.h"
#include "fq_socket.h"
#include "fq_delimiter_list.h"

#define MAX_MSG_LEN 65535
#define FQ_HEADER_SIZE 4

extern int errno;
char g_Progname[256];

static char *make_send_msg(fq_logger_t *l, char *msg, int msglen, int *new_msglen);

static off_t get_last_offset( char *offset_filename )
{
	off_t	offset;
	char	buf[128+1];
	FILE	*offset_fp = NULL;
	char	*p;

	offset_fp = fopen(offset_filename, "r");
	CHECK(offset_fp != NULL);

	p = fgets(buf, 128, offset_fp);
	str_lrtrim(buf);

	offset = atol(buf);

	fclose(offset_fp);
	return(offset);
}

#if 0
static size_t get_filesize(char *data_filename)
{
	int fd;
    struct stat statbuf;
    off_t len=0;

	fd = open(data_filename, O_RDONLY);
	CHECK(fd >= 0 );

    if(( fstat(fd, &statbuf)) < 0 ) {
		perror("fstat");
    }
	close(fd);

    return(statbuf.st_size);
}
#endif

void print_help(char *p)
{
	printf("\n\n Usage: $ %s [-h] [-f config filename] <enter>\n", p);
	return;
}

int main(int ac, char **av)
{
	FILE *data_fp=NULL;
	FILE *offset_fp=NULL;
	int rc;
	flock_obj_t *lock_obj=NULL;
	char *data_filename=NULL;
	char *offset_filename = NULL;
	char *logname = NULL;
	char *loglevel=NULL;
	int i_loglevel=4;
	char *config_filename=NULL;
	char linebuf[MAX_MSG_LEN+1];
	off_t	offset = 0;
	off_t	last_offset = 0;
	long	processing_count = 0L;
	fq_logger_t *l=NULL;
	char *ip_list=NULL;
	char *port=NULL;
	char *ack_mode=NULL;
	char *ack_char=NULL;
	int ch;
	config_t *t=NULL;
	char	buffer[128];
	int sock;
	struct sockaddr_in serv_addr;
	unsigned char header[FQ_HEADER_SIZE+1];

	delimiter_list_obj_t *list_obj=NULL;
	delimiter_list_t	*this_entry = NULL;
	int connect_flag = 0;
	int t_no;
	
	getProgName(av[0], g_Progname);

	if(ac < 3 ) {
		print_help(g_Progname);
		return(0);
	}

	while(( ch = getopt(ac, av, "Hh:f:")) != -1) {
		switch(ch) {
			case 'H':
			case 'h':
				print_help(av[0]);
				return(0);
			case 'f':
				config_filename = strdup(optarg);
				break;
			default:
				print_help(av[0]);
				return(0);
		}
	}

	if(!HASVALUE(config_filename) ) { 
		print_help(g_Progname);
		return(0);
	}


	/* Loading server environment */

	t = new_config(config_filename);

	if( load_param(t, config_filename) < 0 ) {
		fprintf(stderr, "load_param() error.\n");
		return(EXIT_FAILURE);
	}

	get_config(t, "DATA_FILENAME", buffer);
	data_filename = strdup(buffer);
	get_config(t, "OFFSET_FILENAME", buffer);
	offset_filename = strdup(buffer);
	get_config(t, "LOGI_SERVER_IP", buffer);
	ip_list = strdup(buffer);
	get_config(t, "LOGI_SERVER_PORT", buffer);
	port = strdup(buffer);
	get_config(t, "ACK_MODE", buffer);
	ack_mode = strdup(buffer);
	get_config(t, "ACK_CHAR",  buffer);
	ack_char = strdup(buffer);
	get_config(t, "LOG_NAME", buffer);
	logname = strdup(buffer);
	get_config(t, "LOG_LEVEL", buffer);
	loglevel = strdup(buffer);


	/* Checking mandatory parameters */
	if( !logname || !loglevel) {
		printf("There are no LOG_NAME/LOG_LEVEL at config file.\n");
		return(0);
	}

	i_loglevel = get_log_level(loglevel);
	rc = fq_open_file_logger(&l, logname, i_loglevel);
	CHECK( rc > 0 );

	rc = open_delimiter_list(l, &list_obj, ip_list, '|');
	if( rc != TRUE ) {
		fq_log(l, FQ_LOG_ERROR, "Fatal: IP_LIST error in config.");
		goto stop;
	}
	
reconnect:
	connect_flag = 0;
	this_entry = list_obj->head;
	
	for( t_no=0;  (this_entry->next && this_entry->value); t_no++ ) {
		fq_log(l, FQ_LOG_INFO, "ip=[%s]\n", this_entry->value);

		sock = socket(PF_INET, SOCK_STREAM, 0);
		if( sock == -1 ) {
			fq_log(l, FQ_LOG_ERROR, "Fatal: socket(PF_INET) error. reason=[%s].",
				strerror(errno));
			goto stop;
		}

		memset(&serv_addr, 0, sizeof(serv_addr));
		serv_addr.sin_family=AF_INET;
		serv_addr.sin_addr.s_addr = inet_addr(this_entry->value);
		serv_addr.sin_port = htons(atoi(port));

		if( connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) {
			fq_log(l, FQ_LOG_ERROR, "connect(ip=[%s],port=[%s] error. reason=[%s].",
					this_entry->value, port, strerror(errno));
			fq_log(l, FQ_LOG_ERROR, "Please check status of LOGI_SERVER. We'll try reconnect after 1 second.");
			close(sock);

			sleep(1);
			this_entry = this_entry->next;
			continue;
		}

		fq_log(l, FQ_LOG_DEBUG, "Connected to LOGI_SERVER(%s) successfully.",
				this_entry->value);
		connect_flag = 1;
		break;
    }

	if( connect_flag != 1 ) {
		sleep(1);
		goto reconnect; 
	}

	rc = open_flock_obj(l, "/tmp", g_Progname, DE_FLOCK, &lock_obj);
	CHECK(rc==TRUE);

	fq_log(l, FQ_LOG_EMERG, "Server started.(compiled:[%s][%s]", __DATE__, __TIME__);

	while(1) { 
		fq_log(l, FQ_LOG_DEBUG, "locking start.");
		lock_obj->on_flock(lock_obj);
		fq_log(l, FQ_LOG_DEBUG, "locking end.");

		if( is_file( data_filename) == 0) {
			fq_log(l, FQ_LOG_INFO, "There is no data_file[%s].", data_filename);
			lock_obj->on_funlock(lock_obj);
			sleep(1);
			continue;
		}
		fq_log(l, FQ_LOG_DEBUG, "found datafile.");

		if( (data_fp = fopen(data_filename, "r")) == NULL ) {
			fq_log(l, FQ_LOG_ERROR, "Fatal: Can't open a file[%s].reason=[%s]", 
				data_filename, strerror(errno));
			lock_obj->on_funlock(lock_obj);
			break;
		}
		fq_log(l, FQ_LOG_DEBUG, "open datafile.");

		if( can_access_file(NULL, offset_filename) == TRUE ) {
			size_t filesize;
			last_offset = get_last_offset( offset_filename );
			filesize = get_filesize( data_filename );
		
			if( last_offset == filesize ) {
				fq_log(l, FQ_LOG_INFO, "There is no changing at the datafile.");
				if(data_fp) fclose(data_fp);
				lock_obj->on_funlock(lock_obj);
				sleep(5);
				continue;
			}
			else {
				fq_log(l, FQ_LOG_DEBUG, "last_offset[%ld] is diffent to filesize[%zu].", last_offset, filesize);
			}

			offset_fp = fopen(offset_filename, "w");
			if( offset_fp == NULL ) {
				fq_log(l, FQ_LOG_ERROR, "Fatal: Can't open a file[%s].reason=[%s]", 
					data_filename, strerror(errno));
				lock_obj->on_funlock(lock_obj);
				break;
			}

			if( last_offset > 0 ) {
				/* move file pointer */
				if( fseek(data_fp, last_offset, SEEK_SET) < 0 ) {
					fq_log(l, FQ_LOG_ERROR, "Fatal: Can't fseek( offset=[%ld] ) error!. reason=[%s]", last_offset, strerror(errno));
					offset = 0;
					lock_obj->on_funlock(lock_obj);
					break;
				}
				fq_log(l, FQ_LOG_DEBUG, "last offset =[%ld].", last_offset);
			} 
		} else {
			offset_fp = fopen(offset_filename, "w");
			if( offset_fp ==NULL) {
				fq_log(l, FQ_LOG_ERROR, "Fatal: Can't open a file[%s].reason=[%s]", 
					data_filename, strerror(errno));
				lock_obj->on_funlock(lock_obj);
				return(0);
			}
			offset = 0;
			fq_log(l, FQ_LOG_DEBUG, "There is no last offset. offset = [%ld] \n", offset);
		}

		while(fgets(linebuf, MAX_MSG_LEN, data_fp) ) {
			int msglen;
			char *send_msg = NULL;
			int new_msglen = 0;
			int n;
			
			/* remove newline */
			msglen = strlen(linebuf);
			linebuf[msglen-1] = 0x00; /* remove newline charactor */
			/*
			** ToDo your job in here.
			** Send a msg to logi_server with TCP client socket.
			*/
			fq_log(l, FQ_LOG_DEBUG, "linebuf=[%s] len=[%d]\n", linebuf, msglen);

			send_msg = make_send_msg(l, linebuf, msglen, &new_msglen);

			set_bodysize(header, FQ_HEADER_SIZE, new_msglen, 0);
			n = writen(sock, header, FQ_HEADER_SIZE);
			if( n != FQ_HEADER_SIZE ) {
				fq_log(l, FQ_LOG_ERROR, "Fatal: socket header writen() error. n=[%d]. Check LogCollector Process.", n);
				SAFE_FREE(send_msg);
				close(sock);
				goto reconnect;
			}
			fq_log(l, FQ_LOG_DEBUG, "header sending OK.");

			n = writen(sock, send_msg, new_msglen);
			if( n != new_msglen ) {
				fq_log(l, FQ_LOG_ERROR, "Fatal: socket body writen() error. n=[%d]. Check LogCollector Process.", n);
				SAFE_FREE(send_msg);
				close(sock);
				goto reconnect;
			}
			SAFE_FREE(send_msg);
			fq_log(l, FQ_LOG_DEBUG, "body sending OK.");

			char ack_buf[2];

			if( *ack_mode == *ack_char ) {
				/* receive ACK of only a charactor */
				memset(ack_buf, 0x00, sizeof(ack_buf));
				n = readn(sock, ack_buf, 1);
				if( n < 0 ) {
					fq_log(l, FQ_LOG_ERROR, "Fatal: readn() error. n=[%d]. Check LogCollector Process.", n);
					close(sock);
					goto reconnect;
				}

				if( ack_buf[0] != 'O') {
					fq_log(l, FQ_LOG_ERROR, "Fatal: Check LogCollector, ACK error. is not O.\n");
					close(sock);
					goto reconnect;
				}
			}

			/* update current offset */
			fseek(offset_fp, 0L, SEEK_SET); /* go to first of file */
			offset = ftell(data_fp);
			fprintf(offset_fp, "%ld", offset);  
			fflush(offset_fp);

			processing_count++;
		}
		fq_log(l, FQ_LOG_INFO, "Finished! processing_count = [%ld]\n", processing_count);

		fclose(data_fp);
		fclose(offset_fp);

		lock_obj->on_funlock(lock_obj);

		sleep(1);
	}

stop:
	fq_log(l, FQ_LOG_ERROR, "We'll stop processing. Good bye." );

	if(lock_obj) close_flock_obj(NULL, &lock_obj);

	if(list_obj) close_delimiter_list( l, &list_obj);

	if(t) free_config(&t);
	fq_close_file_logger(&l);
}

#define HEADER_SIZE 65
#define MAX_AGENT_HOSTNAME_LEN 33

static char *make_send_msg(fq_logger_t *l, char *msg, int msglen, int *new_msglen)
{

	FQ_TRACE_ENTER(l);

	char *dst = calloc( HEADER_SIZE+msglen+1, sizeof(char));
	char header[HEADER_SIZE+1];
	char hostname[MAX_AGENT_HOSTNAME_LEN+1];
	char date[9], time[7], date_time[16];
	
	get_time(date, time);
	gethostname(hostname, sizeof(hostname));
	sprintf(date_time, "%s:%s", date,time);
	sprintf(header, "%-32.32s%-33.33s", date_time, hostname);
	memcpy(dst, header, HEADER_SIZE);
	memcpy(dst+HEADER_SIZE, msg, msglen);

	*new_msglen = strlen(dst);

	FQ_TRACE_EXIT(l);
	return(dst);
}
