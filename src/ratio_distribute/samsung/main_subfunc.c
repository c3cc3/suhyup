#include <stdio.h>
#include <stdbool.h>
#include <libgen.h>

#include "fq_common.h"
#include "fq_linkedlist.h"
#include "fq_delimiter_list.h"
#include "fq_ratio_distribute.h"

#include "fq_codemap.h"
#include "fqueue.h"
#include "fq_file_list.h"
#include "ums_common_conf.h"
#include "ratio_dist_conf.h"
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

#include "main_subfunc.h"

int LocalGetHash( fq_logger_t *l, hashmap_obj_t *hash_obj, char *key, char **value);

/* you must free dst pointer after using */
bool json_get_value_malloc( fq_logger_t *l, JSON_Object *ums_JSONObject, char *json_key, char *datatype, char **dst, long *dst_len)
{
	char buffer[8192];
	char *p=NULL;


	FQ_TRACE_ENTER(l);

	ASSERT(ums_JSONObject);
	ASSERT(json_key);
	ASSERT(datatype);
	
	p = strstr(json_key, ".");

	memset(buffer, 0x00, sizeof(buffer));

	if(strcmp( datatype, "String") == 0 ) {
		if( !p ) { 
			sprintf(buffer, "%s", json_object_get_string(ums_JSONObject, json_key));
		}
		else {
			sprintf(buffer, "%s", json_object_dotget_string(ums_JSONObject, json_key)); 
		}
		*dst_len = strlen(buffer);
		*dst = strdup(buffer);
	}
	else if(strcmp( datatype, "Number") == 0 ) {
		if( !p ) { 
			sprintf(buffer, "%f", json_object_get_number(ums_JSONObject, json_key));
		}
		else {
			sprintf(buffer, "%f", json_object_dotget_number(ums_JSONObject, json_key));
		}
		*dst = strdup(buffer);
	}
	else if(strcmp( datatype, "Boolean") == 0 ) {
		if( !p ) { 
			sprintf(buffer, "%d", json_object_get_boolean(ums_JSONObject, json_key));
		}
		else {
			sprintf(buffer, "%d", json_object_dotget_boolean(ums_JSONObject, json_key));
		}
		*dst = strdup(buffer);
	}
	else {
		FQ_TRACE_EXIT(l);
		return(false);
	}
	
	FQ_TRACE_EXIT(l);
	return true;
}
	
bool json_auto_caching( fq_logger_t *l, cache_t *cache_short_for_gssi, JSON_Object *ums_JSONObject, json_key_datatype_t *t)
{
	char *dst = NULL;
	long dst_len=0L;


	FQ_TRACE_ENTER(l);

	ASSERT(cache_short_for_gssi);
	ASSERT(ums_JSONObject);
	ASSERT(t);
	
	// printf("auto caching: key=[%s] , mandatory=[%d], length=[%d]\n", t->json_key, t->mandatory, t->length);

	if( json_get_value_malloc( l, ums_JSONObject, t->json_key, t->datatype, &dst, &dst_len) ) {

		// fq_log(l, FQ_LOG_DEBUG, "dst_len = [%ld], dst[0] = [%d]", dst_len, dst[0]);
		// printf("\t- dst_len = [%ld], dst = [%s]\n", dst_len, dst);

		// if( (mandatory == true) && (dst[0]==0 || dst[0] == 40) ) {
		if(  dst[0] == 40 && t->mandatory ) {
			fq_log(l, FQ_LOG_ERROR, "[%s] is mandatory.", t->json_key);
			fprintf(stderr, "[%s] is mandatory.\n", t->json_key);
			FQ_TRACE_EXIT(l);
			return(false);
		} 
		else {
			if( t->length != 0 ) {
				int len = strlen(dst);

				if( t->length != len ) {
					fq_log(l, FQ_LOG_ERROR, "lengh error.  key=[%s] value=[%s], rule:[%d] , input=[%d].", 
						 t->json_key, dst, t->length, len);
					FQ_TRACE_EXIT(l);
					return(false);
				}
			}
			cache_update(cache_short_for_gssi, t->cache_key, dst, 0);
			fq_log(l, FQ_LOG_INFO, "caching: key='%s', value='%s'.", t->cache_key, dst);
		}
	}
	else {
		fq_log(l, FQ_LOG_ERROR, "Unsupport DataType. Check your auto cache mapfile.");
		fprintf(stderr, "Unsupport DataType. Check your auto cache mapfile \n");
		FQ_TRACE_EXIT(l);
		return(false);
	}
	SAFE_FREE(dst);

	FQ_TRACE_EXIT(l);
	return true;
}

