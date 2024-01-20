/*
** fq_alarm_svr.c
** Author: gwisang.choi@gmail.com
** Suhyup UMS Project: 2023.3 ~ 2023.11
** NNwise Corperation.
**********************************************************************
*/
#include <stdio.h>
#include <stdbool.h>
#include <libgen.h>

#include "fq_common.h"
#include "fq_delimiter_list.h"

#include "fq_codemap.h"
#include "fqueue.h"
#include "fq_file_list.h"
#include "ums_common_conf.h"
#include "queue_ctl_lib.h"
#include "fq_timer.h"
#include "fq_hashobj.h"
#include "fq_tci.h"
#include "fq_cache.h"
#include "fq_conf.h"
#include "fq_gssi.h"
#include "fq_mem.h"
#include "parson.h"
#include "fq_linkedlist.h"
#include "fq_alarm_svr_conf.h"
#include "hash_manage.h"
#include "fq_monitor.h"
#include "fq_scanf.h"

typedef struct {
	char *hostname;
	char *req_dttm;
	char *qpath;
	char *qname;
	char *msg;
	int  msglen;
} alarm_msg_t;

typedef struct _channel_co_queue_t channel_co_queue_t;
struct _channel_co_queue_t {
	char channel_initial[5]; /* key: SM-K */
	fqueue_obj_t	*obj;
};

typedef struct _admin_phone_map {
	char phone_no[12];
	char name[128];
} admin_phone_map_t;

static int telecom_send_eventlog(fq_logger_t *l, char *logpath, ...);
fqueue_obj_t *find_least_gap_qobj_in_channel_co_queue_map( fq_logger_t *l, char *channel, linkedlist_t *ll );
static bool json_parsing_and_get_alarm_msg( fq_logger_t *l, JSON_Value **in_json, JSON_Object **root, unsigned char *alarm_msg, alarm_msg_t *alarm_msg_st);
bool MakeLinkedList_co_SS_initial_FQ_obj_with_mapfile(fq_logger_t *l, char *mapfile, char delimiter, linkedlist_t *ll);
static bool MakeLinkedList_admin_phone_mapfile( fq_logger_t *l, char *mapfile, char delimiter, linkedlist_t	*ll);
static bool make_telecom_JSON_by_template( fq_logger_t *l, char *telecom_json_template_file, char *phone_no, alarm_msg_t *alarm_msg_st, char **out_json, char **history_key_rtn );

