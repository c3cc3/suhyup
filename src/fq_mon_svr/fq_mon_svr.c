/*
** monitor.c
** Description: This program sees status of all file queue semi-graphically.
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <curses.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

#include "fqueue.h"
#include "shm_queue.h"
#include "fq_socket.h"
#include "fq_file_list.h"
#include "fuser_popen.h"
#include "fq_manage.h"
#include "parson.h"
#include "fq_external_env.h"
// #include "monitor.h"
#include "fq_hashobj.h"
#include "fq_mon_svr_conf.h"
#include "ums_common_conf.h"
#include "fq_gssi.h"

/* Customizing Area */
#define PRODUCT_NAME "M&Wise UMS Distributor"
/* Please type your ethernet device name with  $ ip addr command */
#define ETHERNET_DEVICE_NAME "eno1"

#define CURSOR_X_POSITION (50)  // key waiting cursor position
#define MAX_FILES_QUEUES (100)
#define USAGE_THRESHOLD (90.0)
#define DISK_THRESHOLD (1500000)

#define MONOTORING_WAITING_MSEC (1000000)

hashmap_obj_t *heartbeat_hash_obj=NULL;

typedef struct _alarm_msg_items {
	char *hostname;
	char *qpath;
	char *qname;
	char *q_status_msg;
} alarm_msg_items_t;

file_list_obj_t *exclude_list_obj=NULL;

static int uchar_enQ(fq_logger_t *l, unsigned char *data, size_t data_len, fqueue_obj_t *obj);
static bool Open_Alarm_Q( fq_logger_t *l, fq_mon_svr_conf_t *my_conf, fqueue_obj_t **ararm_q_obj);
static bool enQ_alarm_msg(fq_logger_t *l, fqueue_obj_t *alarm_q_obj, char *json_template_pathfile, alarm_msg_items_t *alarm_items);
static bool Unchanged_for_a_period_of_time( fq_logger_t *l, time_t last_update_time, long period_sec );
static int alarm_eventlog(fq_logger_t *l, char *logpath, ...);

static bool is_exclude_queue_name( fq_logger_t *l, file_list_obj_t *exclude_list_obj, char *qname);
bool append_mypid_fq_pid_listfile_for_heartbeat( fq_logger_t *l, char *file );

typedef struct main_thread_params {
	fq_logger_t *l;
	char *ums_common_log_path;
	char *log_filename;
	int	i_log_level;
	char *hashmap_path;
	char *hashmap_name;
	long	collect_cycle_usec;
	char *listfile_to_exclude;
	float alarm_usage_threshold;
	long	unchanged_period_sec_en;
	long	unchanged_period_sec_de;
	int	 de_tps_threshold;
	bool fqpm_use_flag;
	bool do_not_flow_check_flag;
	int	 do_not_flow_check_sec;
	bool alarm_use_flag;
	fqueue_obj_t *alarm_q_obj;
	char *json_template_pathfile;
	long	lock_threshold_micro_sec;
	int	 delay_sec_threshold;
} main_thread_params_t;

static void *main_thread( void *arg );

char g_eth_name[16];
char g_log_open_date[9];

hashmap_obj_t *hash_obj;
/* Global variables */
typedef struct Queues {
	fqueue_obj_t *obj;
	unsigned char old_data[65536];
	char desc[MAX_DESC_LEN+1];
	bool_t	status;
	int		en_tps;
	int		de_tps;
	long	before_en;
	long	before_de;
	time_t	en_competition;
	time_t	de_competition;
	long	en_sum;
	long	de_sum;
	int 	big;	
	int		shmq_flag;
	int		q_status;
	time_t	last_en_time;
	time_t	last_de_time;
	pthread_mutex_t	mutex;
} Queues_t;

Queues_t Qs[MAX_FILES_QUEUES];

pthread_mutex_t mutex;

void init_Qs() {
	int i, rc;
	for(i=0; i<MAX_FILES_QUEUES; i++) {
		Qs[i].obj = NULL;
		Qs[i].old_data[0] = 0x00;
		Qs[i].desc[0] = 0x00;
		Qs[i].status = False;
		Qs[i].en_tps = 0;
		Qs[i].de_tps = 0;
		Qs[i].before_en = 0L;
		Qs[i].before_de = 0L;
		Qs[i].en_competition = 0L;
		Qs[i].de_competition = 0L;
		Qs[i].en_sum = 0L;
		Qs[i].de_sum = 0L;
		rc = pthread_mutex_init (&Qs[i].mutex, NULL);
		CHECK( rc == 0);
		Qs[i].big = 0;
		Qs[i].shmq_flag = 0;
		Qs[i].q_status = 0;
		Qs[i].last_en_time = 0L;
		Qs[i].last_de_time = 0L;

	}
}

char *g_path = NULL;
char fq_data_home[16+1];

