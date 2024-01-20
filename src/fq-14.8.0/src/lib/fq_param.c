/*
** fq_param.c
*/
#define FQ_PARAM_C_VERSION "1.0.0"

#include "config.h"

#include "fq_config.h"
#include "fq_common.h"


int load_param(config_t *t, const char* config_filename)
{
	ASSERT(config_filename);

	if ( read_config(t, config_filename) == -1 )
		return(-1);

	return(0);
}

int load_multi_param(config_t *t, const char* config_filename, const char *svrname)
{
	ASSERT(config_filename);

	if ( read_multi_config(t, config_filename, svrname) == -1 )
		return(-1);

	return(0);
}