int main( int ac, char **av)
{
	/* logging */
	fq_logger_t *l=NULL;
	char log_pathfile[256];

	/* ums common config */
	ums_common_conf_t *cm_conf = NULL;

	/* my_config */
	fq_alarm_svr_conf_t	*my_conf=NULL;

	/* cache short */
	cache_t *cache_short_for_gssi;

	char *errmsg=NULL;

	/* common */
	int rc;
	bool tf;

	/* dequeue objects */
	fqueue_obj_t *deq_obj=NULL; 

	hashmap_obj_t *mon_svr_hash_obj=NULL;

	/* admin code map */
	codemap_t *admin_phone_map=NULL;

	if( ac != 2 ) {
		fprintf(stderr, "Usage: $ %s [your config file] <enter>\n", av[0]);
		return 0;
	}

	/* Loading common configuration */
	if(Load_ums_common_conf(&cm_conf, &errmsg) == false) {
		fprintf(stderr, "Load_ums_common_conf() error. reason='%s'\n", errmsg);
		return(-1);
	}	
	fprintf(stdout, "ums common conf loading ok.\n");

	/* Loading my configuration */
	if(LoadConf(av[1], &my_conf, &errmsg) == false) {
		fprintf(stderr, "LoadMyConf('%s') error. reason='%s'\n", av[1], errmsg);
		return(-1);
	}
	fprintf(stdout, "ums my conf loading ok.\n");

	/* logging */
	sprintf(log_pathfile, "%s/%s", cm_conf->ums_common_log_path, my_conf->log_filename);
	rc = fq_open_file_logger(&l, log_pathfile, my_conf->i_log_level);
	CHECK(rc==TRUE);
	int mypid = getpid();
	fprintf(stdout, "This program will be logging to '%s'. pid='%d'.\n", log_pathfile, mypid);

	if( FQ_IS_ERROR_LEVEL(l) ) { // in real, We do not print to STDOUT
		fclose(stdout);
	}

	/* make a linkedlist phone map */
	linkedlist_t *admin_phone_map_ll;
	admin_phone_map_ll = linkedlist_new("admin_phone_list");
	tf = MakeLinkedList_admin_phone_mapfile( l, my_conf->admin_phone_map_filename, 0x20, admin_phone_map_ll );
	CHECK(tf);
	
	// rc = get_codeval(l, t, "02", value);
	// CHECK(rc == 0);

	char map_delimiter;
	map_delimiter = ':';
	linkedlist_t    *channel_co_queue_obj_ll = NULL;
	channel_co_queue_obj_ll = linkedlist_new("channel_co_queue_obj_list");
	CHECK(channel_co_queue_obj_ll);
	tf = MakeLinkedList_co_SS_initial_FQ_obj_with_mapfile(l, my_conf->channel_co_queue_map_file, map_delimiter, channel_co_queue_obj_ll);
	CHECK(tf);
	fq_log(l, FQ_LOG_INFO, "MakeLinkedList channel_co_initial/fq object  map ok. length=[%d]", channel_co_queue_obj_ll->length);

	/* Open alarm_history hashobj */
	hashmap_obj_t *alarm_history_hash_obj=NULL;
	rc = OpenHashMapFiles(l, my_conf->alarm_history_hash_path, my_conf->alarm_history_hash_name, &alarm_history_hash_obj);
	if(rc != TRUE) {
		fq_log(l, FQ_LOG_ERROR, "Can't open ums hashmap(path='%s', name='%s'). Please check hashmap.", 
				my_conf->alarm_history_hash_path, my_conf->alarm_history_hash_name);
		return(0);
	}
	fq_log(l, FQ_LOG_INFO, "alarm history HashMap(path='%s', name='%s') open ok.", my_conf->alarm_history_hash_path, my_conf->alarm_history_hash_name);

	/* Open Queue */
	tf = Open_takeout_Q(l, my_conf, &deq_obj);
	if(tf == false) {
		fq_log(l, FQ_LOG_ERROR, "Can't open queue(path='%s', qname='%s'. Please check queue.", my_conf->qpath, my_conf->qname);
		return(0);
	}
	fq_log(l, FQ_LOG_INFO, "ums takeout Queue(%s/%s) Open ok.", my_conf->qpath, my_conf->qname);

#if 0
	/* Open Hashmap */
	/* General purpose hashmap for UMS */
	/* For creating, Use /utl/ManageHashMap command */
	/* We do not use it now. */
	hashmap_obj_t *hash_obj=NULL;
	rc = OpenHashMapFiles(l, my_conf->hash_map_path, my_conf->ums_hash_map_name, &hash_obj);
	if(tf == false) {
		fq_log(l, FQ_LOG_ERROR, "Can't open ums hashmap(path='%s', name='%s'). Please check hashmap.", 
				my_conf->hash_map_path, my_conf->ums_hash_map_name);
		return(0);
	}
	fq_log(l, FQ_LOG_INFO, "ums HashMap(path='%s', name='%s') open ok.", my_conf->hash_map_path, my_conf->ums_hash_map_name);
#endif

	fq_log(l, FQ_LOG_EMERG, "Program(%s) start.", av[0]);

	/* It have to upgrade by control working when order is arrived at ControlQ. */
	start_timer();
	while(1) {
		cache_short_for_gssi = cache_new('S', "Short term cache");

		JSON_Value *in_json = NULL;
		JSON_Object *root = NULL;

		int rc;
		unsigned char  *alarm_msg = NULL;
		char	*unlink_filename = NULL;
		long	l_seq;
		double passed_time;

		rc = takeout_uchar_msg_from_Q(l, my_conf, deq_obj, &alarm_msg, &unlink_filename, &l_seq);
		if(rc < 0) { /* fatal error */
			fq_log(l, FQ_LOG_ERROR, "fatal error. Good bye.!");
			exit(0);
		} 
		else if( rc == 0 ) { /* queue is empty */
			passed_time = end_timer();
			if( passed_time > 5.0 ) {
				fq_log(l, FQ_LOG_INFO, "We are waiting a alarm message in queue('%s'-'%s').", deq_obj->path, deq_obj->qname);
				start_timer();
			}

			sleep(1);
			if( cache_short_for_gssi ) cache_free(&cache_short_for_gssi);
			continue;
		}
		else {
			fq_log(l, FQ_LOG_INFO, "We take out a message that fq_mon_svr sent from fq(%s/%s). msg=[%s],  rc=[%d]", deq_obj->path, deq_obj->qname,  alarm_msg, rc);

			// parsing JSON and get alarm info.
			alarm_msg_t *alarm_msg_st = NULL;
			alarm_msg_st = (alarm_msg_t *)calloc(1, sizeof(alarm_msg_t));

			tf = json_parsing_and_get_alarm_msg(l, &in_json, &root, alarm_msg, alarm_msg_st);
			if( tf == false ) {
				fq_log(l, FQ_LOG_ERROR, "json_parsing_and_get_alarm_msg() error. We throw it.");
				goto NEXT;
			}
			
			// check duplicated message in hash(key= qpath/qname value=last_send_time) .
			// if( dup ) 
				// commit_Q(l, deq_obj, unlink_filename, l_seq); // We do not sent it.
				// continue;
			//

			char alarm_history_hash_key[256];
			char *hash_value_send_time = NULL;
			sprintf(alarm_history_hash_key, "%s/%s", alarm_msg_st->qpath, alarm_msg_st->qname);
			rc = GetHash(l, alarm_history_hash_obj, alarm_history_hash_key, &hash_value_send_time);
			fq_log(l, FQ_LOG_DEBUG, "GetHash() rc=[%d]\n", rc);

			if( rc == TRUE ) { // Already sent, There is a sending history same queue.
				fq_log(l, FQ_LOG_DEBUG, "hash_value_send_time=[%s]", hash_value_send_time);
				time_t send_time_bin = bin_time(hash_value_send_time);
				fq_log(l, FQ_LOG_DEBUG, "send_time_bin=[%ld]", send_time_bin);
				time_t cur_bin_time = time(0);
				fq_log(l, FQ_LOG_DEBUG, "cur_bin_time=[%ld]", cur_bin_time);

				time_t diff = cur_bin_time - send_time_bin;
				fq_log(l, FQ_LOG_DEBUG, "diff=[%ld]", diff);
			
				if( diff <  my_conf->dup_prevent_sec ) {
					fq_log(l, FQ_LOG_INFO, "Since the last time it was sent was only [%d] seconds, sending is skipped.", diff);
					SAFE_FREE(hash_value_send_time);
					goto NEXT;
				}
			}
			SAFE_FREE(hash_value_send_time);

			// 1st: history key 
			// 2nd: admin phone_number
			// 3th: en_Queue
			// 4rd: Telecom initial
			// 5th: Message
			fqueue_obj_t *enq_obj = NULL;
			while(true) {
				enq_obj = find_least_gap_qobj_in_channel_co_queue_map(l, "SS", channel_co_queue_obj_ll);
				if( enq_obj == NULL ) {
					fq_log(l, FQ_LOG_ERROR, "We can't find a best SMS Telecom queue object. After 1 second retry.");
					// cancel_Q( l, deq_obj, unlink_filename, l_seq);
					sleep(1);
					continue;
				}
				fq_log(l, FQ_LOG_INFO, "We will put a SMS message to [%s/%s].", enq_obj->path, enq_obj->qname);
				break; // find a queue.
			}

			// send SMS alarm to admin
			//
			fq_log(l, FQ_LOG_INFO, "We send SMS alarm to admin(%d-persons) list.", admin_phone_map_ll->length );
			ll_node_t *node_p = NULL;
			int t_no;
			for( node_p=admin_phone_map_ll->head, t_no=0; (node_p != NULL); (node_p=node_p->next, t_no++) ) {
				admin_phone_map_t *tmp;
				tmp = (admin_phone_map_t *) node_p->value;
				fq_log(l, FQ_LOG_INFO, "We send SMS to Admin phone no:: [%s] name: [%s]\n",
					tmp->phone_no,
					tmp->name);

				bool tf;
				char *out_json = NULL;
				char *history_key = NULL;
				tf =  make_telecom_JSON_by_template(l, my_conf->telecom_json_template_filename, tmp->phone_no, alarm_msg_st, &out_json, &history_key );

				if(tf == false) {
					fq_log(l, FQ_LOG_ERROR, "json_parsing_and_get_alarm_msg() error. We throw it.");
					goto NEXT;
				}

				fq_log(l, FQ_LOG_INFO, "We made a new json message by [%s] successfully. history_key = '%s'", 	
					my_conf->telecom_json_template_filename, history_key);

				int rc;
				rc = uchar_enQ(l, out_json, strlen(out_json), enq_obj);
				if( rc < 0 ) {
					fq_log(l, FQ_LOG_ERROR, "uchar_enQ('%s', '%s') error. We throw it.", enq_obj->path, enq_obj->qname);
					goto NEXT;
				}
				fq_log(l, FQ_LOG_INFO, "We put a SMS to [%s][%s] successfully.", enq_obj->path, enq_obj->qname);

				SAFE_FREE(out_json);

				// For dup checking, Regist queue info to hashmap.
				char hash_key[255];
				char hash_value_send_time[32];
				sprintf(hash_key, "%s/%s", alarm_msg_st->qpath, alarm_msg_st->qname);
		
				char *p=NULL;
				p=asc_time(time(0));
				fq_log(l, FQ_LOG_DEBUG, "asc_time = '%s'", p);
				
				rc = PutHash(l, alarm_history_hash_obj, hash_key, p);
				if( rc != TRUE ) {	
					fq_log(l, FQ_LOG_ERROR, "PutHash('%s', '%s') error.", hash_key, p);
					goto NEXT;
				}
				fq_log(l, FQ_LOG_INFO, "PutHash('%s', '%s') success.", hash_key, p);
				SAFE_FREE(p);

				// send eventlog
				telecom_send_eventlog(l, cm_conf->ums_common_log_path, alarm_msg_st->hostname, history_key, 
								tmp->phone_no, alarm_msg_st->qname,  alarm_msg_st->msg);


			}
		}
NEXT:

		commit_Q(l, deq_obj, unlink_filename, l_seq);

		cache_free(&cache_short_for_gssi);

		FREE(alarm_msg);

		if( in_json ) json_value_free(in_json);

		/**********************************************************************************************************/

	} /* while end */

	if( alarm_history_hash_obj )  CloseHashMapFiles(l, &alarm_history_hash_obj);

	linkedlist_free( &admin_phone_map_ll);
	fq_close_file_logger(&l);

	// close_ratio_distributor(l, &obj);

	return 0;
}

