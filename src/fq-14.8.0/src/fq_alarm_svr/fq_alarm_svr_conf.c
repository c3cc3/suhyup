/* 
** fq_alarm_svr_conf.c
*/

#include "fq_config.h"
#include "fq_param.h"
#include "fq_alarm_svr_conf.h"
#include "fq_logger.h"
#include "fq_common.h"
#include <stdio.h>

bool LoadConf(char *filename, fq_alarm_svr_conf_t **my_conf, char **ptr_errmsg)
{
	fq_alarm_svr_conf_t *c = NULL;
	config_t *_config=NULL;
	char ErrMsg[512];
	char buffer[256];

	_config = new_config(filename);
	c = (fq_alarm_svr_conf_t *)calloc(1, sizeof(fq_alarm_svr_conf_t));

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

	if(get_config(_config, "CHANNEL_CO_QUEUE_MAP_FILE", buffer) == NULL) {
		sprintf(ErrMsg, "CHANNEL_CO_QUEUE_MAP_FILE is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->channel_co_queue_map_file = strdup(buffer);

	if(get_config(_config, "ADMIN_PHONE_MAP_FILENAME", buffer) == NULL) {
		sprintf(ErrMsg, "ADMIN_PHONE_MAP_FILENAME is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->admin_phone_map_filename = strdup(buffer);


	if(get_config(_config, "TELECOM_JSON_TEMPLATE_FILENAME", buffer) == NULL) {
		sprintf(ErrMsg, "TELECOM_JSON_TEMPLATE_FILENAME is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->telecom_json_template_filename = strdup(buffer);

	if(get_config(_config, "ALARM_HISTORY_HASH_PATH", buffer) == NULL) {
		sprintf(ErrMsg, "ALARM_HISTORY_HASH_PATH is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->alarm_history_hash_path = strdup(buffer);

	if(get_config(_config, "ALARM_HISTORY_HASH_NAME", buffer) == NULL) {
		sprintf(ErrMsg, "ALARM_HISTORY_HASH_NAME is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->alarm_history_hash_name = strdup(buffer);

	if(get_config(_config, "DUP_PREVENT_SEC", buffer) == NULL) {
		sprintf(ErrMsg, "DUP_PREVENT_SEC is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->dup_prevent_sec = atoi(buffer);

	*my_conf = c;

	if(_config) free_config(&_config);

	return true;
}

void FreeConf( fq_alarm_svr_conf_t *t)
{
	SAFE_FREE( t->log_filename);
	SAFE_FREE( t->log_level);
	SAFE_FREE( t->qpath);
	SAFE_FREE( t->qname);
	SAFE_FREE( t->admin_phone_map_filename);
	SAFE_FREE( t->channel_co_queue_map_file);
	SAFE_FREE( t->telecom_json_template_filename);

	SAFE_FREE( t );
}
