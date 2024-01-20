/*
 * $(HOME)/src/utl/SetQueueHeader.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include "fqueue.h"

#define LOG_FILE_NAME "SetMaster.log"

int SetMaster( char *path, char *qname, int flag, char *hostname );

int main(int ac, char **av)  
{  
	char *path="/home/pi/data";
	char *qname="TST";
	char	hostname[32];
	int flag;

	if( ac != 5 ) {
		printf("Usage:$  %s [path] [qname] [flag] [hostname] <enter>\n", av[0]);
		printf("      $  %s /home/pi/data TST 1 raspsvr01 <enter>\n", av[0]);
		return(0);
	}
	path = &av[1][0];
	qname = &av[2][0];
	flag = atoi(av[3]);
	sprintf(hostname, "%s", av[4]);
	
	// gethostname(hostname, 32);

	SetMaster( path, qname, flag, hostname);

	return(0);
}

int SetMaster( char *path, char *qname, int flag, char *hostname )
{
	int rc;
	fqueue_obj_t *obj=NULL;
	fq_logger_t *l=NULL;
	char *logname = LOG_FILE_NAME;

	rc = fq_open_file_logger(&l, logname, FQ_LOG_ERROR_LEVEL);
	CHECK( rc > 0 );

	rc = OpenFileQueue(l, NULL, NULL, NULL, NULL, path, qname, &obj);
	CHECK(rc==TRUE);

	rc = obj->on_set_master(l, obj, flag, hostname);
	CHECK(rc==TRUE);

	CloseFileQueue(l, &obj);
	CHECK(rc==TRUE);

	fq_close_file_logger(&l);

	return(0);
}
