/* 
** fq_alarm_client_conf.c
*/

#include "fq_config.h"
#include "fq_param.h"
#include "fq_alarm_client_conf.h"
#include "fq_logger.h"
#include "fq_common.h"
#include <stdio.h>

bool LoadConf(char *filename, fq_alarm_client_conf_t **my_conf, char **ptr_errmsg)
{
	fq_alarm_client_conf_t *c = NULL;
	config_t *_config=NULL;
	char ErrMsg[512];
	char buffer[256];

	_config = new_config(filename);
	c = (fq_alarm_client_conf_t *)calloc(1, sizeof(fq_alarm_client_conf_t));

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

	if(get_config(_config, "EN_Q_PATH", buffer) == NULL) {
		sprintf(ErrMsg, "EN_Q_PATH is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->qpath = strdup(buffer);

	if(get_config(_config, "EN_Q_NAME", buffer) == NULL) {
		sprintf(ErrMsg, "EN_Q_NAME is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->qname = strdup(buffer);

	if(get_config(_config, "JSON_TEMPLATE_PATHFILE", buffer) == NULL) {
		sprintf(ErrMsg, "JSON_TEMPLATE_PATHFILE is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->json_template_pathfile = strdup(buffer);

	*my_conf = c;

	if(_config) free_config(&_config);

	return true;
}

void FreeConf( fq_alarm_client_conf_t *t)
{
	SAFE_FREE( t->log_filename);
	SAFE_FREE( t->log_level);
	SAFE_FREE( t->qpath);
	SAFE_FREE( t->qname);
	SAFE_FREE( t->json_template_pathfile);

	SAFE_FREE( t );
}