bool update_channel_ratio(fq_logger_t *l, ctrl_msg_t *ctrl_msg, linkedlist_t *channel_co_ratio_obj_ll)
{
	FQ_TRACE_ENTER(l);

	ASSERT(ctrl_msg);
	ASSERT(channel_co_ratio_obj_ll);

	fq_log(l, FQ_LOG_EMERG, "Takeout new_ratio from control Q.");
	fq_log(l, FQ_LOG_EMERG, "\t- Takeout cmd: %d." , ctrl_msg->cmd);
	fq_log(l, FQ_LOG_EMERG, "\t- Takeout channel: %s." , ctrl_msg->channel);
	fq_log(l, FQ_LOG_EMERG, "\t- Takeout new_ratio: '%s'.", ctrl_msg->new_ratio_string);
	
	ll_node_t *channel_co_ratio_obj_map_node=NULL;

	channel_co_ratio_obj_map_node = linkedlist_find(l, channel_co_ratio_obj_ll, ctrl_msg->channel);

	if( channel_co_ratio_obj_map_node ) { /* found */
		char *new_ratio_src = ctrl_msg->new_ratio_string;
		char *new_rand_seed_string = NULL;

		/* We will forward it to media  directly */
		channel_ratio_obj_t *this = (channel_ratio_obj_t *) channel_co_ratio_obj_map_node->value;
		ratio_obj_t	*ratio_obj = this->ratio_obj;
		// fprintf(stdout, "Selected a ratio object. It's string: '%s'.\n", ratio_obj->ratio_string);
		fq_log(l, FQ_LOG_INFO,  "Selected a ratio object. It's string: '%s'.", 
			ratio_obj->ratio_string);

		char  delimiter = ',';
		char  sub_delimiter = ':';

		new_rand_seed_string  = make_seed_string(l, ratio_obj, new_ratio_src, delimiter, sub_delimiter );
		if( new_rand_seed_string ) {
			ratio_obj->on_change_ratio_seed(l, ratio_obj, new_rand_seed_string);

			fprintf(stdout, "ratio changed: %s\n", new_ratio_src);
			fq_log(l, FQ_LOG_EMERG, "ratio changed: %s.", new_ratio_src);
		}
		else {
			fq_log(l, FQ_LOG_ERROR, "make_seed_string() error.");
			FQ_TRACE_EXIT(l);
			return false;
		}
		FREE(new_rand_seed_string);
	}
	else {
		fq_log(l, FQ_LOG_ERROR, "Undefined channel: We can't found a node of [%s] in the linkedlist. We will throw it.", ctrl_msg->channel);
		fprintf( stderr, "Undefined channel: We can't found a node of [%s] in the linkedlist. We will throw it.\n", ctrl_msg->channel);
		// We return true for continue;
	}

	FQ_TRACE_EXIT(l);
	return true;
}
bool down_channel_ratio(fq_logger_t *l, ctrl_msg_t *ctrl_msg, linkedlist_t *channel_co_ratio_obj_ll)
{

	FQ_TRACE_ENTER(l);

	ASSERT(ctrl_msg);
	ASSERT(channel_co_ratio_obj_ll);

	fq_log(l, FQ_LOG_EMERG, "Takeout CO_DOWN from control Q.");
	fq_log(l, FQ_LOG_EMERG, "\t- Takeout cmd: %d." , ctrl_msg->cmd);
	fq_log(l, FQ_LOG_EMERG, "\t- Takeout channel: %s." , ctrl_msg->channel);
	fq_log(l, FQ_LOG_EMERG, "\t- Takeout co_name: '%s'.", ctrl_msg->co_name);

	ll_node_t *channel_co_ratio_obj_map_node=NULL;
	channel_co_ratio_obj_map_node = linkedlist_find(l, channel_co_ratio_obj_ll, ctrl_msg->channel);

	if( channel_co_ratio_obj_map_node ) { /* found */
		channel_ratio_obj_t *this = (channel_ratio_obj_t *) channel_co_ratio_obj_map_node->value;
		ratio_obj_t	*ratio_obj = this->ratio_obj;

		fq_log(l, FQ_LOG_EMERG, "\t- Current ratio_string: '%s'.", ratio_obj->ratio_string);
		// fprintf( stdout, "Selected a ratio object. It's ratio_string: '%s'.\n", ratio_obj->ratio_string);
		co_down_new_ratio( l, ratio_obj, ctrl_msg->co_name[0]);
		
		fq_log(l, FQ_LOG_EMERG, "\t- Changed new ratio_string: '%s'.", ratio_obj->ratio_string);
		fprintf( stdout, "Chaned new ratio_string: '%s'.\n", ratio_obj->ratio_string);
	}
	else {
		fq_log(l, FQ_LOG_ERROR, "Undefined channel: We can't found a node of [%s] in the linkedlist. We will throw it.", ctrl_msg->channel);
		fprintf( stderr, "Undefined channel: We can't found a node of [%s] in the linkedlist. We will throw it.\n", ctrl_msg->channel);
	}

	FQ_TRACE_EXIT(l);
	return true;
}
bool json_caching_and_SSIP( fq_logger_t *l, JSON_Value *in_json, JSON_Object *root,  ratio_dist_conf_t *my_conf, linkedlist_t *json_key_datatype_obj_ll, unsigned char *ums_msg, cache_t *cache_short_for_gssi, char *channel, char *seq_check_id)
{
	FQ_TRACE_ENTER(l);

	ASSERT(in_json);
	ASSERT(root);
	ASSERT(ums_msg);
	ASSERT(cache_short_for_gssi);

	char *dst = NULL;
	long dst_len;
	bool tf;

	tf = json_get_value_malloc(l, root, my_conf->seq_check_key_name, "String", &dst, &dst_len);
	CHECK(tf);

	sprintf(seq_check_id, "%s", dst);
	fq_log( l, FQ_LOG_INFO, "seq_check_id =>%s.", seq_check_id);

	if( strlen(seq_check_id) <  6 ) { 
		fprintf(stderr, "There is no seq_check_id.\n");
		fq_log(l, FQ_LOG_ERROR, "There is no seq_check_id . We will throw it.\n");
		FQ_TRACE_EXIT(l);
		return false;
	}

	fq_log(l, FQ_LOG_INFO, "We succeeded in getting the seq_check_id information from json.('%s')", seq_check_id);
	// fprintf(stdout, "We succeeded in getting the seq_check_id information from json.('%s').\n", seq_check_id);

	SAFE_FREE(dst);

#if 0
	bool GssiFlag = json_object_get_boolean(root, "GssiFlag");
	if( GssiFlag ) {
		char buf_var_data[2048];
		sprintf(buf_var_data, "%s", json_object_get_string(root, my_conf->var_data_json_key ));
		fq_log(l, FQ_LOG_INFO, "Vardata='%s'.", buf_var_data);
		
		char buf_template[2048];
		sprintf(buf_template, "%s", json_object_get_string(root, my_conf->template_json_key));
		fq_log(l, FQ_LOG_INFO, "Template='%s'", buf_template);

		if( strlen(buf_var_data) < 1 || strlen(buf_template) < 6) {
			fprintf(stderr, "illegal grammer.\n");
			fq_log(l, FQ_LOG_ERROR, "illegal grammer. We will throw it.\n");
			FQ_TRACE_EXIT(l);
			return false;
		}

		unsigned char SSIP_buf[8192];
		int rc;
		rc = gssi_proc( l, buf_template, buf_var_data, "", cache_short_for_gssi, '|', SSIP_buf, sizeof(SSIP_buf));
		if( rc < 0 ) {
			fq_log(l, FQ_LOG_ERROR, "gssi_proc() error( not found tag ). rc=[%d].", rc);
			FQ_TRACE_EXIT(l);
			return(-1);
		}
		json_object_set_null(root, my_conf->msg_json_target_key);
		json_object_set_string(root, my_conf->msg_json_target_key, SSIP_buf);

		fq_log(l, FQ_LOG_INFO, "GSSI processing was successful. GssiFlag=[%d]", GssiFlag);
	}
	else {
		fq_log(l, FQ_LOG_INFO, "There is No GSSI processing.  GssiFlag=[%d]", GssiFlag);
	}
#endif

	/* auto caching */
	/* json_object_dotget_string(ums_JSONObject,  tmp->json_key)); */
	ll_node_t *node_p = NULL;
	for( node_p=json_key_datatype_obj_ll->head; (node_p != NULL); (node_p=node_p->next) ) {

		json_key_datatype_t *tmp;
		tmp = (json_key_datatype_t *) node_p->value;

		bool cache_result = json_auto_caching( l, cache_short_for_gssi, root,  tmp);

		if( cache_result == false ) {
			fq_log(l, FQ_LOG_ERROR, "json caching error.");
			FQ_TRACE_EXIT(l);
			return false;
		}
	}
	
	char date[8+1], time[6+1], current_date_time[17];
	get_time(date, time);
	sprintf(current_date_time, "%s%s", date, time);
	cache_update(cache_short_for_gssi, "DIST_Q_DTTM", current_date_time, 0);

	fq_log(l, FQ_LOG_INFO, "It was done by doing the necessary caching on the distributor(splitter).");
	fq_log(l, FQ_LOG_INFO, "\t - DIST_Q_DTTM: %s.", current_date_time);

	FQ_TRACE_EXIT(l);
	return true;
}

