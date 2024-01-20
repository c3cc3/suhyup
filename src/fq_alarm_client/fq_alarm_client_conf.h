/*
** fq_alarm_client_conf.h
*/
#ifndef _FQ_ALARM_SVR_CONF_H
#define _FQ_ALARM_SVR_CONF_H

#include <stdbool.h>
#include "fq_config.h"

#ifdef __cpluscplus
extern "C"
{
#endif 

typedef struct _fq_alarm_client_conf fq_alarm_client_conf_t;
struct _fq_alarm_client_conf {
	char	*log_filename;
	char	*log_level;
	int		i_log_level;
	char	*qpath; /* file queue for reading */
	char	*qname;
	char	*json_template_pathfile;
	char	*err_qpath;
	char	*err_qname;
	char	*err_msg;
};

bool LoadConf(char *filename, fq_alarm_client_conf_t **c, char **ptr_errmsg);
void FreeConf(fq_alarm_client_conf_t *c);

#ifdef __cpluscplus
}
#endif
#endif
