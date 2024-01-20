/*
** input: 수집서버가 넣어 놓은 발송메시지를 꺼내어
** 통신사 분배비율에 따라 분배하여 해당 큐에 넣는다.
** ratio_dist_main.c
** Author: gwisang.choi@gmail.com
** Suhyup UMS Project: 2023.3 ~ 2023.11
** NNwise Corperation.
**********************************************************************
** bug fix and source changing history.
** 2023/07/16: memory leak( size 31 )
	There is no below syntax before continue in while().
		if( cache_short_for_gssi ) cache_free(&cache_short_for_gssi);
	-> 15165
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
#include "my_linkedlist.h"
#include "fq_linkedlist.h"
#include "fq_ratio_distribute.h"

#include "main_subfunc.h"
#include "ratio_dist_conf.h"
#include "hash_manage.h"

#include "fq_monitor.h"

typedef enum { FQ_NORMAL=0, FQ_MANAGER_STOP=1, FQ_USAGE_OVER=2, FQ_SLOW=3, FQ_GET_FAILED=4 } fq_status_t;
fq_status_t get_fq_status( fq_logger_t *l, fqueue_obj_t *obj, hashmap_obj_t *mon_svr_hash_obj, float usage_threshold, int tps_threshold);
bool Store_Malformed_Requeus_to_file(fq_logger_t *l, unsigned char *ums_msg, char *Filename);

static int dist_eventlog(fq_logger_t *l, char *logpath, ...);
bool append_mypid_fq_pid_listfile( fq_logger_t *l, char *file );

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

int main( int ac, char **av)
{
	FILELIST	*this_entry = NULL;

	/* cache short */
	cache_t *cache_short_for_gssi;

	/* touch timer */
	double passed_time;

	/* ums common config */
	ums_common_conf_t *cm_conf = NULL;

	/* my_config */
	ratio_dist_conf_t	*my_conf=NULL;
	char *errmsg=NULL;

	/* logging */
	fq_logger_t *l=NULL;
	char log_pathfile[256];

	/* common */
	int rc;
	bool tf;

	/* dequeue objects */
	fqueue_obj_t *deq_obj=NULL; 
	fqueue_obj_t *ctrl_obj=NULL;
	fqueue_obj_t *del_hash_q_obj=NULL;
	fqueue_obj_t *return_q_obj=NULL;

	hashmap_obj_t *seq_check_hash_obj=NULL;
	hashmap_obj_t *mon_svr_hash_obj=NULL;

	char seq_check_id[SEQ_CHECK_ID_MAX_LEN+1];

	if( ac != 2 ) {
		fprintf(stderr, "Usage: $ %s [your config file] <enter>\n", av[0]);
		return 0;
	}

	fprintf(stdout, "\n\nProcess started.\n");

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
	fprintf(stdout, "log file: '%s'\n", log_pathfile);
	

	char log_open_date[9];
	get_time(log_open_date, NULL);


	if( FQ_IS_ERROR_LEVEL(l) ) { // in real, We do not print to STDOUT
		fclose(stdout);
	}


	append_mypid_fq_pid_listfile(l, my_conf->fq_pid_list_file);

	/* Open Queues */
	tf = Open_takeout_Q(l, my_conf, &deq_obj);
	if(tf == false) {
		fq_log(l, FQ_LOG_ERROR, "Can't open queue(path='%s', qname='%s'. Please check queue.", my_conf->qpath, my_conf->qname);
		return(0);
	}
	fq_log(l, FQ_LOG_INFO, "ums takeout Queue(%s/%s) Open ok.", my_conf->qpath, my_conf->qname);

	tf = Open_control_Q(l, my_conf, &ctrl_obj);
	if(tf == false) {
		fq_log(l, FQ_LOG_ERROR, "Can't open control queue(path='%s', qname='%s'). Please check queue.", 
				my_conf->ctrl_qpath, my_conf->ctrl_qname);
		return(0);
	}
	fq_log(l, FQ_LOG_INFO, "ums control Queue(%s/%s) Open ok.", my_conf->ctrl_qpath, my_conf->ctrl_qname);

	fq_log(l, FQ_LOG_INFO, " open del_hash_queue(path='%s', qname='%s'. Please check queue.", my_conf->del_hash_q_path, my_conf->del_hash_q_name);
	tf = Open_del_hash_Q( l, my_conf, &del_hash_q_obj);
	if(tf == false) {
		fq_log(l, FQ_LOG_ERROR, "Can't open del_hash_queue(path='%s', qname='%s'. Please check queue.", my_conf->del_hash_q_path, my_conf->del_hash_q_name);
		return(0);
	}
	fq_log(l, FQ_LOG_INFO, "del hash Put Queue(%s/%s) Open ok.", my_conf->del_hash_q_path, my_conf->del_hash_q_name);

	tf = Open_return_Q( l, my_conf, &return_q_obj);
	if(tf == false) {
		fq_log(l, FQ_LOG_ERROR, "Can't open return_queue(path='%s', qname='%s'. Please check queue.", 
				my_conf->return_q_path, my_conf->return_q_name);
		return(0);
	}
	fq_log(l, FQ_LOG_INFO, "return Put Queue(%s/%s) Open ok.", my_conf->return_q_path, my_conf->return_q_name);

	/* Open Hashmap */
	/* General purpose hashmap for UMS */
	/* For creating, Use /utl/ManageHashMap command */
	/* We do not use it now. */
	hashmap_obj_t *hash_obj=NULL;
	rc = OpenHashMapFiles(l, my_conf->hash_map_path, my_conf->ums_hash_map_name, &hash_obj);
	if(rc == FALSE) {
		fq_log(l, FQ_LOG_ERROR, "Can't open ums hashmap(path='%s', name='%s'). Please check hashmap.", 
				my_conf->hash_map_path, my_conf->ums_hash_map_name);
		return(0);
	}
	fq_log(l, FQ_LOG_INFO, "ums HashMap(path='%s', name='%s') open ok.", my_conf->hash_map_path, my_conf->ums_hash_map_name);

	hashmap_obj_t *heartbeat_hash_obj=NULL;
	rc = OpenHashMapFiles(l, my_conf->heartbeat_hash_map_path, my_conf->heartbeat_hash_map_name, &heartbeat_hash_obj);
	if(rc == FALSE) {
		fq_log(l, FQ_LOG_ERROR, "Can't open ums hashmap(path='%s', name='%s'). Please check hashmap.", 
				my_conf->heartbeat_hash_map_path, my_conf->heartbeat_hash_map_name);
		return(0);
	}
	fq_log(l, FQ_LOG_INFO, "ums HashMap(path='%s', name='%s') open ok.", my_conf->heartbeat_hash_map_path, my_conf->heartbeat_hash_map_name);

	/*
	** We update channel_co_ratio to hashmap for webgate
	** webgate references this value.
	*/
	tf = init_Hash_channel_co_ratio_obj_with_mapfile(l, my_conf->channel_co_ratio_string_map_file, '|', hash_obj);
	CHECK(tf);
	fq_log(l, FQ_LOG_INFO, "Successfully, We update channel_co_retio of '%s'  to ums HashMap.", my_conf->channel_co_ratio_string_map_file);


	/* For creating, Use /utl/ManageHashMap command */
	/* Hash map for sequence gurarantee */
	rc = OpenHashMapFiles(l, my_conf->hash_map_path, my_conf->history_hash_map_name, &seq_check_hash_obj);
	CHECK(rc==TRUE);
	fq_log(l, FQ_LOG_INFO, "ums seq check HashMap(%s, %s) open ok.", my_conf->hash_map_path, my_conf->history_hash_map_name);

	/* fq_mon_svr hashmap open for checking queue status */
	rc = OpenHashMapFiles(l, my_conf->fq_mon_svr_hash_path, my_conf->fq_mon_svr_hash_name, &mon_svr_hash_obj);
	CHECK(rc==TRUE);
	fq_log(l, FQ_LOG_INFO, "fq_mon_svr hashmap('%s', '%s') open ok.", my_conf->fq_mon_svr_hash_path, my_conf->fq_mon_svr_hash_name, &mon_svr_hash_obj);

	/* We clean history hashmap before start */
	seq_check_hash_obj->on_clean_table(l, seq_check_hash_obj);
	fq_log(l, FQ_LOG_INFO, "Clean Hashmap for sequence checking.");
	// rc = hash_manage_one(l, "/home/ums/fq/hash", "history", HASH_CLEAN);
	// CHECK(rc==TRUE);

	/* Make all linkedlist */
	in_params_map_list_t in;
	out_params_map_list_t ctx;
	tf = Load_map_and_linkedlist( l, my_conf, &in, &ctx);
	if( tf == false ) {
		fq_log(l, FQ_LOG_ERROR, "Load_map_and_linkedlist() error.");
		return 0;
	}
	fq_log(l, FQ_LOG_INFO, "Loading map and Making linkedlist ok.");


	pid_t	mypid = getpid();

	/* fqpm: File Queue Process Manager */
	if( my_conf->fqpm_use_flag == true ) {
		tf = check_duplicate_me_on_fq_framework( l, av[1]);
		CHECK(tf);
		fq_log(l, FQ_LOG_INFO, "process dup checking on framework ok.");

		tf = regist_me_on_fq_framework(l, NULL, mypid, true, 60 ); // NULL : use env=FQ_DATA_PATH
		CHECK(tf);
		fq_log(l, FQ_LOG_INFO, "process regist on framework ok.");
	}
	fprintf(stdout, "Gateway Protocol is [%s].\n", my_conf->ums_gw_protocol);
	fprintf(stdout, "This program will be logging to '%s'. pid='%d'.\n", log_pathfile, mypid);

	fq_log(l, FQ_LOG_EMERG, "- UMS Distributor server start.(ver='%s')(fq ver='%s')", __DATE__, __TIME__, FQUEUE_LIB_VERSION);
	fq_log(l, FQ_LOG_EMERG, "- Program(%s) start.(version= %s, %s", av[0], __DATE__, __TIME__);
	fq_log(l, FQ_LOG_EMERG, "- Current UMS protocol(config): %s.", my_conf->ums_gw_protocol );

	/* It have to upgrade by control working when order is arrived at ControlQ. */
	start_timer();
	while(1) {
		char *svc_code = NULL;
		char *channel=NULL;

		char curr_date[9];

		tf = heartbeat_ums(l, heartbeat_hash_obj, av[0]);
		CHECK(tf);

		get_time(curr_date, 0);

		cache_short_for_gssi = cache_new('S', "Short term cache");

		fq_log(l, FQ_LOG_DEBUG, "If we found some error, We will send alarm message to ADMIN(%s)", my_conf->admin_phone_no);
		cache_update(cache_short_for_gssi, "ADMIN_PHONE_NO", my_conf->admin_phone_no, 0);

		JSON_Value *in_json = NULL;
		JSON_Object *root = NULL;

		char date[9], time[7], sz_current_time[15];
		get_time(date, time);
		sprintf(sz_current_time, "%s%s", date, time);	
		cache_update(cache_short_for_gssi, "DIST_CURRENT_TIME", sz_current_time, 0);

		passed_time = end_timer();
		if( passed_time > 3.0 ) {
			ctrl_msg_t	*ctrl_msg=NULL;
			rc = takeout_ctrl_msg_from_Q(l, my_conf, ctrl_obj, &ctrl_msg);
			
			if( rc > 0 ) {
				if( ctrl_msg->cmd == NEW_RATIO ) { /* It is transfered by from manager or monitor */
					fq_log(l, FQ_LOG_EMERG, "Takeout NEW_RATIO from control Q. msg=[%s]", ctrl_msg);
					rc = update_channel_ratio(l, ctrl_msg, ctx.channel_co_ratio_obj_ll);
					CHECK(rc);
				}
				else if( ctrl_msg->cmd == CO_DOWN ) {
					rc = down_channel_ratio(l, ctrl_msg, ctx.channel_co_ratio_obj_ll);
				}
				else if( ctrl_msg->cmd == PROC_STOP) {
					fprintf(stdout, "Takeout PROC_STOP from control Q. Good bye...\n");
					fq_log(l, FQ_LOG_EMERG, "Takeout PROC_STOP from control Q.");
					fq_log(l, FQ_LOG_EMERG, "\t- Takeout cmd: %d." , ctrl_msg->cmd);
					exit(0);
				}
				else if( ctrl_msg->cmd == Q_TPS_CTRL ) {
					fq_log(l, FQ_LOG_EMERG, "Takeout Q_TPS_CTRL from control Q."); 
					fq_log(l, FQ_LOG_EMERG, "We are developing functios for service."); 
					

					/*
					It is related to webgate
					get path/qname 
					set update TPS of a queue in ums hash
					in the en-queue logic , reference it.
						- get TPS in fq_mon_svr hash
						- compare TPS
						- calculate a usleep value
						- control enQ speed.
					*/
				}
				else {
					fq_log(l, FQ_LOG_ERROR, "Undefined CMD: %s.", ctrl_msg->cmd);
				}
			}
			else if( rc < 0 ) {
				if( rc == MANAGER_STOP_RETURN_VALUE ) { // -99
					fprintf( stderr, "Manager stop!!! rc=[%d]\n", rc);
					fq_log(l, FQ_LOG_EMERG, "[%s] queue is Manager stop.", deq_obj->qname);
					sleep(1);
					
					if( cache_short_for_gssi ) cache_free(&cache_short_for_gssi);
					continue;
				}

				fq_log(l, FQ_LOG_EMERG, "Conrol Q takeout error.: %s,.%s", 
					ctrl_obj->path,  ctrl_obj->qname );
				return 0;
			}
			else { // empty
				fq_log(l, FQ_LOG_DEBUG, "There is no ctrl_msg.");
			}

			if(ctrl_msg) SAFE_FREE(ctrl_msg);
			
			start_timer();
		}

		if( my_conf->working_time_use_flag == true ) {
			bool tf;
			tf = is_working_time(l, my_conf->working_time_from, my_conf->working_time_to); /* 금일 08:00:00 ~ 22:59:59 발송정지 */
			if( tf == false ) {
				fq_log(l, FQ_LOG_INFO, "It is not working time.");
				sleep(1);	
				if( cache_short_for_gssi ) cache_free(&cache_short_for_gssi);
				continue;
			}
		}

		/****************************************************************************************/
		int rc;
		unsigned char  *ums_msg = NULL;
		char	*unlink_filename = NULL;
		long	l_seq;
		rc = takeout_uchar_msg_from_Q(l, my_conf, deq_obj, &ums_msg, &unlink_filename, &l_seq);
		if(rc < 0) { /* fatal error */
			if( rc == MANAGER_STOP_RETURN_VALUE ) { // -99
				fprintf(stderr, "Manager stop!!! rc=[%d]\n", rc);
				fq_log(l, FQ_LOG_EMERG, "[%s] queue is Manager stop.", deq_obj->qname);
				sleep(1);
				if( cache_short_for_gssi ) cache_free(&cache_short_for_gssi);
				continue;
			}
			break;
		} 
		else if( rc == 0 ) { /* queue is empty */
#if 0			
			passed_time = end_timer();
			if( passed_time > 5.0 ) {
				fq_log(l, FQ_LOG_INFO, "We are waiting a message from G/W in queue('%s'-'%s').", deq_obj->path, deq_obj->qname);
				if( my_conf->fqpm_use_flag == true ) {
					touch_me_on_fq_framework( l, NULL, mypid );
				}
				start_timer();
			}
#endif
			sleep(1);
			if( cache_short_for_gssi ) cache_free(&cache_short_for_gssi);
			continue;
		}
		else {
			fq_log(l, FQ_LOG_INFO, "We take out a message that G/W sent from fq(%s/%s). msg=[%s],  rc=[%d]", deq_obj->path, deq_obj->qname,  ums_msg, rc);
		}

		char SSIP_result[8192];
		int ums_msg_len = rc;

		bool tf;
		tf = json_parsing_and_get_channel( l, &in_json, &root, ums_msg, my_conf->channel_key, &channel, my_conf->svc_code_key, &svc_code);
		if( tf == false ) {
			// Malformed request 
			// Incorrectly formatted requests are stored in a file.
			char pathfile[256];
			sprintf( pathfile, "%s/%s", cm_conf->ums_common_log_path, my_conf->malformed_request_file);
			Store_Malformed_Requeus_to_file(l, ums_msg, pathfile);	
			// Store_Malformed_Requeus_to_queue(l, ums_msg);	

			fq_log(l, FQ_LOG_ERROR, "Malformed request: json_parsing_caching_and_SSIP() error. We will throw it.");

			char *error_reason = "Malformed request";
			bool tf;
			tf = request_error_JSON_return_enQ(l, error_reason, ums_msg, ums_msg_len, return_q_obj); 
			CHECK(tf);

			commit_Q(l, deq_obj, unlink_filename, l_seq);
			goto SKIP;
		}

		debug_json( l, in_json );

		// printf("channel='%s'\n", channel);
		if( svc_code ) printf("svc_code='%s'\n", svc_code);

		fq_log(l, FQ_LOG_INFO, "json_parsing_and_get_channel() success. CHANNEL = '%s'. svc_code='%s' ", 
			channel, svc_code?svc_code:"null");

		if( !in_json || !root ) {
			fq_log(l, FQ_LOG_INFO, "JSON error.");
			commit_Q(l, deq_obj, unlink_filename, l_seq);
			goto SKIP;
		}

#if 0
/* here-1 */
		char selected_key[5];	
		char selected_co_initial;
		fqueue_obj_t *selected_q = NULL; /* selected queue object */

		selected_q = Select_a_queue_obj_and_co_initial_by_ratio(l, channel, ctx.channel_co_ratio_obj_ll, ctx.channel_co_queue_obj_ll,  ctx.queue_co_initial_codemap, selected_key, &selected_co_initial);

		if( !selected_q ) {
			commit_Q(l, deq_obj, unlink_filename, l_seq);
			goto SKIP;
		}
			
		CHECK(selected_q);

		printf("main:key=[%s]\n", selected_key);
		printf("main:co_initial=[%c]\n", selected_co_initial);
		printf("main:queue=[%s]\n", selected_q->qname);

		fq_status_t q_status;
		q_status  =  get_fq_status(l, selected_q, mon_svr_hash_obj, 10.0,  10);
		printf("main:fq_status is [%d]\n", q_status);

		// getc(stdin);
#endif
		ll_node_t *forward_channel_queue_map_node=NULL;

		// for debug.
		// linkedlist_scan(ctx.forward_channel_queue_obj_ll, stdout);

		forward_channel_queue_map_node = linkedlist_find(l, ctx.forward_channel_queue_obj_ll, channel);

		if( forward_channel_queue_map_node != NULL ) { /* found: It's means forward processing.  */
			char key[5];
			char co_initial;

			fq_log(l, FQ_LOG_INFO, "Forwording...");
			fqueue_obj_t *this_q = Select_a_queue_obj_and_co_initial_by_ratio(l, channel, ctx.channel_co_ratio_obj_ll, ctx.channel_co_queue_obj_ll,  ctx.queue_co_initial_codemap, key, &co_initial);

			if( !this_q ) { // simple forword
				fq_log( l, FQ_LOG_INFO, "CHANNEL(%S) is Forword processing. forwarding start.", channel);

				tf = forward_channel( l, channel, forward_channel_queue_map_node, ums_msg, ums_msg_len, deq_obj, unlink_filename, l_seq);
				CHECK(tf);
				fq_log( l, FQ_LOG_INFO, "Simple Forwording success.");

				passed_time = end_timer();
				if( passed_time > 5.0) {
					if( my_conf->fqpm_use_flag == true ) {
						touch_me_on_fq_framework( l, NULL, mypid );
					}
					start_timer();
				}
			}
			else { // ratio forward
				fq_status_t q_status;
				q_status  =  get_fq_status(l, this_q, mon_svr_hash_obj, 10.0,  10);
				printf("main:fq_status is [%d]\n", q_status);
				
				fq_log( l, FQ_LOG_INFO, "CHANNEL(%S) is Ratio Forword processing. forwarding start.", channel);
				tf = ratio_forward_channel( l, channel, this_q, ums_msg, ums_msg_len, deq_obj, unlink_filename, l_seq);

				if( tf == false ) {
					fq_log( l, FQ_LOG_INFO, "ratio_forward_channel() error. channel='%s' We will skip it.", channel);
					commit_Q(l, deq_obj, unlink_filename, l_seq);
					goto SKIP;
				}

				fq_log( l, FQ_LOG_INFO, "Ratio Forwording success.");

				char history_key[41];
				sprintf(history_key, "%s", json_object_get_string(root, "HISTORY_KEY"));
				char date[9], time[7];
				get_time(date, time);
				char send_time[16];
				sprintf(send_time, "%s%s", date, time);

				tf = dist_result_enQ(l, cache_short_for_gssi , root, channel, history_key, co_initial, return_q_obj, ctx.dist_result_json_rule_ll);
				if( tf == false ) {
					fq_log(l, FQ_LOG_ERROR, "dist_result_enQ() error.");
				}

				char recv_phone_no[15];
				sprintf(recv_phone_no, "%s", json_object_get_string(root, "recipient"));

				if( my_conf->eventlog_use_flag == true) {
					dist_eventlog( l, cm_conf->ums_common_log_path, send_time, channel, co_initial, history_key, recv_phone_no );
				}

				passed_time = end_timer();
				if( passed_time > 5.0) {
					if( my_conf->fqpm_use_flag == true ) {
						touch_me_on_fq_framework( l, NULL, mypid );
					}
					start_timer();
				}
				
			}
		}
		else { /* It is a channel to need ratio distribution */
			fqueue_obj_t *this_q = NULL; /* selected queue object */
			char co_in_hash;
			char co_initial;
			char key[5]; // channel and co_initial
			bool tf;
			fq_log(l, FQ_LOG_INFO, "is not Forwording...");

			fq_log( l, FQ_LOG_INFO, "CHANNEL(%s) is ratio distribute processing.", channel);

			tf = json_caching_and_SSIP( l, in_json, root,  my_conf, ctx.json_key_datatype_obj_ll,  ums_msg, cache_short_for_gssi, channel, seq_check_id);
			if( tf == false ) {
				// Malformed request 
				// Incorrectly formatted requests are stored in a file.
				char pathfile[256];
				sprintf( pathfile, "%s/%s", cm_conf->ums_common_log_path, my_conf->malformed_request_file);
				Store_Malformed_Requeus_to_file(l, ums_msg, pathfile);	
				// Store_Malformed_Requeus_to_queue(l, ums_msg);	
				fq_log(l, FQ_LOG_INFO, "json_parsing_caching_and_SSIP() error. We will throw it.");

				char *error_reason = "Malformed request";
				bool tf;
				tf = request_error_JSON_return_enQ(l, error_reason, ums_msg, ums_msg_len, return_q_obj); 
				CHECK(tf);

				commit_Q(l, deq_obj, unlink_filename, l_seq);
				goto SKIP;
			} 
			fq_log(l, FQ_LOG_INFO, "json_caching_and_SSIP(Server Side Including Process) success. channel=[%s], seq_check_id=[%s].", channel, seq_check_id);


			if( is_guarantee_user( l, seq_check_hash_obj, seq_check_id, &co_in_hash, channel ) == true ) { // We found seq_check_id in hashmap. It must guarantee sequence.

				fq_log(l, FQ_LOG_INFO, "is_guarantee_user is true.");
				fq_log(l, FQ_LOG_INFO, "seq_check_id('%s') is guarantee_user. We found co_name('%c') in hashmap.", 
						seq_check_id, co_in_hash);

				co_initial = co_in_hash;
				this_q = Select_a_queue_obj_by_co_initial(l, channel, co_initial, ctx.channel_co_queue_obj_ll);
				fq_log(l, FQ_LOG_INFO, "We select a queue object(qname='%s') for '%c'.", this_q->qname, co_initial);

				if( queue_is_warning( l, this_q, my_conf->warning_threshold) ) {

					fq_log(l, FQ_LOG_INFO, "Selected queue('%s') is status of warning(%f). We will select a new queue by least usage.", this_q->qname, this_q->on_get_usage(l, this_q));

					this_q = Select_a_queue_obj_and_co_initial_by_least_usage(l, channel, ctx.channel_co_queue_obj_ll, ctx.queue_co_initial_codemap, key, &co_initial);
					CHECK(this_q);

					fq_log(l, FQ_LOG_INFO, "We select a new queue[%s]/[%s] by least usage('%f').", this_q->path, this_q->qname, this_q->on_get_usage(l, this_q));

					/* Put seq_check_id to hash */
					char str_co_initial[2];
					sprintf(str_co_initial, "%c", co_initial);

					fq_log(l, FQ_LOG_INFO, "hash curr_elements=[%d]\n", seq_check_hash_obj->h->h->curr_elements);

					PutHash_Option(l, my_conf->seq_checking_use_flag,  seq_check_hash_obj, seq_check_id, str_co_initial, channel);
					fq_log(l, FQ_LOG_INFO, "We put this seq_check_id('%s') to hash.", seq_check_id);

					rc = uchar_enQ(l, seq_check_id, sizeof(seq_check_id), del_hash_q_obj);
					CHECK(rc > 0 );
					fq_log(l, FQ_LOG_INFO, "We put this seq_check_id('%s') to queue('%s') for deleting.", seq_check_id, del_hash_q_obj->qname );

				}
				else {
					fq_log(l, FQ_LOG_INFO, " queue co(%s) is normal. Current Usaage of [%c] is [%f].", this_q->qname,  co_initial, this_q->on_get_usage(l, this_q));
					// fprintf(stdout, "Current Usaage of [%c] is [%f].\n", co_initial, this_q->on_get_usage(l, this_q));
				}

				ll_node_t *channel_co_ratio_obj_map_node=NULL;
				channel_co_ratio_obj_map_node = linkedlist_find(l, ctx.channel_co_ratio_obj_ll, channel);
				channel_ratio_obj_t *this = (channel_ratio_obj_t *) channel_co_ratio_obj_map_node->value;
				ratio_obj_t	*ratio_obj = this->ratio_obj;

				float current_co_ratio = get_co_ratio_by_co_initial(l, ratio_obj , co_initial);
				// fprintf(stdout, "current ratio of [%c] is [%f]\n", co_initial, current_co_ratio);
			} // user guarantee end.
			else {
				fq_log(l, FQ_LOG_INFO, "is_guarantee_user is false.");
				/* Where should I send it to get there the fastest? */
				if( strcmp( my_conf->distribute_type, "LEAST_USAGE") == 0 ) {
					fq_log(l, FQ_LOG_INFO, "Send messages to the queue with the least usage.");

					this_q = Select_a_queue_obj_and_co_initial_by_least_usage(l, channel, ctx.channel_co_queue_obj_ll, ctx.queue_co_initial_codemap, key, &co_initial);
					CHECK(this_q);

					fq_log(l, FQ_LOG_INFO, "[%s]/[%s] is selected.", this_q->path, this_q->qname);

					/* Put seq_check_id to hash */
					char str_co_initial[2];
					sprintf(str_co_initial, "%c", co_initial);

					printf("hash curr_elements=[%d]\n", seq_check_hash_obj->h->h->curr_elements);
					PutHash_Option(l, my_conf->seq_checking_use_flag,  seq_check_hash_obj, seq_check_id, str_co_initial, channel);

					rc = uchar_enQ(l, seq_check_id, sizeof(seq_check_id), del_hash_q_obj);
					if( rc < 0 ) {
						fq_log(l, FQ_LOG_INFO, "uchar_enQ() error . rc='%d'.", rc);
						commit_Q(l, deq_obj, unlink_filename, l_seq);
						goto SKIP;
					}
					// CHECK(rc > 0 );
				}
				else if( strcmp( my_conf->distribute_type, "RATIO") == 0 ) {
					this_q = Select_a_queue_obj_and_co_initial_by_ratio(l, channel, ctx.channel_co_ratio_obj_ll, ctx.channel_co_queue_obj_ll,  ctx.queue_co_initial_codemap, key, &co_initial);

					if( !this_q ) {
						fq_log(l, FQ_LOG_ERROR, "I haven't found a suitable queue for a channel(%s).", channel);
						commit_Q(l, deq_obj, unlink_filename, l_seq);

						char *error_reason = "Ratio Settiong error.";
						bool tf;
						tf = request_error_JSON_return_enQ(l, error_reason, ums_msg, ums_msg_len, return_q_obj); 
						CHECK(tf);
						goto SKIP;
					}
					fq_log(l, FQ_LOG_INFO, "Selected a queue('%s/%s') based on the distribution ratio.", this_q->path, this_q->qname);

#if 0
					if( queue_is_warning( l, this_q, my_conf->warning_threshold) ) {
						fq_log(l, FQ_LOG_INFO, "The current state of queue(%s) has crossed the threshold.( my_conf->warning_threshold = %f).", this_q->qname,  my_conf->warning_threshold);
						fq_log(l, FQ_LOG_INFO, "So We will new queue by least usage.\n");
						this_q = Select_a_queue_obj_and_co_initial_by_least_usage(l, channel, ctx.channel_co_queue_obj_ll, ctx.queue_co_initial_codemap, key, &co_initial);
						CHECK(this_q);
						fq_log(l, FQ_LOG_INFO, "New queue[%s] is selected and it is  a healthy state.", this_q->qname);
					}
					else {
						fq_log(l, FQ_LOG_INFO, "Selected a queue[%s] by ratio is normal(%f).\n", this_q->qname, this_q->on_get_usage(l, this_q));
					}
#endif
					fq_log(l, FQ_LOG_INFO, "Selected a queue[%s] by ratio is normal(%f).\n", this_q->qname, this_q->on_get_usage(l, this_q));

					/* Put seq_check_id to hash */
					char str_co_initial[2];
					sprintf(str_co_initial, "%c", co_initial);

					PutHash_Option(l, my_conf->seq_checking_use_flag,  seq_check_hash_obj, seq_check_id, str_co_initial, channel);

					fq_log(l, FQ_LOG_INFO, "We put seq_check_id('%s') in hash and  hash curr_elements=[%d]\n", seq_check_id, seq_check_hash_obj->h->h->curr_elements);

					/* enQ */
					if( my_conf->seq_checking_use_flag == true) {
						rc = uchar_enQ(l, seq_check_id, sizeof(seq_check_id), del_hash_q_obj);
						CHECK(rc > 0 );
						fq_log(l, FQ_LOG_INFO, "We put seq_check_id('%s') to queue(%s)  for deleting.\n", seq_check_id, del_hash_q_obj->qname );
					}
				}
				else {
					fprintf(stderr, "[%s] is undefined distribute type.( available type is LEAST_USAGE or RATIO.\n",  my_conf->distribute_type);
					fq_log(l, FQ_LOG_ERROR, "[%s] is undefined distribute type.( available type is LEAST_USAGE or RATIO. ",  my_conf->distribute_type);
					if( cache_short_for_gssi ) cache_free(&cache_short_for_gssi);
					return 0;
				}
			}

			
			fq_status_t q_status;
			q_status  =  get_fq_status(l, this_q, mon_svr_hash_obj, 10.0,  10);
			fq_log(l, FQ_LOG_INFO, "fq_status is [%d]\n", q_status);


			char date[9], time[7];
			get_time(date, time);
			char send_time[16];
			sprintf(send_time, "%s%s", date, time);
			cache_update(cache_short_for_gssi, "senddate", send_time, 0);
			
			if( STRCMP(my_conf->ums_gw_protocol, "JSON") == 0 )  {
				bool tf;
				tf = make_JSON_stream_by_rule_and_enQ( l, root, key, channel,  SSIP_result, cache_short_for_gssi, my_conf, this_q, deq_obj, unlink_filename, l_seq, co_initial, ctx.co_channel_json_rule_obj_ll);
				CHECK(tf);

				fq_log(l, FQ_LOG_INFO, "We made a new json message by rule. and put to queue('%s') and Commit XA.", this_q->qname);


				char history_key[41];
				char recv_phone_no[15];
				sprintf(history_key, "%s", json_object_get_string(root, "HISTORY_KEY"));
				sprintf(recv_phone_no, "%s", json_object_get_string(root, "RCV_PHN_NO"));
				fq_log( l, FQ_LOG_INFO, "history_key=[%s]", history_key);


				tf = dist_result_enQ(l, cache_short_for_gssi , root, channel, history_key, co_initial, return_q_obj, ctx.dist_result_json_rule_ll);
				if( tf == false ) {
					fq_log(l, FQ_LOG_ERROR, "dist_result_enQ() error.");
				}

			
				if( my_conf->eventlog_use_flag == true) {
					dist_eventlog( l, cm_conf->ums_common_log_path, send_time, channel, co_initial, history_key, recv_phone_no );
				}
				
				passed_time = end_timer();
				if( passed_time > 3.0) {
					if( my_conf->fqpm_use_flag == true ) {
						touch_me_on_fq_framework( l, NULL, mypid );
					}
					start_timer();
				}
			}
			else {
				char *error_reason = "Unsupported Protocol.(rule error)";
				bool tf;
				tf = request_error_JSON_return_enQ(l, error_reason, ums_msg, ums_msg_len, return_q_obj); 
				CHECK(tf);
				fq_log(l, FQ_LOG_INFO, "'%s' is Unsupported Protocol.\n", my_conf->ums_gw_protocol);
				if( cache_short_for_gssi ) cache_free(&cache_short_for_gssi);
				return 0;
			}
		}
SKIP:
		SAFE_FREE(channel);
		SAFE_FREE(svc_code);
		cache_free(&cache_short_for_gssi);

		FREE(ums_msg);

		if( in_json ) json_value_free(in_json);

		/**********************************************************************************************************/

	} /* while end */

	if(ctx.channel_co_queue_obj_ll) linkedlist_free(&ctx.channel_co_queue_obj_ll);
	if(ctx.forward_channel_queue_obj_ll) linkedlist_free(&ctx.forward_channel_queue_obj_ll);
	if(ctx.channel_co_ratio_obj_ll) linkedlist_free(&ctx.channel_co_ratio_obj_ll);

	rc = CloseHashMapFiles(l, &hash_obj);
    CHECK(rc==TRUE);

	rc = CloseHashMapFiles(l, &seq_check_hash_obj);
    CHECK(rc==TRUE);

	rc = CloseHashMapFiles(l, &mon_svr_hash_obj);
    CHECK(rc==TRUE);

	fq_close_file_logger(&l);

	// close_ratio_distributor(l, &obj);

	return 0;
}

