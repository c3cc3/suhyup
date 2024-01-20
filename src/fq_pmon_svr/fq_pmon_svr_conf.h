/*
** ratio_dist_conf.h
*/
#ifndef _FQ_PMON_SVR_CONF_H
#define _FQ_PMON_SVR_CONF_H

#include <stdbool.h>
#include "fq_config.h"

#ifdef __cpluscplus
extern "C"
{
#endif 

typedef struct _fq_pmon_svr_conf fq_pmon_svr_conf_t;
struct _fq_pmon_svr_conf {
	char	*log_filename;
	char	*log_level;
	int		i_log_level;
	char	*hashmap_path;
	char	*hashmap_name;
	bool	alarm_use_flag;
	char 	*alarm_qpath;
	char	*alarm_qname;
	char	*json_template_pathfile;
	long	block_threshold_sec;
	char	*fq_pid_list_file;
};

bool LoadConf(char *filename, fq_pmon_svr_conf_t **c, char **ptr_errmsg);
void FreeConf(fq_pmon_svr_conf_t *c);

#ifdef __cpluscplus
}
#endif
#endif
