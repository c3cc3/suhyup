/* 
** virtual_gw_conf.c
*/

#include "fq_config.h"
#include "fq_param.h"
#include "fq_logger.h"
#include "fq_common.h"
#include <stdio.h>

#include "my_conf.h"

bool LoadConf(char *filename, my_conf_t **my_conf, char **ptr_errmsg)
{
	my_conf_t *c = NULL;
	config_t *_config=NULL;
	char ErrMsg[512];
	char buffer[256];

	_config = new_config(filename);
	c = (my_conf_t *)calloc(1, sizeof(my_conf_t));

	if(load_param(_config, filename) < 0 ) {
		sprintf(ErrMsg, "Can't load '%s'.", filename);
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}

	/* Check whether essential items(mandatory) are omitted. */
	if(get_config(_config, "LOG_PATHFILE", buffer) == NULL) {
		sprintf(ErrMsg, "LOG_PATHFILE is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->log_pathfile = strdup(buffer);

	if(get_config(_config, "LOG_LEVEL", buffer) == NULL) {
		sprintf(ErrMsg, "LOG_LEVEL is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->log_level = strdup(buffer);
	c->i_log_level = get_log_level(c->log_level);

	if(get_config(_config, "DE_Q_PATH", buffer) == NULL) {
		sprintf(ErrMsg, "DE_Q_PATH is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->de_qpath = strdup(buffer);

	if(get_config(_config, "DE_Q_NAME", buffer) == NULL) {
		sprintf(ErrMsg, "DE_Q_NAME is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->de_qname = strdup(buffer);

	if(get_config(_config, "XA_FLAG", buffer) == NULL) {
		sprintf(ErrMsg, "XA_FLAG is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	if(STRCCMP( buffer, "TRUE") == 0 ) {
		c->xa_flag = true;
	}
	else {
		c->xa_flag = false;
	}

	if(get_config(_config, "MULTI_THREAD_FLAG", buffer) == NULL) {
		sprintf(ErrMsg, "MULTI_THREAD_FLAG is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	if(STRCCMP( buffer, "TRUE") == 0 ) {
		c->multi_thread_flag = true;
	}
	else {
		c->multi_thread_flag = false;
	}

	if(get_config(_config, "MAX_MULTI_THREADS", buffer) == NULL) {
		sprintf(ErrMsg, "MAX_MULTI_THREADS is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->max_multi_threads = atoi(buffer);

	if(get_config(_config, "REMOTE_EN_Q_PATH", buffer) == NULL) {
		sprintf(ErrMsg, "REMOTE_EN_Q_PATH is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->remote_en_qpath = strdup(buffer);

	if(get_config(_config, "REMOTE_EN_Q_NAME", buffer) == NULL) {
		sprintf(ErrMsg, "REMOTE_EN_Q_NAME is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->remote_en_qname = strdup(buffer);

	if(get_config(_config, "HANDLER_IP1", buffer) == NULL) {
		sprintf(ErrMsg, "HANDLER_IP1 is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->handler_ip1 = strdup(buffer);

	if(get_config(_config, "HANDLER_IP2", buffer) == NULL) {
		sprintf(ErrMsg, "HANDLER_IP2 is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->handler_ip2 = strdup(buffer);

	if(get_config(_config, "HANDLER_PORT", buffer) == NULL) {
		sprintf(ErrMsg, "HANDLER_PORT is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->handler_port = strdup(buffer);
	c->i_handler_port = atoi(buffer);

	*my_conf = c;

	if(_config) free_config(&_config);

	return true;
}

void FreeConf( my_conf_t *t)
{
	SAFE_FREE( t->log_pathfile);
	SAFE_FREE( t->log_level);
	SAFE_FREE( t->de_qpath);
	SAFE_FREE( t->de_qname);
	SAFE_FREE( t->remote_en_qpath);
	SAFE_FREE( t->remote_en_qname);
	SAFE_FREE( t->handler_ip1);
	SAFE_FREE( t->handler_ip2);
	SAFE_FREE( t->handler_port);

	SAFE_FREE( t );
}
