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

typedef struct _my_conf my_conf_t;
struct _my_conf {
	char	*log_filename;
	char	*log_level;
	int		i_log_level;
	char	*ip; /* file queue for reading */
	char	*port;
	char	*channel;
	char	*ratio_string;
};

bool LoadConf(char *filename, my_conf_t **c, char **ptr_errmsg);
void FreeConf(my_conf_t *c);

#ifdef __cpluscplus
}
#endif
#endif