#if 0
fq_status_t get_fq_status( fq_logger_t *l, fqueue_obj_t *obj, hashmap_obj_t *mon_svr_hash_obj, float usage_threshold, int tps_threshold)
{
	char hash_search_key[128];
	char *hash_value = NULL;

	HashQueueInfo_t HashInfo;

	sprintf(hash_search_key, "%s/%s", obj->path, obj->qname);

	int rc;
	rc = GetHash(l, mon_svr_hash_obj, hash_search_key, &hash_value);
	if( rc == FALSE ) {
		return(FQ_GET_FAILED);
	}
	else {
		int rc;
		int t_no;
		delimiter_list_obj_t    *obj=NULL;
		delimiter_list_t *this_entry=NULL;

		rc = open_delimiter_list(l, &obj, hash_value, '|');
		CHECK(rc == TRUE);

		this_entry = obj->head;

		for( t_no=0;  (this_entry->next && this_entry->value); t_no++ ) {
			// printf("[%s], ", this_entry->value);

			switch(t_no) {
				case 0:
					HashInfo.msglen = atoi(this_entry->value);
					break;
				case 1:
					HashInfo.max_rec_cnt = atoi(this_entry->value);
					break;
				case 2:
					HashInfo.gap = atoi(this_entry->value);
					break;
				case 3:
					HashInfo.usage = atof(this_entry->value);
					break;
				case 4:
					HashInfo.en_competition = atol(this_entry->value);
					break;
				case 5:
					HashInfo.de_competition = atol(this_entry->value);
					break;
				case 6:
					HashInfo.big = atoi(this_entry->value);
					break;
				case 7:
					HashInfo.en_tps = atoi(this_entry->value);
					break;
				case 8:
					HashInfo.de_tps = atoi(this_entry->value);
					break;
				case 9:
					HashInfo.en_sum = atol(this_entry->value);
					break;
				case 10:
					HashInfo.de_sum = atol(this_entry->value);
					break;
				case 11:
					HashInfo.status = atoi(this_entry->value);
					break;
				case 12:
					HashInfo.shmq_flag = atoi(this_entry->value);
					break;
				case 13:
					sprintf(HashInfo.desc, "%s", this_entry->value);
					break;
				case 14:
					HashInfo.last_en_time = atol(this_entry->value);
					break;
				case 15:
					HashInfo.last_de_time = atol(this_entry->value);
					break;
				default:
					break;
			}
			this_entry = this_entry->next;
		} // for end.
		close_delimiter_list(l, &obj);

	}
	SAFE_FREE(hash_value);

	if( obj->stop_signal == 1 || obj->h_obj->h->status != QUEUE_ENABLE ) {
		return FQ_MANAGER_STOP;
	}
	else if( obj->on_get_usage(l, obj) >= usage_threshold ) {
		return FQ_USAGE_OVER;
	}
	else if( obj->on_get_usage(l, obj) > 10.0 &&
			(HashInfo.en_tps < tps_threshold || HashInfo.de_tps < tps_threshold ) ) {

		return FQ_SLOW;
	}
	return FQ_NORMAL;
}
#endif

