/*
 * deFQ.c
 * sample
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include "fqueue.h"
#include "fq_common.h"
#include "fq_sec_counter.h"

int job_time = 0; // 0.1 seconds
long ol_tmp = 0L;
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

	if(ac != 6) {
		printf("Usage: $ %s [path] [qname] [XA_flag] [count or loop] [log level(1-5)] <enter>\n", av[0]);
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
	XA_flag = atoi(av[3]);
	option = av[4];
	if( strcmp(option, "loop") == 0 ) {
		loop_flag = 1;
	}
	else { 
		loop_flag = 0;
		deQ_num = atoi(av[4]);
		if( deQ_num <= 0 ) {
			printf("illegal number of deQ.\n");
			return(0);
		}
	}
	log_level = atoi(av[5]);
	if( log_level < 0 || log_level > 5 ) {
		printf("illegal log_level(0~5).\n");
		return(0);
	}

	sprintf(logname, "%s.log", tempnam("/tmp", "deFQ_"));

    printf("Current Program settings:\n");
    printf("\t- path             : [%s]\n", path);
    printf("\t- qname            : [%s]\n", qname);
    printf("\t- XA_flag          : [%d]\n", XA_flag);
    printf("\t- log file         : [%s]\n", logname);
    printf("\t- log level        : [%s]\n", get_log_level_str(log_level, temp_buf));
    printf("\t- number of deQ  	 : [%s]\n", option);

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
	// buffer_size = obj->h_obj->h->msglen + 1;
	buffer_size = 200000;
	ptr_buf = calloc(buffer_size, sizeof(unsigned char));

	printf("File Queue msglen is [%ld].\n", obj->h_obj->h->msglen);

	loop_count = 0;

loop:
	if( XA_flag ) {
		unlink_filename[0] = 0x00;
		rc = obj->on_de_XA(l, obj, ptr_buf, buffer_size, &l_seq, &run_time, unlink_filename);
	}
	else { 
		rc = obj->on_de(l, obj, ptr_buf, buffer_size, &l_seq, &run_time);
	}

	if( rc == DQ_EMPTY_RETURN_VALUE ) {
		printf("empty..\n");
		SAFE_FREE(buf);
		sleep(1);
		goto loop;
	}
	else if( rc < 0 ) {
		if( rc == MANAGER_STOP_RETURN_VALUE ) {
			printf("Manager asked to stop a processing.\n");
			goto stop;
		}
		printf("error.. rc=[%d]\n", rc);
		goto stop;
	}
	else {
		int job_ret = 0;

		if( FQ_IS_DEBUG_LEVEL(l) ) {
			job_ret = printf("data:[%s], rc=[%d] seq=[%ld]-th\n", ptr_buf, rc, l_seq );
		}
		else {
			char tmp[10];
			long l_tmp;

			memcpy(tmp,ptr_buf, 8);
			tmp[8] = 0x00;
			printf("data:[%s], rc(len)=[%d] seq=[%ld]-th. \r", tmp, rc, l_seq );
			fflush(stdout);

			fq_log(l, FQ_LOG_EMERG, "[%s] [%ld]", tmp, l_seq);
			l_tmp = atol(tmp);
			if( ol_tmp > 0 ) {
				if( ol_tmp >= l_tmp) {
					printf("fatal.\n");
					goto stop;
				} else if( !((ol_tmp+1) == l_tmp)){
					printf("fatal( ol_tmp=[%ld], l_tmp=[%ld].\n", ol_tmp, l_tmp);
					goto stop;
				} else {
					ol_tmp = l_tmp;
				}
			}
		}
		memset(ptr_buf, 0x00, buffer_size);

		if( job_time > 0 ) {
			usleep(job_time);
		}
		if( XA_flag ) {
			if( job_ret < 0 ) {
				obj->on_cancel_XA(l, obj, l_seq, &run_time);	
			}
			else {
				obj->on_commit_XA(l, obj, l_seq, &run_time, unlink_filename);
			}
		}
		usleep(10000);
	}

	if( loop_flag ) {
		goto loop;
	}

	loop_count++;
	if( loop_count < deQ_num ) {
		goto loop;
	}

stop:
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
