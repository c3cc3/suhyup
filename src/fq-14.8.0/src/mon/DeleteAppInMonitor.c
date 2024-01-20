/* vi: set sw=4 ts=4: */
/*
** DeleteAppInMonitor.c
*/

#include <stdio.h>
#include <sys/types.h>
#include "fqueue.h"
#include "fq_logger.h"
#include "fq_common.h"
#include "fq_timer.h"
#include "fq_tokenizer.h"

char g_ProgName[64];


#include "fqueue.h"
#include "fq_monitor.h"
#include "fq_common.h"

int main(int ac, char **av)  
{  
	int rc;
	fq_logger_t *l=NULL;
	char	*logname="/tmp/GetAppStatus.log";
	monitor_obj_t 	*obj=NULL;
	char	*appID=NULL;

	if( ac != 2 ) {

        printf("Usage: $ %s [appID]\n", av[0]);
        printf("Usage: $ %s deFQ\n", av[0]);
        return(0);
    }
	printf("Compiled on %s %s\n", __TIME__, __DATE__);

	appID = av[1];

	rc = fq_open_file_logger(&l, logname, FQ_LOG_ERROR_LEVEL);
	CHECK( rc > 0 );

	rc =  open_monitor_obj(l,  &obj);
	CHECK(rc==TRUE);

	if( obj->on_delete_app_in_monitortable(l, appID, obj) != TRUE ) {
		fprintf(stderr, "on_delete_app_in_monitortable() error.\n");
		return(0);
	}

	fprintf(stdout, "on_delete_app_in_monitortable() OK.[%s]\n", appID);

	rc = close_monitor_obj(l, &obj);
	CHECK(rc==TRUE);

	fq_close_file_logger(&l);

	printf("OK.\n");
	return(rc);
}
