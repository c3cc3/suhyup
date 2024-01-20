/* vi: set sw=4 ts=4: */
/*
** hash_info_param.c
*/
#include "config.h"
#include "hash_info.h"
#include "fq_common.h"

#define HASH_INFO_PARAM_C_VERSION "1.0.0"

int load_hash_info_param(hash_info_t *t, const char* hash_info_filename)
{
	ASSERT(hash_info_filename);

	if ( read_hash_info(t, hash_info_filename) == -1 )
		return(-1);

	return(0);
}
