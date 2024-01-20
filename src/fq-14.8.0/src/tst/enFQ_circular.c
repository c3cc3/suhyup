/*
 * $(HOME)/src/utl/enFQ.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include "fqueue.h"
#include "fq_common.h"
#include "fq_sec_counter.h"

long job_time = 10000;
int main(int ac, char **av)  
{  
	int rc;
	fq_logger_t *l=NULL;
	fqueue_obj_t *obj=NULL;
	char *path=NULL;
    char *qname=NULL;
    char logname[256];
	char *option = NULL;
	unsigned char	*ptr_buf=NULL;
	int	 databuflen;
	long l_seq=0L;
	long run_time=0L;
	char *p= NULL;
	int	datasize;
	int enQ_num=0;
	int loop_flag = 0;
    int loop_count = 0;
	char sz_seq[9];
	long l_sequence;
	int log_level;
	int	data_len;
	char temp_buf[128];
	char *tempname=NULL;

	if( ac != 6 ) {
		printf("Usage: $ %s [path] [qname] [data_len] [count or loop] [log_level] \n", av[0]);
		printf("\t\t -TRACE   : 0\n");
		printf("\t\t -DEBUG   : 1\n");
		printf("\t\t -INFO    : 2\n");
		printf("\t\t -WARNING : 3\n");
		printf("\t\t -ERROR   : 4\n");
		printf("\t\t -EMERG   : 5\n");
		return(0);
	}
	
	path = av[1];
	qname= av[2];
	data_len = atoi(av[3]);
	option = av[4];

	if( strcmp(option, "loop") == 0 ) {
        loop_flag = 1;
    }
    else {
        loop_flag = 0;
        enQ_num = atoi(av[4]);
		printf("enQ_num=[%d].\n", enQ_num);
        if( enQ_num <= 0 ) {
            printf("illegal number(%d) of enQ.\n", enQ_num);
            return(0);
        }
    }

	log_level = atoi(av[5]);

	if( log_level < 0 || log_level > 5 ) {
        printf("illegal log_level(0~5).\n");
        return(0);
    }

	tempname = tempnam("/tmp", "enFQ_");
	CHECK(tempname);
 
    sprintf(logname, "%s.log", tempname);

	printf("Current Program settings:\n");
	printf("\t- send message size: [%d]\n", data_len);
	printf("\t- qpath            : [%s]\n", path);
	printf("\t- qname            : [%s]\n", qname);
	printf("\t- log file         : [%s]\n", logname);
	printf("\t- log level        : [%s]\n", get_log_level_str(log_level, temp_buf));
	printf("\t- number of enQ    : [%s]\n", option);

	printf("\n Are you sure? Press anykey for continue or Press Ctrl+'C' to stop\n");
	getc(stdin);

	rc = fq_open_file_logger(&l, logname, log_level);
	CHECK( rc > 0 );

	rc =  OpenFileQueue(l, NULL, NULL, NULL, NULL, path, qname, &obj);
	CHECK(rc==TRUE);

	/* normal data : data size is less than msglen-1 */
	/* databuflen = obj->h_obj->h->msglen - 4 + 1; */
	/* big data : data size is greater than msglen-1 */

	loop_count = 0;

loop:

	databuflen = data_len+1;
	ptr_buf = calloc(databuflen, sizeof( unsigned char));
	memset(ptr_buf, 'D', data_len);
	l_sequence = fq_get_seq();
	sprintf(sz_seq, "%08ld", l_sequence);
	memcpy(ptr_buf, sz_seq, 8);
	
	rc =  obj->on_en(l, obj, EN_CIRCULAR_MODE, ptr_buf, databuflen, data_len, &l_seq, &run_time );
	if( rc < 0 ) { 
		if( rc == MANAGER_STOP_RETURN_VALUE ) {
			printf("Manager asked to stop a processing.\n");
		}
		printf("error.\n");
		goto stop;
	}
	else {
		if( FQ_IS_DEBUG_LEVEL(l) ) {
			printf("data=[%s] data_len :[%d], rc=[%d]. seq=[%ld]\n", ptr_buf, data_len, rc, l_seq );
		}
		else {
			printf("seq=[%ld]\r", l_seq );
			fflush(stdout);
		}
		usleep(1000);
	}
	SAFE_FREE(ptr_buf);
	
	if(job_time) {
		usleep(job_time);
	}

	if( loop_flag ) {
        goto loop;
    }

    if( loop_count < enQ_num ) {
        loop_count++;
		usleep(10000);
        goto loop;
    }

stop:
	SAFE_FREE(ptr_buf);

	if( obj )  {
		rc = CloseFileQueue(l, &obj);
		CHECK(rc==TRUE);
	}

	if( l ) 
		fq_close_file_logger(&l);

	printf("Good bye !\n");
	return(rc);
}