bool Store_Malformed_Requeus_to_file(fq_logger_t *l, unsigned char *ums_msg, char *pathfile)
{
	FILE *fp=NULL;

	FQ_TRACE_ENTER(l);
	if( (fp = fopen( pathfile, "a+")) == NULL) {
		fq_log(l, FQ_LOG_ERROR, "File(%s) open error.");
		FQ_TRACE_EXIT(l);
		return false;
	}
	fprintf( fp, "%s\n", ums_msg);
	fflush(fp);

	fclose(fp);

	FQ_TRACE_EXIT(l);

	return true;
}

#if 0
bool request_error_JSON_return_enQ(fq_logger_t *l, char *error_reason, unsigned_char *ums_msg, int ums_msg_len, fqueue_obj_t *return_q_obj)
{
	FQ_TRACE_ENTER(l);

	SON_Value *json_root_value = json_value_init_object();
	CHECK(json_root_value);
	JSON_Object *json_root_object = json_value_get_object(json_root_value);
	CHECK(json_root_object);

	char date[9];, time[7];
	get_time(date, time);

	json_object_set_string(json_root_object, "ERROR_REASON", error_reason);
	json_object_set_string(json_root_object, "DATE", date);
	json_object_set_string(json_root_object, "TIME", time);
	json_object_set_string(json_root_object, "ORG_MSG", ums_msg);

	int rc;
	rc = uchar_enQ(l, ums_msg, ums_msg_len, return_q_obj);
	CHECK( rc >= 0);

	json_value_free(json_root_value); 
	return true;
}
#endif

