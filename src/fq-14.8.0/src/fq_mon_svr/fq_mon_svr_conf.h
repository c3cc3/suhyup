/*
** ratio_dist_conf.h
*/
#ifndef _FQ_MON_SVR_CONF_H
#define _FQ_MON_SVR_CONF_H

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
	long	collect_cycle_usec;
	char	*listfile_to_exclude;
	float	alarm_usage_threshold;	
	long	unchanged_period_sec_en;
	long	unchanged_period_sec_de;
	int		de_tps_threshold;
	bool	fqpm_use_flag;
	bool	do_not_flow_check_flag;
	int		do_not_flow_check_sec;
	bool	alarm_use_flag;
	char 	*alarm_qpath;
	char	*alarm_qname;
	char	*json_template_pathfile;
	long	lock_threshold_micro_sec;
};

bool LoadConf(char *filename, fq_mon_svr_conf_t **c, char **ptr_errmsg);
void FreeConf(fq_mon_svr_conf_t *c);

#ifdef __cpluscplus
}
#endif
#endif
