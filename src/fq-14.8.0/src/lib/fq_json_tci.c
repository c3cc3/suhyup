#include <stdio.h>
#include "parson.h"
#include <stdbool.h>
#include <libgen.h>

#include "fq_common.h"
#include "fq_linkedlist.h"
#include "fq_delimiter_list.h"
#include "fq_ratio_distribute.h"

#include "fq_codemap.h"
#include "fqueue.h"
#include "fq_file_list.h"
#include "fq_timer.h"
#include "fq_hashobj.h"
#include "fq_tci.h"
#include "fq_cache.h"
#include "fq_conf.h"
#include "fq_mem.h"
#include "fq_scanf.h"
#include "fq_logger.h"

#include "fq_json_tci.h"
bool Load_json_make_rule( fq_logger_t *l, char *mapfile, char delimiter, linkedlist_t	*ll)
{
	file_list_obj_t *filelist_obj=NULL;
	FILELIST *this_entry=NULL;
	int rc;
	int t_no;

	FQ_TRACE_ENTER(l);

	rc = open_file_list(l, &filelist_obj, mapfile);
	CHECK( rc == TRUE );

	fq_log(l, FQ_LOG_DEBUG, "count = [%d].", filelist_obj->count);

	// filelist_obj->on_print(filelist_obj);

	this_entry = filelist_obj->head;
	for( t_no=0;  (this_entry->next && this_entry->value); t_no++ ) {
		char *errmsg = NULL;

		bool tf;
		char find_key[128];
		char target_key[128];
		char source[16];
		char length_str[5];
		int	length;
		char data_type[16];
		char fixed[64];
		char 	mandatory_str[10];
		bool	mandatory;
		int cnt;

		// printf("this_entry->value=[%s]\n", this_entry->value);

		cnt = fq_sscanf(delimiter, this_entry->value, "%s%s%s%s%s%s%s", 
			find_key, target_key, source, length_str, data_type, fixed, mandatory_str );

		CHECK(cnt == 7);

		fq_log(l, FQ_LOG_DEBUG, "find_key='%s', target_key='%s', source='%s', length='%s', data_type='%s' fixed='%s' mandatory='%s'.", 
			find_key, target_key, source, length_str, data_type, fixed, mandatory_str);

		json_tci_t *tmp=NULL;
		tmp = (json_tci_t *)calloc(1, sizeof(json_tci_t));

		sprintf(tmp->find_key, "%s", find_key);
		sprintf(tmp->target_key, "%s", target_key);
		sprintf(tmp->source, "%s", source);
		tmp->length = atoi(length_str);
		sprintf(tmp->datatype, "%s", data_type);
		sprintf(tmp->fixed, "%s", fixed);

		if( STRCCMP(mandatory_str, "true") == 0 ) {
			tmp->mandatory =  true;
		}
		else {
			tmp->mandatory =  false;
		}

		ll_node_t *ll_node=NULL;
		ll_node = linkedlist_put(ll, find_key, (void *)tmp, sizeof(char)+sizeof(json_tci_t));
		CHECK(ll_node);

		// close_ratio_distributor(l, &tmp->ratio_obj);
		this_entry = this_entry->next;
	}

	if( filelist_obj ) close_file_list(l, &filelist_obj);

	fq_log(l, FQ_LOG_DEBUG, "ll->length is [%d].", ll->length);

	FQ_TRACE_EXIT(l);
	return true;
}

bool make_json_input_sample_data( JSON_Value **json, JSON_Object **root)
{
	*json = json_value_init_object(); // make a json
	*root = json_value_get_object(*json); // get oot pointer
	
    JSON_Value *branch = json_value_init_array();
    JSON_Array *array_contents = json_value_get_array(branch);

 	json_object_set_number(*root, "SETTLE_CODE", 1); // put array to root
 	json_object_set_string(*root, "SUB_STORE_NO", "0011"); // put array to root
 	json_object_set_number(*root, "MediaCount", 3); // put array to root
 	json_object_set_boolean(*root, "GssiFlag", true); // put array to root

 	json_object_dotset_number(*root, "Student.no", 35); // put array to root
 	json_object_dotset_string(*root, "Student.name", "Choi"); // put array to root
 	json_object_dotset_boolean(*root, "Student.flag", true); // put array to root

	int i;
	for(i=0; i<3; i++) {
		JSON_Value *array_one = json_value_init_object();
		JSON_Object *leaf_object = json_value_get_object(array_one);

		json_object_set_number(leaf_object,"sn",i);
		json_object_set_string(leaf_object,"type","TXT");
		json_object_set_string(leaf_object,"kind","T");
		json_object_set_string(leaf_object,"path","/umsnas/contents");
		json_object_set_string(leaf_object,"filename","duc.txt");
		json_array_append_value(array_contents, array_one);
	}

 	json_object_set_value(*root, "Media", branch); // put array to root

#if 0
	printf("json_object_get_array() test\n");
	JSON_Array *in_array = json_object_get_array(*root, "Media");
	CHECK(in_array);
	printf("json_object_get_array() test ----> OK\n");
#endif

	char *dst = json_serialize_to_string_pretty(*json);
	printf("JSON: %s\n", dst);
	json_free_serialized_string(dst);

	return(true);
}

