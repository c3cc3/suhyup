/* vi: set sw=4 ts=4: */
/*
** fq_info_param.c
*/
#include "config.h"
#include "fq_info.h"
#include "fq_common.h"

#define FQ_INFO_PARAM_C_VERSION "1.0.0"

int load_info_param(fq_logger_t *l, fq_info_t *t, const char* fq_info_filename)
{
	FQ_TRACE_ENTER(l);

	ASSERT(fq_info_filename);

	if ( read_fq_info(l, t, fq_info_filename) == -1 ) {
		fq_log(l, FQ_LOG_ERROR, "read_fq_info('%s') error.", fq_info_filename);
		FQ_TRACE_EXIT(l);
		return(-1);
	}

	FQ_TRACE_EXIT(l);
	return(0);
}