bool heartbeat_ums(fq_logger_t *l, hashmap_obj_t *hash_obj, char *progname)
{
	FQ_TRACE_ENTER(l);

	char str_pid[10];
	char str_bintime[16];

	sprintf(str_pid, "%d", getpid());

	time_t now;
	now = time(0);
	sprintf(str_bintime,  "%ld", now);

	int rc;
	rc = PutHash(l, hash_obj, str_pid, str_bintime);
	if( rc != TRUE ) {
		fq_log(l, FQ_LOG_ERROR, "PutHash(key='%s', value='%s')", str_pid, str_bintime);
		return false;
		// obj->on_clean_table(l, seq_check_hash_obj);
	}
	FQ_TRACE_EXIT(l);

	return true;
}

int main(int ac, char **av)
{
    int rc;
    pthread_t thread_main;
    int thread_id;

	/* shared memory */
    key_t     key=5678;
    int     sid = -1;
    int     size;

	g_path = getenv("FQ_DATA_HOME");
	CHECK(g_path);

	ums_common_conf_t *cm_conf = NULL;
	fq_mon_svr_conf_t   *my_conf=NULL;

	if( ac != 2 ) {
		printf("Usage: $ %s [your config file] <enter>\n", av[0]);
		return 0;
	}

	char *errmsg = NULL;
	if(Load_ums_common_conf(&cm_conf, &errmsg) == false) {
		printf("Load_ums_common_conf() error. reason='%s'\n", errmsg);
		return(-1);
	}	

	if(LoadConf(av[1], &my_conf, &errmsg) == false) {
		printf("LoadMyConf('%s') error. reason='%s'\n", av[1], errmsg);
		return(-1);
	}

	char log_pathfile[256];
	fq_logger_t *l = NULL;
	sprintf(log_pathfile, "%s/%s", cm_conf->ums_common_log_path, my_conf->log_filename);
	rc = fq_open_file_logger(&l, log_pathfile, my_conf->i_log_level);
	CHECK(rc==TRUE);
	printf("log file: '%s'\n", log_pathfile);

	get_time(g_log_open_date, NULL);


	bool tf;
	tf = OpenHashMapFiles(l, my_conf->heartbeat_hash_map_path, my_conf->heartbeat_hash_map_name, &heartbeat_hash_obj);
	if(tf == false) {
		fq_log(l, FQ_LOG_ERROR, "Can't open ums hashmap(path='%s', name='%s'). Please check hashmap.", 
				my_conf->heartbeat_hash_map_path, my_conf->heartbeat_hash_map_name);
		return(0);
	}

	fq_log(l, FQ_LOG_INFO, "ums HashMap(path='%s', name='%s') open ok.", my_conf->heartbeat_hash_map_path, my_conf->heartbeat_hash_map_name);


	init_Qs();

	fqueue_obj_t *alarm_q_obj = NULL;
	tf = Open_Alarm_Q( l, my_conf, &alarm_q_obj);
	CHECK(tf);

	/* Make a exclude linkedlist */

	rc = open_file_list(l, &exclude_list_obj, my_conf->listfile_to_exclude);
    CHECK( rc == TRUE );
	fq_log(l, FQ_LOG_INFO, "Count of exclude list is [%d]\n", exclude_list_obj->count);


    rc = pthread_mutex_init (&mutex, NULL);
    if( rc !=0 ) {
        perror("pthread_mutex_init"), exit(0);
    }

	main_thread_params_t *tp;
	tp = calloc(1, sizeof(main_thread_params_t));

	tp->l = l;
	tp->ums_common_log_path = strdup(cm_conf->ums_common_log_path);
	tp->log_filename = strdup(my_conf->log_filename);
	tp->i_log_level = my_conf->i_log_level;
	tp->hashmap_path = strdup( my_conf->hashmap_path);
	tp->hashmap_name = strdup( my_conf->hashmap_name);
	tp->collect_cycle_usec = my_conf->collect_cycle_usec;
	tp->listfile_to_exclude = strdup( my_conf->listfile_to_exclude);
	tp->alarm_usage_threshold = my_conf->alarm_usage_threshold;
	tp->unchanged_period_sec_en = my_conf->unchanged_period_sec_en;
	tp->unchanged_period_sec_de = my_conf->unchanged_period_sec_de;
	tp->de_tps_threshold =  my_conf->de_tps_threshold;
	tp->fqpm_use_flag = my_conf->fqpm_use_flag;
	tp->do_not_flow_check_flag = my_conf->do_not_flow_check_flag;
	tp->do_not_flow_check_sec = my_conf->do_not_flow_check_sec;

	tp->alarm_use_flag = my_conf->alarm_use_flag;
	tp->alarm_q_obj = alarm_q_obj;
	tp->json_template_pathfile = my_conf->json_template_pathfile;
	tp->lock_threshold_micro_sec = my_conf->lock_threshold_micro_sec;
	tp->delay_sec_threshold = my_conf->delay_sec_threshold;

    pthread_create(&thread_main, NULL, main_thread, tp);
	sleep(2);
    // pthread_create(&thread_cmd, NULL, waiting_key_thread, &thread_id);

	append_mypid_fq_pid_listfile_for_heartbeat(l, my_conf->fq_pid_list_file);

    pthread_join(thread_main, NULL);

	close_file_list(l, &exclude_list_obj);

	CloseFileQueue(l, &alarm_q_obj);
	fq_close_file_logger(&l);

    return(0);
}

