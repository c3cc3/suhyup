/*
** carrier_msg_lib.h
*/
#ifndef _CARRIER_MSG_LIB_H
#define _CARRIER_MSG_LIB_H

#include <stdbool.h>
#include "fq_config.h"
#include "subQ_dist_conf.h"
#include "fq_linkedlist.h"
#include "fq_file_list.h"
#include "queue_ctl_lib.h"
#include "fq_cache.h"
#include "fq_tci.h"

#ifdef __cpluscplus
extern "C"
{
#endif

bool Make_a_carrier_msg_with_umsmsg(fq_logger_t *l, ums_msg_t *ums_msg, qformat_t *q, fqcache_t *cache);

#ifdef __cpluscplus
}
#endif
#endif