bool make_JSON_stream_by_rule_and_enQ( fq_logger_t *l, JSON_Object *in_json_obj, char *key, char *channel, unsigned char *SSIP_result, cache_t *cache_short_for_gssi, ratio_dist_conf_t *my_conf, fqueue_obj_t *this_q, fqueue_obj_t *deq_obj, char *unlink_filename, long l_seq,  char co_initial, linkedlist_t *co_chanel_json_rule_obj_ll)
{
	/* select a qformat */
	ll_node_t *co_chanel_json_rule_map_node = NULL;
	char rule_find_key[5];

	FQ_TRACE_ENTER(l);

	memset(rule_find_key, 0x00, sizeof(rule_find_key));
	sprintf(rule_find_key, "%c-%s", co_initial, channel);

	co_chanel_json_rule_map_node  = linkedlist_find(l, co_chanel_json_rule_obj_ll, rule_find_key);
	CHECK(co_chanel_json_rule_map_node);
	fq_log(l, FQ_LOG_INFO, "Select a rule for creating a sending message. key='%s', channel='%s' rule_find_key='%s'", 
			key, channel, rule_find_key);

	co_channel_json_rule_t *this_json_rule = (co_channel_json_rule_t *) co_chanel_json_rule_map_node->value;
	CHECK(this_json_rule);

	fq_log(l, FQ_LOG_INFO, "One rule was selected normally.");

	linkedlist_t *this_json_rule_ll = this_json_rule->json_tci_ll;

	char *serialized_json_msg = NULL;

	bool pretty_flag = true;
	bool tf;
	tf = make_serialized_json_by_rule(l, this_json_rule_ll, cache_short_for_gssi, in_json_obj, &serialized_json_msg, pretty_flag);
	if( tf == false ) {
		fq_log(l, FQ_LOG_ERROR, "make_serialized_json() error. We will commit for deleting.");
		commit_Q(l, deq_obj, unlink_filename, l_seq);
		SAFE_FREE(serialized_json_msg);
		FQ_TRACE_EXIT(l);
		return true;
	}
	fq_log(l, FQ_LOG_INFO, "A message was created according to the rules.");
	fq_log(l, FQ_LOG_INFO, "serialized json msg(enQ msg): '%s'.", serialized_json_msg);

	int rc;
	rc = uchar_enQ(l, serialized_json_msg, strlen(serialized_json_msg), this_q);

	if( rc < 0 ) {
		cancel_Q(l, deq_obj, unlink_filename, l_seq);		
		fq_log(l, FQ_LOG_ERROR, "Failed to put a message in queue.");
	}
	else {
		commit_Q(l, deq_obj, unlink_filename, l_seq);
		fq_log(l, FQ_LOG_INFO, "Successfully put a message in queue. rc=[%d]", rc);
	}

	SAFE_FREE(serialized_json_msg);

	FQ_TRACE_EXIT(l);

	return true;
}