static int telecom_send_eventlog(fq_logger_t *l, char *logpath, ...)
{
	char fn[256];
	char datestr[40], timestr[40];
	FILE* fp=NULL;
	va_list ap;

	get_time(datestr, timestr);
	snprintf(fn, sizeof(fn), "%s/telecom_alarm_send_%s.eventlog", logpath, datestr);

	fp = fopen(fn, "a");
	if(!fp) {
		fq_log(l, FQ_LOG_ERROR, "failed to open eventlog file. [%s]", fn);
		goto return_FALSE;
	}

	va_start(ap, logpath);
	fprintf(fp, "%s|%s|%s|", "Telecom_enQ", datestr, timestr);

	// 0-th: hostname
	// 1st: history key 
	// 2nd: admin phone_number
	// 3th: en_Queue name
	// 4th: Message
	vfprintf(fp, "%s|%s|%s|%s|%s|\n", ap); 

	if(fp) fclose(fp);
	va_end(ap);

	return(TRUE);

return_FALSE:
	if(fp) fclose(fp);
	return(FALSE);
}

fqueue_obj_t *find_least_gap_qobj_in_channel_co_queue_map( fq_logger_t *l, char *channel, linkedlist_t *ll )
{
	ll_node_t *p;
	fqueue_obj_t *least_fobj = NULL;

	for(p=ll->head; p!=NULL; p=p->next) {
		channel_co_queue_t *tmp;

		size_t   value_sz;
		tmp = (channel_co_queue_t *) p->value;
		value_sz = p->value_sz;

		if(strncmp(channel, tmp->channel_initial, 2) != 0 ) continue;

		if( !least_fobj ) {
			least_fobj = tmp->obj;
		}
		else {
			if( least_fobj->on_get_usage(l, least_fobj) > tmp->obj->on_get_usage(l, tmp->obj) ) {
				least_fobj = tmp->obj;
			}
		}
	}
	fq_log(l , FQ_LOG_DEBUG, "Selected least qobj:  %s/%s,", 
		least_fobj->path, least_fobj->qname, least_fobj->on_get_usage(l, least_fobj));

	return (least_fobj);
}

