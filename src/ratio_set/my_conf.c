/* 
** my_conf.c
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

	if(get_config(_config, "IP", buffer) == NULL) {
		sprintf(ErrMsg, "IP is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->ip = strdup(buffer);

	if(get_config(_config, "PORT", buffer) == NULL) {
		sprintf(ErrMsg, "PORT is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->port = strdup(buffer);

	if(get_config(_config, "CHANNEL", buffer) == NULL) {
		sprintf(ErrMsg, "CHANNEL is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->channel = strdup(buffer);

	if(get_config(_config, "RATIO_STRING", buffer) == NULL) {
		sprintf(ErrMsg, "RATIO_STRING is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->ratio_string = strdup(buffer);

	*my_conf = c;

	if(_config) free_config(&_config);

	return true;
}

void FreeConf( my_conf_t *t)
{
	SAFE_FREE( t->log_filename);
	SAFE_FREE( t->log_level);
	SAFE_FREE( t->ip);
	SAFE_FREE( t->port);
	SAFE_FREE( t->channel);
	SAFE_FREE( t->ratio_string);

	SAFE_FREE( t );
}