bool forward_channel( fq_logger_t *l, char *channel, ll_node_t *forward_channel_queue_map_node, unsigned char *ums_msg, size_t ums_msg_len, fqueue_obj_t *deq_obj, char *unlink_filename, long l_seq)
{

	FQ_TRACE_ENTER(l);

	/* We will forward it to media  directly */
	channel_queue_t *this = (channel_queue_t *) forward_channel_queue_map_node->value;
	fqueue_obj_t	*this_q = this->obj;
	fprintf( stdout, "Selected a queue: path='%s', name='%s'.\n", this_q->path, this_q->qname);
	fq_log(l, FQ_LOG_INFO, "Selected a queue: path='%s', name='%s'.", this_q->path, this_q->qname);
	fprintf( stdout, "We will forward it to [%s].\n", channel);

	int rc;
	rc = uchar_enQ(l, ums_msg, ums_msg_len, this_q);
	if( rc < 0 ) {
		cancel_Q(l, deq_obj, unlink_filename, l_seq);		
	}
	else {
		commit_Q(l, deq_obj, unlink_filename, l_seq);
	}

	fq_log(l, FQ_LOG_INFO, "forwarding ok.");

	FQ_TRACE_EXIT(l);
	return true;
}

bool ratio_forward_channel( fq_logger_t *l, char *channel, fqueue_obj_t *this_q, unsigned char *ums_msg, size_t ums_msg_len, fqueue_obj_t *deq_obj, char *unlink_filename, long l_seq)
{

	FQ_TRACE_ENTER(l);

	/* We will forward it to media  directly */
	fq_log(l, FQ_LOG_INFO, "Selected a queue: path='%s', name='%s'.", this_q->path, this_q->qname);


	int rc;
	rc = uchar_enQ(l, ums_msg, ums_msg_len, this_q);
	if( rc < 0 ) {
		cancel_Q(l, deq_obj, unlink_filename, l_seq);		
	}
	else {
		commit_Q(l, deq_obj, unlink_filename, l_seq);
	}

	fq_log(l, FQ_LOG_INFO, "forwarding ok.");

	FQ_TRACE_EXIT(l);
	return true;
}

fqueue_obj_t *Select_a_queue_obj_and_co_initial_by_least_usage( fq_logger_t *l, char *channel, linkedlist_t *channel_co_queue_obj_ll, codemap_t *queue_co_initial_codemap, char *key, char *co_initial)
{

	/* find a queue having least gap */
	fqueue_obj_t *qobj = NULL;

	FQ_TRACE_ENTER(l);


	ASSERT(channel_co_queue_obj_ll);
	ASSERT(queue_co_initial_codemap);
	ASSERT(key);

	qobj  = find_least_gap_qobj_in_channel_co_queue_map( l, channel, channel_co_queue_obj_ll );
	CHECK(qobj);

	fprintf(stdout, "Current least queue is [%s/%s] for [%s].\n",  
		qobj->path, qobj->qname, channel);
	fq_log(l, FQ_LOG_INFO, "Current least queue is [%s/%s] for [%s].",  
		qobj->path, qobj->qname, channel);

	/* find a co_initial with qpath/qname in co_initial and queue map. */
	char tmp_key[128]; /* qpath/qname */
	char tmp_co_initial[16]; /* co-initial */
	int rc;
	
	sprintf(tmp_key, "%s/%s", qobj->path, qobj->qname);

	rc = get_codeval(l, queue_co_initial_codemap, tmp_key, tmp_co_initial);
	CHECK(rc == 0);

	sprintf(key, "%s-%s", channel, tmp_co_initial);

	*co_initial = tmp_co_initial[0];

	FQ_TRACE_EXIT(l);
	return qobj;
}

