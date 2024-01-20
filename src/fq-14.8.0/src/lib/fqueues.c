#include <stdio.h>
#include <stdbool.h>
#include <libgen.h>

#include "fq_common.h"
#include "fqueues.h"
#include "fq_scanf.h"


static fqueue_obj_t *on_get_key_qobj(fq_objs_t *objs, char group_initial);
static fqueue_obj_t *on_get_least_qobj(fq_objs_t *objs);
static fqueue_obj_t *on_get_key_least_qobj(fq_objs_t *objs, char group_initial);
static fqueue_obj_t *on_find_qobj(fq_objs_t *objs, char *path, char *qname);
static bool MakeLinkedList_with_mapfile(fq_logger_t *l, char *mapfile, char delimiter, linkedlist_t *ll, int *length);

static fqueue_obj_t *on_get_key_qobj(fq_objs_t *objs, char group_initial)
{
	ll_node_t *p;

	if( !objs ) return(false);
	
	p = objs->ll->head;
	while(p) {
		int rc;
		char initial;
		char kind;

		group_queue_t *this = (group_queue_t *)p->value;

		initial = this->initial;
		kind = this->kind;
		if( kind != 'D' ) {
			p = p->next;
			continue;
		}

		if( group_initial == initial ) return( this->obj );

		p = p->next;
	}
	return(NULL);
}

static fqueue_obj_t *on_get_least_qobj(fq_objs_t *objs)
{
	ll_node_t *p;
	fqueue_obj_t *qobj=NULL;
	fqueue_obj_t *least_qobj=NULL;


	if( !objs ) return(false);
	
	p = objs->ll->head;

	while(p) {
		int rc;
		char initial;
		char kind;

		group_queue_t *this = (group_queue_t *)p->value;

		initial = this->initial;
		qobj = this->obj;
		kind = this->kind;
		if( kind != 'D' ) {
			p = p->next;
			continue;
		}

		if(least_qobj == NULL) { /* first scan */
			least_qobj = qobj;
		}
		else {
			int current_diff =  qobj->on_get_diff( 0, qobj);
			int least_diff =  least_qobj->on_get_diff( 0, least_qobj);

			fq_log(objs->l, FQ_LOG_DEBUG, "current_diff=%d, least_diff=%d.", current_diff, least_diff);

			if( current_diff < least_diff ) {
				least_qobj = qobj;
			}
		}

		p = p->next;
	}

	return(least_qobj);
}

static fqueue_obj_t *on_get_key_least_qobj(fq_objs_t *objs, char group_initial)
{
	ll_node_t *p;
	fqueue_obj_t *qobj=NULL;
	fqueue_obj_t *least_qobj=NULL;

	if( !objs ) return(false);
	
	p = objs->ll->head;

	while(p) {
		int rc;
		char initial;
		char kind;

		group_queue_t *this = (group_queue_t *)p->value;

		initial = this->initial;
		qobj = this->obj;
		kind = this->kind;
		if( kind != 'D' ) {
			p = p->next;
			continue;
		}

		if(initial != group_initial ) {
			p = p->next;
			continue;
		}

		if(least_qobj == NULL) { /* first scan */
			least_qobj = qobj;
		}
		else {
			int current_diff =  qobj->on_get_diff( 0, qobj);
			int least_diff =  least_qobj->on_get_diff( 0, least_qobj);

			fq_log(objs->l, FQ_LOG_DEBUG, "current_diff=%d, least_diff=%d.", current_diff, least_diff);

			if( current_diff < least_diff ) {
				least_qobj = qobj;
			}
		}

		p = p->next;
	}

	return(least_qobj);
}

static fqueue_obj_t *on_find_qobj(fq_objs_t *objs, char *path, char *qname)
{
	ll_node_t *p;
	fqueue_obj_t *qobj=NULL;

	if( !objs ) return(false);
	
	p = objs->ll->head;
	while(p) {
		int rc;
		char initial;
		char kind;

		group_queue_t *this = (group_queue_t *)p->value;

		qobj = this->obj;
		initial = this->initial;
		kind = this->kind;

		if( strcmp(qobj->path, path) == 0 && strcmp(qobj->qname, qname)==0 ) { /* found */
				return( this->obj );
		}

		p = p->next;
	}

	return(NULL); /* not found */
}

bool OpenFileQueues(fq_logger_t *l, char *list_file, fq_objs_t **objs)
{
	fq_objs_t *rc = NULL;

	FQ_TRACE_ENTER(l);

	rc = (fq_objs_t *)calloc(1,  sizeof(fq_objs_t));
	if(!rc) {
		fq_log(l, FQ_LOG_ERROR, "calloc() error.");		
		FQ_TRACE_EXIT(l);
		return(false);
	}
	rc->l = l;

	rc->ll = linkedlist_new("queues_list");
	if(!rc->ll) {
		fq_log(l, FQ_LOG_ERROR, "linkedlist_new() error.");		
		FQ_TRACE_EXIT(l);
		return(false);
	}

	char map_delimiter=':';
	bool tf;
	tf = MakeLinkedList_with_mapfile(l, list_file, map_delimiter, rc->ll, &rc->length);
	if(tf == false) {
		fq_log(l, FQ_LOG_ERROR, "linkedlist_new() error.");		
		FQ_TRACE_EXIT(l);
		return(false);
	}

	rc->on_get_key_qobj = on_get_key_qobj;
	rc->on_get_least_qobj = on_get_least_qobj;
	rc->on_get_key_least_qobj = on_get_key_least_qobj;
	rc->on_find_qobj = on_find_qobj;

	*objs = rc;
	FQ_TRACE_EXIT(l);
	
	return(true);
}

