/* 
** subQ_dist_conf.c
*/

#include "fq_config.h"
#include "fq_param.h"
#include "subQ_dist_conf.h"
#include "fq_logger.h"
#include "fq_common.h"
#include <stdio.h>

bool LoadConf(char *filename, subQ_dist_conf_t **my_conf, char **ptr_errmsg)
{
	subQ_dist_conf_t *c = NULL;
	config_t *_config=NULL;
	char ErrMsg[512];
	char buffer[256];

	_config = new_config(filename);
	c = (subQ_dist_conf_t *)calloc(1, sizeof(subQ_dist_conf_t));

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

	if(get_config(_config, "SUB_QUEUE_LIST_FILE", buffer) == NULL) {
		sprintf(ErrMsg, "SUB_QUEUE_LIST_FILE is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->sub_queue_list_file = strdup(buffer);

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

	if(get_config(_config, "QFORMAT_FILE", buffer) == NULL) {
		sprintf(ErrMsg, "QFORMAT_FILE is undefined in your config file.");
		*ptr_errmsg = strdup(ErrMsg);
		return false;
	}
	c->qformat_file = strdup(buffer);

	*my_conf = c;

	if(_config) free_config(&_config);

	return true;
}

void FreeConf( subQ_dist_conf_t *t)
{
	SAFE_FREE( t->log_filename);
	SAFE_FREE( t->log_level);
	SAFE_FREE( t->qpath);
	SAFE_FREE( t->qname);
	SAFE_FREE( t->sub_queue_list_file);
	SAFE_FREE( t->qformat_file);

	SAFE_FREE( t );
}