fqueue_obj_t *Select_a_queue_obj_and_co_initial_by_ratio( fq_logger_t *l, char *channel, linkedlist_t *channel_co_ratio_obj_ll, linkedlist_t *channel_co_queue_obj_ll, codemap_t *queue_co_initial_codemap, char *key, char *co_initial)
{
	FQ_TRACE_ENTER(l);

	// We get a ratio object with a channel ID and
	ll_node_t	*this_ratio_node=NULL;
	this_ratio_node = linkedlist_find(l,  channel_co_ratio_obj_ll , channel);
	if( !this_ratio_node ) {
		fq_log(l, FQ_LOG_ERROR, "Unknown ratio channel ID.(value='%s') We will skip it.", channel);
		fprintf( stderr, "Unknown ratio channel ID.(value='%s') We will skip it.\n", channel);
		FQ_TRACE_EXIT(l);
		return NULL;
	}

	channel_ratio_obj_t *this = (channel_ratio_obj_t *) this_ratio_node->value;
	ratio_obj_t *this_ratio = this->ratio_obj;

	fq_log(l, FQ_LOG_INFO, "Current ratio string is =[%s] for [%s].", 
		this_ratio->ratio_string, channel);

	// We get a co_initial of selected co.
	*co_initial = this_ratio->on_select(l, this_ratio);
	fq_log(l, FQ_LOG_INFO, "selected co_initial = [%c].", *co_initial);

	// We get a queue object with co_initial .
	ll_node_t *map_node = NULL;
	sprintf(key, "%s-%c", channel, *co_initial);
	fq_log(l, FQ_LOG_INFO, "key=[%s]\n", key);

	/* We get a fqueue object with a key(Co initial) */
	map_node = linkedlist_find(l, channel_co_queue_obj_ll, key);
	CHECK(map_node);

	fq_log(l, FQ_LOG_INFO, "selected a queue object by ratio. ok. co_initial = [%c]", *co_initial);

	channel_co_queue_t *this_coq = (channel_co_queue_t *) map_node->value;
	fqueue_obj_t *qobj = this_coq->obj;

	fq_log(l, FQ_LOG_INFO, "Selected a queue: path='%s', name='%s'.", qobj->path, qobj->qname);

	FQ_TRACE_EXIT(l);
	return qobj;
}

fqueue_obj_t *Select_a_queue_obj_by_co_initial(fq_logger_t *l, char *channel, char co_initial,  linkedlist_t *channel_co_queue_obj_ll)
{
	FQ_TRACE_ENTER(l);

	// We get a queue object with co_initial .
	char key[16];

	memset(key, 0x00, sizeof(key));
	sprintf(key, "%s-%c", channel, co_initial);
	// fprintf( stdout, "key=[%s]\n", key);

	/* We get a fqueue object with a key(Co initial) */
	ll_node_t *map_node = NULL;
	map_node = linkedlist_find(l, channel_co_queue_obj_ll, key);
	CHECK(map_node);

	fq_log(l, FQ_LOG_INFO, "selected a queue object by co_initial. ok. co_initial = [%c]", co_initial);

	channel_co_queue_t *this_coq = (channel_co_queue_t *) map_node->value;
	fqueue_obj_t *qobj = this_coq->obj;

	fq_log(l, FQ_LOG_INFO, "Selected a queue: path='%s', name='%s'.", qobj->path, qobj->qname);

	FQ_TRACE_EXIT(l);
	return qobj;
}

/* This function has memory leak */
bool is_guarantee_user( fq_logger_t *l, hashmap_obj_t *seq_check_hash_obj, char *seq_check_id, char *co_in_hash, char *channel ) 
{
	char *get_value = NULL;
	int rc;


	if( !seq_check_hash_obj ) return false;
	if( !seq_check_id ) return false;

	FQ_TRACE_ENTER(l);

retry:
	fq_log(l, FQ_LOG_INFO, "start ");
	fq_log(l, FQ_LOG_INFO, "seq_check_id='%s, channel='%s'", seq_check_id, channel);

	rc = LocalGetHash(l, seq_check_hash_obj, seq_check_id, &get_value);
	if( rc != TRUE ) { /* not found */
		SAFE_FREE(get_value);
		FQ_TRACE_EXIT(l);
		fq_log(l, FQ_LOG_INFO, "end");
		return false;
	}
	else { /* already used a co */
		if(get_value == NULL) {
			fq_log(l, FQ_LOG_ERROR, "LocalGetHash() value is null. retry.");
			SAFE_FREE(get_value);
			usleep(100000);
			goto retry;		
		}
		char *p =  NULL;
		p = strstr(get_value, channel);
		if( p && get_value) {
			// printf("We will put [%s] to [%s] for sequence guarantee.\n", seq_check_id, get_value );
			fq_log(l, FQ_LOG_INFO, "We will put [%s] to [%s] for sequence guarantee.", seq_check_id, get_value );

			*co_in_hash = *get_value;
			SAFE_FREE(get_value);
			FQ_TRACE_EXIT(l);
			fq_log(l, FQ_LOG_INFO, "end");
			return true;
		}
		else {
			SAFE_FREE(get_value);
			FQ_TRACE_EXIT(l);
			fq_log(l, FQ_LOG_INFO, "end");
			return false;
		}
	}
}

