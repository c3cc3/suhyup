/*
 * $(HOME)/src/utl/enFQ_bundle.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include "fqueue.h"
#include "fq_common.h"

#define SEND_MSG_SIZE 4000
#define LOG_FILE_NAME "./enFQ_bundle.log"
#define FQ_DATA_LEN 500 /* size of array */

int main(int ac, char **av)  
{  
	int rc;
	fq_logger_t *l=NULL;
	fqueue_obj_t *obj=NULL;
	char *path=NULL;
    char *qname=NULL;
    char *logname = LOG_FILE_NAME;
	long run_time=0L;
	fqdata_t src[FQ_DATA_LEN];
	int i;
	int c;

	if( ac != 2 ) {
		fprintf( stderr, "Usage: $ %s [qpath] [qname] <enter> \n", av[0]);
		fprintf( stderr, "ex)  : $ %s /home/fq/data TST <enter> \n", av[0]);
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

	printf("Current Program settings:\n");
	printf("\t- send message size: [%d]\n", SEND_MSG_SIZE);
	printf("\t- qpath            : [%s]\n", path);
	printf("\t- qname            : [%s]\n", qname);
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

	/* make data */
	for(i=0; i<FQ_DATA_LEN; i++) {
		src[i].data = calloc(SEND_MSG_SIZE+1, sizeof(unsigned char));
		memset(src[i].data, 'D', SEND_MSG_SIZE);
		src[i].len = SEND_MSG_SIZE;
	}

	rc =  obj->on_en_bundle_struct(l, obj, FQ_DATA_LEN, src, &run_time );
	if( rc < 0 ) { 
		printf("error.\n");
	}
	else {
		printf("on_en_bundle_struct() OK. number of enqueue =[%d]\n", rc);
	}

	/* free */
	for(i=0; i<FQ_DATA_LEN; i++) {
		SAFE_FREE(src[i].data);
	}

	c = yesno();
	if( c == _KEY_CONTINUE || c == _KEY_YES ) goto looping;

	rc = CloseFileQueue(l, &obj);
	CHECK(rc==TRUE);

	fq_close_file_logger(&l);

	printf("Good bye !\n");
	return(rc);
}
