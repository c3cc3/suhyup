/*
** fq_handler_chnf.h.h
*/
#ifndef _FQ_HANDLER_CONF_H
#define _FQ_HANDLER_CONF_H

#include <stdbool.h>
#include "fq_config.h"

#ifdef __cpluscplus
extern "C"
{
#endif 

typedef struct _fq_handler_conf fq_handler_conf_t;
struct _fq_handler_conf {
	char	*log_filename;
	char	*log_level;
	int		i_log_level;
	char 	*server_ip;
	char	*server_port;
	int		i_server_port;
	char	*bin_dir;
	int		sess_timeout_sec;
};

bool LoadConf(char *filename, fq_handler_conf_t **c, char **ptr_errmsg);
void FreeConf(fq_handler_conf_t *c);

#ifdef __cpluscplus
}
#endif
#endif