bool json_get_value_malloc_local( fq_logger_t *l, JSON_Object *ums_JSONObject, char *json_key, char *datatype, char **dst, long *dst_len)
{

	FQ_TRACE_ENTER(l);

	char buffer[8192];
	char *p=NULL;
	p = strstr(json_key, ".");

	memset(buffer, 0x00, sizeof(buffer));

	if(strcmp( datatype, "string") == 0 ) {
		if( !p ) { 
			sprintf(buffer, "%s", json_object_get_string(ums_JSONObject, json_key));
			fq_log(l, FQ_LOG_DEBUG, "json_object_get_string('%s') result='%s'\n", json_key, buffer);
		}
		else {
			sprintf(buffer, "%s", json_object_dotget_string(ums_JSONObject, json_key)); 
		}
		*dst_len = strlen(buffer);
		*dst = strdup(buffer);
	}
	else if(strcmp( datatype, "number") == 0 ) {
		if( !p ) { 
			sprintf(buffer, "%f", json_object_get_number(ums_JSONObject, json_key));
		}
		else {
			sprintf(buffer, "%f", json_object_dotget_number(ums_JSONObject, json_key));
		}
		*dst = strdup(buffer);
	}
	else if(strcmp( datatype, "bool") == 0 ) {
		if( !p ) { 
			sprintf(buffer, "%d", json_object_get_boolean(ums_JSONObject, json_key));
		}
		else {
			sprintf(buffer, "%d", json_object_dotget_boolean(ums_JSONObject, json_key));
		}
		*dst = strdup(buffer);
	}
	else {
		fq_log(l, FQ_LOG_ERROR, "[%s] -> Unsupport data type.", datatype);
		FQ_TRACE_EXIT(l);
		return(false);
	}
	
	FQ_TRACE_EXIT(l);
	return true;
}

