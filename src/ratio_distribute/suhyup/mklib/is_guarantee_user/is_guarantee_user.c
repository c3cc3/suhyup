#include <unistd.h>
#include "fq_hashobj.h"
#include "fq_common.h"
#include "fq_logger.h"


/* This function has memory leak */
static bool is_guarantee_user( fq_logger_t *l, hashmap_obj_t *seq_check_hash_obj, char *seq_check_id, char *co_in_hash ) 
{
	char *get_value = NULL;
	int rc;

	rc = GetHash(l, seq_check_hash_obj, seq_check_id, &get_value);
	if( rc != TRUE ) { /* not found */
		// rc = PutHash(l, seq_check_hash_obj, seq_check_id, "KT" );
		SAFE_FREE(get_value);
		return false;
	}
	else { /* already used a co */
		printf("We will put [%s] to [%s] for sequence guarantee.\n", seq_check_id, get_value );
		fq_log(l, FQ_LOG_INFO, "We will put [%s] to [%s] for sequence guarantee.", seq_check_id, get_value );

		*co_in_hash = *get_value;
		SAFE_FREE(get_value);
		return true;
	}
}

int main()
{
	hashmap_obj_t *seq_check_hash_obj;
	int rc;
	fq_logger_t *l=NULL;

	/* For creating, Use /utl/ManageHashMap command */
    rc = OpenHashMapFiles(l, "/home/ums/fq/hash", "history", &seq_check_hash_obj);
    CHECK(rc==TRUE);

loop:
	char co_in_hash;
	char tempbuf[13];
	char tel[13];
	char *tel_const ="01072021516"; 
	sprintf(tel, "%s", get_seed_ratio_rand_str( tempbuf, 12, "01"));
	printf("tel=[%s]\n", tel);
	
	is_guarantee_user(NULL, seq_check_hash_obj, tel, &co_in_hash);

	usleep(1000);
	goto loop;

	rc = CloseHashMapFiles(l, &seq_check_hash_obj);
    CHECK(rc==TRUE);
	return 0;
}
