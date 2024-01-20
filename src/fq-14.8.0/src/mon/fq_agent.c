/*
** fq_agent.c
** This program is a monitoring agent program. Install it to the server for monitering.
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
#include "fq_info_param.h"

#define FQ_AGENT_C_VERSION "1.0"

typedef struct _QU_thread_params {
	fq_logger_t *l;
	char		*qpath;
	int			qu_loop_cycle_sec;
	int			su_loop_cycle_sec;
	int			ps_loop_cycle_sec;
	mem_queue_obj_t *memq_obj;
} thread_params_t;

void *QU_thread(void *arg); /* Queue usage */
void *SU_thread(void *arg); /* System usage */
void *PS_thread(void *arg); /* Process status */
void *PS_each_thread(void *arg);
void *Consumer( void *arg );

static int read_fields_CPU(FILE *fp, unsigned long long int *fields);
static int get_meminfo(FILE *fp, char *dst);
static int get_diskinfo( char *fnPath, char *dst);

char svr_id[36];
char g_Progname[128];
char dst_server_ip[16];
char log_directory[256];
char log_filename[128];
char log_full_name[1024];
char log_level[10];
char qpath[256];
char    QU_qformat_file[256];
char    SU_qformat_file[256];
char    PS_qformat_file[256];
char    PS_each_qformat_file[256];
char	PS_each_type[4]; /* YES or NO */
int  qu_loop_cycle_sec;
int  su_loop_cycle_sec;
int  ps_loop_cycle_sec;
int	 i_loglevel;
int  dst_server_port;
int	cpu_info_index;
int mem_info_index;
int check_file_system_index;
char	check_file_system_name[128];
fq_logger_t *l=NULL;
conf_obj_t *conf_obj=NULL; /* configuration object */


static void print_help(char *p);

