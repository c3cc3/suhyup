/*
 * deFQ_bundle.c
 * sample
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include "fqueue.h"
#include "fq_common.h"
#include "fq_monitor.h"

#define MAX_DATA_LEN 65000
#define QPATH "/home/pi/data"
#define QNAME "TST"
#define LOG_FILE_NAME "deFQ_bundle.log"

#define ARRAY_BUFFER_LEN 100

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
    printf("\t- Max Data Len     : [%d]\n", MAX_DATA_LEN);
    printf("\t- array buffer Len : [%d]\n", ARRAY_BUFFER_LEN);
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

	printf("File Queue msglen is [%ld].\n", obj->h_obj->h->msglen);

	while(1) {
		fqdata_t dst[ARRAY_BUFFER_LEN];
		long run_time=0L;
		int data_count = 0;

		data_count = obj->on_de_bundle_struct(l, obj, ARRAY_BUFFER_LEN, MAX_DATA_LEN, dst, &run_time);

		if( data_count < 0 ) {
			printf("error..\n");
			break;
		} else if( data_count == 0) {
			printf("empty...\n");
			sleep(1);
			continue;
		} else {
			int i;

			for( i=0; i<data_count; i++) {
				printf("[%d]-th: len=[%d] seq=[%ld] msg=[%-80.80s]\n", i, dst[i].len, dst[i].seq, dst[i].data);
				deFQ_packet_dump(l, dst[i].data, dst[i].len, dst[i].seq, run_time);
				SAFE_FREE(dst[i].data);
			}
		}
	}

	rc = close_monitor_obj(l, &mon_obj);
	CHECK(rc==TRUE);

	rc = CloseFileQueue(l, &obj);
	CHECK(rc==TRUE);

	fq_close_file_logger(&l);

	return(rc);
}
