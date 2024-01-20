/*
** gssi_test.c
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#include "fq_logger.h"
#include "fq_common.h"
#include "fq_fstat.h"
#include "fq_replace.h"

int main(int ac, char **av)
{
	int rc;
	char *src = "LOG_PATH=/home/fq/logs..........akdkfkdkff/home/fq.aaaadfdfd/// /home/fq/logs";
	char *find_str=NULL;
	char *replace_str = NULL;
	char *logpath = ".";
	char *logname = "replace_test.log";
	char *loglevel = "error";
	char logpathfile[256];
	fq_logger_t *l;


	if( ac != 5 ) {
		printf( "$ %s [path] [find_filename_sub_string] [find_string] [replac_string] <enter>\n", av[0]);
		exit(0);
	}

	find_str = av[3];
	replace_str = av[4];

	sprintf(logpathfile, "%s/%s", logpath, logname);
	rc = fq_open_file_logger(&l, logpathfile, get_log_level(loglevel));
	CHECK( rc > 0 );

	fq_dir_replace( l, av[1], av[2], av[3], av[4]);
	// fq_file_replace( l, av[1], av[2], av[3], av[4]);
	// fq_replace( l, av[1], av[2], av[3], av[4]);

	return(0);
}
