/*
** view_heartbeat_table.c
*/
#include "fq_logger.h"
#include "fq_common.h"
#include "fq_heartbeat.h"

int main(int ac, char **av)
{
	int rc;
	fq_logger_t *l=NULL;
	char	logname[256];
	heartbeat_obj_t *obj=NULL;
	int	cnt=0;
	char *all_string = NULL;
	char hostname[HOST_NAME_LEN+1];
	char	*db_path=NULL;

	sprintf(logname, "./%s.log", "view_heartbeat_table");

	rc = fq_open_file_logger(&l, logname, FQ_LOG_TRACE_LEVEL);
	CHECK(rc==TRUE);

	gethostname(hostname, sizeof(hostname));

	db_path = getenv("FQ_DATA_HOME");
	if(db_path) {
		rc =  get_heartbeat_obj( l, hostname, db_path, HB_DB_NAME, HB_LOCK_FILENAME, &obj);
	}
	else {
		rc =  get_heartbeat_obj( l, hostname, HB_DB_PATH , HB_DB_NAME, HB_LOCK_FILENAME, &obj);
	}
	if( rc < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "get_heartbeat_obj() failed.");
		return(0);
	}
re_do:

#if 1
	obj->on_print_table(NULL, obj);
#else
	obj->on_getlist(l, obj, &cnt, &all_string);
	printf("cnt = [%d]\n", cnt);
	printf("all_string = [%s]\n", all_string);
	
	if( cnt > 0 )
		printf("cnt=[%d] all_string = [%s]\n", cnt, all_string);

	if( all_string ) free(all_string);
#endif

	sleep(1);
	goto re_do;

	close_heartbeat_obj(l, &obj);

	printf("all success!\n");

	return(0);
}
