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
#include "fq_manage.h"
#include "parson.h"
#include "fq_external_env.h"
// #include "monitor.h"
#include "fq_hashobj.h"
#include "fq_pmon_svr_conf.h"
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

typedef struct _alarm_msg_items {
	char *hostname;
	char *qpath;
	char *qname;
	char *q_status_msg;
} alarm_msg_items_t;

file_list_obj_t *exclude_list_obj=NULL;

int Sure_GetHash( fq_logger_t *l, hashmap_obj_t *hash_obj, char *key, char **value);
static bool is_expired_time( time_t stamp, int threshold_sec);
static int uchar_enQ(fq_logger_t *l, unsigned char *data, size_t data_len, fqueue_obj_t *obj);
static bool Open_Alarm_Q( fq_logger_t *l, fq_pmon_svr_conf_t *my_conf, fqueue_obj_t **ararm_q_obj);
static bool enQ_alarm_msg(fq_logger_t *l, fqueue_obj_t *alarm_q_obj, char *json_template_pathfile, alarm_msg_items_t *alarm_items);
static bool Unchanged_for_a_period_of_time( fq_logger_t *l, time_t last_update_time, long period_sec );
static int alarm_eventlog(fq_logger_t *l, char *logpath, ...);

char *g_path = NULL;
char fq_data_home[16+1];

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
	fq_pmon_svr_conf_t   *my_conf=NULL;

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


	/* For creating, Use /utl/ManageHashMap command */

	hashmap_obj_t *hash_obj=NULL;
	rc = OpenHashMapFiles(l, my_conf->hashmap_path, my_conf->hashmap_name, &hash_obj);
	CHECK(rc==TRUE);
	fq_log(l, FQ_LOG_INFO, "ums HashMap open ok.");

	if( FQ_IS_DEBUG_LEVEL(l) ) {
		hash_obj->h->on_print(l, hash_obj->h);
	}

	fqueue_obj_t *alarm_q_obj = NULL;
	bool tf;
	tf = Open_Alarm_Q( l, my_conf, &alarm_q_obj);
	CHECK(tf);

	fq_log(l, FQ_LOG_INFO, "%s started. version=[%s %s]", av[0], __DATE__, __TIME__);

	while(1) {
		file_list_obj_t *file_list_obj=NULL;

		rc = open_file_list(l, &file_list_obj, my_conf->fq_pid_list_file);
		if( rc != TRUE ) {
			fq_log(l, FQ_LOG_ERROR, "open_file_list('%s') error.", my_conf->fq_pid_list_file);
			goto stop;
		}
		fq_log(l, FQ_LOG_DEBUG, "Count of list is [%d]\n", file_list_obj->count);

		FILELIST *this_entry=NULL;
		this_entry = file_list_obj->head;

		while (this_entry->next && this_entry->value ) {
			// printf("list value=[%s]\n",  this_entry->value);
			char* str_bintime=NULL;
			rc = Sure_GetHash(l, hash_obj, this_entry->value, &str_bintime);
			if( rc == TRUE ) { // found
				// printf("bin_time=[%s]\n", str_bintime);
				// rc = DeleteHash( l, hash_obj, this_entry->value);
				// CHECK(rc == TRUE);
				time_t bin_tm = atol(str_bintime);
				tf = is_expired_time(bin_tm, my_conf->block_threshold_sec);
				pid_t pid = atoi(this_entry->value);
				if( tf == true ) {
					if( is_alive_pid_in_general(pid) == 1 ) {
						fq_log(l, FQ_LOG_EMERG, "Found block process. I will kill it.(pid='%s').", this_entry->value);
						kill(pid, 9);
					}
					else {
						fq_log(l, FQ_LOG_EMERG, "Already Dead process. Please delete it[pid=%s] in file=[%s].", 
							this_entry->value, my_conf->fq_pid_list_file);
					}
				}
				else {
					fq_log(l, FQ_LOG_DEBUG, "pid='%s' is normal status.", this_entry->value);
				}
				SAFE_FREE(str_bintime);
			}
			else {
				fq_log(l, FQ_LOG_DEBUG, "There is no [%s] in hash.\n",   this_entry->value);
			}
			this_entry = this_entry->next;
		}
		close_file_list(l, &file_list_obj);

		sleep(1);
	}

stop:
	fq_log(l, FQ_LOG_INFO, "%s stopped. version=[%s %s]", av[0], __DATE__, __TIME__);

	CloseFileQueue(l, &alarm_q_obj);
	fq_close_file_logger(&l);

    return(0);
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

static bool Open_Alarm_Q( fq_logger_t *l, fq_pmon_svr_conf_t *my_conf, fqueue_obj_t **alarm_q_obj)
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

static bool is_expired_time( time_t stamp, int threshold_sec)
{
	time_t current_bintime;

	current_bintime = time(0);
	if( (stamp + threshold_sec) < current_bintime) {
		// printf("bin=[%ld], threahold=[%d], curr=[%ld]\n", stamp, threshold_sec, current_bintime);
		return(true);
	}
	else {
		return(false);
	}
}
int Sure_GetHash( fq_logger_t *l, hashmap_obj_t *hash_obj, char *key, char **value)
{
	int rc;
	char *out=NULL;
	
	FQ_TRACE_ENTER(l);

reget:
	rc = GetHash( l, hash_obj, key, &out);
	if( rc == TRUE) {
		if( out[0] == 0x00) {
			SAFE_FREE(out);	
			usleep(100000);
			goto reget;
		}
		*value = strdup(out);
	}
	else {
		return FALSE;
	}

	SAFE_FREE(out);
	FQ_TRACE_EXIT(l);

	return(TRUE);
}