static bool json_parsing_and_get_alarm_msg( fq_logger_t *l, JSON_Value **in_json, JSON_Object **root, unsigned char *alarm_msg, alarm_msg_t *alarm_msg_st)
{

	FQ_TRACE_ENTER(l);

	*in_json = json_parse_string(alarm_msg);
	if( !*in_json ) {
		fprintf(stderr, "illegal json format.\n");
		fq_log(l, FQ_LOG_ERROR, "illegal json format. We throw it.\n");
		FQ_TRACE_EXIT(l);
		return false;
	}
	*root = json_value_get_object(*in_json); 
	if( !*root ) {
		fprintf(stderr, "illegal json format.\n");
		fq_log(l, FQ_LOG_ERROR, "illegal json format. We throw it.\n");
		FQ_TRACE_EXIT(l);
		return false;
	}

	alarm_msg_st->hostname = strdup(json_object_get_string(*root, "HOSTNAME"));
	alarm_msg_st->req_dttm = strdup(json_object_get_string(*root, "REQ_DTTM"));
	alarm_msg_st->qpath = strdup(json_object_get_string(*root, "QPATH"));
	alarm_msg_st->qname = strdup(json_object_get_string(*root, "QNAME"));
	alarm_msg_st->msg = strdup(json_object_get_string(*root, "MESSAGE"));
	alarm_msg_st->msglen = atoi(json_object_get_string(*root, "MSGLEN"));

	fq_log( l, FQ_LOG_INFO, "HOSTNAME =>%s.", alarm_msg_st->hostname);
	fq_log( l, FQ_LOG_INFO, "REQ_DTTM =>%s.", alarm_msg_st->req_dttm);
	fq_log( l, FQ_LOG_INFO, "QPATH =>%s.", alarm_msg_st->qpath);
	fq_log( l, FQ_LOG_INFO, "QNAME =>%s.", alarm_msg_st->qname);
	fq_log( l, FQ_LOG_INFO, "MSGLEN =>%d.", alarm_msg_st->msglen);
	
	FQ_TRACE_EXIT(l);

	return true;
}

