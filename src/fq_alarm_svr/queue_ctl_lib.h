/*
** queue_ctl_lib.h
*/
#ifndef _QUEUE_CTL_LIB_H
#define _QUEUE_CTL_LIB_H

#include <stdbool.h>
#include "fq_config.h"
#include "fq_linkedlist.h"
#include "fq_file_list.h"
#include "fq_alarm_svr_conf.h"

#ifdef __cpluscplus
extern "C"
{
#endif
#define MAX_UMS_MSG_SIZE 4000

typedef struct _ums_msg_t ums_msg_t;
struct _ums_msg_t {
	char start_byte;
	int msg_type;
	char phone_no[12];
	int seq;
	char data[MAX_UMS_MSG_SIZE+1];
};

typedef enum { NEW_RATIO=0, CO_DOWN, PROC_STOP } ctrl_cmd_t;
typedef struct _ctrl_msg_t ctrl_msg_t;
struct _ctrl_msg_t {
	ctrl_cmd_t	 cmd; /* cmd : NEW_RATIO: 0  , CO_DOWN:1, PROC_STOP:2 */
	char channel[3];
	char new_ratio_string[128];
	char co_name[2]; /* initial */
};

bool Open_takeout_Q( fq_logger_t *l, fq_alarm_svr_conf_t *conf, fqueue_obj_t **deq_obj);

int takeout_uchar_msg_from_Q(fq_logger_t *l, fq_alarm_svr_conf_t *my_conf, fqueue_obj_t *deq_obj, unsigned char **msg, char **uf, long *seq);
void commit_Q( fq_logger_t *l, fqueue_obj_t *deq_obj, char *unlink_filename, long seq);
void cancel_Q( fq_logger_t *l, fqueue_obj_t *deq_obj, char *unlink_filename, long seq);
int uchar_enQ(fq_logger_t *l, unsigned char *data, size_t data_len, fqueue_obj_t *obj);

bool queue_is_warning( fq_logger_t *l, fqueue_obj_t *qobj, float limit );

#ifdef __cpluscplus
}
#endif
#endif

