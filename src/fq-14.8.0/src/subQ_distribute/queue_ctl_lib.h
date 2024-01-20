/*
** queue_ctl_lib.h
*/
#ifndef _QUEUE_CTL_LIB_H
#define _QUEUE_CTL_LIB_H

#include <stdbool.h>
#include "fq_config.h"
#include "subQ_dist_conf.h"
#include "fq_linkedlist.h"
#include "fq_file_list.h"

#ifdef __cpluscplus
extern "C"
{
#endif

typedef struct _sub_queue_obj_t sub_queue_obj_t;
struct _sub_queue_obj_t {
	fqueue_obj_t	*obj;
};

typedef struct _ums_msg_t ums_msg_t;
struct _ums_msg_t {
	int length;
	int seq;
	char svc_code[4+1];
	char name[20+1];
	char address[64+1];
};

bool Open_takeout_Q( fq_logger_t *l, subQ_dist_conf_t *conf, fqueue_obj_t **deq_obj);
bool Make_sub_Q_obj_list(fq_logger_t *l, subQ_dist_conf_t *my_conf, linkedlist_t *subQ_obj_list);
int takeout_ums_msg_from_Q(fq_logger_t *l, subQ_dist_conf_t *my_conf, fqueue_obj_t *deq_obj, ums_msg_t **msg, char **uf, long *seq);
void commit_Q( fq_logger_t *l, fqueue_obj_t *deq_obj, char *unlink_filename, long seq);
void cancel_Q( fq_logger_t *l, fqueue_obj_t *deq_obj, char *unlink_filename, long seq);
fqueue_obj_t *Choose_a_subQ(fq_logger_t *l,  linkedlist_t *subQ_obj_list);
int put_carrier_msg_to_subQ(fq_logger_t *l, unsigned char *msg, size_t buf_sz, size_t msglen, fqueue_obj_t *obj);

#ifdef __cpluscplus
}
#endif
#endif

