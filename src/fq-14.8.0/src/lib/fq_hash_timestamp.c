/***********************************************
* fq_hash_timestamp.c
*/
#include "fq_defs.h"
#include "fq_common.h"
#include "fq_hashobj.h"
#include "fq_common.h"
#include "fq_logger.h"
#include "fq_hash_timestamp.h"

#define FQ_HASH_TIMESTAMP_C_VERSION "1.0.0"

static int on_stamp( timestamp_obj_t *obj);

int open_timestamp_obj( fq_logger_t *l, timestamp_obj_t **obj, char *hash_path, char *hash_name, char *hash_key, int cycle_sec)
{
	FQ_TRACE_ENTER(l);

	timestamp_obj_t *rc=NULL;

	rc = (timestamp_obj_t *)calloc(1, sizeof(timestamp_obj_t));
	if( rc ) {
		char    tmp[128];
		int rtn;
		char	hash_value[36];

		rc->cycle_sec = cycle_sec;

		rtn = OpenHashMapFiles(l, hash_path, hash_name, &rc->hash_obj);
		if( rtn != TRUE ) {
			fq_log(l, FQ_LOG_ERROR, "OpenHashMapFiles('%s', '%s') error.", hash_path, hash_name);
			goto return_FALSE;
		}

		time(&rc->last_stamp);
		sprintf(hash_value, "%ld", rc->last_stamp);
		
		rc->hash_key = strdup(hash_key);

		rtn = PutHash(l, rc->hash_obj, rc->hash_key, hash_value);
		if(rtn != TRUE) {
			fq_log(l, FQ_LOG_ERROR, "PutHash('%s', '%s') error.", rc->hash_key, hash_value);
			goto return_FALSE;
		}

		/* Warning */
		rc->on_stamp = on_stamp;

		*obj = rc;

		FQ_TRACE_EXIT(l);
		return(TRUE);
	}

return_FALSE:
	SAFE_FREE( (*obj) );

	FQ_TRACE_EXIT(l);
	return(FALSE);
}

int close_timestamp_obj( fq_logger_t *l, timestamp_obj_t **obj)
{
	FQ_TRACE_ENTER(l);

	SAFE_FREE( (*obj)->hash_key);

	CloseHashMapFiles(NULL, &(*obj)->hash_obj);

	SAFE_FREE( (*obj) );

	FQ_TRACE_EXIT(l);
	return(TRUE);
}

/* If you have a opend hashobj, Use it */
int opened_timestamp_obj(  fq_logger_t *l, timestamp_obj_t **obj, hashmap_obj_t *hash_obj, char *hash_key, int cycle_sec)
{
    timestamp_obj_t *rc=NULL;

	FQ_TRACE_ENTER(l);

    rc = (timestamp_obj_t *)calloc(1, sizeof(timestamp_obj_t));
    if( rc ) {
        char    tmp[128];
        int rtn;
        char    hash_value[36];

        rc->cycle_sec = cycle_sec;
        rc->hash_obj = hash_obj;

        time(&rc->last_stamp);
        sprintf(hash_value, "%ld", rc->last_stamp);

        rc->hash_key = strdup(hash_key);

        PutHash(NULL, rc->hash_obj, rc->hash_key, hash_value);

        /* Warning */
        rc->on_stamp = on_stamp;

        *obj = rc;

		FQ_TRACE_EXIT(l);
        return(TRUE);
    }

return_FALSE:
    SAFE_FREE( (*obj) );
	FQ_TRACE_EXIT(l);
    return(FALSE);
}

int closed_timestamp_obj( fq_logger_t *l, timestamp_obj_t **obj)
{
	FQ_TRACE_ENTER(l);

    SAFE_FREE( (*obj)->hash_key);
    SAFE_FREE( (*obj) );

	FQ_TRACE_EXIT(l);
    return(TRUE);
}

static int on_stamp( timestamp_obj_t *obj )
{
	time_t current;

	if( !obj ) { 
		goto return_FALSE;
	}
	time(&current);

	if( (current - obj->cycle_sec) > obj->last_stamp) {
		char    hash_value[36];

		time(&obj->last_stamp);
		sprintf(hash_value, "%ld", obj->last_stamp);
		PutHash(NULL, obj->hash_obj, obj->hash_key, hash_value);
	}
	return(TRUE);

return_FALSE:

	return(FALSE);
}
