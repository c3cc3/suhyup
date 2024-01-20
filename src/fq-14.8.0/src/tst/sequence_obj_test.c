#include "fq_defs.h"
#include "fq_logger.h"
#include "fq_common.h"
#include "fq_sequence.h"
#include "fq_sec_counter.h"

int main()
{
	int rc;
	fq_logger_t *l=NULL;
	char *logname="./sequence_obj_test.log";
	sequence_obj_t *sobj=NULL;
	second_counter_obj_t *cobj=NULL;
	char buf[21];

	rc = fq_open_file_logger(&l, logname, FQ_LOG_ERROR_LEVEL);
	CHECK( rc > 0 );

	rc = open_sequence_obj(l, ".", "test", "A", 10, &sobj);
	CHECK( rc==TRUE );

	if(open_second_counter_obj(".", "sequence_count.result", 100000, &cobj) == FALSE) {
		return(0);
	}

re_test:
	rc = sobj->on_get_sequence(sobj, buf, sizeof(buf));
	CHECK( rc==TRUE );
	printf("buf=[%s]\n", buf);

	rc = cobj->on_second_count(cobj);
	if( rc == TEST_CONTINUE ) {
		goto re_test;
	}

#if 0
	sobj->on_reset_sequence(NULL);
	CHECK( rc==TRUE );
	printf("reset OK\n");

	rc = sobj->on_get_sequence(NULL, "OBJ", 20, buf, sizeof(buf));
	CHECK( rc==TRUE );
	printf("buf=[%s]\n", buf);
#endif

	close_second_counter_obj(&cobj);
	close_sequence_obj(&sobj);
	fq_close_file_logger( &l );

	return(EXIT_SUCCESS);
}
