/*
 * on_view_tst.c
 * on_view() function sess a data to be read next in queue.
 * sample
	descrioption: 
The program notifies the waiting queue of waring if there is no change for the specified number of seconds..
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include "fqueue.h"
#include "fq_common.h"
#include "fq_sec_counter.h"

int try_count=0;

int main(int ac, char **av)  
{  
	fq_logger_t *l=NULL;
	fqueue_obj_t *obj=NULL;
	char *path=NULL;
    char *qname=NULL;
    char *option=NULL;
    char logname[256];
	char *waiting_seconds = NULL;
	int i_waiting_seconds = 0;
	char	temp_buf[256];
	int log_level;
	int rc;

	if(ac != 5) {
		printf("Usage: $ %s [path] [qname] [log level(1-5)] [waiting sec] <enter>\n", av[0]);
		printf("\t\t -TRACE   : 0\n");
		printf("\t\t -DEBUG   : 1\n");
		printf("\t\t -INFO    : 2\n");
		printf("\t\t -WARNING : 3\n");
		printf("\t\t -ERROR   : 4\n");
		printf("\t\t -EMERG   : 5\n");
		return(0);
	}

	path = av[1];
	qname = av[2];

	log_level = atoi(av[3]);
	if( log_level < 0 || log_level > 5 ) {
		printf("illegal log_level(0~5).\n");
		return(0);
	}
	waiting_seconds = av[4];

	sprintf(logname, "%s", "/tmp/on_do_not_flow.log");

    printf("Current Program settings:\n");
    printf("\t- path             : [%s]\n", path);
    printf("\t- qname            : [%s]\n", qname);
    printf("\t- log file         : [%s]\n", logname);
    printf("\t- log level        : [%s]\n", get_log_level_str(log_level, temp_buf));
	printf("\t- waiting seconds  : [%s]\n", waiting_seconds);

	printf("\n Are you sure? Press anykey for continue or Press Ctrl+'C' to stop\n");
    getc(stdin);

	/*
	#define FQ_LOG_TRACE_LEVEL   0
	#define FQ_LOG_DEBUG_LEVEL   1
	#define FQ_LOG_INFO_LEVEL    2
	#define FQ_LOG_WARNING_LEVEL 3
	#define FQ_LOG_ERROR_LEVEL   4
	#define FQ_LOG_EMERG_LEVEL   5
	*/
	rc = fq_open_file_logger(&l, logname, log_level);
	CHECK( rc > 0 );

	rc =  OpenFileQueue(l, NULL, NULL, NULL, NULL, path, qname, &obj);
	CHECK(rc==TRUE);

	i_waiting_seconds = atoi(waiting_seconds);
	

	while(1) {
		bool_t	is_do_not_flow;
		time_t	waiting_msec = (1000000 * i_waiting_seconds); // 10 seconds
		try_count++;
		is_do_not_flow = obj->on_do_not_flow(l, obj, waiting_msec);
		if( is_do_not_flow ) {
			fprintf(stdout, "Warning!!! ---> There is no deQ processing.(%d-th try).\r", try_count);
			fflush(stdout);
			usleep(1000000);
			continue;
		} else {
			fprintf(stdout, "Normal( Status is changing. )(%d-th try).\r", try_count);
			fflush(stdout);
			usleep(1000000);
			continue;
		}
		sleep(1);
	}

	if(obj) {
		rc = CloseFileQueue(l, &obj);
		CHECK(rc==TRUE);
	}

	if(l) {
		fq_close_file_logger(&l);
	}

	printf("Good bye !\n");
	return(rc);
}
