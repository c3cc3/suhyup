/*
** heartbeat_change.c
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
	char	*hostname = NULL;
	char	*distname = NULL;

	if(ac != 3) {
		printf("Usage: $ %s [Current Master hostname] [DistName]\n", av[0]);
		printf("ex)    $ %s server-1 dist_1 \n", av[0]);
		printf("ex)    $ %s server-2 dist_1 \n", av[0]);
		exit(0);
	}

	hostname = av[1];
	distname = av[2];

	sprintf(logname, "/tmp/%s.log", "heartbeat_change");

	rc = fq_open_file_logger(&l, logname, FQ_LOG_TRACE_LEVEL);
	// rc = fq_open_file_logger(&l, logname, FQ_LOG_INFO_LEVEL);
	CHECK(rc==TRUE);

	rc =  get_heartbeat_obj( l, hostname, "/tmp" , "heartbeat.DB", "heartbeat",  &obj);
	if(rc<0) {
		fq_log(l, FQ_LOG_ERROR, "get_heartbeat_obj() failed.");
        return(0);
	}

	obj->on_change(l, obj, distname, NULL);
	// obj->on_change(l, obj, distname, "prog_1");

	close_heartbeat_obj(l, &obj);

	printf("all success!\n");

	return(0);
}
