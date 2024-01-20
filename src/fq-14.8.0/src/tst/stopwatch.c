/*
 * stopwatch.c
 * sample
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include "fqueue.h"
#include "fq_common.h"

int main(int ac, char **av)  
{  
	stopwatch_micro_t p;

	while(1) {
		on_stopwatch_micro(&p);

		usleep(1500);

		printf("deley time: (%14ld) [micro sec]\n", off_stopwatch_micro_result(&p));

	}
	return(0);
}
