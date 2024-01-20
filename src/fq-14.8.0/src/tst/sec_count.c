#include <stdio.h>
#include "fq_common.h"
#include "fq_sec_counter.h"

int main(int ac, char **av)
{
	int i;
	int rc;
	second_counter_obj_t *obj=NULL;

	if(open_second_counter_obj(".", "second.count.log", 100000, &obj) == FALSE) {
		return(0);
	}

re_test:
	rc = obj->on_second_count(obj);
	if( rc == TEST_CONTINUE ) {
		usleep(1000);
		goto re_test;
	}

	printf("tot_cnt=[%d]\n", obj->tot_cnt);

	close_second_counter_obj(&obj);
	return(0);
}
