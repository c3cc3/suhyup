/*
 * deFQ.c
 * sample
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include "fqueue.h"
#include "fq_common.h"
#include "fq_monitor.h"

#define BUFFER_SIZE 65000
#define QPATH "/home/pi/data"
#define QNAME "TST"
#define LOG_FILE_NAME "deFQ.log"

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
	// monitor_obj_t   *mon_obj=NULL;

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

	rc = fq_open_file_logger(&l, logname, FQ_LOG_TRACE_LEVEL);
	CHECK( rc > 0 );

	rc =  OpenFileQueue(l, NULL,NULL, NULL, NULL, path, qname, &obj);
	CHECK(rc==TRUE);

	// rc =  open_monitor_obj(l,  &mon_obj);
	// CHECK(rc==TRUE);


	/* buffer_size = obj->h_obj->h->msglen-4+1; */ /* queue message size */
	buffer_size = BUFFER_SIZE;

	printf("File Queue msglen is [%ld].\n", obj->h_obj->h->msglen);

	while(1) {
		long l_seq=0L;
		unsigned char *buf=NULL;
		long run_time=0L;

		buf = malloc(buffer_size);
		memset(buf, 0x00, sizeof(buffer_size));

		rc = obj->on_de(l, obj, buf, buffer_size, &l_seq, &run_time);
		// mon_obj->on_send_action_status(l, progname, obj->path, obj->qname, obj->h_obj->h->desc, DE_ACTION, mon_obj);

		if( rc == DQ_EMPTY_RETURN_VALUE ) {
			printf("empty..\n");
			SAFE_FREE(buf);
			usleep(100000);
		}
		else if( rc < 0 ) {
			if( rc == MANAGER_STOP_RETURN_VALUE ) {
				printf("Manager asked to stop a processing.\n");
				break;
			}
			printf("error..\n");
			break;
		}
		else {
			printf("rc=[%d] seq=[%ld]-th\n", rc, l_seq );
			SAFE_FREE(buf);
		}
	}

	// rc = close_monitor_obj(l, &mon_obj);
	CHECK(rc==TRUE);

	rc = CloseFileQueue(l, &obj);
	CHECK(rc==TRUE);

	fq_close_file_logger(&l);

	return(rc);
}