int main(int ac, char **av)
{
	int ch;
	char *conf_file=NULL;
	int	rc;

	void *rtn=NULL;
	pthread_t QU_thread_id;
	pthread_t SU_thread_id;
	pthread_t PS_thread_id;
	pthread_t	Consumer_id;
	thread_params_t	QU_thread_params;

	/* memory queue */
	mem_queue_obj_t *memq_obj;


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
	conf_obj->on_get_str(conf_obj, "DST_SVR_IP", dst_server_ip);
	conf_obj->on_get_str(conf_obj, "LOG_DIRECTORY", log_directory);
	conf_obj->on_get_str(conf_obj, "LOG_FILE_NAME", log_filename);
	conf_obj->on_get_str(conf_obj, "LOG_LEVEL", log_level);
    dst_server_port = conf_obj->on_get_int(conf_obj, "DST_SVR_PORT");
	conf_obj->on_get_str(conf_obj, "QPATH", qpath);

	qu_loop_cycle_sec = conf_obj->on_get_int(conf_obj, "QU_LOOP_CYCLE_SEC");
	su_loop_cycle_sec = conf_obj->on_get_int(conf_obj, "SU_LOOP_CYCLE_SEC");
	ps_loop_cycle_sec = conf_obj->on_get_int(conf_obj, "PS_LOOP_CYCLE_SEC");

	conf_obj->on_get_str(conf_obj, "CHECK_FILE_SYSTEM_NAME", check_file_system_name);
	cpu_info_index = conf_obj->on_get_int(conf_obj, "CPU_INFO_INDEX");
	mem_info_index = conf_obj->on_get_int(conf_obj, "MEM_INFO_INDEX");
	check_file_system_index = conf_obj->on_get_int(conf_obj, "CHECK_FILE_SYSTEM_INDEX");

	conf_obj->on_get_str(conf_obj, "QU_FORMAT_FILE", QU_qformat_file);
	conf_obj->on_get_str(conf_obj, "SU_FORMAT_FILE", SU_qformat_file);
	conf_obj->on_get_str(conf_obj, "PS_FORMAT_FILE", PS_qformat_file);
	conf_obj->on_get_str(conf_obj, "PS_EACH_FORMAT_FILE", PS_each_qformat_file);
	conf_obj->on_get_str(conf_obj, "PS_EACH_TYPE", PS_each_type);

	// conf_obj->on_print_conf(conf_obj);

	i_loglevel = get_log_level(log_level);

	sprintf(log_full_name, "%s/%s", log_directory , log_filename);
	rc = fq_open_file_logger(&l, log_full_name, i_loglevel);
	if(rc <= 0) {
		fprintf(stderr, "fq_open_file_logger(%s) error.\n", log_full_name);
		return(-4);
	}

	fq_log(l, FQ_LOG_EMERG, "[%s] process is started.\n", g_Progname);

	if( open_mem_queue_obj( &memq_obj ) != TRUE ) {
        fq_log(l, FQ_LOG_ERROR, "open_mem_queue_obj() error.\n");
        exit(0);
    }

	QU_thread_params.l = l;
	QU_thread_params.qpath = qpath;
	QU_thread_params.qu_loop_cycle_sec = qu_loop_cycle_sec;
	QU_thread_params.su_loop_cycle_sec = su_loop_cycle_sec;
	QU_thread_params.ps_loop_cycle_sec = ps_loop_cycle_sec;
	QU_thread_params.memq_obj = memq_obj;

	pthread_create(&QU_thread_id, NULL, &QU_thread, &QU_thread_params);
	pthread_create(&SU_thread_id, NULL, &SU_thread, &QU_thread_params);

	if(strncmp( PS_each_type, "YES", 3)==0) {
		pthread_create(&PS_thread_id, NULL, &PS_each_thread, &QU_thread_params);
	}
	else {
		pthread_create(&PS_thread_id, NULL, &PS_thread, &QU_thread_params);
	}

	pthread_create(&Consumer_id, NULL, &Consumer, &QU_thread_params);

	pthread_join( QU_thread_id, &rtn);
	pthread_join( SU_thread_id, &rtn);
	pthread_join( PS_thread_id, &rtn);
	pthread_join( Consumer_id, &rtn);


	fq_log(l, FQ_LOG_EMERG, "[%s] process is stopped.\n", g_Progname);

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

void *QU_thread(void *arg)
{
	thread_params_t *t_param = (thread_params_t *)arg;

	fq_log(t_param->l, FQ_LOG_INFO, "%s", "QU_thread started.");

	while(1) {
		char	filename[256];
		FILE	*fp = NULL;
		char	qname[80];
		char	postdata[2048];

		qformat_t *q=NULL;
		fqlist_t  *t=NULL;
		/* output */
		unsigned char   *packet=NULL;



		sprintf(filename, "%s/%s", t_param->qpath, FQ_LIST_FILE_NAME);

		fp = fopen(filename, "r");
		if(fp == NULL) {
			fq_log(t_param->l, FQ_LOG_ERROR, "fopen(%s) error. reason=[%s]", filename, strerror(errno));
			break;
		}		

		while( fgets( qname, 80, fp) ) {
			char    info_filename[256];
			fq_info_t   *_fq_info=NULL;
			char    info_qpath[256];
			char    info_qname[128];
			fqueue_info_t qi;
			char 	*errmsg = NULL;
			int		rc;
			char	date[9], time[7];
			int		len;
			char	szHostName[64+1];
			queue_message_t d;

			q = new_qformat(QU_qformat_file, &errmsg);
			CHECK(q);

			rc=read_qformat( q, QU_qformat_file, &errmsg);
			CHECK(rc >= 0);

			str_lrtrim(qname);
			if( qname[0] == '#' ) continue;

			sprintf(info_filename, "%s/%s.Q.info", t_param->qpath, qname);
			if( !can_access_file(t_param->l, info_filename)) {
				fq_log(t_param->l, FQ_LOG_ERROR, "Can't access to [%s].", info_filename);
				break;
			}

			_fq_info = new_fq_info(info_filename);
			if(!_fq_info) {
				fq_log(t_param->l, FQ_LOG_ERROR, "new_fq_info() error.");
				break;
			}
			if( load_info_param(l, _fq_info, info_filename) < 0 ) {
				fq_log(t_param->l, FQ_LOG_ERROR, "load_param() error.");
				if(_fq_info) free_fq_info(&_fq_info);
				break;
			}
			
			get_fq_info(_fq_info, "QPATH", info_qpath);
			get_fq_info(_fq_info, "QNAME", info_qname);

			if( GetQueueInfo(t_param->l, info_qpath, info_qname, &qi) == FALSE ) {
				fq_log(t_param->l, FQ_LOG_ERROR, "GetQueueInfo(%s, %s) Error.", info_qpath, info_qname);
				break;
			}

			gethostname(szHostName, sizeof(szHostName));

			memset(postdata, 0x00, sizeof(postdata));
			sprintf( postdata, "HostName=%s&QueuePath=%s&QueueName=%s&XA_OnOff=%s&MsgLen=%d&MaxRecCnt=%d&EnSum=%ld&DeSum%ld&Diff=%ld", 
				szHostName, info_qpath, info_qname, qi.XA_ON_OFF, qi.msglen, qi.max_rec_cnt, qi.en_sum, qi.de_sum, qi.diff);


			t = fqlist_new('A', "QU_INFO");
			CHECK(t);

			get_time(date, time);
			fqlist_add(t, "gen_hms", time, 0);

			fill_qformat(q, postdata, t, NULL);

			len = assoc_qformat(q, NULL);
			CHECK(len > 0 );

			packet = malloc(len+1);
			memset(packet, 0x00, len+1);
			memcpy(packet, q->data, q->datalen);

			/*
			printf("-----< qformat output value >------\n");
			printf("\t-data:[%s]\n", q->data);
			printf("\t-len=[%d]\n", q->datalen);
			*/
			

			/* enQ into memory */
			memset(d.msg, 0x00, sizeof(d.msg));
			memcpy(d.msg, q->data, q->datalen);
			// sprintf(d.msg, "%s", q->data);
			d.len = q->datalen;
			pthread_mutex_lock( &t_param->memq_obj->mtx);
			if( t_param->memq_obj->on_putItem( t_param->memq_obj, &d)  == MEM_QUEUE_FULL ) {  /* full */
				pthread_cond_wait(&t_param->memq_obj->Buffer_Not_Full, &t_param->memq_obj->mtx);
			}

			pthread_cond_signal(&t_param->memq_obj->Buffer_Not_Empty);
			pthread_mutex_unlock(&t_param->memq_obj->mtx);


			SAFE_FREE(packet);

			if(q) free_qformat(&q);

			FreeQueueInfo(NULL, &qi);
		}

		if(fp) fclose(fp);
	
		sleep(t_param->qu_loop_cycle_sec);
	}

	fq_log(t_param->l, FQ_LOG_ERROR, "%s.", "QU_thread stoped.");

	pthread_exit(0);
}

/* linux version */
void *SU_thread(void *arg)
{
	thread_params_t *t_param = (thread_params_t *)arg;

	/* for CPU usage */
	FILE *fp_stat=NULL;
	unsigned long long int fields[10], total_tick, total_tick_old, idle, idle_old, del_total_tick, del_idle;
	int update_cycle = 0, i;
	double percent_usage;


	/* for MEM usage */
	FILE *fp_meminfo=NULL;

	/* for Disk usage */


	fq_log(t_param->l, FQ_LOG_INFO, "%s", "SU_thread started.");

	fp_stat = fopen ("/proc/stat", "r");
	if( fp_stat == NULL ) {
		fq_log(t_param->l, FQ_LOG_ERROR, "fopen('/proc/stat') error.");
		goto L0;
	}

	fp_meminfo = fopen ("/proc/meminfo", "r");
	if( fp_meminfo == NULL ) {
		fq_log(t_param->l, FQ_LOG_ERROR, "fopen('/proc/meminfo') error.");
		goto L0;
	}


	if (!read_fields_CPU (fp_stat, fields)) {
		fq_log(t_param->l, FQ_LOG_ERROR, "read_fileds_CPU() error.");
		goto L0;
	}

	for (i=0, total_tick = 0; i<10; i++) {
		total_tick += fields[i];
	}

	idle = fields[3]; /* idle ticks index */
	sleep(1);

	while(1) {
		char	postdata[2048];

		qformat_t *q=NULL;
		fqlist_t  *t=NULL;
		/* output */
		unsigned char   *packet=NULL;

		char    date[9], time[7];
		char    CPU_usage[10], MEM_usage[16], DISK_usage[16];
		int		rc;
		int		len;
		char	szHostName[64];
		queue_message_t d;
		char	*errmsg=NULL;

		q = new_qformat(SU_qformat_file, &errmsg);
		CHECK(q);

		rc=read_qformat( q, SU_qformat_file, &errmsg);
		CHECK(rc >= 0);


		/* get CPU usage */
		total_tick_old = total_tick;
		idle_old = idle;

		fseek (fp_stat, 0, SEEK_SET);
		fflush (fp_stat);

		if (!read_fields_CPU(fp_stat, fields)) {
			fq_log(t_param->l, FQ_LOG_ERROR, "read_fileds_CPU() error.");
			goto L0;
		}
		for (i=0, total_tick = 0; i<10; i++) {
			total_tick += fields[i];
		}
		idle = fields[3];

		del_total_tick = total_tick - total_tick_old;
		del_idle = idle - idle_old;

		percent_usage = ((del_total_tick - del_idle) / (double) del_total_tick) * 100; /* 3 is index of idle time */
		// printf("percent_usage='%3.2lf%%\n", percent_usage);

		sprintf(CPU_usage,  "%3.2lf%%", percent_usage);
		update_cycle++;



		/* get MEM usage */
		rc = get_meminfo( fp_meminfo,  MEM_usage );
		CHECK( rc > 0 );


		get_time(date, time);

		rc = get_diskinfo( check_file_system_name, DISK_usage);
		// rc = get_used_DISK(check_file_system_name, DISK_usage, check_file_system_index);
		// CHECK(rc > 0);

		gethostname(szHostName, sizeof(szHostName));

		memset(postdata, 0x00, sizeof(postdata));
		sprintf( postdata, "HostName=%s&CPU=%s&MEM=%s&DISK=%s", szHostName, CPU_usage, MEM_usage, DISK_usage );

		// printf("postdata=[%s]\n", postdata);

		t = fqlist_new('A', "SU_INFO");
		CHECK(t);

		get_time(date, time);
		fqlist_add(t, "gen_hms", time, 0);

		fill_qformat(q, postdata, t, NULL);

		len = assoc_qformat(q, NULL);
		CHECK(len > 0 );

		packet = malloc(len+1);
		memset(packet, 0x00, len+1);
		memcpy(packet, q->data, q->datalen);

		/*
		printf("-----< qformat output value >------\n");
		printf("\t-data:[%s]\n", q->data);
		printf("\t-len=[%d]\n", q->datalen);
		*/
		

		/* enQ into memory */
		memset(d.msg, 0x00, sizeof(d.msg));
		memcpy(d.msg, q->data, q->datalen);
		d.len = q->datalen;
		pthread_mutex_lock( &t_param->memq_obj->mtx);
		if( t_param->memq_obj->on_putItem( t_param->memq_obj, &d)  == MEM_QUEUE_FULL ) {  /* full */
			pthread_cond_wait(&t_param->memq_obj->Buffer_Not_Full, &t_param->memq_obj->mtx);
		}

		pthread_cond_signal(&t_param->memq_obj->Buffer_Not_Empty);
		pthread_mutex_unlock(&t_param->memq_obj->mtx);


		SAFE_FREE(packet);

		if(q) free_qformat(&q);
		if(errmsg) free(errmsg);

		sleep(t_param->su_loop_cycle_sec);
	}
L0:
	if( fp_stat ) fclose(fp_stat);

	fq_log(t_param->l, FQ_LOG_ERROR, "%s.", "SU_thread stoped.");

	pthread_exit(0);
}

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/uio.h>


/* deQ threads */
void *Consumer( void *arg)
{
	thread_params_t *p = (thread_params_t *)arg;


	int socketDescriptor;
	struct sockaddr_in serverAddress;
	unsigned short int serverPort = dst_server_port;

	if ((socketDescriptor = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		fq_log(p->l, FQ_LOG_ERROR, "%s.", "socket() error reason=[%s]",strerror(errno));
		pthread_exit( (void *)0);
	}
	serverAddress.sin_family = AF_INET; // sets the server address to type AF_INET, similar to the socket we will use. 
	serverAddress.sin_addr.s_addr = inet_addr(dst_server_ip); // this sets the server address. 
	serverAddress.sin_port = serverPort;  // sets the server port.
	
    for(;;)
    {
		queue_message_t	d;

		d.len = 0;
		memset(d.msg, 0x00, sizeof(d.msg));

        pthread_mutex_lock(&p->memq_obj->mtx);

        if( p->memq_obj->on_isEmpty(p->memq_obj) )
        {
            pthread_cond_wait(&p->memq_obj->Buffer_Not_Empty,&p->memq_obj->mtx);
        }
		p->memq_obj->on_getItem(p->memq_obj, &d);

        fq_log(p->l, FQ_LOG_INFO, "Consumer : %d, %s  \n", d.len, d.msg);

		if (sendto(socketDescriptor, d.msg, d.len, 0, (struct sockaddr *) &serverAddress, sizeof(serverAddress) ) < 0) {
			fq_log(p->l, FQ_LOG_ERROR, "%s.", "sendto() error reason=[%s]",strerror(errno));
			goto L0;
		}

        pthread_cond_signal(&p->memq_obj->Buffer_Not_Full);
		// usleep(1000);
		
        pthread_mutex_unlock(&p->memq_obj->mtx);
    }
L0:
		pthread_exit( (void *)0);
}

#define BUF_MAX 2048

static int read_fields_CPU (FILE *fp, unsigned long long int *fields)
{
  int retval;
  char buffer[BUF_MAX];


  if (!fgets (buffer, BUF_MAX, fp))
  { perror ("Error"); }
  retval = sscanf (buffer, "cpu %Lu %Lu %Lu %Lu %Lu %Lu %Lu %Lu %Lu %Lu", 
                            &fields[0], 
                            &fields[1], 
                            &fields[2], 
                            &fields[3], 
                            &fields[4], 
                            &fields[5], 
                            &fields[6], 
                            &fields[7], 
                            &fields[8], 
                            &fields[9]); 
  if (retval < 4) /* Atleast 4 fields is to be read */
  {
    fprintf (stderr, "Error reading /proc/stat cpu field\n");
    return 0;
  }
  return 1;
}
static int get_meminfo(FILE *fp, char *dst)
{
	char contents[32];
	char unit[6];

	float value;
	float MemTotal=0.0;
	float MemFree=0.0;
	float MemUsed=0.0;
	float percent_usage=0.0;
	int  total_found = 0, free_found = 0;
	char	buf[128];

	fseek (fp, 0, SEEK_SET);
	fflush (fp);

	while(fgets( buf, 128, fp)) {
		char *p=NULL;


		sscanf(buf, "%s %f %s\n", contents, &value, unit);

		p = strstr( contents, "MemTotal");
		if( p != NULL ) {
			MemTotal = value;
			// printf("MemTotal found = %f\n", MemTotal);
			total_found = 1;
			continue;
		}
		p = strstr( contents, "MemFree");
		if( p != NULL ) {
			MemFree = value;
			// printf("MemFree found = %f\n", MemFree);
			free_found = 1;	
			continue;
		}

		if( total_found && free_found ) {
			break;
		}
	}

	if( total_found && free_found ) { /* success */
	
		MemUsed = MemTotal - MemFree;
		// percent_usage = (MemFree/MemTotal)*(float)100.0;
		percent_usage = (MemUsed/MemTotal)*(float)100.0;
		// printf ("[%f] [%f] -> Total MEM Usage: %3.2f%%\n", MemTotal, MemFree, percent_usage);
		sprintf( dst, " %3.2f%%", percent_usage);
		return 1;
	}
	else {
		return 0;
	}
}

#include <sys/statvfs.h>
static int get_diskinfo( char *fnPath, char *dst)
{
	struct statvfs fiData;
	float	total_block;
	float	free_block;
	// float	used_block;
	float percent_usage;
	
	if((statvfs(fnPath,&fiData)) < 0 ) {
		return(-1);
	}
	else {
		total_block = (float)fiData.f_blocks;
		free_block = (float)fiData.f_bfree;
		// used_block = (float)(fiData.f_blocks - fiData.f_bfree);

		// percent_usage = (used_block/total_block)*100;
		percent_usage = (free_block/total_block)*100;
		sprintf( dst, " %3.2f%%", percent_usage);
		return(1);
	}
}

#include "fq_monitor.h"
#include "fq_tokenizer.h"

/* This is a all_string send type */
void *PS_thread(void *arg)
{
	thread_params_t *t_param = (thread_params_t *)arg;
	const char *delimeter="`";
	const char *delimeter_bub=",";
	int rc;


	monitor_obj_t   *obj=NULL;

	rc =  open_monitor_obj(l,  &obj);
	CHECK(rc==TRUE);

	while(1) {
		tokened *tokened_head=NULL,*cur=NULL;
		char    *all_string=NULL;
		int     use_rec_count;
		char	postdata[2048];

		qformat_t *q=NULL;
		fqlist_t  *t=NULL;
		/* output */
		unsigned char   *packet=NULL;

		char    date[9], time[7];
		int		rc;
		int		len;
		char	szHostName[64];
		queue_message_t d;
		char	*errmsg=NULL;

		q = new_qformat(PS_qformat_file, &errmsg);
		CHECK(q);

		rc=read_qformat( q, PS_qformat_file, &errmsg);
		CHECK(rc >= 0);

		if( obj->on_getlist_mon(l, obj, &use_rec_count, &all_string) < 0 ) {
			fq_log(t_param->l, FQ_LOG_ERROR, "on_getlist_mon() error" );
			goto L0;
		}
		fq_log(t_param->l, FQ_LOG_DEBUG,  "on_getlist_mon() OK.[%d][%s]\n", use_rec_count, all_string);

		tokenizer(all_string, delimeter, &tokened_head); /* extract 1 row from all_string by delimeter */
		cur=tokened_head;
		while(cur != NULL) {
			int size;
			char *buf=NULL;

			size=(cur->end - cur->sta); /* length of 1 row */
			buf = calloc( size+1, sizeof(char));
			memcpy(buf, cur->sta, size);

			if( cur->type !=TOK_TOKEN_O ) {
				int sub_size=0;
				tokened *sub_tokened_head=NULL, *sub_cur=NULL;

				fq_log(t_param->l, FQ_LOG_DEBUG,  "row=[%s]\n", buf);
				
				tokenizer(buf, delimeter_bub, &sub_tokened_head);
				sub_cur = sub_tokened_head;
				while(sub_cur != NULL ) {
					char *sub_buf=NULL;

					sub_size = (sub_cur->end - sub_cur->sta);
					sub_buf=calloc( sub_size+1, sizeof(char));
					memcpy(sub_buf,sub_cur->sta,sub_size);

					if( sub_cur->type !=TOK_TOKEN_O ) {
						fq_log(t_param->l, FQ_LOG_DEBUG,  "\t-[%s]\n", sub_buf);
					}

					sub_cur = sub_cur->next;
					SAFE_FREE(sub_buf);
				}
				tokenizer_free(sub_tokened_head);
			}
			cur=cur->next;
			SAFE_FREE(buf);
		}
		tokenizer_free(tokened_head);

		
		get_time(date, time);
		gethostname(szHostName, sizeof(szHostName));
		memset(postdata, 0x00, sizeof(postdata));
		sprintf( postdata, "HostName=%sPSLIST=%s", szHostName, all_string );


		// printf("postdata=[%s]\n", postdata);

		t = fqlist_new('A', "PS_INFO");
		CHECK(t);

		get_time(date, time);
		fqlist_add(t, "gen_hms", time, 0);

		fill_qformat(q, postdata, t, NULL);

		len = assoc_qformat(q, NULL);
		CHECK(len > 0 );

		packet = malloc(len+1);
		memset(packet, 0x00, len+1);
		memcpy(packet, q->data, q->datalen);

		/*
		printf("-----< qformat output value >------\n");
		printf("\t-data:[%s]\n", q->data);
		printf("\t-len=[%d]\n", q->datalen);
		*/
		

		/* enQ into memory */
		memset(d.msg, 0x00, sizeof(d.msg));
		memcpy(d.msg, q->data, q->datalen);
		d.len = q->datalen;
		pthread_mutex_lock( &t_param->memq_obj->mtx);
		if( t_param->memq_obj->on_putItem( t_param->memq_obj, &d)  == MEM_QUEUE_FULL ) {  /* full */
			pthread_cond_wait(&t_param->memq_obj->Buffer_Not_Full, &t_param->memq_obj->mtx);
		}

		pthread_cond_signal(&t_param->memq_obj->Buffer_Not_Empty);
		pthread_mutex_unlock(&t_param->memq_obj->mtx);


		SAFE_FREE(all_string); /* Don't forget it. */

		SAFE_FREE(packet);

		if(q) free_qformat(&q);
		if(errmsg) free(errmsg);

		sleep(t_param->ps_loop_cycle_sec);
	} /* while */
L0:

	fq_log(t_param->l, FQ_LOG_ERROR, "%s.", "PS_thread stoped.");

	pthread_exit(0);
}

/* This is a each send type */
void *PS_each_thread(void *arg)
{
	thread_params_t *t_param = (thread_params_t *)arg;
	const char *delimeter="`";
	const char *delimeter_bub=",";
	int rc;


	monitor_obj_t   *obj=NULL;

	rc =  open_monitor_obj(l,  &obj);
	CHECK(rc==TRUE);

	while(1) {
		tokened *tokened_head=NULL,*cur=NULL;
		char    *all_string=NULL;
		int     use_rec_count;
		char	postdata[2048];

		qformat_t *q=NULL;
		/* output */
		unsigned char   *packet=NULL;

		char    date[9], time[7];
		int		rc;
		int		len;
		char	szHostName[64];
		queue_message_t d;
		char	*errmsg=NULL;

		q = new_qformat(PS_each_qformat_file, &errmsg);
		CHECK(q);

		rc=read_qformat( q, PS_each_qformat_file, &errmsg);
		CHECK(rc >= 0);

		if( obj->on_getlist_mon(l, obj, &use_rec_count, &all_string) < 0 ) {
			fq_log(t_param->l, FQ_LOG_ERROR, "on_getlist_mon() error" );
			goto L0;
		}
		fq_log(t_param->l, FQ_LOG_INFO,  "on_getlist_mon() OK.[%d][%s]\n", use_rec_count, all_string);

		tokenizer(all_string, delimeter, &tokened_head); /* extract 1 row from all_string by delimeter */
		cur=tokened_head;
		while(cur != NULL) {
			int size;
			char *buf=NULL;
			int column_index=0;

			size=(cur->end - cur->sta); /* length of 1 row */
			buf = calloc( size+1, sizeof(char));
			memcpy(buf, cur->sta, size);

			if( cur->type !=TOK_TOKEN_O ) {
				int sub_size=0;
				tokened *sub_tokened_head=NULL, *sub_cur=NULL;
				fqlist_t  *t=NULL;

				t = fqlist_new('A', "PS_INFO");
				CHECK(t);

				fq_log(t_param->l, FQ_LOG_DEBUG,  "row=[%s]\n", buf);
				
				tokenizer(buf, delimeter_bub, &sub_tokened_head);
				sub_cur = sub_tokened_head;
				column_index = 0;
				while(sub_cur != NULL ) {
					char *sub_buf=NULL;

					sub_size = (sub_cur->end - sub_cur->sta);
					sub_buf=calloc( sub_size+1, sizeof(char));
					memcpy(sub_buf,sub_cur->sta,sub_size);

					if( sub_cur->type !=TOK_TOKEN_O ) {
						switch( column_index ) {
							case 0:
								fqlist_add(t, "PROCESS_NAME", sub_buf, 0);
								break;
							case 1:
								fqlist_add(t, "QPATH", sub_buf, 0);
								break;
							case 2:
								fqlist_add(t, "QNAME", sub_buf, 0);
								break;
							case 3:
								fqlist_add(t, "DESC", sub_buf, 0);
								break;
							case 4:
								fqlist_add(t, "ACTION", sub_buf, 0);
								break;
							case 5:
								fqlist_add(t, "STATUS", sub_buf, 0);
								break;
							case 6:
								fqlist_add(t, "PID", sub_buf, 0);
								break;
							default:
								break;
						}
						column_index++;
					}

					sub_cur = sub_cur->next;
					SAFE_FREE(sub_buf);
				} /* sub_while end */

				tokenizer_free(sub_tokened_head);

				gethostname(szHostName, sizeof(szHostName));
				memset(postdata, 0x00, sizeof(postdata));
				sprintf( postdata, "HostName=%s", szHostName);

				// printf("postdata=[%s]\n", postdata);

				get_time(date, time);
				fqlist_add(t, "gen_hms", time, 0);

				fill_qformat(q, postdata, t, NULL);

				len = assoc_qformat(q, NULL);
				CHECK(len > 0 );

				packet = malloc(len+1);
				memset(packet, 0x00, len+1);
				memcpy(packet, q->data, q->datalen);

				/*
				printf("-----< qformat output value >------\n");
				printf("\t-data:[%s]\n", q->data);
				printf("\t-len=[%d]\n", q->datalen);
				*/
				
				/* enQ into memory */
				memset(d.msg, 0x00, sizeof(d.msg));
				memcpy(d.msg, q->data, q->datalen);
				d.len = q->datalen;
				pthread_mutex_lock( &t_param->memq_obj->mtx);
				if( t_param->memq_obj->on_putItem( t_param->memq_obj, &d)  == MEM_QUEUE_FULL ) {  /* full */
					pthread_cond_wait(&t_param->memq_obj->Buffer_Not_Full, &t_param->memq_obj->mtx);
				}

				pthread_cond_signal(&t_param->memq_obj->Buffer_Not_Empty);
				pthread_mutex_unlock(&t_param->memq_obj->mtx);

				SAFE_FREE(all_string); /* Don't forget it. */

				SAFE_FREE(packet);
			}
			cur=cur->next;
			SAFE_FREE(buf);
		}
		tokenizer_free(tokened_head);

		if(q) free_qformat(&q);
		if(errmsg) free(errmsg);

		sleep(t_param->ps_loop_cycle_sec);
	} /* while */
L0:

	fq_log(t_param->l, FQ_LOG_ERROR, "%s.", "PS_thread stoped.");

	pthread_exit(0);
}