static bool json_get_value_malloc( fq_logger_t *l, JSON_Object *ums_JSONObject, char *json_key, char *datatype, char **dst, long *dst_len)
{
	char buffer[8192];

	char *p=NULL;

	FQ_TRACE_ENTER(l);
	p = strstr(json_key, ".");

	memset(buffer, 0x00, sizeof(buffer));

	if(strcmp( datatype, "string") == 0 ) {
		const char *out = NULL;
		if( !p ) { 
			out = json_object_get_string(ums_JSONObject, json_key);
			if(out) {
				sprintf(buffer, "%s", out);
				fq_log(l, FQ_LOG_DEBUG, "json_object_get_string('%s') result='%s'\n", json_key, buffer);
			}
			else {
				fq_log(l, FQ_LOG_INFO, "There is no json value. We set space.");
				sprintf(buffer, "%s", " ");
			}
		}
		else {
			out = json_object_dotget_string(ums_JSONObject, json_key);
			if(out) {
				sprintf(buffer, "%s", out);
				fq_log(l, FQ_LOG_DEBUG, "json_object_get_string('%s') result='%s'\n", json_key, buffer);
			}
			else {
				fq_log(l, FQ_LOG_INFO, "There is no json value. We set space.");
				sprintf(buffer, "%s", " ");
			}
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

static bool json_set_value( fq_logger_t *l, JSON_Object *ums_JSONObject, char *json_key, char *datatype, char *src)
{
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
		return(false);
	}
	
	return true;
}

bool make_serialized_json_by_rule( fq_logger_t *l, linkedlist_t *json_tci_ll, cache_t *cache, JSON_Object *in_json, char **target, bool pretty_flag )
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
	
		fq_log(l, FQ_LOG_INFO, "find_key=[%s], target_key=[%s], datatype=[%s] source=[%s], fixed=[%s], mandatory=[%d]\n", tmp->find_key, tmp->target_key, tmp->datatype, tmp->source, tmp->fixed, tmp->mandatory  );
		if( strcmp( tmp->datatype, "string") == 0 ) {
			if(  strcmp( tmp->source, "json" ) == 0 ) {
#if 1
				bool tf;
				char *dst=NULL;
				long  dst_len;
				tf = json_get_value_malloc( l, in_json, tmp->find_key, tmp->datatype, &dst, &dst_len);
				CHECK(tf);

				json_set_value(l, root, tmp->target_key, tmp->datatype, dst); // put array to root

				SAFE_FREE(dst);
#else
				const char *p=NULL;
				p = json_object_get_string(in_json, tmp->find_key);
				if( !p ) {
					fq_log(l, FQ_LOG_ERROR, "'%s' not found from in_json.",  tmp->find_key);
					goto return_false;
				json_object_set_string(root, tmp->target_key, p); // put array to root

				json_object_set_string(root, tmp->target_key, dst); // put array to root
				SAFE_FREE(dst);
#endif
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
				return false;
			}
		}
		else if( strcmp( tmp->datatype, "number") == 0 ) {
			double d;
			if(  strcmp( tmp->source, "json" ) == 0 ) {
#if 1
				bool tf;
				char *dst=NULL;
				long  dst_len;
				tf = json_get_value_malloc( l, in_json, tmp->find_key, tmp->datatype, &dst, &dst_len);
				CHECK(tf);

				json_set_value(l, root, tmp->target_key, tmp->datatype, dst); // put array to root
				// json_object_set_number(root, tmp->target_key, atol(dst)); // put array to root
				SAFE_FREE(dst);
#else
				d = json_object_get_number(in_json, tmp->find_key);
				json_object_set_number(root, tmp->target_key, d); // put array to root
#endif
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
				char *p;
				JSON_Value *in_value;
	
				p = strstr(tmp->find_key, ".");
				if( p ) {
					in_value = json_object_dotget_value( in_json, tmp->find_key);
				}
				else {
					in_value = json_object_get_value( in_json, tmp->find_key);
				}

				if( !in_value ) {
					if( tmp->mandatory == true) {
						fq_log(l, FQ_LOG_ERROR, "[%s] value : not found from in json.",  tmp->find_key);
						goto return_false;
					}
					else {
						JSON_Value *new_value = NULL;
						json_object_set_value(root, tmp->target_key, new_value);
						fq_log(l, FQ_LOG_DEBUG, "value is null, so We set value by null.(%s)->(%s),", tmp->find_key, tmp->target_key);
					}
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
#if 1
				bool tf;
				char *dst=NULL;
				long  dst_len;
				tf = json_get_value_malloc( l, in_json, tmp->find_key, tmp->datatype, &dst, &dst_len);
				CHECK(tf);
				if( atol(dst) == 1 ) {
					json_set_value(l, root, tmp->target_key, tmp->datatype, dst); // put array to root
					// json_object_set_boolean(root, tmp->target_key, true); // put array to root
				}
				else {
					json_set_value(l, root, tmp->target_key, tmp->datatype, dst); // put array to root
					// json_object_set_boolean(root, tmp->target_key, false); // put array to root
				}
				SAFE_FREE(dst);
#else
				bool tf;
				tf = json_object_get_boolean(in_json, tmp->find_key);
				json_object_set_boolean(root, tmp->target_key, tf);
#endif
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
					// json_object_set_boolean(root, tmp->target_key, true ); // put array to root
				}
				else {
					json_set_value(l, root, tmp->target_key, tmp->datatype, p); // put array to root
					// json_object_set_boolean(root, tmp->target_key, false ); // put array to root
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
	*target = strdup(tmp);

	debug_json(l, out_json);

	SAFE_FREE(tmp);
	json_array_clear(array_contents);
	json_value_free(branch);
	json_value_free(out_json);

	return true; 

return_false:
	FQ_TRACE_EXIT(l);
	return false;
}
