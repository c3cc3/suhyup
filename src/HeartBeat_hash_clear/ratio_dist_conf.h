/*
** ratio_dist_conf.h
*/
#ifndef _RATIO_DIST_CONF_H
#define _RATIO_DIST_CONF_H

#include <stdbool.h>
#include "fq_config.h"

#ifdef __cpluscplus
extern "C"
{
#endif 

typedef struct _ratio_dist_conf ratio_dist_conf_t;
struct _ratio_dist_conf {
	char	*log_filename;
	char	*log_level;
	int		i_log_level;
	char	*qpath; /* file queue for reading */
	char	*qname;
	char	*ctrl_qpath; /* Contrl file queue */
	char	*ctrl_qname; 

	char	*hash_map_path;
	char	*ums_hash_map_name;
	char	*history_hash_map_name;

	char	*del_hash_q_path;
	char	*del_hash_q_name;

	char	*return_q_path;
	char	*return_q_name;

	char 	*channel_co_ratio_string_map_file;
	char	*co_initial_map_file;
	char	*queue_co_initial_map_file;
	char	*channel_co_queue_map_file;
	char	*forward_channel_queue_map_file;
	char	*co_qformat_map_file;
	char	*co_json_qformat_map_file;
	char	*co_initial_name_map_file;
	char	*co_channel_json_rule_map_file;

	bool	check_msgsize_flag;
	bool	fqpm_use_flag; /* Use fqpm */
	char 	*ums_gw_protocol; /* JSON or BYTESTREAM */
	char	*auto_cache_mapfile;
	char	*distribute_type; /* least usage or ratio */
	float	warning_threshold;
	char	*admin_phone_no;
	char	*sms_alarm_json_format_file;
	char 	*var_data_json_key;
	char	*template_json_key;
	char	*msg_json_target_key;
	char	*working_time_from;
	char	*working_time_to;
	bool	working_time_use_flag;
	char	*seq_check_hash_map_file;
	char	*seq_check_key_name;
	bool	seq_checking_use_flag;

	char 	*fq_mon_svr_hash_path;
	char	*fq_mon_svr_hash_name;

	char 	*malformed_request_file;
	char	*channel_key;
	char	*svc_code_key;
	char	*dist_result_json_rule_file;
	char	*heartbeat_hash_map_path;
	char	*heartbeat_hash_map_name;
	char	*fq_pid_list_file;
};

bool LoadConf(char *filename, ratio_dist_conf_t **c, char **ptr_errmsg);
void FreeConf(ratio_dist_conf_t *c);

#ifdef __cpluscplus
}
#endif
#endif