typedef struct thread_params {
	int array_index;
	fqueue_obj_t *obj;
	fq_logger_t *l;
	time_t waiting_msec;
} thread_params_t;

/*
** This thread gets values of self and puts it to Qs array.
*/
void *mon_thread( void *arg ) {

	thread_params_t	*tp = arg;
	fqueue_obj_t *obj = tp->obj;
	fq_logger_t	*l = tp->l;
	int array_index = tp->array_index;
	time_t waiting_msec = tp->waiting_msec;
	
	fq_log(l, FQ_LOG_EMERG, "[%d]-th mon_thread() started.", array_index);

	while(1) {
		bool_t status = False;

		pthread_mutex_lock(&Qs[array_index].mutex);

		bool tf;
		tf = heartbeat_ums(l, heartbeat_hash_obj, "fq_mon_svr_thread");
		CHECK(tf);

		Qs[array_index].en_competition = obj->on_check_competition( l, obj, FQ_EN_ACTION);
#if 0
		if( Qs[array_index].en_competition > tp->lock_threshold_micro_sec) {
			fq_log(l, FQ_LOG_ERROR, "en_competition: [%ld] micro second. threshold_micro_sec=[%ld]", Qs[array_index].en_competition, tp->lock_threshold_micro_sec);
		}
		Qs[array_index].de_competition = obj->on_check_competition( l, obj, FQ_DE_ACTION);
		if( Qs[array_index].de_competition > tp->lock_threshold_micro_sec) {
			fq_log(l, FQ_LOG_ERROR, "de_competition: [%ld] micro second. threshold_micro_sec=[%ld]", Qs[array_index].de_competition, tp->lock_threshold_micro_sec);
		}
#endif
		// This has a bug, so we display alway Good.
		// bug
		// status = obj->on_do_not_flow( l, obj, waiting_msec );
		if( Qs[array_index].en_competition < 1000 && Qs[array_index].de_competition < 1000) {
			Qs[array_index].status = 0; // Good
		}
		else {
			Qs[array_index].status = 0; // Bad
		}

		if( Qs[array_index].before_en > 0 )  Qs[array_index].en_tps = obj->h_obj->h->en_sum - Qs[array_index].before_en;
		else Qs[array_index].en_tps = Qs[array_index].before_en;
		Qs[array_index].before_en = obj->h_obj->h->en_sum;

		if( Qs[array_index].before_de > 0 )  Qs[array_index].de_tps = obj->h_obj->h->de_sum - Qs[array_index].before_de;
		else Qs[array_index].de_tps = Qs[array_index].before_de;
		Qs[array_index].before_de = obj->h_obj->h->de_sum;

		if( status ) {
			fq_log(l, FQ_LOG_ERROR, "index=[%d]: path=[%s] qname=[%s] status=[%d].", array_index, obj->path, obj->qname, status);
		}
		Qs[array_index].en_sum = obj->h_obj->h->en_sum;
		Qs[array_index].de_sum = obj->h_obj->h->de_sum;

		sprintf(Qs[array_index].desc, "%s", obj->h_obj->h->desc);
		Qs[array_index].q_status = obj->h_obj->h->status;

		Qs[array_index].last_en_time = obj->h_obj->h->last_en_time;
		Qs[array_index].last_de_time = obj->h_obj->h->last_de_time;

		pthread_mutex_unlock(&Qs[array_index].mutex);
		usleep(1000000); // 1 second
	}

	fq_log(l, FQ_LOG_INFO, "[%d]-th mon_thread() exit.", array_index);
	pthread_exit( (void *)0 );
}


