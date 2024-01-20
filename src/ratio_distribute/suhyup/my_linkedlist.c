/*
** my_linked_list.c
*/
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
#include "fq_mem.h"
#include "fq_scanf.h"

#include "parson.h"

#include "my_linkedlist.h"

bool MakeLinkedList_auto_cache_with_mapfile( fq_logger_t *l, char *mapfile, char delimiter, linkedlist_t	*ll)
{
	file_list_obj_t *filelist_obj=NULL;
	FILELIST *this_entry=NULL;
	int rc;
	int t_no;

	FQ_TRACE_ENTER(l);

	fq_log(l, FQ_LOG_INFO, "mapfile='%s", mapfile);
	rc = open_file_list(l, &filelist_obj, mapfile);
	CHECK( rc == TRUE );

	printf("count = [%d]\n", filelist_obj->count);

	// filelist_obj->on_print(filelist_obj);

	this_entry = filelist_obj->head;
	for( t_no=0;  (this_entry->next && this_entry->value); t_no++ ) {
		char *errmsg = NULL;
		printf("this_entry->value=[%s]\n", this_entry->value);

		bool tf;
		char json_key[128];
		char cache_key[128];
		char data_type[16];
		char mandatory_str[2];
		bool mandatory;
		char length_str[5];
		int	lenth;

		int cnt;
		cnt = fq_sscanf(delimiter, this_entry->value, "%s%s%s%s%s", 
			json_key, cache_key, data_type, mandatory_str, length_str);

		CHECK(cnt == 5);

		printf("json_key='%s', cache_key='%s', data_type='%s' mandatory='%s' length='%s'.\n", 
			json_key, cache_key, data_type, mandatory_str, length_str);

		json_key_datatype_t *tmp=NULL;
		tmp = (json_key_datatype_t *)calloc(1, sizeof(json_key_datatype_t));

		sprintf(tmp->json_key, "%s", json_key);
		sprintf(tmp->cache_key, "%s", cache_key);
		sprintf(tmp->datatype, "%s", data_type);
		if( mandatory_str[0] == 'Y' ||  mandatory_str[0] == 'y') {
			tmp->mandatory = true;
		}
		else {
			tmp->mandatory = false;
		}
		tmp->length = atoi(length_str);

		ll_node_t *ll_node=NULL;
		ll_node = linkedlist_put(ll, json_key, (void *)tmp, sizeof(char)+sizeof(json_key_datatype_t));
		CHECK(ll_node);

		// close_ratio_distributor(l, &tmp->ratio_obj);
		this_entry = this_entry->next;
	}

	if( filelist_obj ) close_file_list(l, &filelist_obj);

	printf("ll->length is [%d]\n", ll->length);

	FQ_TRACE_EXIT(l);
	return true;
}
bool MakeLinkedList_channel_co_ratio_obj_with_mapfile(fq_logger_t *l, char *mapfile, char delimiter, linkedlist_t *ll)
{
	file_list_obj_t *filelist_obj=NULL;
	FILELIST *this_entry=NULL;
	int rc;
	int t_no;

	fq_log(l, FQ_LOG_INFO, "mapfile='%s", mapfile);
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
		// char hash_put_key[128];
		// sprintf(put_key, "%s_RATIO", channel_name);
		// rc = PutHash(l, hash_obj, put_key, co_ratio_string);
    	// CHECK(rc == TRUE);


		char fields_delimiter=',';
		char value_delimiter = ':';

		channel_ratio_obj_t *tmp=NULL;
		tmp = (channel_ratio_obj_t *)calloc(1, sizeof(channel_ratio_obj_t));
		sprintf(tmp->channel_name, "%s", channel_name);

		tf =  open_ratio_distributor( l, co_ratio_string, fields_delimiter, value_delimiter, &tmp->ratio_obj);
		CHECK(tf);

		ll_node_t *ll_node=NULL;
		ll_node = linkedlist_put(ll, channel_name, (void *)tmp, sizeof(char)+sizeof(ratio_obj_t));
		CHECK(ll_node);

		// close_ratio_distributor(l, &tmp->ratio_obj);
		this_entry = this_entry->next;
	}
	if( filelist_obj ) close_file_list(l, &filelist_obj);


	printf("ll->length is [%d]\n", ll->length);

	FQ_TRACE_EXIT(l);
	return true;
}
bool MakeLinkedList_co_initial_FQ_obj_with_mapfile(fq_logger_t *l, char *mapfile, char delimiter, linkedlist_t *ll)
{
	file_list_obj_t *filelist_obj=NULL;
	FILELIST *this_entry=NULL;
	int rc;
	int t_no;

	FQ_TRACE_ENTER(l);

	// here-1
	printf("mapfile: %s.\n", mapfile);
	fq_log(l, FQ_LOG_INFO, "mapfile: %s.", mapfile);
	rc = open_file_list(l, &filelist_obj, mapfile);
	CHECK( rc == TRUE );

	printf("count = [%d]\n", filelist_obj->count);

	// filelist_obj->on_print(filelist_obj);

	this_entry = filelist_obj->head;
	for( t_no=0;  (this_entry->next && this_entry->value); t_no++ ) {
		printf("this_entry->value=[%s]\n", this_entry->value);

		bool tf;
		char channel_co_initial[5];
		char q_path_qname[255];
		tf = get_LR_value(this_entry->value, delimiter, channel_co_initial, q_path_qname);
		printf("channel_co_initial='%s', q_path_qname='%s'.\n", channel_co_initial, q_path_qname);

		char *ts1 = strdup(q_path_qname);
		char *ts2 = strdup(q_path_qname);
		char *qpath=dirname(ts1);
		char *qname=basename(ts2);
		
		fprintf(stdout, "qpath='%s', qname='%s'\n", qpath, qname);

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

bool MakeLinkedList_channel_name_FQ_obj_with_mapfile(fq_logger_t *l, char *mapfile, char delimiter, linkedlist_t *ll)
{
	file_list_obj_t *filelist_obj=NULL;
	FILELIST *this_entry=NULL;
	int rc;
	int t_no;

	FQ_TRACE_ENTER(l);

	fq_log(l, FQ_LOG_INFO, "mapfile='%s", mapfile);

	rc = open_file_list(l, &filelist_obj, mapfile);
	CHECK( rc == TRUE );

	printf("count = [%d]\n", filelist_obj->count);

	// filelist_obj->on_print(filelist_obj);

	this_entry = filelist_obj->head;
	for( t_no=0;  (this_entry->next && this_entry->value); t_no++ ) {
		printf("this_entry->value=[%s]\n", this_entry->value);

		bool tf;
		char channel_name[3];
		char q_path_qname[255];
		tf = get_LR_value(this_entry->value, delimiter, channel_name, q_path_qname);
		printf("channel_name='%s', q_path_qname='%s'.\n", channel_name, q_path_qname);

		char *ts1 = strdup(q_path_qname);
		char *ts2 = strdup(q_path_qname);
		char *qpath=dirname(ts1);
		char *qname=basename(ts2);
		
		fprintf(stdout, "qpath='%s', qname='%s'\n", qpath, qname);

		channel_queue_t *tmp=NULL;
		tmp = (channel_queue_t *)calloc(1, sizeof(channel_queue_t));
		sprintf(tmp->channel_name, "%s", channel_name);
		rc =  OpenFileQueue(l, NULL, NULL, NULL, NULL, qpath, qname, &tmp->obj);
		CHECK(rc == TRUE);

		ll_node_t *ll_node=NULL;
		ll_node = linkedlist_put(ll, channel_name, (void *)tmp, sizeof(char)+sizeof(fqueue_obj_t));
		CHECK(ll_node);

		if(ts1) free(ts1);
		if(ts2) free(ts2);

		this_entry = this_entry->next;
	}
	if( filelist_obj ) close_file_list(l, &filelist_obj);

	FQ_TRACE_EXIT(l);
	return true;
}

bool MakeLinkedList_co_json_qformat_obj_with_mapfile(fq_logger_t *l, char *mapfile, char delimiter, linkedlist_t *ll)
{
	file_list_obj_t *filelist_obj=NULL;
	FILELIST *this_entry=NULL;
	int rc;
	int t_no;

	FQ_TRACE_ENTER(l);

	fq_log(l, FQ_LOG_INFO, "mapfile='%s", mapfile);
	rc = open_file_list(l, &filelist_obj, mapfile);
	CHECK( rc == TRUE );

	printf("file list count = [%d]\n", filelist_obj->count);

	// filelist_obj->on_print(filelist_obj);

	this_entry = filelist_obj->head;
	for( t_no=0;  (this_entry->next && this_entry->value); t_no++ ) {
		char *errmsg = NULL;
		printf("this_entry->value=[%s]\n", this_entry->value);

		bool tf;
		char co_initial[2];
		char channel[3];
		char filename[255];

		int cnt;
		cnt = fq_sscanf(delimiter, this_entry->value, "%s%s%s", co_initial, channel, filename);

		printf("co_initial='%s', channel='%s', filename='%s'.\n", co_initial, channel,  filename);

		co_json_qformat_t *tmp=NULL;
		tmp = (co_json_qformat_t *)calloc(1, sizeof(co_json_qformat_t));

		tmp->co_initial =  co_initial[0];
		sprintf(tmp->channel, "%s", channel);

		int file_len;
		rc = LoadFileToBuffer(filename, &tmp->json_string, &file_len,  &errmsg);
		CHECK(rc > 0);

		char key[5];
		sprintf(key, "%s-%s", co_initial, channel);

		ll_node_t *ll_node=NULL;
		ll_node = linkedlist_put(ll, key, (void *)tmp, sizeof(char)+sizeof(co_json_qformat_t));
		CHECK(ll_node);

		// close_ratio_distributor(l, &tmp->ratio_obj);
		this_entry = this_entry->next;
	}
	if( filelist_obj ) close_file_list(l, &filelist_obj);

	printf("ll->length is [%d]\n", ll->length);

	FQ_TRACE_EXIT(l);
	return true;
}
bool MakeLinkedList_dist_result_json_rule( fq_logger_t *l, char *rule_file, char delimiter, linkedlist_t *ll)
{
	bool tf;

	FQ_TRACE_ENTER(l);

	fq_log(l, FQ_LOG_INFO, "rule_file='%s", rule_file);
	tf = Load_json_make_rule(l, rule_file, delimiter, ll);
	CHECK(tf);

	FQ_TRACE_EXIT(l);

	return true;
}
bool MakeLinkedList_co_channel_json_rule_obj_with_mapfile(fq_logger_t *l, char *mapfile, char delimiter, linkedlist_t *ll)
{
	file_list_obj_t *filelist_obj=NULL;
	FILELIST *this_entry=NULL;
	int rc;
	int t_no;

	FQ_TRACE_ENTER(l);

	fq_log(l, FQ_LOG_INFO, "mapfile='%s", mapfile);
	rc = open_file_list(l, &filelist_obj, mapfile);
	CHECK( rc == TRUE );

	printf("[%s]:file list count = [%d]\n", mapfile, filelist_obj->count);

	// filelist_obj->on_print(filelist_obj);

	this_entry = filelist_obj->head;
	for( t_no=0;  (this_entry->next && this_entry->value); t_no++ ) {
		char *errmsg = NULL;
		printf("this_entry->value=[%s]\n", this_entry->value);

		bool tf;
		char co_initial[2];
		char channel[3];
		char filename[255];

		int cnt;
		cnt = fq_sscanf(delimiter, this_entry->value, "%s%s%s", co_initial, channel, filename);

		printf("co_initial='%s', channel='%s', filename='%s'.\n", co_initial, channel,  filename);

		co_channel_json_rule_t *tmp=NULL;
		tmp = (co_channel_json_rule_t *)calloc(1, sizeof(co_channel_json_rule_t));

		tmp->co_initial =  co_initial[0];
		sprintf(tmp->channel, "%s", channel);

		tmp->json_tci_ll =  linkedlist_new("json_tci_obj_list");
		tf = Load_json_make_rule( 0x00, filename, '|', tmp->json_tci_ll);
		CHECK(tf);

		char key[5];
		sprintf(key, "%s-%s", co_initial, channel);

		ll_node_t *ll_node=NULL;
		ll_node = linkedlist_put(ll, key, (void *)tmp, sizeof(char)+sizeof(co_channel_json_rule_t));
		CHECK(ll_node);

		// close_ratio_distributor(l, &tmp->ratio_obj);
		this_entry = this_entry->next;
	}
	if( filelist_obj ) close_file_list(l, &filelist_obj);

	printf("ll->length is [%d]\n", ll->length);

	FQ_TRACE_EXIT(l);
	return true;
}

bool Load_map_and_linkedlist( fq_logger_t *l, ratio_dist_conf_t *my_conf, in_params_map_list_t *in, out_params_map_list_t *out) 
{
	int rc;
	int tf;
	char map_delimiter;

	map_delimiter = '|';
	out->json_key_datatype_obj_ll =  linkedlist_new("json_key_datatype_obj_list");
	tf = MakeLinkedList_auto_cache_with_mapfile( l, my_conf->auto_cache_mapfile, map_delimiter, out->json_key_datatype_obj_ll);
	CHECK(tf);
	fq_log(l, FQ_LOG_INFO, "MakeLinkedList auto caching map ok.");

	map_delimiter = ':';
	out->channel_co_queue_obj_ll = linkedlist_new("channel_co_queue_obj_list");
	CHECK(out->channel_co_queue_obj_ll);
	tf = MakeLinkedList_co_initial_FQ_obj_with_mapfile(l, my_conf->channel_co_queue_map_file, map_delimiter, out->channel_co_queue_obj_ll);
	CHECK(tf);
	fq_log(l, FQ_LOG_INFO, "MakeLinkedList channel_co_initial/fq object  map ok.");

	out->channel_co_ratio_obj_ll = linkedlist_new("channel_co_ratio_obj_list");
	map_delimiter = '|';
	tf = MakeLinkedList_channel_co_ratio_obj_with_mapfile(l, my_conf->channel_co_ratio_string_map_file, map_delimiter, out->channel_co_ratio_obj_ll);
	CHECK(tf);
	fq_log(l, FQ_LOG_INFO, "MakeLinkedList channel_co_ratio map ok.");

	map_delimiter = ':';
	out->forward_channel_queue_obj_ll = linkedlist_new("forward_channel_queue_obj_list");
	CHECK(out->forward_channel_queue_obj_ll);
	tf = MakeLinkedList_channel_name_FQ_obj_with_mapfile(l, my_conf->forward_channel_queue_map_file, map_delimiter, out->forward_channel_queue_obj_ll);
	CHECK(tf);
	fq_log(l, FQ_LOG_INFO, "MakeLinkedList channel name/fq object map  ok.");

	out->t = new_codemap(l, "co_initial_name");
	rc=read_codemap(l, out->t, my_conf->co_initial_name_map_file);
	CHECK(rc >= 0);
	fq_log(l, FQ_LOG_INFO, "Codemap co_initial / co_name map  ok.");

	out->queue_co_initial_codemap = new_codemap(l, "queue_co_initial");
	rc=read_codemap(l, out->queue_co_initial_codemap, my_conf->queue_co_initial_map_file);
	CHECK(rc >= 0);
	fq_log(l, FQ_LOG_INFO, "Codemap queue_co_initial map ok.");

	map_delimiter = '|';
	out->co_channel_json_rule_obj_ll = linkedlist_new("co_channel_json_rule_list");
	CHECK(out->co_channel_json_rule_obj_ll);
	tf = MakeLinkedList_co_channel_json_rule_obj_with_mapfile(l, my_conf->co_channel_json_rule_map_file, map_delimiter, out->co_channel_json_rule_obj_ll);
	CHECK(tf);
	fq_log(l, FQ_LOG_INFO, "MakeLinkedList co / co_json rule map  ok.");

	map_delimiter = '|';
	out->dist_result_json_rule_ll = linkedlist_new("dist_result_json_rule_list");
	tf = MakeLinkedList_dist_result_json_rule( l, my_conf->dist_result_json_rule_file, map_delimiter,  out->dist_result_json_rule_ll);
	CHECK(tf);
	fq_log(l, FQ_LOG_INFO, "MakeLinkedList dist_result json rule OK.");

	return true;
}
