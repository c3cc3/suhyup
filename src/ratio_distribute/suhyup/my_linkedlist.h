#ifndef _MY_LINKEDLIST_H
#define _MY_LINKEDLIST_H

#ifdef  __cplusplus
extern "C" {
#endif

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
#include "parson.h"
#include "fq_json_tci.h"

typedef struct _json_key_datatype_t json_key_datatype_t;
struct _json_key_datatype_t {
	char json_key[128];
	char cache_key[128];
	char datatype[16];
	bool mandatory; // compulsary  Y/N
	int	length;
};

typedef struct _channel_ratio_obj_t channel_ratio_obj_t;
struct _channel_ratio_obj_t {
	char channel_name[16];
	ratio_obj_t	*ratio_obj;
};

typedef struct _channel_co_queue_t channel_co_queue_t;
struct _channel_co_queue_t {
	char channel_initial[5]; /* key: SM-K */
	fqueue_obj_t	*obj;
};

typedef struct _channel_queue_t channel_queue_t;
struct _channel_queue_t {
	char channel_name[3];
	fqueue_obj_t	*obj;
};

typedef struct _co_qformat_t co_qformat_t;
struct _co_qformat_t {
	char co_initial;
	qformat_t	*q;
};

typedef struct _co_json_qformat_t co_json_qformat_t;
struct _co_json_qformat_t {
	char co_initial;
	char channel[3];
	unsigned char *json_string;
};

typedef struct _co_channel_json_rule_t co_channel_json_rule_t;
struct _co_channel_json_rule_t {
	char co_initial;
	char channel[3];
	linkedlist_t *json_tci_ll;
};

struct in_params_map_list {
	int i;
	char c;
};
typedef struct in_params_map_list in_params_map_list_t;

struct out_params_map_list {
	linkedlist_t 	*json_key_datatype_obj_ll;
	linkedlist_t 	*channel_co_ratio_obj_ll;
	linkedlist_t 	*channel_co_queue_obj_ll;
	linkedlist_t 	*forward_channel_queue_obj_ll;
	linkedlist_t 	*co_qformat_obj_ll;
	linkedlist_t *	co_json_qformat_obj_ll;
	codemap_t		*t;
	codemap_t		*queue_co_initial_codemap;
	linkedlist_t	*co_channel_json_rule_obj_ll;
	linkedlist_t	*dist_result_json_rule_ll;
};
typedef struct out_params_map_list out_params_map_list_t;


bool MakeLinkedList_auto_cache_with_mapfile( fq_logger_t *l, char *mapfile, char delimiter, linkedlist_t	*ll);
bool MakeLinkedList_channel_co_ratio_obj_with_mapfile(fq_logger_t *l, char *mapfile, char delimiter, linkedlist_t *ll);
bool MakeLinkedList_co_initial_FQ_obj_with_mapfile(fq_logger_t *l, char *mapfile, char delimiter, linkedlist_t *ll);
fqueue_obj_t *find_least_gap_qobj_in_channel_co_queue_map( fq_logger_t *l, char *channel, linkedlist_t *ll );
bool MakeLinkedList_channel_name_FQ_obj_with_mapfile(fq_logger_t *l, char *mapfile, char delimiter, linkedlist_t *ll);
// bool MakeLinkedList_ratio_obj_with_mapfile(fq_logger_t *l, char *mapfile, char delimiter, linkedlist_t *ll);
bool MakeLinkedList_co_qformat_obj_with_mapfile(fq_logger_t *l, char *mapfile, char delimiter, linkedlist_t *ll);
bool MakeLinkedList_co_json_qformat_obj_with_mapfile(fq_logger_t *l, char *mapfile, char delimiter, linkedlist_t *ll);

bool Load_map_and_linkedlist( fq_logger_t *l, ratio_dist_conf_t *my_conf, in_params_map_list_t *in, out_params_map_list_t *out) ;


#ifdef __cplusplus
}
#endif

#endif