bool json_set_value( fq_logger_t *l, JSON_Object *ums_JSONObject, char *json_key, char *datatype, char *src)
{

	FQ_TRACE_ENTER(l);

	char buffer[8192];
	char *p=NULL;
	p = strstr(json_key, ".");

	if(strcmp( datatype, "string") == 0 ) {
		if( !p ) { 
			json_object_set_string(ums_JSONObject, json_key, src);
			fq_log(l, FQ_LOG_DEBUG, "json_object_set_string('%s', '%s').", json_key, src);
		}
		else {
			json_object_dotset_string(ums_JSONObject, json_key, src); 
		}
	}
	else if(strcmp( datatype, "number") == 0 ) {
		long n;
		n = atol(src);
		if( !p ) { 
			json_object_set_number(ums_JSONObject, json_key, n);
		}
		else {
			json_object_dotset_number(ums_JSONObject, json_key, n);
		}
	}
	else if(strcmp( datatype, "bool") == 0 ) {
		if( !p ) { 
			if( STRCCMP(src, "1") == 0 ) {
				json_object_set_boolean(ums_JSONObject, json_key, true);
			}
			else {
				json_object_set_boolean(ums_JSONObject, json_key, false);
			}
		}
		else {
			if( STRCCMP(src, "1") == 0 ) {
				json_object_dotset_boolean(ums_JSONObject, json_key, true);
			}
			else {
				json_object_dotset_boolean(ums_JSONObject, json_key, false);
			}
		}
	}
	else {
		fq_log(l, FQ_LOG_ERROR, "[%s] -> Unsupport data type.", datatype);
		FQ_TRACE_EXIT(l);
		return(false);
	}
	
	FQ_TRACE_EXIT(l);
	return true;
}
bool make_serialized_json_by_rule_local( fq_logger_t *l, linkedlist_t *json_tci_ll, cache_t *cache, JSON_Object *in_json, char **target, bool pretty_flag )
{
	JSON_Value *out_json = json_value_init_object();
	JSON_Object *root = json_value_get_object(out_json);
	JSON_Value *branch = json_value_init_array();
	JSON_Array *array_contents = json_value_get_array(branch);

	FQ_TRACE_ENTER(l);

    CHECK(json_tci_ll);
    CHECK(cache);
    CHECK(in_json);

	ll_node_t *node_p = NULL;
	for( node_p=json_tci_ll->head; (node_p != NULL); (node_p=node_p->next) ) {

		json_tci_t *tmp;
		tmp = (json_tci_t *) node_p->value;
	
		printf("find_key=[%s], target_key=[%s], datatype=[%s] source=[%s], fixed=[%s], mandatory=[%d]\n",
            tmp->find_key, tmp->target_key, tmp->datatype, tmp->source, tmp->fixed, tmp->mandatory  );


		if( strcmp( tmp->datatype, "string") == 0 ) {
			if(  strcmp( tmp->source, "json" ) == 0 ) {
				bool tf;
				char *dst=NULL;
				long  dst_len;
				tf = json_get_value_malloc_local( l, in_json, tmp->find_key, tmp->datatype, &dst, &dst_len);
				CHECK(tf);

				json_set_value(l, root, tmp->target_key, tmp->datatype, dst); // put array to root

				SAFE_FREE(dst);
			}
			else if(  strcmp( tmp->source, "cache" ) == 0 ) {
				char *p=NULL;
				p = cache_getval_unlocked(cache, tmp->find_key);
				if( !p ) {
					fq_log(l, FQ_LOG_ERROR, "'%s' not found from cache.",  tmp->find_key);
					goto return_false;
				}
				json_set_value(l, root, tmp->target_key, tmp->datatype, p); // put array to root
				//  json_object_set_string(root, tmp->target_key, p); // put array to root
			}
			else if(  strcmp( tmp->source, "fixed" ) == 0 ) {
				json_set_value(l, root, tmp->target_key, tmp->datatype, tmp->fixed); // put array to root
				// json_object_set_string(root, tmp->target_key, tmp->fixed); // put array to root
			}
			else {
				fq_log(l, FQ_LOG_ERROR, "[%s] -> unsupported source data.", tmp->source);
				FQ_TRACE_EXIT(l);
				return false;
			}
		}
		else if( strcmp( tmp->datatype, "number") == 0 ) {
			double d;
			if(  strcmp( tmp->source, "json" ) == 0 ) {
				bool tf;
				char *dst=NULL;
				long  dst_len;
				tf = json_get_value_malloc( l, in_json, tmp->find_key, tmp->datatype, &dst, &dst_len);
				CHECK(tf);

				json_set_value(l, root, tmp->target_key, tmp->datatype, dst); // put array to root
				// json_object_set_number(root, tmp->target_key, atol(dst)); // put array to root
				SAFE_FREE(dst);
			}
			else if(  strcmp( tmp->source, "cache" ) == 0 ) {
				char *p=NULL;
				p = cache_getval_unlocked(cache, tmp->find_key);
				if( !p ) {
					fq_log(l, FQ_LOG_ERROR, "'%s' not found from cache.",  tmp->find_key);
					goto return_false;
				}
				json_set_value(l, root, tmp->target_key, tmp->datatype, p); // put array to root
				// json_object_set_number(root, tmp->target_key, atol(p)); // put array to root
			}
			else {
				fq_log(l, FQ_LOG_ERROR, "[%s] -> unsupported source data.", tmp->source);
				goto return_false;
			}
		}
		else if( strcmp( tmp->datatype, "array") == 0 ) {
			if(  strcmp( tmp->source, "json" ) == 0 ) {

				// JSON_Array *in_array = json_object_get_array(in_json, "C0NTENTS");
				JSON_Array *in_array = json_object_get_array(in_json, (const char *)tmp->find_key);
				if( !in_array ) {
					fq_log(l, FQ_LOG_ERROR, "[%s] array : not found from in json.",  tmp->find_key);
					goto return_false;
				}
				fq_log(l, FQ_LOG_DEBUG, "array_count is [%ld].", json_array_get_count(in_array));

				long i;
				for (i = 0; i < json_array_get_count(in_array); i++) {
					JSON_Value *value_one = json_array_get_value(in_array, i);

					JSON_Value *new_value_one;
					new_value_one = json_value_deep_copy(value_one);

					CHECK(json_value_equals(value_one, new_value_one));

					JSON_Status rc;
					rc = json_array_append_value(array_contents, new_value_one);
					CHECK( rc == JSONSuccess);

				}
 				json_object_set_value(root, tmp->target_key, branch); // put array to root
			}
			else {
				fq_log(l, FQ_LOG_ERROR, "array: [%s] -> unsupported source data.", tmp->source);
				goto return_false;
			}
		}
		else if( strcmp( tmp->datatype, "value") == 0 ) {
			if(  strcmp( tmp->source, "json" ) == 0 ) {
				JSON_Value *in_value = json_object_get_value( in_json, tmp->find_key);
				if( !in_value ) {
					fq_log(l, FQ_LOG_ERROR, "[%s] value : not found from in json.",  tmp->find_key);
					goto return_false;
				}
				else {
					JSON_Value *new_value = json_value_deep_copy(in_value);
					json_object_set_value(root, tmp->target_key, new_value);
					fq_log(l, FQ_LOG_DEBUG, "value('%s') to value('%s') copy success!", tmp->find_key, tmp->target_key);
				}
			}
			else {
				fq_log(l, FQ_LOG_ERROR, "array: [%s] -> unsupported source data.", tmp->source);
				goto return_false;
			}
		}
		else if( strcmp( tmp->datatype, "bool") == 0 ) {
			if(  strcmp( tmp->source, "json" ) == 0 ) {
				bool tf;
				char *dst=NULL;
				long  dst_len;
				tf = json_get_value_malloc( l, in_json, tmp->find_key, tmp->datatype, &dst, &dst_len);
				CHECK(tf);
				if( atol(dst) == 1 ) {
					json_set_value(l, root, tmp->target_key, tmp->datatype, dst); // put array to root
				}
				else {
					json_set_value(l, root, tmp->target_key, tmp->datatype, dst); // put array to root
				}
				SAFE_FREE(dst);
			}
			else if( strcmp(tmp->source, "cache" )== 0 ){
				char *p=NULL;
				p = cache_getval_unlocked(cache, tmp->find_key);
				if( !p ) {
					fq_log(l, FQ_LOG_ERROR, "'%s' not found from cache.",  tmp->find_key);
					goto return_false;
				}
				if( STRCCMP( p, "true") == 0 ) {
					json_set_value(l, root, tmp->target_key, tmp->datatype, p); // put array to root
				}
				else {
					json_set_value(l, root, tmp->target_key, tmp->datatype, p); // put array to root
				}
			}
			else {
				fq_log(l, FQ_LOG_ERROR, "array: [%s] -> unsupported source data.", tmp->source);
				goto return_false;
			}
		}
		else {
			fq_log(l, FQ_LOG_ERROR, "[%s] -> unsupported data type data.\n", tmp->datatype);
			goto return_false;
		}
	}

	char *tmp=NULL;
    tmp = json_serialize_to_string(out_json);

	debug_json(l, out_json);

	*target = strdup(tmp);
	SAFE_FREE(tmp);


	json_array_clear(array_contents);
	json_value_free(branch);
	json_value_free(out_json);

	return true; 

return_false:
	FQ_TRACE_EXIT(l);
	return false;
}

