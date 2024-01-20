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

#ifdef __cpluscplus
extern "C"
{
#endif 

#define SEQ_CHECK_ID_MAX_LEN (40)

bool update_channel_ratio(fq_logger_t *l, ctrl_msg_t *ctrl_msg, linkedlist_t *channel_co_ratio_obj_ll);
bool make_serialized_json_by_rule_local( fq_logger_t *l, linkedlist_t *json_tci_ll, cache_t *cache, JSON_Object *in_json, char **dst, bool pretty_flag );
bool json_get_value_malloc( fq_logger_t *l, JSON_Object *ums_JSONObject, char *key, char *datatype, char **dst, long *dst_len);
bool json_auto_caching( fq_logger_t *l, cache_t *cache_short_for_gssi, JSON_Object *ums_JSONObject, json_key_datatype_t *tmp);
bool down_channel_ratio(fq_logger_t *l, ctrl_msg_t *ctrl_msg, linkedlist_t *channel_co_ratio_obj_ll);
bool json_caching_and_SSIP( fq_logger_t *l, JSON_Value *in_json, JSON_Object *root,  ratio_dist_conf_t *my_conf, linkedlist_t *json_key_datatype_obj_ll, unsigned char *ums_msg, cache_t *cache_short_for_gssi, char *channel, char *seq_check_id);
bool make_JSON_stream_by_rule_and_enQ( fq_logger_t *l, JSON_Object *in_json_obj, char *key, char *channel, unsigned char *SSIP_result, cache_t *c, ratio_dist_conf_t *my_conf, fqueue_obj_t *this_q, fqueue_obj_t *deq_obj, char *unlink_filename, long l_seq,  char co_initial, linkedlist_t *co_json_rule_obj_ll);
bool forward_channel( fq_logger_t *l, char *channel, ll_node_t *channel_queue_map_node, unsigned char *ums_msg, size_t ums_msg_len, fqueue_obj_t *deq_obj, char *unlink_filename, long l_seq);
bool ratio_forward_channel( fq_logger_t *l, char *channel, fqueue_obj_t *this_q, unsigned char *ums_msg, size_t ums_msg_len, fqueue_obj_t *deq_obj, char *unlink_filename, long l_seq);
fqueue_obj_t *Select_a_queue_obj_and_co_initial_by_least_usage( fq_logger_t *l, char *channel, linkedlist_t *channel_co_queue_obj_ll, codemap_t *queue_co_initial_codemap, char *key, char *co_initial);
fqueue_obj_t *Select_a_queue_obj_and_co_initial_by_ratio( fq_logger_t *l, char *channel, linkedlist_t *channel_co_ratio_obj_ll, linkedlist_t *channel_co_queue_obj_ll, codemap_t *queue_co_initial_codemap, char *key, char *co_initial);
fqueue_obj_t *Select_a_queue_obj_by_co_initial(fq_logger_t *l, char *channel, char co_initial,  linkedlist_t *channel_co_queue_obj_ll);
bool is_guarantee_user( fq_logger_t *l, hashmap_obj_t *seq_check_hash_obj, char *seq_check_id, char *co_in_hash , char *channel);
void PutHash_Option( fq_logger_t *l, bool seq_checking_use_flag,  hashmap_obj_t *seq_check_hash_obj, char *seq_check_id, char *str_co_initial, char *channel );

bool init_Hash_channel_co_ratio_obj_with_mapfile(fq_logger_t *l, char *mapfile, char delimiter, hashmap_obj_t *hash_obj);
bool json_parsing_and_get_channel( fq_logger_t *l, JSON_Value **in_json, JSON_Object **root, unsigned char *ums_msg,  char *channel_key, char **channel, char *svc_code_key, char **svc_code);
int Sure_GetHash( fq_logger_t *l, hashmap_obj_t *hash_obj, char *key, char **value);


#ifdef __cpluscplus
}
#endif
#endif
