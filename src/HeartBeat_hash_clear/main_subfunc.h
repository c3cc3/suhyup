/*
** main_subfunc.h
*/
#ifndef _MAIN_SUBFUNC_H
#define _MAIN_SUBFUNC_H

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

#ifdef __cpluscplus
extern "C"
{
#endif 

#define SEQ_CHECK_ID_MAX_LEN (40)

void PutHash_Option( fq_logger_t *l, bool seq_checking_use_flag,  hashmap_obj_t *seq_check_hash_obj, char *seq_check_id, char *str_co_initial, char *channel );
int LocalGetHash( fq_logger_t *l, hashmap_obj_t *hash_obj, char *key, char **value);
bool is_guarantee_user( fq_logger_t *l, hashmap_obj_t *seq_check_hash_obj, char *seq_check_id, char *co_in_hash, char *channel );

#ifdef __cpluscplus
}
#endif
#endif
