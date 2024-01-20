/*
 * /tst/bundle_move.c
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
	fqueue_obj_t *de_obj=NULL;
	fqueue_obj_t *en_obj=NULL;
	char *progname=NULL;
	char *de_path=QPATH;
    char *de_qname;
	char *en_path=QPATH;
    char *en_qname;
    char *logname = LOG_FILE_NAME;
	int	 buffer_size;
	char *p=NULL;
	monitor_obj_t   *mon_obj=NULL;

	progname = av[0];

	if( ac != 5 ) {
		printf("usage: $ %s [de_path] [de_qname] [en_path] [en_qname] <enter>\n", av[0]);
		exit(0);
	}
	else {
		de_path = av[1];
		de_qname = av[2];
		en_path = av[3];
		en_qname = av[4];
	}

    printf("Current Program settings:\n");
    printf("\t- Max Data Len     : [%d]\n", MAX_DATA_LEN);
    printf("\t- array buffer Len : [%d]\n", ARRAY_BUFFER_LEN);
    printf("\t- de_path             : [%s]\n", de_path);
    printf("\t- en_path             : [%s]\n", en_path);
    printf("\t- de_qname            : [%s]\n", de_qname);
    printf("\t- en_qname            : [%s]\n", en_qname);
    printf("\t- log file         : [%s]\n", LOG_FILE_NAME);

	printf("\n Are you sure? Press anykey for continue or Press Ctrl+'C' to stop\n");
    getc(stdin);

	rc = fq_open_file_logger(&l, logname, FQ_LOG_ERROR_LEVEL);
	CHECK( rc > 0 );

	/* deQ file open */
	rc =  OpenFileQueue(l, NULL,NULL, NULL, NULL, de_path, de_qname, &de_obj);
	CHECK(rc==TRUE);

	/* enQ file open */
	rc =  OpenFileQueue(l, NULL, NULL, NULL, NULL, en_path, en_qname, &en_obj);
	CHECK(rc==TRUE);

	rc =  open_monitor_obj(l,  &mon_obj);
	CHECK(rc==TRUE);


	/* buffer_size = obj->h_obj->h->msglen-4+1; */ /* queue message size */

	printf("File Queue msglen is [%ld].\n", de_obj->h_obj->h->msglen);

	while(1) {
		fqdata_t dst[ARRAY_BUFFER_LEN];
		long run_time=0L;
		int data_count = 0;
		int i;

		data_count = de_obj->on_de_bundle_struct(l, de_obj, ARRAY_BUFFER_LEN, MAX_DATA_LEN, dst, &run_time);

		if( data_count < 0 ) {
			printf("error..\n");
			break;
		} else if( data_count == 0) {
			printf("empty...\n");
			sleep(1);
			continue;
		} else {
			int rc;

			rc =  en_obj->on_en_bundle_struct(l, en_obj, data_count, dst, &run_time );
			if( rc < 0 ) { 
				printf("error.\n");
			}
			else {
				if ( data_count != rc ) {
					printf("bundle_struct() moving NOK. number of enqueue/dequeue=[%d][%d]\n", rc, data_count);
				}
				else {
					printf("bundle_struct() moving OK. number of enqueue/dequeue=[%d][%d]\n", rc, data_count);
				}
			}
		}

		for(i=0; i<data_count; i++) {
			if( dst[i].data ) free(dst[i].data);

		}
	}

	rc = close_monitor_obj(l, &mon_obj);
	CHECK(rc==TRUE);

	rc = CloseFileQueue(l, &de_obj);
	CHECK(rc==TRUE);
	rc = CloseFileQueue(l, &en_obj);
	CHECK(rc==TRUE);

	fq_close_file_logger(&l);

	return(rc);
}