/*
** This function open all file queues and create each monitoring thread.
*/
int open_all_file_queues_and_run_mon_thread(fq_logger_t *l, fq_container_t *fq_c, int *opened_q_cnt) {
	dir_list_t *d=NULL;
	fq_node_t *f;
	int opened = 0;

	for( d=fq_c->head; d!=NULL; d=d->next) {
		pthread_attr_t attr;
		pthread_t *mon_thread_id=NULL;
		thread_params_t *tp=NULL;

		for( f=d->head; f!=NULL; f=f->next) {
			int rc;

			if( is_exclude_queue_name( l, exclude_list_obj, f->qname) == true) {
				fq_log(l, FQ_LOG_INFO, "We exclude [%s] in monitoring.", f->qname);
				continue;
			}

			fq_log(l, FQ_LOG_DEBUG, "OPEN: [%s/%s].", d->dir_name, f->qname);
			
			char *p=NULL;
			if( (p= strstr(f->qname, "SHM_")) == NULL ) {
				rc =  OpenFileQueue(l, NULL, NULL, NULL, NULL, d->dir_name, f->qname, &Qs[opened].obj);
			} 
			else {
				rc =  OpenShmQueue(l, d->dir_name, f->qname, &Qs[opened].obj);
				Qs[opened].shmq_flag = 1;
			}
			if( rc != TRUE ) {
				fq_log(l, FQ_LOG_ERROR, "OpenFileQueue('%s') error.", f->qname);
				return(-1);
			}
			fq_log(l, FQ_LOG_DEBUG, "%s:%s open success.", d->dir_name, f->qname);

			mon_thread_id = calloc(1, sizeof(pthread_t));
			tp = calloc(1, sizeof(thread_params_t));

			/*
			** Because of shared memory queue, We change it to continue.
			*/
			// CHECK(rc == TRUE);

			tp->array_index = opened;
			tp->obj = Qs[opened].obj;
			tp->l = l;
			tp->waiting_msec = MONOTORING_WAITING_MSEC;

			pthread_attr_init(&attr);
			pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
			rc = pthread_create( mon_thread_id, &attr, mon_thread, tp);
			CHECK(rc == 0 );
			pthread_attr_destroy(&attr);
			opened++;
		}
	}

	fq_log(l, FQ_LOG_INFO, "Number of opened filequeue is [%d].", opened);
	*opened_q_cnt = opened;
	return(0);
}

void close_all_file_queues(fq_logger_t *l) {
	int i;

	for(i=0; Qs[i].obj; i++) {
		if( Qs[i].obj  && (Qs[i].shmq_flag == 0)) CloseFileQueue(l, &Qs[i].obj);
		else if( Qs[i].obj  && (Qs[i].shmq_flag == 1)) CloseShmQueue(l, &Qs[i].obj);
	}
}

typedef struct body_contents {
	int	odinal;
	char *qpath;
	char *qname;
	int max_rec_cnt;
	int gap;
	float usage;
	time_t	en_competition;
	time_t	de_competition;
	unsigned char data[65536];
	bool_t	status;
	int	en_tps;
	int de_tps;
	int	big;
	int msglen;
	long en_sum;
	long de_sum;
	int	shmq_flag;
	char desc[MAX_DESC_LEN+1];
	int q_status;
	time_t last_en_time;
	time_t last_de_time;

} body_contents_t;