bool CloseFileQueues(fq_logger_t *l, fq_objs_t *objs)
{
	ll_node_t *p;

	if( !objs ) return(false);
	
	p = objs->ll->head;
	while(p) {
		int rc;
		group_queue_t *this = (group_queue_t *)p->value;

		rc=CloseFileQueue(l, &this->obj);
		CHECK(rc==TRUE);

		p = p->next;
	}

	linkedlist_free(&objs->ll);

	if(objs) free(objs);

	return(true);
}


static bool MakeLinkedList_with_mapfile(fq_logger_t *l, char *mapfile, char delimiter, linkedlist_t *ll, int *length)
{
	file_list_obj_t *filelist_obj=NULL;
	FILELIST *this_entry=NULL;
	int rc;
	int t_no;

	rc = open_file_list(l, &filelist_obj, mapfile);
	CHECK( rc == TRUE );

	fq_log(l, FQ_LOG_DEBUG, "filelist count = [%d].", filelist_obj->count);

	*length = filelist_obj->count;

	if( FQ_IS_DEBUG_LEVEL(l) ) {
	 	filelist_obj->on_print(filelist_obj);
	}

	this_entry = filelist_obj->head;
	for( t_no=0;  (this_entry->next && this_entry->value); t_no++ ) {
		fq_log(l, FQ_LOG_DEBUG, "this_entry->value=[%s].", this_entry->value);

		bool tf;
		char co_initial[2];
		char qkind[2];
		char q_path_qname[255];
#if 0
		tf = get_LR_value(this_entry->value, delimiter, co_initial, q_path_qname);
		fq_log(l, FQ_LOG_DEBUG, "co_initial='%s', q_path_qname='%s'.", co_initial, q_path_qname);
#else
		int cnt;
		cnt = fq_sscanf(delimiter, this_entry->value, "%s%s%s", co_initial, qkind, q_path_qname);
		CHECK(cnt == 3);
#endif

		char *ts1 = strdup(q_path_qname);
		char *ts2 = strdup(q_path_qname);
		char *qpath=dirname(ts1);
		char *qname=basename(ts2);
		
		fq_log(l, FQ_LOG_DEBUG, "qpath='%s', qname='%s'.", qpath, qname);

		group_queue_t *tmp=NULL;
		tmp = (group_queue_t *)calloc(1, sizeof(group_queue_t));
		tmp->initial = co_initial[0];
		tmp->kind = qkind[0];
		rc =  OpenFileQueue(l, NULL, NULL, NULL, NULL, qpath, qname, &tmp->obj);
		CHECK(rc == TRUE);

		ll_node_t *ll_node=NULL;
		ll_node = linkedlist_put(ll, co_initial, (void *)tmp, sizeof(char)+sizeof(fqueue_obj_t));
		CHECK(ll_node);

		if(ts1) free(ts1);
		if(ts2) free(ts2);

		this_entry = this_entry->next;
	}

	if( filelist_obj ) close_file_list(l, &filelist_obj);

	return true;
}
#if 0
int main(int ac, char **av)
{
	fq_logger_t *l=NULL;
	fq_objs_t *objs = NULL;
	bool tf;
	char group_initial[2];

	tf = OpenFileQueues(l, "/home/gwisangchoi/work/qobjs/co_queue.map", &objs);
	CHECK(tf == true);

	fqueue_obj_t *qobj=NULL;

loop:
	/* get a queue object by same group_initial */
	get_seed_ratio_rand_str( group_initial, sizeof(group_initial), "KLSN");
	qobj = objs->on_get_key_qobj(objs, group_initial[0]);
	CHECK(qobj);
	printf("group=[%c] path=%s, qname=%s\n", group_initial[0], qobj->path, qobj->qname);

	/* get a queue object with least diff */
	qobj = objs->on_get_least_qobj(objs);
	CHECK(qobj);
	printf("least -> path=%s, qname=%s\n", qobj->path, qobj->qname);

	/* get a queue object with least diff */
	qobj = objs->on_find_qobj(objs, "/home/gwisangchoi/enmq", "TST4");
	CHECK(qobj);
	printf("found -> path=%s, qname=%s\n", qobj->path, qobj->qname);

 	usleep(1000);
 	goto loop;

	tf = CloseFileQueues(l, objs);
	CHECK(tf == true);
	return(0);
}
#endif
