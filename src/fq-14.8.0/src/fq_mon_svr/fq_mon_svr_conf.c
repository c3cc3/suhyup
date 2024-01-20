/* 
** fq_mon_svr_conf.c
*/

#include "fq_config.h"
#include "fq_param.h"
#include "fq_logger.h"
#include "fq_common.h"
#include <stdio.h>

#include "fq_mon_svr_conf.h"

bool LoadConf(char *filename, fq_mon_svr_conf_t **my_conf, char **ptr_errmsg)
{
	fq_mon_svr_conf_t *c = NULL;
	config_t *_config=NULL;
	char ErrMsg[512];
	char buffer[256];

	_config = new_config(filename);
	c = (fq_mon_svr_conf_t *)calloc(1, sizeof(fq_mon_svr_conf_t));

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

	if(get_config(_config, "HASHMAP_PATH", buffer) == NULL) {
		sprintf(ErrMsg, "HASHMAP_PATH is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->hashmap_path = strdup(buffer);

	if(get_config(_config, "HASHMAP_NAME", buffer) == NULL) {
		sprintf(ErrMsg, "HASHMAP_NAME is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->hashmap_name = strdup(buffer);

	if(get_config(_config, "COLLECT_CYCLE_USEC", buffer) == NULL) {
		sprintf(ErrMsg, "COLLECT_CYCLE_USECHASHMAP_NAME is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->collect_cycle_usec = atol(buffer);

	if(get_config(_config, "LISTFILE_TO_EXCLUDE", buffer) == NULL) {
		sprintf(ErrMsg, "LISTFILE_TO_EXCLUDE is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->listfile_to_exclude = strdup(buffer);

	if(get_config(_config, "ALARM_USAGE_THRESHOLD", buffer) == NULL) {
		sprintf(ErrMsg, "ALARM_USAGE_THRESHOLD is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->alarm_usage_threshold = atof(buffer);

	if(get_config(_config, "UNCHANGED_PERIOD_SEC_EN", buffer) == NULL) {
		sprintf(ErrMsg, "UNCHANGED_PERIOD_SEC_EN is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->unchanged_period_sec_en = atol(buffer);

	if(get_config(_config, "UNCHANGED_PERIOD_SEC_DE", buffer) == NULL) {
		sprintf(ErrMsg, "UNCHANGED_PERIOD_SEC_DE is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->unchanged_period_sec_de = atol(buffer);

	if(get_config(_config, "DE_TPS_THRESHOLD", buffer) == NULL) {
		sprintf(ErrMsg, "DE_TPS_THRESHOLD is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->de_tps_threshold = atol(buffer);

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

	if(get_config(_config, "DO_NOT_FLOW_CHECK_FLAG", buffer) == NULL) {
		sprintf(ErrMsg, "DO_NOT_FLOW_CHECK_FLAG is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}

	if( STRCCMP(buffer, "true") == 0 ) {
		c->do_not_flow_check_flag = true;
	}
	else {
		c->do_not_flow_check_flag = false;
	}

	if(get_config(_config, "DO_NOT_FLOW_CHECK_SEC", buffer) == NULL) {
		sprintf(ErrMsg, "DO_NOT_FLOW_CHECK_SEC is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->do_not_flow_check_sec = atoi(buffer);

	if(get_config(_config, "ALARM_USE_FLAG", buffer) == NULL) {
		sprintf(ErrMsg, "ALARM_USE_FLAG is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}

	if( STRCCMP(buffer, "true") == 0 ) {
		c->alarm_use_flag = true;
	}
	else {
		c->alarm_use_flag = false;
	}

	if(get_config(_config, "ALARM_Q_PATH", buffer) == NULL) {
		sprintf(ErrMsg, "ALARM_Q_PATH is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->alarm_qpath = strdup(buffer);

	if(get_config(_config, "ALARM_Q_NAME", buffer) == NULL) {
		sprintf(ErrMsg, "ALARM_Q_NAME is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->alarm_qname = strdup(buffer);

	if(get_config(_config, "JSON_TEMPLATE_PATHFILE", buffer) == NULL) {
		sprintf(ErrMsg, "JSON_TEMPLATE_PATHFILE is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->json_template_pathfile = strdup(buffer);

	if(get_config(_config, "LOCK_THRESHOLD_MICRO_SEC", buffer) == NULL) {
		sprintf(ErrMsg, "LOCK_THRESHOLD_MICRO_SEC is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->lock_threshold_micro_sec = atol(buffer);

	*my_conf = c;

	if(_config) free_config(&_config);

	return true;
}

void FreeConf( fq_mon_svr_conf_t *t)
{
	SAFE_FREE( t->log_filename);
	SAFE_FREE( t->log_level);
	SAFE_FREE( t->alarm_qpath);
	SAFE_FREE( t->alarm_qname);
	SAFE_FREE( t->json_template_pathfile);

	SAFE_FREE( t );
}
