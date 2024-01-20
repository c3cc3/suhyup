/*
 * find_pid_fds.c
 * sample
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include "fq_lsof.h"

int main(int ac, char **av)  
{  
	int count_fds = 0;
	long int mypid;
	FILE	*fp=NULL;

	fp = fopen("/tmp/1", "r");

	mypid = getpid();
	count_fds = find_pid_fds( mypid );

	printf("count_fds = [%d]\n", count_fds);

	fclose(fp);
	return(0);
}
