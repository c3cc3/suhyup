#include "fq_json_tci.h"
#include "fq_file_list.h"
#include "fq_timer.h"
#include "fq_hashobj.h"
#include "fq_tci.h"
#include "fq_cache.h"
#include "fq_conf.h"
#include "fq_mem.h"
#include "fq_scanf.h"
#include "fq_logger.h"
#include "fq_common.h"
#include "parson.h"

int main(int ac, char **av)
{
	bool tf;
	char *rule_filename = "./json_make.rule";
	char delimiter = '|';
	linkedlist_t *ll;
	fq_logger_t *l;
	char *log_pathfile = "./make_rule.log";
	int i_log_level = 1;
	int rc;
	char *target = NULL;
	bool pretty_flag = true;

	JSON_Value *rootValue = json_value_init_object();
	JSON_Object *rootObject = json_value_get_object(rootValue);

	char	*in_serialized_json_file = "./sample_in.json";
	unsigned char 	*json_string;
	int		file_len;
	char 	*errmsg = NULL;


	rc = fq_open_file_logger(&l, log_pathfile, i_log_level);
	CHECK(rc==TRUE);


	/* make input json */
	rc = LoadFileToBuffer( in_serialized_json_file,  &json_string, &file_len,  &errmsg);
    CHECK(rc > 0);

	rootValue = json_parse_string(json_string);
	CHECK(rootValue);

	rootObject = json_value_get_object(rootValue);
	CHECK(rootObject);

	debug_json(l, rootValue);
	
	/* make cache */
	cache_t *cache_short = cache_new('S', "Short term cache");
	char *history_key = "1234567890";
	cache_update(cache_short, "history_key", history_key, 0);
	char *dist_q_dttm = "20230916121213";
	cache_update(cache_short, "DIST_Q_DTTM", dist_q_dttm, 0);


	/* make output serialized_json by a rule file */
	ll =  linkedlist_new("json_tci_obj_list");
	tf = Load_json_make_rule( l, rule_filename, delimiter, ll);
	CHECK(tf);

	tf =  make_serialized_json_by_rule(l, ll, cache_short, rootObject, &target,  pretty_flag );
	CHECK(tf);

	cache_free(&cache_short);


	linkedlist_free(&ll);

	json_value_free(rootValue);

	fq_close_file_logger(&l);
}
