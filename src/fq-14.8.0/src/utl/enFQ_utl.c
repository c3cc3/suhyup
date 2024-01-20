/*
 * $(HOME)/src/utl/enFQ.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include "fqueue.h"
#include "fq_common.h"

#define SEND_MSG_SIZE 4000
#define LOG_FILE_NAME "./enFQ.log"

#ifndef LOOPING_TEST
#define LOOPING_TEST
#define USLEEP_TIME 0 
#endif

int main(int ac, char **av)  
{  
	int rc;
	fq_logger_t *l=NULL;
	fqueue_obj_t *obj=NULL;
	char *path=NULL;
    char *qname=NULL;
	char *mode = NULL;
    char *logname = LOG_FILE_NAME;
	unsigned char	*buf=NULL;
	int	 databuflen;
	long	l_seq=0L;
	long run_time=0L;
	char *p= NULL;
	int	datasize;

	if( ac != 4 ) {
		fprintf( stderr, "Usage: $ %s [qpath] [qname] [mode] <enter> \n", av[0]);
		fprintf( stderr, "ex)  : $ %s /home/fq/data TST EN_NORMAL_MODE <enter> \n", av[0]);
		fprintf( stderr, "ex)  : $ %s /home/fq/data TST EN_CIRCULAR_MODE <enter> \n", av[0]);
		exit(0);
	}

	/*
    p = getenv("LD_LIBRARY_PATH");
    printf("Current LD_LIBRARY_PATH=[%s]\n\n", p);
    p = getenv("FQ_DATA_HOME");
    printf("Current FQ_DATA_HOME=[%s]\n\n", p);
	if( p ) {
		path = strdup(p);
	}
	*/
	path = av[1];
	qname = av[2];
	mode = av[3];

	printf("Current Program settings:\n");
	printf("\t- send message size: [%d]\n", SEND_MSG_SIZE);
	printf("\t- qpath            : [%s]\n", path);
	printf("\t- qname            : [%s]\n", qname);
	printf("\t- mode             : [%s]\n", mode);
	printf("\t- log file         : [%s]\n", LOG_FILE_NAME);

	printf("\n Are you sure? Press anykey for continue or Press Ctrl+'C' to stop\n");
	getc(stdin);

	rc = fq_open_file_logger(&l, logname, FQ_LOG_ERROR_LEVEL);
	// rc = fq_open_file_logger(&l, logname, FQ_LOG_DEBUG_LEVEL);
	// rc = fq_open_file_logger(&l, logname, FQ_LOG_TRACE_LEVEL);
	CHECK( rc > 0 );

	rc =  OpenFileQueue(l, NULL, NULL, NULL, NULL, path, qname, &obj);
	CHECK(rc==TRUE);

	/* normal data : data size is less than msglen-1 */
	/* databuflen = obj->h_obj->h->msglen - 4 + 1; */
	/* big data : data size is greater than msglen-1 */

looping:

	databuflen = SEND_MSG_SIZE+1;
	buf = malloc(databuflen);
	memset(buf, 0x00, databuflen);
	memset(buf, 'D', databuflen-1);
	
	datasize = strlen((char *)buf);

	if( strcmp(mode, "EN_NORMAL_MODE") == 0 ) { 
		rc =  obj->on_en(l, obj, EN_NORMAL_MODE, buf, databuflen, datasize, &l_seq, &run_time );
	}
	else if( strcmp(mode, "EN_CIRCULAR_MODE") == 0 ) { 
		rc =  obj->on_en(l, obj, EN_CIRCULAR_MODE, buf, databuflen, datasize, &l_seq, &run_time );
	}
	else {
		fprintf(stderr, "Unsupported mode. Put EN_NORMAL_MODE or EN_CIRCULAR_MODE.");
		goto stop;
	}
	if( rc == EQ_FULL_RETURN_VALUE )  {
		printf("full.\n");
	}
	else if( rc < 0 ) { 
		if( rc == MANAGER_STOP_RETURN_VALUE ) {
			printf("Manager asked to stop a processing.\n");
		}
		printf("error.\n");
	}
	else {
		printf("enFQ OK. rc=[%d]\n", rc);
		// getc(stdin);
	}

	if(buf) free(buf);

#ifdef LOOPING_TEST
	usleep(USLEEP_TIME);
	goto looping;
#endif

stop:
	rc = CloseFileQueue(l, &obj);
	CHECK(rc==TRUE);

	fq_close_file_logger(&l);

	printf("Good bye !\n");
	return(rc);
}