int fill_body_contents_with_qobj( fq_logger_t *l, main_thread_params_t *tp,  int index, fqueue_obj_t *obj, body_contents_t *bc) 
{
	int rc;

	unsigned char buf[65536];
	long l_seq, run_time;
	alarm_msg_items_t	*alarm = NULL;
	
	bc->odinal = index;
	bc->qpath = obj->path;
	bc->qname = obj->qname, alarm;
	bc->max_rec_cnt = obj->h_obj->h->max_rec_cnt;
	bc->gap = obj->on_get_diff(l, obj);
	bc->usage = obj->on_get_usage(l, obj);

	alarm = (alarm_msg_items_t *)calloc(1, sizeof(alarm_msg_items_t) );

	if( bc->usage > tp->alarm_usage_threshold ) {
		fq_log(l, FQ_LOG_EMERG, "%s/%s: Current usage(%5.2f) > threshold(%5.2f) ",  
			obj->path, obj->qname, bc->usage, tp->alarm_usage_threshold);


		char	hostname[HOST_NAME_LEN+1];
		gethostname(hostname, sizeof(hostname));

		alarm->hostname = strdup(hostname);
		alarm->qpath = strdup(obj->path);
		alarm->qname = strdup(obj->qname);

		char snd_msg[128];

		sprintf(snd_msg, "WARN: Current usage(%5.2f) > threshold(%5.2f).", bc->usage, tp->alarm_usage_threshold);
		alarm->q_status_msg = strdup(snd_msg);

		bool tf;
		tf = enQ_alarm_msg(l, tp->alarm_q_obj, tp->json_template_pathfile, alarm);
		CHECK(tf);
	}

	// here-1
	time_t curr_time = time(0);

	time_t delay_time = curr_time-obj->h_obj->h->last_en_time;

	if( bc->gap > 0 && delay_time > tp->delay_sec_threshold) {
		fq_log(l, FQ_LOG_EMERG, "%s/%s: Current Gap(%d) delay_time(%ld), threshold(%d sec)",  
			obj->path, obj->qname, bc->gap, delay_time, tp->delay_sec_threshold);

		char	hostname[HOST_NAME_LEN+1];
		gethostname(hostname, sizeof(hostname));

		alarm->hostname = strdup(hostname);
		alarm->qpath = strdup(obj->path);
		alarm->qname = strdup(obj->qname);

		char snd_msg[128];

		sprintf(snd_msg, "WARN: Current Gap(%d): delay_sec(%ld), threshold(%d sec).", bc->gap, delay_time, tp->delay_sec_threshold);
		alarm->q_status_msg = strdup(snd_msg);

		bool tf;
		tf = enQ_alarm_msg(l, tp->alarm_q_obj, tp->json_template_pathfile, alarm);
		CHECK(tf);
	}

	bc->en_competition = Qs[index].en_competition;
	if( bc->en_competition == 0 ) {
		char lockfilename[256];
		sprintf(lockfilename, "%s/.%s.en.flcok", obj->path, obj->qname);
		// fq_log(l, FQ_LOG_EMERG, "%s/%s: en-queue: locked.(%s) .", obj->path, obj->qname, lockfilename);
		// fq_log(l, FQ_LOG_EMERG, "%s/%s: en-queue: locked. ->We force lock off.", 
		// unlink(lockfilename);
	}
	else if( bc->en_competition > tp->lock_threshold_micro_sec ) { 
		fq_log(l, FQ_LOG_EMERG, "%s/%s: en-queue: Queuing is very competitive and slow: I/O time sec=[%ld] > threshold=[%ld]", 
			obj->path, obj->qname, bc->en_competition, tp->lock_threshold_micro_sec);
	}

	bc->de_competition = Qs[index].de_competition;
	if( bc->de_competition == 0 ) {
		char lockfilename[256];
		sprintf(lockfilename, "%s/.%s.en.flcok", obj->path, obj->qname);
		// fq_log(l, FQ_LOG_EMERG, "%s/%s: de-queue: locked(%s).", obj->path, obj->qname, lockfilename);
		// fq_log(l, FQ_LOG_EMERG, "%s/%s: de-queue: locked ->We force lock off.", obj->path, obj->qname);
		// unlink(lockfilename);
	}
	else if( bc->en_competition > tp->lock_threshold_micro_sec ) { 
		fq_log(l, FQ_LOG_EMERG, "%s/%s: de-queue: Competition to pull out of the queue is so slow: I/O time sec=[%ld] > threshold=[%ld]", 
			obj->path, obj->qname, bc->de_competition, tp->lock_threshold_micro_sec);
	}

	bc->en_sum = Qs[index].en_sum;
	bc->de_sum = Qs[index].de_sum;

	sprintf(bc->data, "%20.20s", "Ready...");

	bc->status = Qs[index].status;
	bc->en_tps = Qs[index].en_tps;

	bc->de_tps = Qs[index].de_tps;
	if(  bc->usage > 5.0  && (bc->de_tps < tp->de_tps_threshold) ) {
		fq_log(l, FQ_LOG_EMERG, "%s/%s: de-queue: slow TPS (%d) < threshold TPS(%d). Usage(%5.2f)", obj->path, obj->qname, bc->de_tps, tp->de_tps_threshold, bc->usage);

		char	hostname[HOST_NAME_LEN+1];
		gethostname(hostname, sizeof(hostname));

		alarm->hostname = strdup(hostname);
		alarm->qpath = strdup(obj->path);
		alarm->qname = strdup(obj->qname);

		char snd_msg[128];

		sprintf(snd_msg, "WARN: TPS Current deQ TPS(%d) > threshold TPS(%d) Usage:(%5.2f).", bc->de_tps, tp->de_tps_threshold, bc->usage);
		alarm->q_status_msg = strdup(snd_msg);

		bool tf;
		tf = enQ_alarm_msg(l, tp->alarm_q_obj, tp->json_template_pathfile, alarm);
		CHECK(tf);
		
	}

	bc->big = obj->on_get_big(l, obj);
	if( bc->big > 0 ) {
		fq_log(l, FQ_LOG_EMERG, "%s/%s: A message larger than the specified queue size(%d) is being made.",
			obj->path, obj->qname, bc->msglen);
	}

	bc->msglen = obj->h_obj->h->msglen;
	bc->shmq_flag = Qs[index].shmq_flag;
	bc->q_status = Qs[index].q_status;
	bc->last_en_time = Qs[index].last_en_time;
	bc->last_de_time = Qs[index].last_de_time;
	sprintf(bc->desc, "%s",  Qs[index].desc);

	bool tf;
	tf = Unchanged_for_a_period_of_time(l, bc->last_en_time, tp->unchanged_period_sec_en );
	if( tf ) {
		fq_log(l, FQ_LOG_EMERG, "%s/%s: Unchanged(en_queue) for a %ld sec.",  
			obj->path, obj->qname, tp->unchanged_period_sec_en);

		char	hostname[HOST_NAME_LEN+1];
		gethostname(hostname, sizeof(hostname));

		alarm->hostname = strdup(hostname);
		alarm->qpath = strdup(obj->path);
		alarm->qname = strdup(obj->qname);

		char snd_msg[128];

		sprintf(snd_msg, "WARN: Unchanged(en_queue) for a %ld seconds.", tp->unchanged_period_sec_en);
		alarm->q_status_msg = strdup(snd_msg);

		bool tf;
		tf = enQ_alarm_msg(l, tp->alarm_q_obj, tp->json_template_pathfile, alarm);
		CHECK(tf);
	}
	tf = Unchanged_for_a_period_of_time(l, bc->last_de_time, tp->unchanged_period_sec_de );
	if( tf ) {
		fq_log(l, FQ_LOG_EMERG, "%s/%s: Unchanged(de_queue) for a %ld sec.",  
			obj->path, obj->qname, tp->unchanged_period_sec_de);

		char	hostname[HOST_NAME_LEN+1];
		gethostname(hostname, sizeof(hostname));

		alarm->hostname = strdup(hostname);
		alarm->qpath = strdup(obj->path);
		alarm->qname = strdup(obj->qname);

		char snd_msg[128];

		sprintf(snd_msg, "WARN: Unchanged(de_queue) for a %ld seconds.", tp->unchanged_period_sec_de);
		alarm->q_status_msg = strdup(snd_msg);

		bool tf;
		tf = enQ_alarm_msg(l, tp->alarm_q_obj, tp->json_template_pathfile, alarm);
		CHECK(tf);
	}
	SAFE_FREE(alarm);

	return(0);
}
/*
** Update body contents to hashmap */
int Update_Hashmap( fq_logger_t *l,  body_contents_t *bc) {
	int i;

	if( FQ_IS_DEBUG_LEVEL(l) ) {
	printf("key=%s/%s, valules=%d: %d: %d: %5.2f: %ld: %ld: %d: %d: %d: %ld: %ld: %d: %d: %s %ld %ld.\n"
				,bc->qpath
				,bc->qname
				,bc->max_rec_cnt
				,bc->msglen
				,bc->gap
				,bc->usage
				,bc->en_competition
				,bc->de_competition
				,bc->big
				,bc->en_tps
				,bc->de_tps
				,bc->en_sum
				,bc->de_sum
				,bc->q_status
				,bc->shmq_flag
				,bc->desc
				,bc->last_en_time
				,bc->last_de_time
			);
	}

	char buf[512];
	sprintf( buf, "%d|%d|%d|%5.2f|%ld|%ld|%d|%d|%d|%ld|%ld|%d|%d|%s|%ld|%ld"
				,bc->msglen
				,bc->max_rec_cnt
				,bc->gap
				,bc->usage
				,bc->en_competition
				,bc->de_competition
				,bc->big
				,bc->en_tps
				,bc->de_tps
				,bc->en_sum
				,bc->de_sum
				,bc->q_status
				,bc->shmq_flag
				,bc->desc
				,bc->last_en_time
				,bc->last_de_time
			);

	/* Put current queue status to hash_map */
	char key[512];
	sprintf(key, "%s/%s", bc->qpath, bc->qname);
	int rc;
	rc = PutHash(NULL, hash_obj, key, buf);
	CHECK(rc==TRUE);

	return(0);
}

