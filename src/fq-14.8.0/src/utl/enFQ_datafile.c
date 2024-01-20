/*
 * $(HOME)/src/utl/enFQ_datafile.c
 * Descriptions: This puts data of a file to FileQueue.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>

#include "fqueue.h"
#include "fq_common.h"

#define SEND_MSG_SIZE 4000
#define LOG_FILE_NAME "./enFQ_datafile.log"

int LoadFile(char *filename, char**ptr_dst, int *flen, char** ptr_errmsg);

int main(int ac, char **av)  
{  
	int rc;
	fq_logger_t *l=NULL;
	fqueue_obj_t *obj=NULL;
	char *path=NULL;
    char *qname=NULL;
	char *mode = NULL;
	char *datafile = NULL;
    char *logname = LOG_FILE_NAME;
	unsigned char	*buf=NULL;
	int	 databuflen;
	long	l_seq=0L;
	long run_time=0L;
	char *p= NULL;
	int	datasize;
	char *dst=NULL;
	int  flen;
	char *errmsg=NULL;

	if( ac != 5 ) {
		fprintf( stderr, "Usage: $ %s [qpath] [qname] [mode] [datafile]<enter> \n", av[0]);
		fprintf( stderr, "ex)  : $ %s /home/fq/data TST EN_NORMAL_MODE  /home/pi/data/test.dat <enter> \n", av[0]);
		fprintf( stderr, "ex)  : $ %s /home/fq/data TST EN_CIRCULAR_MODE  /home/pi/data/test.dat<enter> \n", av[0]);
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
	datafile = av[4];

	printf("Current Program settings:\n");
	printf("\t- send message size: [%d]\n", SEND_MSG_SIZE);
	printf("\t- qpath            : [%s]\n", path);
	printf("\t- qname            : [%s]\n", qname);
	printf("\t- mode             : [%s]\n", mode);
	printf("\t- log file         : [%s]\n", LOG_FILE_NAME);
	printf("\t- data file        : [%s]\n", datafile);

	printf("\n Are you sure? Press anykey for continue or Press Ctrl+'C' to stop\n");
	getc(stdin);

	rc = fq_open_file_logger(&l, logname, FQ_LOG_ERROR_LEVEL);
	// rc = fq_open_file_logger(&l, logname, FQ_LOG_DEBUG_LEVEL);
	// rc = fq_open_file_logger(&l, logname, FQ_LOG_TRACE_LEVEL);
	CHECK( rc > 0 );

	/* Loading a datafile to memory */
	rc =  LoadFile(datafile, &dst, &flen, &errmsg);
	if( rc != 0 ) {
		fq_log(l, FQ_LOG_ERROR, "LoadFile() error. reason=[%s].", errmsg);
		return(-1);
	}

	rc =  OpenFileQueue(l, NULL, NULL, NULL, NULL, path, qname, &obj);
	CHECK(rc==TRUE);

	/* normal data : data size is less than msglen-1 */
	/* databuflen = obj->h_obj->h->msglen - 4 + 1; */
	/* big data : data size is greater than msglen-1 */

	databuflen = flen+1;

	if( strcmp(mode, "EN_NORMAL_MODE") == 0 ) { 
		rc =  obj->on_en(l, obj, EN_NORMAL_MODE, dst, databuflen, flen, &l_seq, &run_time );
	}
	else if( strcmp(mode, "EN_CIRCULAR_MODE") == 0 ) { 
		rc =  obj->on_en(l, obj, EN_CIRCULAR_MODE, dst, databuflen, flen, &l_seq, &run_time );
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
	}

	if(buf) free(buf);

stop:
	if(dst) free(dst);
	if(errmsg) free(errmsg);

	rc = CloseFileQueue(l, &obj);
	CHECK(rc==TRUE);

	fq_close_file_logger(&l);

	printf("Good bye !\n");
	return(rc);
}

/*
** input
	- filename
	- ptr_dst
	- flen
** output
	- ptr_errmsg
	- return success 0
	         fail    -1
*/
int LoadFile(char *filename, char**ptr_dst, int *flen, char** ptr_errmsg)
{
	int fdin, rc=-1;
	struct stat statbuf;
	off_t len=0;
	char	*p=NULL;
	int	n;
	char buf[1024];

	if(( fdin = open(filename ,O_RDONLY)) < 0 ) { 
		sprintf(buf, "open('%s') error.", filename);	
		*ptr_errmsg = strdup(buf);
		goto L0;
	}

	if(( fstat(fdin, &statbuf)) < 0 ) {
		sprintf(buf, "fstat('%s') error.", filename);	
		*ptr_errmsg = strdup(buf);
		goto L1;
	}

	len = statbuf.st_size;
	*flen = len;

	p = calloc(len+1, sizeof(char));
	if(!p) {
		*ptr_errmsg = strdup("memory leak!! Check your free memory!");	
		goto L1;
	}

	if(( n = read( fdin, p, len)) != len ) {
		*ptr_errmsg = strdup("read() error.");
		goto L1;
    }

	*ptr_dst = p;

	rc = 0;
L1:
	SAFE_FD_CLOSE(fdin);
L0:
	return(rc);
}
