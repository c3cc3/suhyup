/* vi: set sw=4 ts=4: */
/******************************************************************
 ** hash_manage.c
 */
#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

#include "fq_hashobj.h"
#include "hash_manage.h"
#include "fq_common.h"
#include "fq_logger.h"

#define HASH_MANAGER_C_VERSION "1.0.0"

/******************************************************************
**
*/
int hash_manage_one(fq_logger_t *l, char *path, char *hashname, hash_cmd_t cmd )
{
	int rc=FALSE;

	FQ_TRACE_ENTER(l);

	switch(cmd) {
		case HASH_CREATE:
			rc = CreateHashMapFiles(l, path, hashname);
			break;
		case HASH_UNLINK:
			printf("[%s][%s].\n", path, hashname);
			rc = UnlinkHashMapFiles(l, path, hashname);
			rc = TRUE;
			break;
		case HASH_CLEAN:
			rc = CleanHashMap(l, path, hashname);
			break;
		case HASH_EXTEND:
			rc = ExtendHashMap(l, path, hashname);
			break;
		case HASH_VIEW:
			rc = ViewHashMap(l, path, hashname);
			break;
		case HASH_SCAN:
			rc = ScanHashMap(l, path, hashname);
			break;
		default:
			rc = FALSE;
			fq_log(l, FQ_LOG_ERROR, "unsupporting cmd = [%d].", cmd);
			break;
	}
	
	FQ_TRACE_EXIT(l);
	return(rc);
}

int hash_manage_all(fq_logger_t *l, char *path, hash_cmd_t cmd )
{
	int		rc=FALSE;
	FILE	*fp=NULL;
	char	filename[128];
	char	hashname[80];

	FQ_TRACE_ENTER(l);

	sprintf(filename, "%s/%s", path, HASH_LIST_FILE_NAME);

	fp = fopen(filename, "r");
	if(fp == NULL) {
		fq_log(l, FQ_LOG_ERROR, "fopen() error. reason=[%s]", strerror(errno));
		goto return_FALSE;
	}

	while( fgets( hashname, 80, fp) ) {
		str_lrtrim(hashname);
		if( hashname[0] == '#' ) continue;
		rc = hash_manage_one(l, path, hashname, cmd);
		if( rc == FALSE) {
			fq_log(l, FQ_LOG_ERROR, ">>>>>>>>  managing result : failed: %s, %s\n", path, hashname);
			goto return_FALSE;
		}
		else {
			fq_log(l, FQ_LOG_DEBUG, ">>>>>>>>  managing result : success: %s, %s\n", path, hashname);
		}
	}

	if(fp) fclose(fp);

	return(TRUE);

return_FALSE:
	if(fp) fclose(fp);

	FQ_TRACE_EXIT(l);
	return(FALSE);
}
