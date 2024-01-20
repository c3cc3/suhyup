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
#include "subQ_dist_conf.h"
#include "fq_file_list.h"
#include "queue_ctl_lib.h"

#if 0
typedef struct _ums_msg_t ums_msg_t;
struct _ums_msg_t {
	int length;
	int seq;
	char svc_code[4+1];
	char name[20+1];
	char address[64+1];
};
#endif

bool Open_takeout_Q( fq_logger_t *l, subQ_dist_conf_t *conf, fqueue_obj_t **deq_obj)
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
bool Make_sub_Q_obj_list(fq_logger_t *l, subQ_dist_conf_t *my_conf, linkedlist_t *subQ_obj_list)
{
	file_list_obj_t *subQ_list_obj = NULL;
	linkedlist_t	*ll=subQ_obj_list;

	int rc;

	FQ_TRACE_ENTER(l);
	rc = open_file_list(l, &subQ_list_obj, my_conf->sub_queue_list_file);
	if( rc == FALSE ) {
		fq_log(l, FQ_LOG_ERROR, "open_file_list('%s') error.", my_conf->sub_queue_list_file);
		FQ_TRACE_EXIT(l);
		return false;
	}

	FILELIST *this_entry_subQ=NULL;
	this_entry_subQ = subQ_list_obj->head;
	while( this_entry_subQ->next && this_entry_subQ->value ) {
		fq_log(l, FQ_LOG_DEBUG, "'%s'.", this_entry_subQ->value);
	
		char *ts1 = strdup(this_entry_subQ->value);
		char *ts2 = strdup(this_entry_subQ->value);
		char *qpath=dirname(ts1);
		char *qname=basename(ts2);

		fq_log(l, FQ_LOG_DEBUG,  "qpath='%s', qname='%s'\n", qpath, qname);

		sub_queue_obj_t *tmp=NULL;

		tmp = calloc(1, sizeof(sub_queue_obj_t));

		rc =  OpenFileQueue(l, NULL, NULL, NULL, NULL, qpath, qname, &tmp->obj);
		if( rc != TRUE ) {
			if(ts1) free(ts1);
			if(ts2) free(ts2);
			SAFE_FREE(tmp);
			close_file_list(l, &subQ_list_obj);
			FQ_TRACE_EXIT(l);
			return false;
		}

		ll_node_t *ll_node=NULL;
		ll_node = linkedlist_put(ll, this_entry_subQ->value, (void *)tmp, sizeof(fqueue_obj_t));
		if( !ll_node ) {
			fq_log(l, FQ_LOG_ERROR, "linkedlist_put('%s') error.", this_entry_subQ->value);
			if(ts1) free(ts1);
			if(ts2) free(ts2);
			SAFE_FREE(tmp);
			CloseFileQueue(l, &tmp->obj);
			close_file_list(l, &subQ_list_obj);
			FQ_TRACE_EXIT(l);
			return false;
		}

		if(ts1) free(ts1);
		if(ts2) free(ts2);

		this_entry_subQ = this_entry_subQ->next;
    }
	FQ_TRACE_EXIT(l);

	return true;
}

int takeout_ums_msg_from_Q(fq_logger_t *l, subQ_dist_conf_t *my_conf, fqueue_obj_t *deq_obj, ums_msg_t **msg, char **uf, long *p_seq)
{
	long l_seq=0L;
	unsigned char *buf=NULL;
	long run_time=0L;
	int cnt_rc;
	char unlink_filename[256];
	size_t buffer_size;
	
	buffer_size = deq_obj->h_obj->h->msglen;
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

fqueue_obj_t *Choose_a_subQ(fq_logger_t *l,  linkedlist_t *subQ_obj_list)
{
	fqueue_obj_t *least_subQ=NULL;

	CHECK(subQ_obj_list);

	ll_node_t *p=NULL;
	for(p=subQ_obj_list->head; p!=NULL; p=p->next) {
		sub_queue_obj_t *tmp;
		size_t   value_sz;
		tmp = (sub_queue_obj_t *)p->value;
		value_sz = p->value_sz;

		fqueue_obj_t *qobj =  tmp->obj;
		fq_log(l, FQ_LOG_INFO, "qname=[%s]", tmp->obj->qname);
		// printf("qname=[%s]\n", tmp->obj->qname);
	
		if( least_subQ == NULL ) {
			least_subQ = tmp->obj;
		}
		else {
			float leastQ_usage = least_subQ->on_get_usage(l, least_subQ);
			float nowQ_usage = tmp->obj->on_get_usage(l,tmp->obj);
			if( nowQ_usage < leastQ_usage ) {
				least_subQ = tmp->obj;
			}
		}
	}
	fq_log(l, FQ_LOG_INFO, "Choosed subQ is '%s',", least_subQ->qname);
	return(least_subQ);
}
int put_carrier_msg_to_subQ(fq_logger_t *l, unsigned char *msg, size_t buf_sz, size_t msglen, fqueue_obj_t *obj)
{
	int rc;
	long    l_seq=0L;
    long run_time=0L;

	while(1) {
		rc =  obj->on_en(l, obj, EN_NORMAL_MODE, msg, buf_sz, msglen, &l_seq, &run_time );
		if( rc == EQ_FULL_RETURN_VALUE )  {
			usleep(1000);
			continue;
		}
		else if( rc < 0 ) { 
			return(rc);
		}
		else {
			break;
		}
	}
	return(rc);
}
