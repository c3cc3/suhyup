/* 
** ManageHashMap.c 
*/
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <unistd.h>
# include <pthread.h>

#include "hash_manage.h"
#include "fq_logger.h"
#include "fq_common.h"

int main(int ac, char **av)
{
	int rc;
	fq_logger_t *l=NULL;
	char *path=NULL;
	char *hashname=NULL;
	char *cmd=NULL;	
	char *logname = "/tmp/ManageHashMap.log";
	char *data_home = NULL;
	
	hash_cmd_t hash_cmd;

	data_home = getenv("HASH_DATA_HOME");
	if( !data_home ) {
		data_home = getenv("HOME");
		if( !data_home ) {
			data_home = strdup("/data");
		}
	}

	printf("Compiled on %s %s\n", __TIME__, __DATE__);
	printf("logging path: [%s]. \n", logname);

	if( ac != 3 && ac != 4 ) {
		printf("\x1b[1;32m Usage \x1b[0m: $ %s [path] [cmd]\n", av[0]);
		printf("Usage: $ %s [path] [qname] [cmd]\n", av[0]);

		printf("Usage: $ %s %s [create|unlink|clean|extend]\n", av[0], data_home);
		printf("Usage: $ %s %s [hashname] [create|unlink|clean|extend|view|scan]\n", av[0], data_home);
	}

	rc = fq_open_file_logger(&l, logname, FQ_LOG_ERROR_LEVEL);
	CHECK( rc > 0 );

	if( ac == 3 ) { /* all test */
		path = av[1];
		cmd = av[2];

		if( strcmp( cmd, "create") == 0 ) {
			hash_cmd = HASH_CREATE;
		}
		else if( strcmp( cmd, "unlink") == 0 ) {	
			hash_cmd = HASH_UNLINK;
		}
		else if( strcmp( cmd, "clean") == 0 ) {	
			hash_cmd = HASH_CLEAN;
		}
		else if( strcmp( cmd, "extend") == 0 ) {	
			hash_cmd = HASH_EXTEND;
		}
		else if( strcmp( cmd, "view") == 0 ) {	
			hash_cmd = HASH_VIEW;
		}
		else {
			printf("un-supported cmd.\n");
			return(0);
		}
		rc = hash_manage_all(l, path, hash_cmd);
		CHECK(rc==TRUE);
	}
	else if( ac == 4 ) { /* one by one test */
		path = av[1];
		hashname = av[2];
		cmd = av[3];

		if( strcmp( cmd, "create") == 0 ) {
			hash_cmd = HASH_CREATE;
		}
		else if( strcmp( cmd, "unlink") == 0 ) {	
			hash_cmd = HASH_UNLINK;
		}
		else if( strcmp( cmd, "clean") == 0 ) {	
			hash_cmd = HASH_CLEAN;
		}
		else if( strcmp( cmd, "extend") == 0 ) {	
			hash_cmd = HASH_EXTEND;
		}
		else if( strcmp( cmd, "view") == 0 ) {	
			hash_cmd = HASH_VIEW;
		}
		else if( strcmp( cmd, "scan") == 0 ) {	
			hash_cmd = HASH_SCAN;
		}
		else {
			printf("un-supported cmd.\n");
			return(0);
		}
		rc = hash_manage_one(l, path, hashname, hash_cmd);

		CHECK(rc==TRUE);
	}

	fq_close_file_logger(&l);

	printf("Success!! OK.\n");
	return(rc);
}
