/*
 * queue_ctl_lib.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <libgen.h> /* for basename() */

#include "fqueue.h"
#include "fq_common.h"
#include "fq_sec_counter.h"
#include "fq_file_list.h"
#include "queue_ctl_lib.h"
#include "fq_json_tci.h"

#if 0
typedef struct _ums_msg_t ums_msg_t;
struct _ums_msg_t {
	int msg_type; /* SMS/LMS/MMS/RCS/kakao */
	char phone_no[12];
	int seq;
	unsigned char data[MAX_MSG_SIZE+1];
};
#endif

bool Open_takeout_Q( fq_logger_t *l, ratio_dist_conf_t *conf, fqueue_obj_t **deq_obj)
{
	int rc;
	fqueue_obj_t *obj=NULL;

	rc =  OpenFileQueue(l, NULL, NULL, NULL, NULL, conf->qpath, conf->qname, &obj);
	if(rc==FALSE) return false;
	else {
		*deq_obj = obj;
		return true;
	}
}

bool Open_control_Q( fq_logger_t *l, ratio_dist_conf_t *conf, fqueue_obj_t **deq_obj)
{
	int rc;
	fqueue_obj_t *obj=NULL;

	rc =  OpenFileQueue(l, NULL, NULL, NULL, NULL, conf->ctrl_qpath, conf->ctrl_qname, &obj);
	if(rc==FALSE) return false;
	else {
		*deq_obj = obj;
		return true;
	}
}

bool Open_del_hash_Q( fq_logger_t *l, ratio_dist_conf_t *conf, fqueue_obj_t **deq_obj)
{
	int rc;
	fqueue_obj_t *obj=NULL;

	rc =  OpenFileQueue(l, NULL, NULL, NULL, NULL, conf->del_hash_q_path, conf->del_hash_q_name, &obj);
	if(rc==FALSE) return false;
	else {
		*deq_obj = obj;
		return true;
	}
}

bool Open_return_Q( fq_logger_t *l, ratio_dist_conf_t *conf, fqueue_obj_t **deq_obj)
{
	int rc;
	fqueue_obj_t *obj=NULL;

	rc =  OpenFileQueue(l, NULL, NULL, NULL, NULL, conf->return_q_path, conf->return_q_name, &obj);
	if(rc==FALSE) return false;
	else {
		*deq_obj = obj;
		return true;
	}
}

int takeout_ums_msg_from_Q(fq_logger_t *l, ratio_dist_conf_t *my_conf, fqueue_obj_t *deq_obj, ums_msg_t **msg, char **uf, long *p_seq)
{
	long l_seq=0L;
	unsigned char *buf=NULL;
	long run_time=0L;
	int cnt_rc;
	char unlink_filename[256];
	size_t buffer_size;
	
	buffer_size = deq_obj->h_obj->h->msglen;
	// buffer_size = sizeof(ums_msg_t)+1;
	buf = calloc(buffer_size, sizeof(unsigned char));

	int rc;
	rc = deq_obj->on_de_XA(l, deq_obj, buf, buffer_size, &l_seq, &run_time, unlink_filename);
	if( rc == DQ_EMPTY_RETURN_VALUE || rc < 0 ) {
		SAFE_FREE(buf);
	}
	else {
		fq_log(l, FQ_LOG_DEBUG, "deQ msg:'%s'.", buf);
		fq_log(l, FQ_LOG_DEBUG, "deQ msglen:'%d'.", rc);
		*msg = (ums_msg_t *)buf;
		*p_seq = l_seq;

		if( my_conf->check_msgsize_flag == true) {
			if( sizeof(ums_msg_t) != rc ) {
				fq_log(l, FQ_LOG_ERROR, "Data is not ums message type. ums_msg_t=(%d) rc=(%d)", sizeof(ums_msg_t), rc);
				SAFE_FREE(buf);
				return(-1);
			}
		}
		*uf = strdup(unlink_filename);
		// SAFE_FREE(buf);
	}
	return rc;
}

int takeout_uchar_msg_from_Q(fq_logger_t *l, ratio_dist_conf_t *my_conf, fqueue_obj_t *deq_obj, unsigned char **msg, char **uf, long *p_seq)
{
	long l_seq=0L;
	unsigned char *buf=NULL;
	long run_time=0L;
	int cnt_rc;
	char unlink_filename[256];
	size_t buffer_size;
	
	buffer_size = deq_obj->h_obj->h->msglen;
	// buffer_size = sizeof(ums_msg_t)+1;
	buf = calloc(buffer_size, sizeof(unsigned char));

	int rc;
	rc = deq_obj->on_de_XA(l, deq_obj, buf, buffer_size, &l_seq, &run_time, unlink_filename);
	if( rc == DQ_EMPTY_RETURN_VALUE || rc < 0 ) {
		SAFE_FREE(buf);
	}
	else {
		fq_log(l, FQ_LOG_DEBUG, "deQ msg:'%s'.", buf);
		fq_log(l, FQ_LOG_DEBUG, "deQ msglen:'%d'.", rc);
		*msg = (unsigned char *)buf;
		*p_seq = l_seq;

		*uf = strdup(unlink_filename);
		// SAFE_FREE(buf);
	}
	return rc;
}

