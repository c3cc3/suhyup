/*
** Description: This program adds an item to Heartbeat Table.
** add_heartbeat_table.c
*/

#include "fq_logger.h"
#include "fq_common.h"
#include "fq_heartbeat.h"

int main(int ac, char **av)
{
	int rc;
	char *db_path=NULL;
	fq_logger_t *l=NULL;
	char	logname[256];
	heartbeat_obj_t *obj=NULL;
	char	hostname[HOST_NAME_LEN+1];
	char	*distname = NULL;
	char	*progname = NULL;

	if(ac != 3) {
		printf("Usage: $ %s [DistName] [Progname]\n", av[0]);
		printf("ex)    $ %s dist1 prog_1\n", av[0]);
		exit(0);
	}

	distname = av[1];
	progname = av[2];

	gethostname(hostname, sizeof(hostname));

	sprintf(logname, "./%s.log", "add_heartbeat_table");

	rc = fq_open_file_logger(&l, logname, FQ_LOG_TRACE_LEVEL);
	CHECK(rc==TRUE);

	db_path = getenv("FQ_DATA_HOME");
	if( db_path ) {
		rc =  get_heartbeat_obj( l, hostname, db_path, HB_DB_NAME, HB_LOCK_FILENAME, &obj);
	}
	else {
		rc =  get_heartbeat_obj( l, hostname, HB_DB_PATH, HB_DB_NAME, HB_LOCK_FILENAME, &obj);
	}
	if(rc<0) {
		fq_log(l, FQ_LOG_ERROR, "get_heartbeat_obj() failed.");
        return(0);
	}

	rc = obj->on_add_table(l, obj, distname, progname, hostname);
	if( rc == FALSE) {
		fq_log(l, FQ_LOG_ERROR, "on_add_table() failed.");
	}

	close_heartbeat_obj(l, &obj);

	printf("all success!\n");

	return(0);
}
