/*
** heartbeat_status.c
*/
#include "fq_logger.h"
#include "fq_common.h"
#include "fq_scanf.h"
#include "fq_heartbeat.h"

#define MAX_LINE 128
int main(int argc, char **argv)
{
	int rc;
	int my_index = -1;
	fq_logger_t *l=NULL;
	char	logname[256];
	char	hostname[HOST_NAME_LEN+1];
	p_status_t	status;
	heartbeat_obj_t *obj=NULL;
	FILE *fp=NULL;
	char	distname[DIST_NAME_LEN+1];
	char	appID[PROG_NAME_LEN+1];
	char	buf[128];
	int	delimiter = '|';
	char	*db_path=NULL;

	if( argc != 2 ) {
		printf("Usage: $ %s [process list filename] <enter> \n", argv[0]);
		return -1;
	}

	gethostname(hostname, sizeof(hostname));
	sprintf(logname, "./%s.log", "heartbeat_status");

	rc = fq_open_file_logger(&l, logname, FQ_LOG_TRACE_LEVEL);
	CHECK(rc==TRUE);

	db_path = getenv("FQ_DATA_HOME");
	if( db_path ) {
		rc =  get_heartbeat_obj( l, hostname, db_path, HB_DB_NAME, HB_LOCK_FILENAME, &obj);
	}
	else {
		rc =  get_heartbeat_obj( l, hostname, HB_DB_PATH, HB_DB_NAME, HB_LOCK_FILENAME, &obj);
	}
	if( rc < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "open_heartbeat_obj() failed.");
		return(0);
	}

re_do:
	fp = fopen(argv[1], "r");
	CHECK(fp != NULL);

	while( fgets( buf, MAX_LINE, fp )) {
		buf[strlen(buf)-1] = 0x00; /* remove newline charactor */
		fq_sscanf(delimiter, buf, "%s%s", distname, appID);
		printf("distname=[%s], appID(monID)=[%s], hostname=[%s]\n", distname, appID, hostname);

		status = -1;
		rc = obj->on_get_status(l, obj, distname, appID, hostname, &status);
		CHECK(rc==TRUE);

		print_heartbeat_status(status);
	}
	fclose(fp);

	sleep(1);
	goto re_do;
	

	
	printf("all success!\n");

	return(0);
}