int takeout_ctrl_msg_from_Q(fq_logger_t *l, ratio_dist_conf_t *my_conf, fqueue_obj_t *deq_obj, ctrl_msg_t **msg)
{
	long l_seq=0L;
	unsigned char *buf=NULL;
	long run_time=0L;
	size_t buffer_size;
	
	buffer_size = deq_obj->h_obj->h->msglen;
	buf = calloc(buffer_size, sizeof(unsigned char));

	int rc;
	rc = deq_obj->on_de(l, deq_obj, buf, buffer_size, &l_seq, &run_time);
	if( rc == DQ_EMPTY_RETURN_VALUE || rc < 0 ) {
		SAFE_FREE(buf);
	}
	else {
		fq_log(l, FQ_LOG_DEBUG, "deQ msglen:'%d'.", rc);
		*msg = (ctrl_msg_t *)buf;
	}
	return rc;
}

void commit_Q( fq_logger_t *l, fqueue_obj_t *deq_obj, char *unlink_filename, long l_seq)
{
	long run_time=0L;
	deq_obj->on_commit_XA(l, deq_obj, l_seq, &run_time, unlink_filename);
	
	SAFE_FREE(unlink_filename);
	return;
}

void cancel_Q( fq_logger_t *l, fqueue_obj_t *deq_obj, char *unlink_filename, long l_seq)
{
	long run_time=0L;
	deq_obj->on_cancel_XA(l, deq_obj, l_seq, &run_time);
	
	SAFE_FREE(unlink_filename);
	return;
}

int uchar_enQ(fq_logger_t *l, unsigned char *data, size_t data_len, fqueue_obj_t *obj)
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
			if( run_time > 10000 ) {
				fq_log(l, FQ_LOG_EMERG, "[%s][%s] -> enQ delay: [%ld].", obj->path, obj->qname, run_time);
			}
			fq_log(l, FQ_LOG_INFO, "queue('%s/%s'): enqueued msg len(rc) = [%d].", obj->path, obj->qname, rc);
			break;
		}
	}
	FQ_TRACE_EXIT(l);
	return(rc);
}

int ums_msg_type_enQ(fq_logger_t *l, ums_msg_t *data, size_t data_len, fqueue_obj_t *obj)
{
	int rc;
	long l_seq=0L;
    long run_time=0L;

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
			return(rc);
		}
		else {
			if( run_time > 10000 ) {
				fq_log(l, FQ_LOG_EMERG, "[%s][%s] -> enQ delay: [%ld].", obj->path, obj->qname, run_time);
			}
			fq_log(l, FQ_LOG_INFO, "enqueued ums_msg len(rc) = [%d].", rc);
			break;
		}
	}
	return(rc);
}

bool queue_is_warning( fq_logger_t *l, fqueue_obj_t *qobj, float limit ) 
{
	FQ_TRACE_ENTER(l);
	if( qobj->on_get_usage(l, qobj) > limit ) {
		fq_log(l, FQ_LOG_INFO, "Usage of queue(%s) is warning. current_usage='%f'", qobj->qname, qobj->on_get_usage(l, qobj));
		FQ_TRACE_EXIT(l);
		return true;
	}			
	FQ_TRACE_EXIT(l);
	return false;
}

