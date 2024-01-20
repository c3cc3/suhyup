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
#include "fq_timer.h"
#include "fq_hashobj.h"
#include "fq_tci.h"
#include "fq_cache.h"
#include "fq_conf.h"
#include "fq_gssi.h"
#include "fq_mem.h"
#include "parson.h"

#include "main_subfunc.h"

int LocalGetHash( fq_logger_t *l, hashmap_obj_t *hash_obj, char *key, char **value);


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

/* This function has memory leak */
bool is_guarantee_user( fq_logger_t *l, hashmap_obj_t *seq_check_hash_obj, char *seq_check_id, char *co_in_hash, char *channel ) 
{
	char *get_value = NULL;
	int rc;

	FQ_TRACE_ENTER(l);

	CHECK(seq_check_id);
	CHECK(channel);

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
		char *p =  NULL;
		if( get_value ) {
			p = strstr(get_value, channel);
			if( p ) {
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
		return false;
	}
}
