/*
** queue_ctl_lib.h
*/
#ifndef _QUEUE_CTL_LIB_H
#define _QUEUE_CTL_LIB_H

#include <stdbool.h>
#include "fq_cache.h"
#include "fq_config.h"
#include "fq_linkedlist.h"
#include "fq_file_list.h"
#include "ratio_dist_conf.h"

#ifdef __cpluscplus
extern "C"
{
#endif
#define MAX_UMS_MSG_SIZE 4000

typedef struct _ums_msg_t ums_msg_t;
struct _ums_msg_t {
	int msg_type;
	char phone_no[12];
	int seq;
	char data[MAX_UMS_MSG_SIZE+1];
};

typedef enum { NEW_RATIO=0, CO_DOWN, PROC_STOP, Q_TPS_CTRL } ctrl_cmd_t;
typedef struct _ctrl_msg_t ctrl_msg_t;
struct _ctrl_msg_t {
	char	start_byte; /* 0x01 */
	ctrl_cmd_t	 cmd; /* cmd : NEW_RATIO: 0  , CO_DOWN:1, PROC_STOP:2, Q_TPS_CTRL:3 */
	char channel[3];
	char new_ratio_string[128];
	char co_name[2]; /* initial */
};

bool Open_takeout_Q( fq_logger_t *l, ratio_dist_conf_t *conf, fqueue_obj_t **deq_obj);
bool Open_control_Q( fq_logger_t *l, ratio_dist_conf_t *conf, fqueue_obj_t **deq_obj);
bool Open_del_hash_Q( fq_logger_t *l, ratio_dist_conf_t *conf, fqueue_obj_t **deq_obj);
bool Open_return_Q( fq_logger_t *l, ratio_dist_conf_t *conf, fqueue_obj_t **deq_obj);

bool Make_sub_Q_obj_list(fq_logger_t *l, ratio_dist_conf_t *my_conf, linkedlist_t *subQ_obj_list);
int takeout_ums_msg_from_Q(fq_logger_t *l, ratio_dist_conf_t *my_conf, fqueue_obj_t *deq_obj, ums_msg_t **msg, char **uf, long *seq);
int takeout_uchar_msg_from_Q(fq_logger_t *l, ratio_dist_conf_t *my_conf, fqueue_obj_t *deq_obj, unsigned char **msg, char **uf, long *seq);
int takeout_ctrl_msg_from_Q(fq_logger_t *l, ratio_dist_conf_t *my_conf, fqueue_obj_t *deq_obj, ctrl_msg_t **msg);
void commit_Q( fq_logger_t *l, fqueue_obj_t *deq_obj, char *unlink_filename, long seq);
void cancel_Q( fq_logger_t *l, fqueue_obj_t *deq_obj, char *unlink_filename, long seq);
fqueue_obj_t *Choose_a_subQ(fq_logger_t *l,  linkedlist_t *subQ_obj_list);
int put_carrier_msg_to_subQ(fq_logger_t *l, unsigned char *msg, size_t buf_sz, size_t msglen, fqueue_obj_t *obj);
int uchar_enQ(fq_logger_t *l, unsigned char *data, size_t data_len, fqueue_obj_t *obj);
int ums_msg_type_enQ(fq_logger_t *l, ums_msg_t *data, size_t data_len, fqueue_obj_t *obj);

bool queue_is_warning( fq_logger_t *l, fqueue_obj_t *qobj, float limit );
bool request_error_JSON_return_enQ(fq_logger_t *l, char *error_reason, unsigned char *ums_msg, int ums_msg_len, fqueue_obj_t *return_q_obj);
bool dist_result_enQ( fq_logger_t *l, cache_t *cache_short, JSON_Object *rootObject, char *channel, char *history_key, char co, fqueue_obj_t *return_q_obj, linkedlist_t *ll);


#ifdef __cpluscplus
}
#endif
#endif

