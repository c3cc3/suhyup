#include <stdio.h>
#include "fq_common.h"
#include "fq_logger.h"
#include "fq_flock.h"
#include "fq_sec_counter.h"

#define TEST_COUNT 1000000

int lock_count=0;

int main(int ac, char **av)
{
	int i;
	int rc;
	flock_obj_t *flobj=NULL;
	fq_logger_t *l=NULL;
	char *logname="flock_test.log";
	second_counter_obj_t *obj=NULL;

	rc = fq_open_file_logger(&l, logname, FQ_LOG_TRACE_LEVEL);
    CHECK( rc > 0 );

	rc = open_second_counter_obj(".", "flock_second_count.result", 1000000, &obj);
	CHECK( rc == TRUE );

	rc = open_flock_obj( l, ".", "TST", ETC_FLOCK, &flobj);
    CHECK( rc == TRUE );

re_test:
	flobj->on_flock(flobj);

	flobj->on_funlock(flobj);

	rc = obj->on_second_count(obj);
	if( rc == TEST_CONTINUE ) {
        goto re_test;
    }

	printf("tot_cnt=[%d]\n", obj->tot_cnt);

	close_second_counter_obj(&obj);
	close_flock_obj(l, &flobj);

	fq_close_file_logger(&l);

	return(0);
}
