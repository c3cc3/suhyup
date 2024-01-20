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

bool Open_takeout_Q( fq_logger_t *l, fq_alarm_svr_conf_t *conf, fqueue_obj_t **deq_obj)
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

int takeout_uchar_msg_from_Q(fq_logger_t *l, fq_alarm_svr_conf_t *my_conf, fqueue_obj_t *deq_obj, unsigned char **msg, char **uf, long *p_seq)
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
			fq_log(l, FQ_LOG_INFO, "queue('%s/%s'): enqueued msg len(rc) = [%d].", obj->path, obj->qname, rc);
			break;
		}
	}
	FQ_TRACE_EXIT(l);
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

