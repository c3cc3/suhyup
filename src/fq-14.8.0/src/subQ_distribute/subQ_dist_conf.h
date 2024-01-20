/*
** subQ_dist_conf.h
*/
#ifndef _SUBQ_DIST_CONF_H
#define _SUBQ_DIST_CONF_H

#include <stdbool.h>
#include "fq_config.h"

#ifdef __cpluscplus
extern "C"
{
#endif

typedef struct _subQ_dist_conf {
	char	*log_filename;
	char	*log_level;
	int		i_log_level;
	char	*qpath; /* file queue */
	char	*qname;
	char	*sub_queue_list_file;
	bool	check_msgsize_flag;
	char	*qformat_file;
	bool	fqpm_use_flag;

}subQ_dist_conf_t;

bool LoadConf(char *filename, subQ_dist_conf_t **c, char **ptr_errmsg);
void FreeConf(subQ_dist_conf_t *c);

#ifdef __cpluscplus
}
#endif
#endif