static void *main_thread( void *arg )
{
	main_thread_params_t	*tp = arg;
    int     rc;
	int status=0;
	int	port;
	int	fd, lockfd;
	char *path = NULL;
	char ListInfoFile[256];
	fq_logger_t *l=NULL;
	int opened_q_cnt = 0;
	unsigned char buf[65536];
	long l_seq, run_time;
	external_env_obj_t *external_env_obj = NULL;

	fq_container_t *fq_c=NULL;

	l = tp->l;

	/* here error : core dump */
	rc = load_fq_container(l, &fq_c);
	if( rc != TRUE ) {
		fq_log(l, FQ_LOG_ERROR, "load_fq_container() error.");
		goto stop;
	}

	/* For creating, Use /utl/ManageHashMap command */
	rc = OpenHashMapFiles(l, tp->hashmap_path, tp->hashmap_name, &hash_obj);
	CHECK(rc==TRUE);
	fq_log(l, FQ_LOG_INFO, "ums HashMap open ok.");


	if( FQ_IS_DEBUG_LEVEL(l) ) {
		hash_obj->h->on_print(l, hash_obj->h);
	}

	rc = open_all_file_queues_and_run_mon_thread(l, fq_c, &opened_q_cnt);
	if(rc != 0 ) {
		fq_log(l, FQ_LOG_ERROR, "open_all_file_queues_and_run_mon_thread() error.");
		goto stop;
	}

	open_external_env_obj( l, g_path, &external_env_obj);
	if( external_env_obj ) {
		external_env_obj->on_get_extenv( l, external_env_obj, "eth", g_eth_name);
		fq_log(l, FQ_LOG_EMERG, "eth='%s'.", g_eth_name);
	}
	else {
		sprintf(g_eth_name, "%s", "eth0");
	}

	pid_t   mypid = getpid();
	if( tp->fqpm_use_flag == true ) {
		bool tf;
		tf = check_duplicate_me_on_fq_framework( l, "fq_mon_svr.conf");
		CHECK(tf);
		fq_log(l, FQ_LOG_INFO, "process dup checking on framework ok.");

		tf = regist_me_on_fq_framework(l, NULL, mypid, true, 60 ); // NULL : use env=FQ_DATA_PATH
		CHECK(tf);
		fq_log(l, FQ_LOG_INFO, "process regist on framework ok.");
	}

	fq_log(l, FQ_LOG_EMERG, "fq_mon_svr main thread start.");

	while(1) {
		int i;

#if 0
		char curr_date[9];
		get_time(curr_date, 0);
		if( strcmp(g_log_open_date, curr_date) != 0 ) {
			fq_close_file_logger(&l);

			char log_pathfile[256];
			sprintf(log_pathfile, "%s/%s", tp->ums_common_log_path, tp->log_filename);
			backup_log( log_pathfile, g_log_open_date);

			int rc;
			rc = fq_open_file_logger(&l, log_pathfile, tp->i_log_level);
			CHECK(rc==TRUE);
			get_time(g_log_open_date, 0);
		}
#endif

		if( tp->fqpm_use_flag == true ) {
			touch_me_on_fq_framework( l, NULL, mypid );
		}
        status = pthread_mutex_lock (&mutex);

        if( status !=0 ) {
            fq_log(l, FQ_LOG_ERROR, "pthread_mutex_lock error.");
			break;
        }

		/* display body values */
		for(i=0; Qs[i].obj; i++) {
			body_contents_t bc;

			// rc = fill_body_contents_with_qobj(l, tp->alarm_usage_threshold, tp->unchanged_period_sec_en, tp->unchanged_period_sec_de, i, Qs[i].obj, &bc);
			rc = fill_body_contents_with_qobj(l, tp, i, Qs[i].obj, &bc);
			CHECK(rc == 0);

			rc = Update_Hashmap( l, &bc);
			CHECK( rc == 0 );

		}

        pthread_mutex_unlock (&mutex);
        usleep(tp->collect_cycle_usec); /* If you remove it, you will get core-dump */
    }

	if(fq_c) fq_container_free(&fq_c);

	close_all_file_queues(l);

	if(l) fq_close_file_logger(&l);

stop:

    pthread_exit((void *)0);
}
static bool Unchanged_for_a_period_of_time( fq_logger_t *l, time_t last_update_time, long period_sec )
{
	time_t current_bintime;
	FQ_TRACE_ENTER(l);

	if(period_sec == 0) {
		return false;
	}
	
	current_bintime = time(0);
	if( (last_update_time + period_sec) < current_bintime) {
		fq_log(l, FQ_LOG_EMERG, "last_update_time=[%ld], period_sec=[%ld], current_bintime=[%ld].", 
			last_update_time, period_sec, current_bintime);
		
		FQ_TRACE_EXIT(l);
		return true; /* expired */
	}
	else {
		FQ_TRACE_EXIT(l);
		return false;
	}
}