bool request_error_JSON_return_enQ(fq_logger_t *l, char *error_reason, unsigned char *ums_msg, int ums_msg_len, fqueue_obj_t *return_q_obj)
{
	FQ_TRACE_ENTER(l);

	JSON_Value *json_root_value = json_value_init_object();
	CHECK(json_root_value);
	JSON_Object *json_root_object = json_value_get_object(json_root_value);
	CHECK(json_root_object);

	char date[9], time[7];
	get_time(date, time);

	json_object_set_string(json_root_object, "RESP_KIND", "DIST");

	char done_date[15];
	sprintf(done_date,"%s%s", date, time);
	json_object_set_string(json_root_object, "DONE_DATE", done_date);
	json_object_set_string(json_root_object, "BILL_DATE", "");
	json_object_set_string(json_root_object, "COMP_CODE", "");

	json_object_set_string(json_root_object, "HISTORY_KEY", "");
	json_object_set_string(json_root_object, "SEND_TIME", done_date);
	json_object_set_string(json_root_object, "RECV_TIME", done_date);
	json_object_set_string(json_root_object, "MSG_KIND", "");
	json_object_set_string(json_root_object, "RESULT_GROUP_CODE", "03");
	json_object_set_string(json_root_object, "RESULT_CODE", "UMS51");
	json_object_set_string(json_root_object, "REASON", error_reason);
	json_object_set_string(json_root_object, "TELECOM_CODE", "");
	json_object_set_string(json_root_object, "CUSTOMER", "");
	json_object_set_string(json_root_object, "SESSION_ID", "");
	json_object_set_string(json_root_object, "SESSION_NO", "");

	char *buf=NULL;
    size_t buf_size;

    buf_size = json_serialization_size(json_root_value) + 1;
    buf = calloc(sizeof(char), buf_size);

    json_serialize_to_buffer(json_root_value, buf, buf_size);

	debug_json(l, json_root_value);

	int rc;

	rc = uchar_enQ(l, buf, buf_size, return_q_obj);
	CHECK( rc >= 0);

    SAFE_FREE(buf);
	json_value_free(json_root_value); 

	FQ_TRACE_EXIT(l);
	return true;
}
bool dist_result_enQ( fq_logger_t *l, cache_t *cache_short, JSON_Object *rootObject, char *channel, char *history_key, char co, fqueue_obj_t *return_q_obj, linkedlist_t *ll)
{
	FQ_TRACE_ENTER(l);
	
#if 1
	JSON_Value *json_root_value = json_value_init_object();
	CHECK(json_root_value);
	JSON_Object *json_root_object = json_value_get_object(json_root_value);
	CHECK(json_root_object);

	char date[9], time[7];
	get_time(date, time);

	json_object_set_string(json_root_object, "RESP_KIND", "DIST");
	char done_date[15];
	sprintf(done_date,"%s%s", date, time);
	json_object_set_string(json_root_object, "DONE_DATE", done_date);
	json_object_set_string(json_root_object, "BILL_DATE", "");

	if( co == 'K' ) {
		json_object_set_string(json_root_object, "COMP_CODE", "1001");
	}
	else if( co == 'L' ) {
		json_object_set_string(json_root_object, "COMP_CODE", "1002");
	}
	else if( co == 'B' ) {
		json_object_set_string(json_root_object, "COMP_CODE", "2001");
	}
	else {
		json_object_set_string(json_root_object, "COMP_CODE", "0000");
	}

	json_object_set_string(json_root_object, "HISTORY_KEY", history_key);
	json_object_set_string(json_root_object, "SEND_TIME", done_date);
	json_object_set_string(json_root_object, "RECV_TIME", done_date);
	json_object_set_string(json_root_object, "MSG_KIND", channel);
	json_object_set_string(json_root_object, "RESULT_GROUP_CODE", "01");
	json_object_set_string(json_root_object, "RESULT_CODE", "");
	json_object_set_string(json_root_object, "REASON", "");
	json_object_set_string(json_root_object, "TELECOM_CODE", "");
	json_object_set_string(json_root_object, "CUSTOMER", "");
	json_object_set_string(json_root_object, "SESSION_ID", "");
	json_object_set_string(json_root_object, "SESSION_NO", "");

	char *buf=NULL;
    size_t buf_size;

    buf_size = json_serialization_size(json_root_value) + 1;
    buf = calloc(sizeof(char), buf_size);

    json_serialize_to_buffer(json_root_value, buf, buf_size);

	fq_log(l, FQ_LOG_INFO, "dist_result_enQ() success. msg=[%s]", buf);

	debug_json(l, json_root_value);
	int rc;

	rc = uchar_enQ(l, buf, buf_size, return_q_obj);
	CHECK( rc >= 0);

    SAFE_FREE(buf);
	json_value_free(json_root_value); 

#else
	int pretty_flag = 1;
	char *target = NULL;
	bool tf;
	tf = make_serialized_json_by_rule(l, ll, cache_short, rootObject, &target, pretty_flag);
	if( tf == false ) {
		fq_log(l, FQ_LOG_ERROR, "make_serialized_json_by_rule() error.");
		return false;
	}

	int rc;

	rc = uchar_enQ(l, target, strlen(target)+1, return_q_obj);
	CHECK( rc >= 0);

	SAFE_FREE(target);
#endif

	FQ_TRACE_EXIT(l);
	return true;
}
