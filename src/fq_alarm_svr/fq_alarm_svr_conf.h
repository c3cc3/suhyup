/*
** fq_alarm_svr_conf.h
*/
#ifndef _FQ_ALARM_SVR_CONF_H
#define _FQ_ALARM_SVR_CONF_H

#include <stdbool.h>
#include "fq_config.h"

#ifdef __cpluscplus
extern "C"
{
#endif 

typedef struct _fq_alarm_svr_conf fq_alarm_svr_conf_t;
struct _fq_alarm_svr_conf {
	char	*log_filename;
	char	*log_level;
	int		i_log_level;
	char	*qpath; /* file queue for reading */
	char	*qname;
	char    *dist_hist_qpath;
    char    *dist_hist_qname;
	char	*channel_co_queue_map_file;
	char	*admin_phone_map_filename;
	char	*telecom_json_template_filename;
	char	*alarm_history_hash_path;
	char	*alarm_history_hash_name;
	int		dup_prevent_sec;
	char	*send_phone_no;
};

bool LoadConf(char *filename, fq_alarm_svr_conf_t **c, char **ptr_errmsg);
void FreeConf(fq_alarm_svr_conf_t *c);

#ifdef __cpluscplus
}
#endif
#endif