static bool enQ_alarm_msg(fq_logger_t *l, fqueue_obj_t *alarm_q_obj, char *json_template_pathfile, alarm_msg_items_t *alarm_msg )
{
	// make a json message for KT or LG.
	char *json_msg = NULL;
	int  json_msg_len = 0;

	cache_t *cache_short_for_gssi=NULL;
	cache_short_for_gssi = cache_new('S', "Short term cache");

	char date[9], time[7];
	get_time(date, time);
	char req_dttm[16];
	sprintf(req_dttm, "%s%s", date, time);

	
	cache_update(cache_short_for_gssi, "req_dttm", req_dttm, 0);
	cache_update(cache_short_for_gssi, "hostname", alarm_msg->hostname, 0);
	cache_update(cache_short_for_gssi, "qpath", alarm_msg->qpath, 0);
	cache_update(cache_short_for_gssi, "qname", alarm_msg->qname, 0);
	cache_update(cache_short_for_gssi, "q_status_msg", alarm_msg->q_status_msg, 0);

	char str_msglen[20];
	sprintf(str_msglen, "%ld", (long)strlen(alarm_msg->q_status_msg));
	cache_update(cache_short_for_gssi, "msglen", str_msglen, 0);

	fq_log(l, FQ_LOG_DEBUG, "host=[%s], qpath=[%s], qname=[%s] msg=[%s]. ", alarm_msg->hostname, alarm_msg->qpath, alarm_msg->qname, alarm_msg->q_status_msg);

	char  *var_data = "Gwisang|Kookmin|57|gwisang.choi@gmail.com|M";
	char	dst[8192];
	int rc;
	int file_len;
	unsigned char *json_template_string=NULL;
	
	fq_log(l, FQ_LOG_DEBUG, "json_template_pathfile: %s.",  json_template_pathfile);
	char *errmsg;
	rc = LoadFileToBuffer( json_template_pathfile, &json_template_string, &file_len,  &errmsg);
	CHECK(rc > 0);

	fq_log(l, FQ_LOG_DEBUG, "JSON Templete: '%s'", json_template_string);
	rc = gssi_proc( l, json_template_string, var_data, "", cache_short_for_gssi, '|', dst, sizeof(dst));
	CHECK(rc==0);

	fq_log(l, FQ_LOG_DEBUG, "gssi_proc() result. [%s], rc=[%d] len=[%ld]\n", dst, rc, strlen(dst));

	bool tf;
	tf  = uchar_enQ(l, dst, strlen(dst), alarm_q_obj);
	CHECK(tf);

	if( cache_short_for_gssi ) cache_free(&cache_short_for_gssi);

	SAFE_FREE(json_template_string);

	return true;
}

