/*
** heartbeat_test.c
*/
#include "fq_logger.h"
#include "fq_common.h"
#include "fq_heartbeat.h"
#include "fq_cache.h"

int main(int ac, char **av)
{
	int rc;
	fq_logger_t *l=NULL;
	char	logname[256];
	fqlist_t *t=NULL;
	char	path[128];
	char	db_filename[128];
	char	lock_filename[128];
	char	value[128];
	char	hostname[32+1];
	char	filename[128];
	char	*db_path=NULL;

	if( ac != 2 ) {
		printf("Usage: %s [process list filename] <enter>\n", av[0]);
		exit(0);
	}

	sprintf(logname, "./%s.log", "init_heartbeat_table");

	rc = fq_open_file_logger(&l, logname, FQ_LOG_TRACE_LEVEL);
	CHECK(rc==TRUE);

	gethostname(hostname, sizeof(hostname));

	t = load_process_list_from_file( av[1] );
	if( !t ) {
		printf("load_process_list_from_file() failed.\n");
		return(-1);
	}

	db_path = getenv("FQ_DATA_HOME");
	if( db_path ) {
		rc =  init_heartbeat_table( l, t, db_path, HB_DB_NAME, HB_LOCK_FILENAME);
	}
	else {
		rc =  init_heartbeat_table( l, t, HB_DB_PATH, HB_DB_NAME, HB_LOCK_FILENAME);
	}
	CHECK(rc==TRUE);

	if(t) fqlist_free(&t);

	printf("heartbeat table was initialized successfully.\n" );

	return(0);
}
