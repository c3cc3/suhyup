#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "fq_common.h"

int main(int ac, char **av)
{
	char buf[128];
	char before='A';
	int second_count=0;

	while(1) {
		get_milisecond(buf);
		if( before != 'A' && buf[5] != before) {
			printf("Count=[%d]\n", second_count);
			second_count = 0;
		}
		printf("current_time = [%s].\n", buf);
		before = buf[5];
		second_count++;
	}
	return 0;
}


