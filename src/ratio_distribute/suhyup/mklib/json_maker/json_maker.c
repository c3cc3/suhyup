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

typedef struct _test_t test_t;
struct _test_t {
	int sn;
	char type[4];
	char kind[2];
	char path[256];
	char filename[128];
};

static bool parsing_serialized_json(char *serialized_json_msg, linkedlist_t *test_ll);
static bool make_json_input_sample_data_local( JSON_Value **json, JSON_Object **root);

int main(int ac, char **av)
{
	char map_delimiter = '|';
	bool tf;
	linkedlist_t *json_tci_ll = NULL;
	char *json_tci_rule_file = av[1];
	cache_t *cache = NULL;
	int count=0;
	fq_logger_t *l=NULL;
	linkedlist_t *test_ll;

	JSON_Object *root;
	JSON_Value *in_json;
	
	map_delimiter = '|';
	json_tci_ll =  linkedlist_new("json_tci_obj_list");
	tf = Load_json_make_rule( 0x00, json_tci_rule_file, map_delimiter, json_tci_ll);
	CHECK(tf);
	printf("JSON rule: Loading success.\n");

loop:
	
	tf = make_json_input_sample_data_local( &in_json, &root);
	CHECK(tf);

	/* Make cache data */
	cache= cache_new('S', "Short term cache");
	cache_update(cache, "HISTORY_KEY", "0123456789012345678901234567890123456789", 0);

	char *serialized_json_msg = NULL;
	bool pretty_flag = true;
	tf = make_serialized_json_by_rule( l, json_tci_ll, cache, root, &serialized_json_msg, pretty_flag);
	CHECK(tf);
	
	printf("dst: %s\n", serialized_json_msg);


	test_ll = linkedlist_new("test");
	parsing_serialized_json( serialized_json_msg, test_ll );

	linkedlist_scan(test_ll, stdout);
	
	ll_node_t *p;

    fprintf(stdout, " list '%s' contains %d elements\n", test_ll->key, test_ll->length);
    for ( p=test_ll->head; p != NULL ; p = p->next ) {
        fprintf(stdout, " value= of %s is \'%p\'.\n", p->key, p->value);
		test_t *tmp =  p->value;;
		printf("\t - tmp->sn: %d\n", tmp->sn);
		printf("\t - tmp->type: %s\n", tmp->type);
		printf("\t - tmp->kind: %s\n", tmp->kind);
		printf("\t - tmp->path: %s\n", tmp->path);
		printf("\t - tmp->filename: %s\n", tmp->filename);
    }

	linkedlist_free(&test_ll);

	SAFE_FREE(serialized_json_msg);

	cache_free(&cache);

	json_value_free(in_json);


	usleep(1000);
	count++;
	printf("[%d]-th test.\n", count);

	goto loop;

	linkedlist_free(&json_tci_ll);
	return 0;
}

static bool parsing_serialized_json(char *serialized_json_msg, linkedlist_t *test_ll)
{
	JSON_Value *json_value = NULL;
	JSON_Object *json_obj = NULL;
	
	json_value = json_parse_string(serialized_json_msg);
	CHECK(json_value);
	json_obj = json_value_get_object(json_value);
	CHECK(json_obj);

	fprintf(stdout, "HISTORY_KEY = '%s'\n", json_object_get_string(json_obj, "HISTORY_KEY"));

#if 1
	JSON_Array *array = json_object_get_array(json_obj, "contents");
	for (int i = 0; i < json_array_get_count(array); i++) {
		JSON_Value *value = json_array_get_value(array, i);

		JSON_Value *copy_value = json_value_deep_copy(value);
		JSON_Object *copy_obj = json_value_get_object(copy_value);

		test_t *tmp=NULL;
		tmp = (test_t *)calloc(1, sizeof(test_t));

		for(int j=0; j<5; j++ ) {
			const char *find_key = json_object_get_name(copy_obj, j);

			if( j == 0 ) {
				long n =  json_object_get_number(copy_obj, find_key);
				printf("name='%s', value='%ld' \n", find_key, n);
				tmp->sn = n;
			}
			else {
				const char *const_value =  json_object_get_string(copy_obj, find_key);
				printf("name='%s', value='%s' \n", find_key, const_value);
				switch(j) {
					case 1:
						sprintf(tmp->type, "%s", const_value);
						break;
					case 2:
						sprintf(tmp->kind, "%s", const_value);
						break;
					case 3:
						sprintf(tmp->path, "%s", const_value);
						break;
					case 4:
						sprintf(tmp->filename, "%s", const_value);
						break;
					default:
						break;
				}
			}
		}
		ll_node_t *ll_node=NULL;
		char test_ll_key[4];

		sprintf(test_ll_key, "%03d", i);
		// jll_node = linkedlist_put(test_ll, test_ll_key, (void *)tmp, sizeof(char)+sizeof(test_t));
		ll_node = linkedlist_put(test_ll, test_ll_key, (void *)tmp, sizeof(test_t));

		CHECK(ll_node);
		printf("Successfully, '%s' put to linkedlist.\n", test_ll_key);
		
		if(tmp) free(tmp);
		json_value_free(copy_value);
	}
	json_array_clear(array);
#endif
	json_value_free(json_value);

	return true;
}
static bool make_json_input_sample_data_local( JSON_Value **json, JSON_Object **root)
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
