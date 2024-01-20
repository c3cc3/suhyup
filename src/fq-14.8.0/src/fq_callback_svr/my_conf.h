/*
** ratio_dist_conf.h
*/
#ifndef _MY_CONF_H
#define _MY_CONF_H

#include <stdbool.h>
#include "fq_config.h"

#ifdef __cpluscplus
extern "C"
{
#endif 

typedef struct _my_conf my_conf_t;
struct _my_conf {
	char 	*log_pathfile;
	char	*log_level;
	int		i_log_level;
	char	*qpath; /* file queue for reading */
	char	*qname;
	bool	xa_flag;
	bool	multi_thread_flag;
	int		max_multi_threads;
};

bool LoadConf(char *filename, my_conf_t **c, char **ptr_errmsg);
void FreeConf(my_conf_t *c);

#ifdef __cpluscplus
}
#endif
#endif
