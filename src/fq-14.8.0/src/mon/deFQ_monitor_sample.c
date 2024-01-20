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

int main(int ac, char **av)  
{  
	int rc;
	fq_logger_t *l=NULL;
	fqueue_obj_t *obj=NULL;
	char *path="/home/pi/data";
    char *qname="TST";
    char *logname = "deFQ.log";
	int	 buffer_size;
	char *p=NULL;
	monitor_obj_t   *mon_obj=NULL;

	p = getenv("LD_LIBRARY_PATH");
    printf("Current LD_LIBRARY_PATH=[%s]\n\n", p);

	rc = fq_open_file_logger(&l, logname, FQ_LOG_ERROR_LEVEL);
	CHECK( rc > 0 );

	rc =  OpenFileQueue(l, NULL, NULL,  NULL, NULL, path, qname, &obj);
	CHECK(rc==TRUE);

	rc =  open_monitor_obj(l,  &mon_obj);
	CHECK(rc==TRUE);


	/* buffer_size = obj->h_obj->h->msglen-4+1; */ /* queue message size */
	buffer_size = 65000;

	printf("File Queue msglen is [%zu].\n", obj->h_obj->h->msglen);

	while(1) {
		long l_seq=0L;
		unsigned char *buf=NULL;
		long run_time=0L;

		buf = malloc(buffer_size);
		memset(buf, 0x00, sizeof(buffer_size));

		rc = obj->on_de(l, obj, buf, buffer_size, &l_seq, &run_time);
		mon_obj->on_send_action_status(l, av[0], obj->path, obj->qname, obj->h_obj->h->desc, FQ_DE_ACTION, mon_obj);

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
			printf("[%s]\n", buf);
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
