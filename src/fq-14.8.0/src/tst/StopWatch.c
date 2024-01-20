#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "fq_timer.h"

int main(int ac, char **av)
{
	StopWatch_t	*SW=NULL;
	// double delayed_time;

	open_StopWatch(&SW);

	SW->Start(SW);
	// SW->Work(SW, 1000000); /* nano second -> about 1000 micro sec */

	sleep(10);
	SW->Stop(SW);

	// delayed_time = SW->GetElapsed(SW);
	// printf("delay=[%lf]\n", delayed_time);

	SW->PrintElapsed(SW);

	close_StopWatch(&SW);
	return 0;
}


