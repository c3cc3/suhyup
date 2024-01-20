#include <stdio.h>
#include "fq_common.h"
#include "fq_lsof.h"

int main()
{

	long int mypid;
	int n;

	mypid = getpid();
	n = find_pid_fds(mypid);
	printf("n=[%d]\n", n);

	return(0);
}
