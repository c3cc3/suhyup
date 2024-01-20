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

typedef struct _fq_mon_svr_conf fq_mon_svr_conf_t;
struct _fq_mon_svr_conf {
	char	*log_filename;
	char	*log_level;
	int		i_log_level;
	char	*hashmap_path;
	char	*hashmap_name;
};

bool LoadConf(char *filename, fq_mon_svr_conf_t **c, char **ptr_errmsg);
void FreeConf(fq_mon_svr_conf_t *c);

#ifdef __cpluscplus
}
#endif
#endif
