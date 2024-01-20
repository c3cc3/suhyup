/*
 * deFQ_XA.c
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
#define DEQNAME "TST1"
#define ENQNAME "TST2"
#define LOG_FILE_NAME "./deQ_XA_enQ.log"

static int enQ(fq_logger_t *l, fqueue_obj_t *obj, unsigned char *buf, int len);

int main(int ac, char **av)  
{  
	int rc;
	fq_logger_t *l=NULL;
	fqueue_obj_t *en_obj=NULL;
	fqueue_obj_t *de_obj=NULL;
	char *path=QPATH;
    char *deqname=DEQNAME;
    char *enqname=ENQNAME;
    char *logname = LOG_FILE_NAME;
	int	 buffer_size;
	char *p=NULL;
	monitor_obj_t   *mon_obj=NULL;

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
    printf("\t- deQ_name         : [%s]\n", DEQNAME);
    printf("\t- enQ_name         : [%s]\n", ENQNAME);
    printf("\t- log file         : [%s]\n", LOG_FILE_NAME);

	printf("\n Are you sure? Press anykey for continue or Press Ctrl+'C' to stop\n");
    getc(stdin);

	rc = fq_open_file_logger(&l, logname, FQ_LOG_TRACE_LEVEL);
	CHECK( rc > 0 );

	rc =  OpenFileQueue(l, NULL, NULL, NULL, NULL, path, deqname, &de_obj);
	CHECK(rc==TRUE);
	rc =  OpenFileQueue(l, NULL, NULL, NULL, NULL, path, enqname, &en_obj);
	CHECK(rc==TRUE);

	rc =  open_monitor_obj(l,  &mon_obj);
	CHECK(rc==TRUE);


	/* buffer_size = de_obj->h_obj->h->msglen-4+1; */ /* queue message size */
	buffer_size = BUFFER_SIZE;

	printf("File Queue msglen is [%ld].\n", de_obj->h_obj->h->msglen);

	while(1) {
		long l_seq=0L;
		unsigned char *buf=NULL;
		long run_time=0L;
		char    unlink_filename[256];

		buf = malloc(buffer_size);
		memset(buf, 0x00, sizeof(buffer_size));
		memset(unlink_filename, 0x00, sizeof(unlink_filename));

		rc = de_obj->on_de_XA(l, de_obj, buf, buffer_size, &l_seq, &run_time, unlink_filename);

		// In here, coredump: find a bug 
		// mon_obj->on_send_action_status(l, av[0], de_obj->path, de_obj->qname, de_obj->h_obj->h->desc, DE_ACTION, mon_obj);

		if( rc == DQ_EMPTY_RETURN_VALUE ) {
			printf("empty..\n");
			SAFE_FREE(buf);
			usleep(100000);
			continue;
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
			int en_rc;

			printf("rc = [%d] seq=[%ld]-th\n", rc, l_seq );

			en_rc = enQ( l, en_obj, buf, rc);
			if( en_rc > 0 ) {
				int commit_rc;

				/* success */
				commit_rc = de_obj->on_commit_XA(l, de_obj, l_seq, &run_time, unlink_filename);
				if( commit_rc < 0 ) {
					printf("XA commit error.\n");
					SAFE_FREE(buf);
					break;
				}
			}
			else {
				/* failure */
				rc = de_obj->on_cancel_XA(l, de_obj, l_seq, &run_time);
				break;
			}
			SAFE_FREE(buf);
		}
	}

	rc = close_monitor_obj(l, &mon_obj);
	CHECK(rc==TRUE);

	rc = CloseFileQueue(l, &en_obj);
	CHECK(rc==TRUE);

	rc = CloseFileQueue(l, &de_obj);
	CHECK(rc==TRUE);

	fq_close_file_logger(&l);

	return(rc);
}

static int enQ(fq_logger_t *l, fqueue_obj_t *obj, unsigned char *buf, int len)
{
	int rc;
	long l_seq=0L, run_time=0L; 

	rc =  obj->on_en(l, obj, EN_NORMAL_MODE, buf, len+1, len, &l_seq, &run_time );

	if( rc == EQ_FULL_RETURN_VALUE )  {
		printf("full.\n");
	}
	else if( rc < 0 ) { 
		printf("enQ error.\n");
	}
	else {
		printf("enFQ OK. rc=[%d]\n", rc);
	}
	return(rc);
}