fq_status_t get_fq_status( fq_logger_t *l, fqueue_obj_t *obj, hashmap_obj_t *mon_svr_hash_obj, float usage_threshold, int tps_threshold)
{
	char hash_search_key[128];
	char *hash_value = NULL;

	HashQueueInfo_t HashInfo;

	sprintf(hash_search_key, "%s/%s", obj->path, obj->qname);

	int rc;
	rc = Sure_GetHash(l, mon_svr_hash_obj, hash_search_key, &hash_value);
	if( rc == FALSE ) {
		return(FQ_GET_FAILED);
	}
	else {
		int rc;
		delimiter_list_obj_t    *obj=NULL;
		delimiter_list_t *this_entry=NULL;

		rc = open_delimiter_list(l, &obj, hash_value, '|');
		CHECK(rc == TRUE);

		this_entry = obj->head;

		for( int t_no=0;  (this_entry->next && this_entry->value); t_no++ ) {
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
static int dist_eventlog(fq_logger_t *l, char *logpath, ...)
{
	char fn[256];
	char datestr[40], timestr[40];
	FILE* fp=NULL;
	va_list ap;

	/*
	   pthread_mutex_lock(&log_lock);
	   */

	get_time(datestr, timestr);
	snprintf(fn, sizeof(fn), "%s/FQ_dist_%s.eventlog", logpath, datestr);

	fp = fopen(fn, "a");
	if(!fp) {
		fq_log(l, FQ_LOG_ERROR, "failed to open eventlog file. [%s]", fn);
		goto return_FALSE;
	}

	va_start(ap, logpath);
	fprintf(fp, "%s|%s|%s|", "dist_enQ_event", datestr, timestr);
	vfprintf(fp, "%s|%s|%c|%s|%s|\n", ap); 

	if(fp) fclose(fp);
	va_end(ap);
	/*
	   pthread_mutex_unlock(&log_lock);
	   */

	return(TRUE);

return_FALSE:
	if(fp) fclose(fp);
	/*
	   pthread_mutex_unlock(&log_lock);
	   */
	return(FALSE);
}

#if 0
		HashQueueInfo_t *hash_qi =  get_a_queue_info_in_hashmap_local( l, g_mon_hash_path, g_mon_hash_name,  key);
		
		if( !hash_qi ) {
			fq_log(l, FQ_LOG_ERROR, "Can't find a queue('%s', '%s) infomation in hash(%s).", Qs[i].obj->path, Qs[i].obj->qname, g_mon_hash_name);

			json_object_set_number(subObject, "enTps", hash_qi->en_tps);
			json_object_set_number(subObject, "deTps", hash_qi->de_tps);
		}
		else {
			fq_log(l, FQ_LOG_INFO, "Successfully, We get a queue('%s', '%s') info in haah.", Qs[i].obj->path, Qs[i].obj->qname );
			json_object_set_number(subObject, "enTps", 0);
			json_object_set_number(subObject, "deTps", 0);
		}
#endif

static bool find_a_queue_info( fq_logger_t *l, hashmap_obj_t *hash_obj,  linkedlist_t *ll, char *key, HashQueueInfo_t *dst )
{
	FQ_TRACE_ENTER(l);

	ll_node_t *p;
	int	array_index;
	
	for(p=ll->head, array_index=0; p!=NULL; p=p->next, array_index++) {
		if( strcmp(key, p->key) != 0 ) {
			continue;
		}

		char *ts1=strdup(p->key);
		char *ts2=strdup(p->key);

		char *qpath=dirname(ts1);
		char *qname=basename(ts2);

		sprintf(dst->qpath, "%s", qpath);
		sprintf(dst->qname, "%s", qname);

		free(ts1);
		free(ts2);

		int rc;
		char *hash_value = NULL;
		rc = Sure_GetHash(l, hash_obj, p->key, &hash_value);

		if( rc == TRUE ) {
			fqlist_t *ll = fqlist_new('A', "delimiter");
			CHECK(ll);

			int value_count;
			value_count = MakeListFromDelimiterMsg( ll, hash_value, '|', 0 );
			fq_log(l, FQ_LOG_DEBUG, "value count = %d", value_count);

			tlist_t *t = ll->list;
			node_t *p;

			int t_no=0;
			for (  p = t->head ; p != NULL ; p=p->next, t_no++ ) {
				switch(t_no) {
					case 0:
						dst->msglen = atoi(p->value);
						break;
					case 1:
						dst->max_rec_cnt = atoi(p->value);
						break;
					case 2:
						dst->gap = atoi(p->value);
						break;
					case 3:
						dst->usage = atof(p->value);
						break;
					case 4:
						dst->en_competition = atol(p->value);
						break;
					case 5:
						dst->de_competition = atol(p->value);
						break;
					case 6:
						dst->big = atoi(p->value);
						break;
					case 7:
						dst->en_tps = atoi(p->value);
						break;
					case 8:
						dst->de_tps = atoi(p->value);
						break;
					case 9:
						dst->en_sum = atol(p->value);
						break;
					case 10:
						dst->de_sum = atol(p->value);
						break;
					case 11:
						dst->status = atoi(p->value);
						break;
					case 12:
						dst->shmq_flag = atoi(p->value);
						break;
					case 13:
						sprintf(dst->desc, "%s", p->value);
						break;
					case 14:
						dst->last_en_time = atol(p->value);
						break;
					case 15:
						dst->last_de_time = atol(p->value);
						break;
					default:
						break;

				}
			} // for end.
			if(ll) fqlist_free(&ll);
			SAFE_FREE(hash_value);

			break;
		} else {
			fq_log(l, FQ_LOG_ERROR,  "[%s] is not found in hashmap.", p->key);
		}

	}// for end.

	if( dst->msglen > 0 ) {
		FQ_TRACE_EXIT(l);
		return true;
	}
	else {
		FQ_TRACE_EXIT(l);
		return false;
	}
}
static HashQueueInfo_t *get_a_queue_info_in_hashmap_local( fq_logger_t *l, char *hash_path, char *hash_name,  char *key)
{
	HashQueueInfo_t *p=NULL;

	FQ_TRACE_ENTER(l);

	p = calloc(1, sizeof(HashQueueInfo_t));
	linkedlist_t *ll = linkedlist_new("file queue linkedlist.");
	CHECK(ll);

	hashmap_obj_t *hash_obj=NULL;
	int rc;
	rc = OpenHashMapFiles(l, hash_path, hash_name, &hash_obj);
    CHECK(rc==TRUE);

	bool tf = MakeLinkedList_filequeues(l, ll);
	CHECK(tf);

	p = (HashQueueInfo_t *)calloc(1, sizeof(HashQueueInfo_t));
	CHECK(p);

	tf = find_a_queue_info(l, hash_obj, ll, key, p);
	if( tf == false ) {
		if(hash_obj) CloseHashMapFiles(l, &hash_obj);
		if(ll) linkedlist_free(&ll);

		FQ_TRACE_EXIT(l);
		return NULL;
	}
	// CHECK(tf);

	if(hash_obj) CloseHashMapFiles(l, &hash_obj);

	if(ll) linkedlist_free(&ll);

	FQ_TRACE_EXIT(l);

	return(p);
}

bool append_mypid_fq_pid_listfile( fq_logger_t *l, char *file )
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
