/* 
** fq_handler.c.c
*/

#include "fq_config.h"
#include "fq_param.h"
#include "fq_logger.h"
#include "fq_common.h"
#include <stdio.h>

#include "fq_handler_conf.h"

bool LoadConf(char *filename, fq_handler_conf_t **my_conf, char **ptr_errmsg)
{
	fq_handler_conf_t *c = NULL;
	config_t *_config=NULL;
	char ErrMsg[512];
	char buffer[256];

	_config = new_config(filename);
	c = (fq_handler_conf_t *)calloc(1, sizeof(fq_handler_conf_t));

	if(load_param(_config, filename) < 0 ) {
		sprintf(ErrMsg, "Can't load '%s'.", filename);
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}

	if(get_config(_config, "USE_FILESYSTEM_LIST_FILE", buffer) == NULL) {
		sprintf(ErrMsg, "USE_FILESYSTEM_LIST_FILE is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->use_filesystem_list_file = strdup(buffer);

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

	if(get_config(_config, "SERVER_IP", buffer) == NULL) {
		sprintf(ErrMsg, "SERVER_IP is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->server_ip = strdup(buffer);

	if(get_config(_config, "SERVER_PORT", buffer) == NULL) {
		sprintf(ErrMsg, "SERVER_PORT is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->server_port = strdup(buffer);
	c->i_server_port = atoi(c->server_port);

	if(get_config(_config, "BIN_DIR", buffer) == NULL) {
		sprintf(ErrMsg, "BIN_DIR is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->bin_dir = strdup(buffer);

	if(get_config(_config, "EVENTLOG_FILENAME", buffer) == NULL) {
		sprintf(ErrMsg, "EVENTLOG_FILENAME is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->eventlog_filename = strdup(buffer);

	if(get_config(_config, "SESSION_TIMEOUT_SEC", buffer) == NULL) {
		sprintf(ErrMsg, "SESSION_TIMEOUT_SEC is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->session_timeout_sec = atoi(buffer);

	*my_conf = c;

	if(_config) free_config(&_config);

	return true;
}

void FreeConf( fq_handler_conf_t *t)
{
	SAFE_FREE( t->use_filesystem_list_file);
	SAFE_FREE( t->log_filename);
	SAFE_FREE( t->log_level);
	SAFE_FREE( t->server_ip);
	SAFE_FREE( t->server_port);
	SAFE_FREE( t->bin_dir);

	SAFE_FREE( t );
}
