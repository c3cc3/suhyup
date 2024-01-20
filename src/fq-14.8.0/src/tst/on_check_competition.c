/*
 * on_view_tst.c
 * on_view() function sess a data to be read next in queue.
 * sample
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
	char	temp_buf[256];
	int log_level;
	int rc;

	if(ac != 4) {
		printf("Usage: $ %s [path] [qname] [log level(1-5)] <enter>\n", av[0]);
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

	sprintf(logname, "%s", "/tmp/on_view_tst.log");

    printf("Current Program settings:\n");
    printf("\t- path             : [%s]\n", path);
    printf("\t- qname            : [%s]\n", qname);
    printf("\t- log file         : [%s]\n", logname);
    printf("\t- log level        : [%s]\n", get_log_level_str(log_level, temp_buf));

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

	while(1) {
		time_t 	en_competition_msec;
		time_t 	de_competition_msec;

		try_count++;
		en_competition_msec = obj->on_check_competition(l, obj, FQ_EN_ACTION);
		de_competition_msec = obj->on_check_competition(l, obj, FQ_DE_ACTION);
		fprintf(stdout, "competion micro sec = enQ:[%ld] deQ:[%ld] (%d-th try).\r", 
			en_competition_msec, de_competition_msec, try_count);
		fflush(stdout);
		usleep(1000000);
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
