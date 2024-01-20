/* carrier_msg_lib.c */

#include "fq_common.h"
#include "fq_codemap.h"
#include "fq_manage.h"
#include "fq_linkedlist.h"
#include "fqueue.h"
#include "fq_file_list.h"

#include "subQ_dist_conf.h"
#include "ums_common_conf.h"
#include "queue_ctl_lib.h"
#include "fq_manage.h"
#include "fq_cache.h"
#include "fq_tci.h"

bool Make_a_carrier_msg_with_umsmsg(fq_logger_t *l, ums_msg_t *ums_msg, qformat_t *q, fqcache_t *cache)
{
	// cache_update
	cache_update(cache, "svc_code", ums_msg->svc_code, 0);
	cache_update(cache, "name", ums_msg->name, 0);
	cache_update(cache, "address", ums_msg->address, 0);

	int len;
	fill_qformat(q, NULL, cache, NULL);
	len = assoc_qformat(q, NULL);
	CHECK(len>0);

	fq_log(l, FQ_LOG_DEBUG, "q->data:'%s'.", q->data);
	fq_log(l, FQ_LOG_DEBUG, "q->datalen:'%d'.", q->datalen);

	return true;
}