bool MakeLinkedList_co_SS_initial_FQ_obj_with_mapfile(fq_logger_t *l, char *mapfile, char delimiter, linkedlist_t *ll)
{
	file_list_obj_t *filelist_obj=NULL;
	FILELIST *this_entry=NULL;
	int rc;
	int t_no;

	FQ_TRACE_ENTER(l);

	rc = open_file_list(l, &filelist_obj, mapfile);
	CHECK( rc == TRUE );

	// filelist_obj->on_print(filelist_obj);

	this_entry = filelist_obj->head;
	for( t_no=0;  (this_entry->next && this_entry->value); t_no++ ) {
		// printf("this_entry->value=[%s]\n", this_entry->value);

		bool tf;
		char channel_co_initial[5];
		char q_path_qname[255];
		tf = get_LR_value(this_entry->value, delimiter, channel_co_initial, q_path_qname);
		// printf("channel_co_initial='%s', q_path_qname='%s'.\n", channel_co_initial, q_path_qname);

		if( strncmp(channel_co_initial, "SS", 2) != 0 ) {
			this_entry = this_entry->next;
			continue;
		}

		char *ts1 = strdup(q_path_qname);
		char *ts2 = strdup(q_path_qname);
		char *qpath=dirname(ts1);
		char *qname=basename(ts2);
		
		fq_log(l, FQ_LOG_INFO, "SS QUEUE: key=[%s] qpath='%s', qname='%s'.", channel_co_initial, qpath, qname);

		channel_co_queue_t *tmp=NULL;
		tmp = (channel_co_queue_t *)calloc(1, sizeof(channel_co_queue_t));
		sprintf(tmp->channel_initial, "%s", channel_co_initial);
		rc =  OpenFileQueue(l, NULL, NULL, NULL, NULL, qpath, qname, &tmp->obj);
		CHECK(rc == TRUE);

		ll_node_t *ll_node=NULL;
		ll_node = linkedlist_put(ll, channel_co_initial, (void *)tmp, sizeof(char)+sizeof(fqueue_obj_t));
		CHECK(ll_node);

		if(ts1) free(ts1);
		if(ts2) free(ts2);

		this_entry = this_entry->next;
	}
	if( filelist_obj ) close_file_list(l, &filelist_obj);

	FQ_TRACE_EXIT(l);

	return true;
}
static bool MakeLinkedList_admin_phone_mapfile( fq_logger_t *l, char *mapfile, char delimiter, linkedlist_t	*ll)
{
	file_list_obj_t *filelist_obj=NULL;
	FILELIST *this_entry=NULL;
	int rc;
	int t_no;

	FQ_TRACE_ENTER(l);


	fq_log(l, FQ_LOG_DEBUG, "mapfile='%s'" , mapfile);

	rc = open_file_list(l, &filelist_obj, mapfile);
	CHECK( rc == TRUE );

	printf("count = [%d]\n", filelist_obj->count);

	// filelist_obj->on_print(filelist_obj);

	this_entry = filelist_obj->head;
	for( t_no=0;  (this_entry->next && this_entry->value); t_no++ ) {
		char *errmsg = NULL;
		printf("this_entry->value=[%s]\n", this_entry->value);

		char	phone_no[12]; // SEND, RECV
		char	name[128]; // deQ

		int cnt;

		cnt = fq_sscanf(delimiter, this_entry->value, "%s%s", phone_no, name);

		CHECK(cnt == 2);

		fq_log(l, FQ_LOG_INFO, "phone_no='%s', name='%s'.", phone_no, name);

		admin_phone_map_t *tmp = NULL;
		tmp = (admin_phone_map_t *)calloc(1, sizeof(admin_phone_map_t));

		char	key[11];
		memset(key, 0x00, sizeof(key));
		sprintf(key, "%010d", t_no);

		sprintf(tmp->phone_no, "%s", phone_no);
		sprintf(tmp->name, "%s", name);

		ll_node_t *ll_node=NULL;
		ll_node = linkedlist_put(ll, key, (void *)tmp, sizeof(char)+sizeof(admin_phone_map_t));
		CHECK(ll_node);

		this_entry = this_entry->next;
	}

	if( filelist_obj ) close_file_list(l, &filelist_obj);
	
	fq_log(l, FQ_LOG_INFO, "ll->length is [%d]", ll->length);

	FQ_TRACE_EXIT(l);
	return true;
}

