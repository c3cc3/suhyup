/* 
** ratio_dist_conf.c
*/

#include "fq_config.h"
#include "fq_param.h"
#include "ratio_dist_conf.h"
#include "fq_logger.h"
#include "fq_common.h"
#include <stdio.h>

bool LoadConf(char *filename, ratio_dist_conf_t **my_conf, char **ptr_errmsg)
{
	ratio_dist_conf_t *c = NULL;
	config_t *_config=NULL;
	char ErrMsg[512];
	char buffer[256];

	_config = new_config(filename);
	c = (ratio_dist_conf_t *)calloc(1, sizeof(ratio_dist_conf_t));

	if(load_param(_config, filename) < 0 ) {
		sprintf(ErrMsg, "Can't load '%s'.", filename);
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}

	/* Check whether essential items(mandatory) are omitted. */

	if(get_config(_config, "LOG_FILENAME", buffer) == NULL) {
		sprintf(ErrMsg, "LOG_FILENAME is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->log_filename = strdup(buffer);

	if(get_config(_config, "LOG_LEVEL", buffer) == NULL) {
		sprintf(ErrMsg, "LOG_LEVEL is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->log_level = strdup(buffer);
	c->i_log_level = get_log_level(c->log_level);

	if(get_config(_config, "DEQ_PATH", buffer) == NULL) {
		sprintf(ErrMsg, "DEQ_PATH is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->qpath = strdup(buffer);

	if(get_config(_config, "DEQ_NAME", buffer) == NULL) {
		sprintf(ErrMsg, "DEQ_NAME is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->qname = strdup(buffer);

	if(get_config(_config, "CTRL_QPATH", buffer) == NULL) {
		sprintf(ErrMsg, "CTRL_QPATH is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->ctrl_qpath = strdup(buffer);

	if(get_config(_config, "CTRL_QNAME", buffer) == NULL) {
		sprintf(ErrMsg, "CTRL_QNAME is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->ctrl_qname = strdup(buffer);

	if(get_config(_config, "DEL_HASH_Q_PATH", buffer) == NULL) {
		sprintf(ErrMsg, "DEL_HASH_Q_PATH is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->del_hash_q_path = strdup(buffer);

	if(get_config(_config, "DEL_HASH_Q_NAME", buffer) == NULL) {
		sprintf(ErrMsg, "DEL_HASH_Q_NAME is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->del_hash_q_name = strdup(buffer);

	if(get_config(_config, "RETURN_Q_PATH", buffer) == NULL) {
		sprintf(ErrMsg, "RETURN_Q_PATH is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->return_q_path = strdup(buffer);

	if(get_config(_config, "RETURN_Q_NAME", buffer) == NULL) {
		sprintf(ErrMsg, "RETURN_Q_NAME is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->return_q_name = strdup(buffer);

	if(get_config(_config, "CHANNEL_CO_RATIO_STRING_MAP_FILE", buffer) == NULL) {
		sprintf(ErrMsg, "CHANNEL_CO_RATIO_STRING_MAP_FILE is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->channel_co_ratio_string_map_file = strdup(buffer);

	if(get_config(_config, "CO_INITIAL_MAP_FILE", buffer) == NULL) {
		sprintf(ErrMsg, "CO_INITIAL_MAP_FILE is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->co_initial_map_file = strdup(buffer);

	if(get_config(_config, "QUEUE_CO_INITIAL_MAP_FILE", buffer) == NULL) {
		sprintf(ErrMsg, "QUEUE_CO_INITIAL_MAP_FILE is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->queue_co_initial_map_file = strdup(buffer);

	if(get_config(_config, "CHANNEL_CO_QUEUE_MAP_FILE", buffer) == NULL) {
		sprintf(ErrMsg, "CHANNEL_CO_QUEUE_MAP_FILE is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->channel_co_queue_map_file = strdup(buffer);

	if(get_config(_config, "CO_INITIAL_NAME_MAP_FILE", buffer) == NULL) {
		sprintf(ErrMsg, "CO_INITIAL_NAME_MAP_FILE is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->co_initial_name_map_file = strdup(buffer);

	if(get_config(_config, "FORWARD_CHANNEL_QUEUE_MAP_FILE", buffer) == NULL) {
		sprintf(ErrMsg, "FORWARD_CHANNEL_QUEUE_MAP_FILE is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->forward_channel_queue_map_file = strdup(buffer);


	if(get_config(_config, "HASH_MAP_PATH", buffer) == NULL) {
		sprintf(ErrMsg, "HASH_MAP_PATH is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->hash_map_path = strdup(buffer);

	if(get_config(_config, "UMS_HASH_MAP_NAME", buffer) == NULL) {
		sprintf(ErrMsg, "UMS_HASH_MAP_NAME is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->ums_hash_map_name = strdup(buffer);

	if(get_config(_config, "HISTORY_HASH_MAP_NAME", buffer) == NULL) {
		sprintf(ErrMsg, "HISTORY_HASH_MAP_NAME is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->history_hash_map_name = strdup(buffer);


	if(get_config(_config, "CO_CHANNEL_JSON_RULE_MAP_FILE", buffer) == NULL) {
		sprintf(ErrMsg, "CO_CHANNEL_JSON_RULE_MAP_FILE is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->co_channel_json_rule_map_file = strdup(buffer);

	if(get_config(_config, "CHECK_MSGSIZE_FLAG", buffer) == NULL) {
		sprintf(ErrMsg, "CHECK_MSGSIZE_FLAG is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}

	if( STRCCMP(buffer, "true") == 0 ) {
		c->check_msgsize_flag = true;
	}
	else {
		c->check_msgsize_flag = false;
	}

	if(get_config(_config, "FQPM_USE_FLAG", buffer) == NULL) {
		sprintf(ErrMsg, "FQPM_USE_FLAG is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}

	if( STRCCMP(buffer, "true") == 0 ) {
		c->fqpm_use_flag = true;
	}
	else {
		c->fqpm_use_flag = false;
	}

	if(get_config(_config, "UMS_GW_PROTOCOL", buffer) == NULL) {
		sprintf(ErrMsg, "UMS_GW_PROTOCOL is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->ums_gw_protocol = strdup(buffer);

	if(get_config(_config, "AUTO_CACHE_MAPFILE", buffer) == NULL) {
		sprintf(ErrMsg, "AUTO_CACHE_MAPFILE is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->auto_cache_mapfile = strdup(buffer);

	if(get_config(_config, "DISTRIBUTE_TYPE", buffer) == NULL) {
		sprintf(ErrMsg, "DISTRIBUTE_TYPE is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->distribute_type = strdup(buffer);

	if(get_config(_config, "WARNING_THRESHOLD", buffer) == NULL) {
		sprintf(ErrMsg, "WARNING_THRESHOLD is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->warning_threshold = atof(buffer);

	if(get_config(_config, "ADMIN_PHONE_NO", buffer) == NULL) {
		sprintf(ErrMsg, "ADMIN_PHONE_NO is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->admin_phone_no = strdup(buffer);

	if(get_config(_config, "SMS_ALARM_JSON_FORMAT_FILE", buffer) == NULL) {
		sprintf(ErrMsg, "SMS_ALARM_JSON_FORMAT_FILE is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->sms_alarm_json_format_file = strdup(buffer);

	if(get_config(_config, "VAR_DATA_JSON_KEY", buffer) == NULL) {
		sprintf(ErrMsg, "VAR_DATA_JSON_KEY is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->var_data_json_key = strdup(buffer);

	if(get_config(_config, "TEMPLATE_JSON_KEY", buffer) == NULL) {
		sprintf(ErrMsg, "TEMPLATE_JSON_KEY is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->template_json_key = strdup(buffer);

	if(get_config(_config, "MSG_JSON_TARGET_KEY", buffer) == NULL) {
		sprintf(ErrMsg, "MSG_JSON_TARGET_KEY is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->msg_json_target_key = strdup(buffer);

	if(get_config(_config, "WORKING_TIME_FROM", buffer) == NULL) {
		sprintf(ErrMsg, "WORKING_TIME_FROM is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->working_time_from = strdup(buffer);

	if(get_config(_config, "WORKING_TIME_TO", buffer) == NULL) {
		sprintf(ErrMsg, "WORKING_TIME_TO is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->working_time_to = strdup(buffer);

	if(get_config(_config, "WORKING_TIME_USE_FLAG", buffer) == NULL) {
		sprintf(ErrMsg, "WORKING_TIME_USE_FLAG is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}

	if( STRCCMP(buffer, "true") == 0 ) {
		c->working_time_use_flag = true;
	}
	else {
		c->working_time_use_flag = false;
	}

	if(get_config(_config, "SEQ_CHECK_HASH_MAP_FILE", buffer) == NULL) {
		sprintf(ErrMsg, "SEQ_CHECK_HASH_MAP_FILE is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->seq_check_hash_map_file = strdup(buffer);

	if(get_config(_config, "SEQ_CHECK_KEY_NAME", buffer) == NULL) {
		sprintf(ErrMsg, "SEQ_CHECK_KEY_NAME is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->seq_check_key_name = strdup(buffer);

	if(get_config(_config, "SEQ_CHECKING_USE_FLAG", buffer) == NULL) {
		sprintf(ErrMsg, "SEQ_CHECKING_USE_FLAG is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}

	if( STRCCMP(buffer, "true") == 0 ) {
		c->seq_checking_use_flag = true;
	}
	else {
		c->seq_checking_use_flag = false;
	}

	if(get_config(_config, "FQ_MON_SVR_HASH_PATH", buffer) == NULL) {
		sprintf(ErrMsg, "FQ_MON_SVR_HASH_PATH is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->fq_mon_svr_hash_path = strdup(buffer);

	if(get_config(_config, "FQ_MON_SVR_HASH_NAME", buffer) == NULL) {
		sprintf(ErrMsg, "FQ_MON_SVR_HASH_NAME is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->fq_mon_svr_hash_name = strdup(buffer);

	if(get_config(_config, "MALFORMED_REQUEST_FILE", buffer) == NULL) {
		sprintf(ErrMsg, "MALFORMED_REQUEST_FILE is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->malformed_request_file = strdup(buffer);

	if(get_config(_config, "CHANNEL_KEY", buffer) == NULL) {
		sprintf(ErrMsg, "CHANNEL_KEY is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->channel_key = strdup(buffer);

	if(get_config(_config, "SVC_CODE_KEY", buffer) == NULL) {
		sprintf(ErrMsg, "SVC_CODE_KEY is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->svc_code_key = strdup(buffer);

	if(get_config(_config, "DIST_RESULT_RULE_FILE", buffer) == NULL) {
		sprintf(ErrMsg, "DIST_RESULT_RULE_FILE is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->dist_result_json_rule_file = strdup(buffer);

	if(get_config(_config, "HEARTBEAT_HASH_MAP_PATH", buffer) == NULL) {
		sprintf(ErrMsg, "HEARTBEAT_HASH_MAP_PATH  is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->heartbeat_hash_map_path = strdup(buffer);

	if(get_config(_config, "HEARTBEAT_HASH_MAP_NAME", buffer) == NULL) {
		sprintf(ErrMsg, "HEARTBEAT_HASH_MAP_NAME is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->heartbeat_hash_map_name = strdup(buffer);

	if(get_config(_config, "FQ_PID_LIST_FILE", buffer) == NULL) {
		sprintf(ErrMsg, "FQ_PID_LIST_FILE is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->fq_pid_list_file = strdup(buffer);

	*my_conf = c;

	if(_config) free_config(&_config);

	return true;
}

void FreeConf( ratio_dist_conf_t *t)
{
	SAFE_FREE( t->log_filename);
	SAFE_FREE( t->log_level);
	SAFE_FREE( t->qpath);
	SAFE_FREE( t->qname);
	SAFE_FREE( t->ctrl_qpath);
	SAFE_FREE( t->ctrl_qname);
	SAFE_FREE( t->channel_co_ratio_string_map_file);
	SAFE_FREE( t->co_initial_map_file);
	SAFE_FREE( t->queue_co_initial_map_file);
	SAFE_FREE( t->channel_co_queue_map_file);
	SAFE_FREE( t->forward_channel_queue_map_file);
	SAFE_FREE( t->co_qformat_map_file);
	SAFE_FREE( t->co_json_qformat_map_file);
	SAFE_FREE( t->co_channel_json_rule_map_file);
	SAFE_FREE( t->ums_gw_protocol);
	SAFE_FREE( t->auto_cache_mapfile);
	SAFE_FREE( t->distribute_type);
	SAFE_FREE( t->admin_phone_no);
	SAFE_FREE( t->sms_alarm_json_format_file);
	SAFE_FREE( t->var_data_json_key);
	SAFE_FREE( t->template_json_key);
	SAFE_FREE( t->msg_json_target_key);
	SAFE_FREE( t->working_time_from);
	SAFE_FREE( t->working_time_to);
	SAFE_FREE( t->seq_check_hash_map_file);
	SAFE_FREE( t->seq_check_key_name);
	SAFE_FREE( t->del_hash_q_path);
	SAFE_FREE( t->del_hash_q_name);
	SAFE_FREE( t->return_q_path);
	SAFE_FREE( t->return_q_name);
	SAFE_FREE( t->hash_map_path);
	SAFE_FREE( t->ums_hash_map_name);
	SAFE_FREE( t->history_hash_map_name);
	SAFE_FREE( t->fq_mon_svr_hash_path);
	SAFE_FREE( t->fq_mon_svr_hash_name);
	SAFE_FREE( t->malformed_request_file);
	SAFE_FREE( t->channel_key);
	SAFE_FREE( t->svc_code_key);
	SAFE_FREE( t->dist_result_json_rule_file);
	SAFE_FREE( t->heartbeat_hash_map_path);
	SAFE_FREE( t->heartbeat_hash_map_name);
	SAFE_FREE( t->fq_pid_list_file);


	SAFE_FREE( t );
}
