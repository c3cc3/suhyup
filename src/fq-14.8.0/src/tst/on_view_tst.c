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
	int	 buffer_size;
	char	*ptr_buf=NULL;
	char *p=NULL;
	second_counter_obj_t *cnt_obj=NULL;
	int deQ_num=0;
	int log_level;
	long l_seq=0L;
	unsigned char *buf=NULL;
	long run_time=0L;
	int rc;
	int loop_flag = 0;
	int XA_flag = 0;
	int loop_count = 0;
	char temp_buf[128];
	char unlink_filename[256];

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

	/* buffer_size = obj->h_obj->h->msglen-4+1; */ /* queue message size */
	buffer_size = obj->h_obj->h->msglen + 1;
	ptr_buf = calloc(buffer_size, sizeof(unsigned char));

	printf("File Queue msglen is [%ld].\n\n", obj->h_obj->h->msglen);

	while(1) {
		bool big_flag ;

		try_count++;
		rc = obj->on_view(l, obj, ptr_buf, buffer_size, &l_seq, &run_time, &big_flag);
		if( rc == DQ_EMPTY_RETURN_VALUE ) {
			fprintf(stdout, "There is no reading data(%d-th try).\r", try_count);
			fflush(stdout);
			SAFE_FREE(buf);
			usleep(1000000);
			continue;
		}
		else if( rc < 0 ) {
			if( rc == MANAGER_STOP_RETURN_VALUE ) {
				printf("Manager asked to stop a processing.\n");
				break;
			}
			printf("error.. rc=[%d]\n", rc);
			break;
		}
		// fprintf(stdout, "[%d-th try] data:[%s], rc=[%d] seq=[%ld]-th\r", try_count, ptr_buf, rc, l_seq );
		fprintf(stdout, "[%d-th try] data:[%s], rc=[%d] seq=[%ld]-th\n", try_count, ptr_buf, rc, l_seq );
		fflush(stdout);
		packet_dump(ptr_buf, rc, 16, "Queue data");
		memset(ptr_buf, 0x00, buffer_size);
		getc(stdin);
		usleep(1000);
	}

	SAFE_FREE(ptr_buf);

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
