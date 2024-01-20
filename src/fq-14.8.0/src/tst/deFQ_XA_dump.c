/*
 * deFQ_XA_dump.c
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
#define LOG_FILE_NAME "./deFQ_XA_dump.log"
#define DUMP_COUNT 1

long last_access_time = 0L;

int main(int ac, char **av)  
{  
	int rc;
	fq_logger_t *l=NULL;
	fqueue_obj_t *obj=NULL;
	char *path=QPATH;
    char *qname=QNAME;
    char *logname = LOG_FILE_NAME;
	int	 buffer_size;
	char *p=NULL;
	monitor_obj_t   *mon_obj=NULL;
	int dump_count = 0;
	int i;

	p = getenv("LD_LIBRARY_PATH");
    printf("Current LD_LIBRARY_PATH=[%s]\n\n", p);

	p = getenv("FQ_DATA_HOME");
    printf("Current FQ_DATA_HOME=[%s]\n\n", p);

	if( p ) {
		path = strdup(p);
	}

	printf("Current Program settings:\n");
    printf("\t- recv buffer  size: [%d]\n", BUFFER_SIZE);
    printf("\t- qpath            : [%s]\n", path);
    printf("\t- qname            : [%s]\n", QNAME);
    printf("\t- log file         : [%s]\n", LOG_FILE_NAME);
    printf("\t- number of dump   : [%d]\n", DUMP_COUNT);

	printf("\n Are you sure? Press anykey for continue or Press Ctrl+'C' to stop\n");
    getc(stdin);

	rc = fq_open_file_logger(&l, logname, FQ_LOG_TRACE_LEVEL);
	CHECK( rc > 0 );

	rc =  OpenFileQueue(l, NULL, NULL, NULL, NULL, path, qname, &obj);
	CHECK(rc==TRUE);

	rc =  open_monitor_obj(l,  &mon_obj);
	CHECK(rc==TRUE);


	/* buffer_size = obj->h_obj->h->msglen-4+1; */ /* queue message size */
	buffer_size = BUFFER_SIZE;

	printf("File Queue msglen is [%ld].\n", obj->h_obj->h->msglen);

	
	dump_count = DUMP_COUNT;
	for(i=0; i<dump_count; i++) {
		long l_seq=0L;
		unsigned char *buf=NULL;
		long run_time=0L;
		char    unlink_filename[256];

		buf = malloc(buffer_size);
		memset(buf, 0x00, sizeof(buffer_size));
		memset(unlink_filename, 0x00, sizeof(unlink_filename));

		rc = obj->on_de_XA(l, obj, buf, buffer_size, &l_seq, &run_time, unlink_filename);

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
			rc = queue_data_file_dump( l, obj->qname, l_seq, buf, rc );
			if( rc < 0 ) { /* fail */
				obj->on_cancel_XA(l, obj, l_seq, &run_time);
				printf("queue_data_file_dump() seq=[%ld] failed.\n", l_seq);
			}
			else { /* success */
				obj->on_commit_XA(l, obj, l_seq, &run_time, unlink_filename);
				printf("queue_data_file_dump(/tmp/%s_%ld.dump) dump success.\n", obj->qname, l_seq);
			}
			SAFE_FREE(buf);
		}
	}

	rc = close_monitor_obj(l, &mon_obj);
	CHECK(rc==TRUE);

	rc = CloseFileQueue(l, &obj);
	CHECK(rc==TRUE);

	fq_close_file_logger(&l);

	return(rc);
}