static bool Open_Alarm_Q( fq_logger_t *l, fq_mon_svr_conf_t *my_conf, fqueue_obj_t **alarm_q_obj)
{
	int rc;
	fqueue_obj_t *obj=NULL;

	rc =  OpenFileQueue(l, NULL, NULL, NULL, NULL, my_conf->alarm_qpath, my_conf->alarm_qname, &obj);
	if(rc==FALSE) return false;
	else {
		*alarm_q_obj = obj;
		return true;
	}
}
int alarm_eventlog(fq_logger_t *l, char *logpath, ...)
{
	char fn[256];
	char datestr[40], timestr[40];
	FILE* fp=NULL;
	va_list ap;

	get_time(datestr, timestr);
	snprintf(fn, sizeof(fn), "%s/FQ_alarm_%s.eventlog", logpath, datestr);

	fp = fopen(fn, "a");
	if(!fp) {
		fq_log(l, FQ_LOG_ERROR, "failed to open eventlog file. [%s]", fn);
		goto return_FALSE;
	}

	va_start(ap, logpath);
	fprintf(fp, "%s|%s|%s|", "alarm_enQ", datestr, timestr);

	vfprintf(fp, "%s|%s|%c|%s|%s|\n", ap); 

	if(fp) fclose(fp);
	va_end(ap);

	return(TRUE);

return_FALSE:
	if(fp) fclose(fp);
	return(FALSE);
}
static int uchar_enQ(fq_logger_t *l, unsigned char *data, size_t data_len, fqueue_obj_t *obj)
{
	int rc;
	long l_seq=0L;
    long run_time=0L;

	FQ_TRACE_ENTER(l);
	while(1) {
		rc =  obj->on_en(l, obj, EN_NORMAL_MODE, (unsigned char *)data, data_len+1, data_len, &l_seq, &run_time );
		// rc =  obj->on_en(l, obj, EN_CIRCULAR_MODE, (unsigned char *)data, data_len+1, data_len, &l_seq, &run_time );
		if( rc == EQ_FULL_RETURN_VALUE )  {
			fq_log(l, FQ_LOG_EMERG, "Queue('%s', '%s') is full.\n", obj->path, obj->qname);
			usleep(100000);
			continue;
		}
		else if( rc < 0 ) { 
			if( rc == MANAGER_STOP_RETURN_VALUE ) { // -99
				printf("Manager stop!!! rc=[%d]\n", rc);
				fq_log(l, FQ_LOG_EMERG, "[%s] queue is Manager stop.", obj->qname);
				sleep(1);
				continue;
			}
			fq_log(l, FQ_LOG_ERROR, "Queue('%s', '%s'): on_en() error.\n", obj->path, obj->qname);
			FQ_TRACE_EXIT(l);
			return(rc);
		}
		else {
			fq_log(l, FQ_LOG_INFO, "queue('%s/%s'): enqueued msg len(rc) = [%d].", obj->path, obj->qname, rc);
			break;
		}
	}
	FQ_TRACE_EXIT(l);
	return(rc);
}

static bool is_exclude_queue_name( fq_logger_t *l, file_list_obj_t *exclude_list_obj, char *qname)
{
  FILELIST *this_entry=NULL;

  FQ_TRACE_ENTER(l);
  this_entry = exclude_list_obj->head;

  while (this_entry->next && this_entry->value ) {
	if( strcmp(this_entry->value, qname) == 0 ) {
		FQ_TRACE_EXIT(l);
		return true;
	}
	this_entry = this_entry->next;
  }

  FQ_TRACE_EXIT(l);
  return false;
}
bool append_mypid_fq_pid_listfile_for_heartbeat( fq_logger_t *l, char *file )
{
	FILE *fp=NULL;

	fp = fopen(file, "a");
	if( fp == NULL) {
		fq_log(l, FQ_LOG_ERROR, "append_mypid_fq_pid_listfile('%s'). error.", file);
		return false;
	}
	fprintf(fp, "%d\n", getpid() );
	fflush(fp);
	fclose(fp);
	fq_log(l, FQ_LOG_INFO, "append_mypid_fq_pid_listfile('%s'). OK.", file);
	return true;
}