static bool make_telecom_JSON_by_template( fq_logger_t *l, char *telecom_json_template_file, char *phone_no, alarm_msg_t *alarm_msg_st, char **out_json , char **history_key_rtn)
{
	FQ_TRACE_ENTER(l);

	int file_len;
	unsigned char *json_template_string=NULL;
	int rc;	
	char *errmsg = NULL;
	rc = LoadFileToBuffer( telecom_json_template_file, &json_template_string, &file_len,  &errmsg);
	if( rc <= 0 ) {
		fq_log(l, FQ_LOG_ERROR, "Failed to loading telecom_json_template_file=[%s]. reason=[%s]", 
			telecom_json_template_file, errmsg);
		FQ_TRACE_EXIT(l);
		SAFE_FREE(errmsg);
		return false;
	}

	fq_log(l, FQ_LOG_DEBUG, "JSON Templete: '%s'.", json_template_string);

	pid_t	mypid = getpid();

	cache_t *cache_short_for_gssi;
	cache_short_for_gssi = cache_new('S', "Short term cache");

	char date[9], time[7];
	get_time(date, time);
	char send_time[16];
	sprintf(send_time, "%s%s", date, time);

	cache_update(cache_short_for_gssi, "senddate", send_time, 0);

	char history_key[21];
	get_seed_ratio_rand_str( history_key, 20, "0123456789");
	cache_update(cache_short_for_gssi, "history_key", history_key, 0);
	history_key[0] = 'E';

	*history_key_rtn = strdup(history_key);

	char rand_telno_11[12];

	cache_update(cache_short_for_gssi, "rcv_phn_no", phone_no, 0);

	char SMS_send_msg[128];
	sprintf(SMS_send_msg, "UMS ERR: [%s][%s]:%s", alarm_msg_st->qpath, alarm_msg_st->qname, alarm_msg_st->msg);
	fq_log(l, FQ_LOG_DEBUG, "send msg length: %d.", strlen(SMS_send_msg));
	cache_update(cache_short_for_gssi, "sms_message", SMS_send_msg, 0);
	
	char  *var_data = "msglen|";
	char	dst[8192];

	rc = gssi_proc( l, json_template_string, var_data, "", cache_short_for_gssi, '|', dst, sizeof(dst));
	if( rc != 0 ) {
		fq_log(l, FQ_LOG_ERROR, "gssi_proc() error.");
		FQ_TRACE_EXIT(l);
		return false;
	}

	fq_log(l, FQ_LOG_DEBUG, "gssi_proc() result. [%s], rc=[%d] len=[%ld].", dst, rc, strlen(dst));

	*out_json = strdup(dst);

	SAFE_FREE(errmsg);
	FQ_TRACE_EXIT(l);

	return true;
}
