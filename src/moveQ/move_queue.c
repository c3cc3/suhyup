/*
 * move_queue.c
 * sample
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include "fqueue.h"
#include "fq_common.h"
#include "fq_sec_counter.h"

#define LOG_FILE_NAME "move_queue.log"

int main(int ac, char **av)  
{  
	int rc;
	int moved_count=0;
	fq_logger_t *l=NULL;
	char *from_path=NULL;
    char *from_qname=NULL;
	char *to_path=NULL;
    char *to_qname=NULL;
    char *logname = LOG_FILE_NAME;
	int move_req_count;

	if(ac != 6) {
		printf("Usage: $ %s [from_path] [from_qname] [to_path] [to_qname] [count]<enter>\n", av[0]);
		return(0);
	}

	from_path = av[1];
	from_qname = av[2];
	to_path = av[3];
	to_qname = av[4];
	move_req_count = atoi(av[5]);

	rc = fq_open_file_logger(&l, logname, FQ_LOG_TRACE_LEVEL);
	CHECK( rc > 0 );

	while(1) {
		// moved_count = MoveFileQueue_XA( l, from_path, from_qname, to_path, to_qname, move_req_count);
		moved_count = MoveFileQueue( l, from_path, from_qname, to_path, to_qname, move_req_count);
		if( moved_count < 0 ) {
			printf("MoveFileQueue() error. rc=[%d].\n", moved_count);
		}
		else {
			printf("moved count = [%d]\n", moved_count);
			if(moved_count == 0) {
				printf("Finished.\n");
				break;
			}
			sleep(1);
			continue;
		}
	}

	fq_close_file_logger(&l);

	return(0);
}