void PutHash_Option( fq_logger_t *l, bool seq_checking_use_flag,  hashmap_obj_t *seq_check_hash_obj, char *seq_check_id, char *str_co_initial , char *channel)
{
	int rc;

	FQ_TRACE_ENTER(l);

	if( seq_checking_use_flag == true ) {
		char value[8];
		sprintf(value, "%s-%s", str_co_initial, channel);

		rc = PutHash(l, seq_check_hash_obj, seq_check_id, value);
		if( rc != TRUE ) {
			seq_check_hash_obj->on_clean_table(l, seq_check_hash_obj);
		}
	}
	FQ_TRACE_EXIT(l);
	return;
}

bool init_Hash_channel_co_ratio_obj_with_mapfile(fq_logger_t *l, char *mapfile, char delimiter, hashmap_obj_t *hash_obj)
{
	file_list_obj_t *filelist_obj=NULL;
	FILELIST *this_entry=NULL;
	int rc;
	int t_no;

	FQ_TRACE_ENTER(l);

	rc = open_file_list(l, &filelist_obj, mapfile);
	CHECK( rc == TRUE );

	printf("count = [%d]\n", filelist_obj->count);

	// filelist_obj->on_print(filelist_obj);

	this_entry = filelist_obj->head;
	for( t_no=0;  (this_entry->next && this_entry->value); t_no++ ) {
		printf("this_entry->value=[%s]\n", this_entry->value);

		bool tf;
		char channel_name[16];
		char co_ratio_string[255];
		tf = get_LR_value(this_entry->value, delimiter, channel_name, co_ratio_string);
		printf("channel_name='%s', ratio_string='%s'.\n", channel_name, co_ratio_string);

		// For webgate , We initialize hash value.
		//
		char hash_put_key[128];
		sprintf(hash_put_key, "%s_RATIO", channel_name);
		rc = PutHash(l, hash_obj, hash_put_key, co_ratio_string);
    	CHECK(rc == TRUE);

		this_entry = this_entry->next;
	}
	if( filelist_obj ) close_file_list(l, &filelist_obj);

	FQ_TRACE_EXIT(l);

	return true;
}
bool json_parsing_and_get_channel( fq_logger_t *l, JSON_Value **in_json, JSON_Object **root, unsigned char *ums_msg,  char *channel_key, char **channel, char *svc_code_key, char **svc_code)
{

	FQ_TRACE_ENTER(l);

	*in_json = json_parse_string(ums_msg);
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
		// commit_Q(l, deq_obj, unlink_filename, l_seq);
	}

	char CHANNEL[32];
	char SVC_CODE[32];

	sprintf(CHANNEL, "%s", json_object_get_string(*root, channel_key));
	fq_log( l, FQ_LOG_INFO, "CHANNEL =>%s.", CHANNEL);
	*channel = strdup(CHANNEL);

	sprintf(SVC_CODE, "%s", json_object_get_string(*root, svc_code_key));
	fq_log( l, FQ_LOG_INFO, "SVC_CODE =>%s.", SVC_CODE);
	*svc_code = strdup(SVC_CODE);

	return true;
}
/* Get but do not delete(remain) */
int LocalGetHash( fq_logger_t *l, hashmap_obj_t *hash_obj, char *key, char **value)
{
	int rc;
	void *out=NULL;
	
	FQ_TRACE_ENTER(l);

	fq_log(l, FQ_LOG_INFO, "LocalGetHash(key='%s') start.", key);

	out = calloc(hash_obj->h->h->max_data_length+1, sizeof(unsigned char));
	rc = hash_obj->on_get(l, hash_obj, key, &out);

	fq_log(l, FQ_LOG_INFO, "on_get() end. rc=[%d]", rc);

	if( rc == MAP_MISSING ) { // rc == -3
		fq_log(l, FQ_LOG_INFO, "Not found: '%s', '%s': key(%s) missing in Hashmap.", hash_obj->h->h->path, hash_obj->h->h->hashname, key);
		SAFE_FREE(out);
		fq_log(l, FQ_LOG_INFO, "LocalGetHash() end.");
		return(FALSE);
	}
	else { // rc == 0
		if( out && key ) {
			fq_log(l, FQ_LOG_INFO, "'%s', '%s': Get OK. key=[%s] value=[%s]", hash_obj->h->h->path, hash_obj->h->h->hashname, key, out);
			//memcpy(*value, out, hash_obj->h->h->max_data_length);
			*value = strdup(out);
		}
		else {
			fq_log(l, FQ_LOG_INFO, "There is no out. LocalGetHash() end.");
            return(FALSE);
		}
	}
	SAFE_FREE(out);
	FQ_TRACE_EXIT(l);

	fq_log(l, FQ_LOG_INFO, "LocalGetHash() end.");
	return(TRUE);
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
