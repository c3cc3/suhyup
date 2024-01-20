/*
 * lock_perfermance_TPS.c
 * This get TPS for locking of fqueue.
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include "fqueue.h"
#include "fq_common.h"
#include "fq_monitor.h"

#include "fq_timer.h"

#define BUFFER_SIZE 65000
#define QPATH "/home/pi/data"
#define QNAME "TST"
#define LOG_FILE_NAME "lock_performance.log"

double e;
int count = 0;
long last_access_time = 0L;

int 
on_diag(fq_logger_t *l, fqueue_obj_t *obj)
{

	FQ_TRACE_ENTER(l);

	/* For thread safe */
	/* ´õ ÀÌ»ó ½Å±Ô Ã»À» ¸øÇÏµµ·Ï ÇÑ´Ù.*/

	obj->h_obj->de_flock->on_flock(obj->h_obj->de_flock);

	obj->h_obj->h->last_en_time = time(0);
	obj->h_obj->h->last_de_time = time(0);

	obj->h_obj->de_flock->on_funlock(obj->h_obj->de_flock);

	FQ_TRACE_EXIT(l);
	return(TRUE);
}

int main(int ac, char **av)  
{  
	int rc;
	fq_logger_t *l=NULL;
	fqueue_obj_t *obj=NULL;
	char *progname=NULL;
	char *path=QPATH;
    char *qname;
    char *logname = LOG_FILE_NAME;
	int	 buffer_size;
	char *p=NULL;
	monitor_obj_t   *mon_obj=NULL;

	progname = av[0];

	if(ac == 2) {
		qname = av[1];
	}
	else {
		qname = QNAME;
	}

	p = getenv("LD_LIBRARY_PATH");
	printf("Current LD_LIBRARY_PATH=[%s]\n\n", p);
	p = NULL;

	p = getenv("FQ_DATA_HOME");
	printf("Current FQ_DATA_HOME=[%s]\n\n", p);

	if( p ) {
		path = strdup(p);
	}

    printf("Current Program settings:\n");
    printf("\t- recv buffer  size: [%d]\n", BUFFER_SIZE);
    printf("\t- path             : [%s]\n", path);
    printf("\t- qname            : [%s]\n", qname);
    printf("\t- log file         : [%s]\n", LOG_FILE_NAME);

	printf("\n Are you sure? Press anykey for continue or Press Ctrl+'C' to stop\n");
    getc(stdin);

	rc = fq_open_file_logger(&l, logname, FQ_LOG_ERROR_LEVEL);
	CHECK( rc > 0 );

	rc =  OpenFileQueue(l, NULL,NULL, NULL, NULL, path, qname, &obj);
	CHECK(rc==TRUE);

	rc =  open_monitor_obj(l,  &mon_obj);
	CHECK(rc==TRUE);

	/* buffer_size = obj->h_obj->h->msglen-4+1; */ /* queue message size */
	buffer_size = BUFFER_SIZE;

	printf("File Queue msglen is [%ld].\n", obj->h_obj->h->msglen);

	while(1) {
		if( (last_access_time+10) <  time(0)) {
			mon_obj->on_send_action_status(l, progname, obj->path, obj->qname, obj->h_obj->h->desc, FQ_DE_ACTION, mon_obj);
			last_access_time = time(0);
		}

		// rc = obj->on_diag(l, obj);
		rc = on_diag(l, obj);
		
        count++;
		e = end_timer();
		if( e > 1.0) {
			printf("locking TPS is [%d].\n", (unsigned)(count/e));
			start_timer();
			count = 0;
		}
	}

	rc = close_monitor_obj(l, &mon_obj);
	CHECK(rc==TRUE);

	rc = CloseFileQueue(l, &obj);
	CHECK(rc==TRUE);

	fq_close_file_logger(&l);

	return(rc);
}
