#include "fq_logger.h"
#include "fq_common.h"
#include "fq_msg_conv.h"

int main(int ac, char **av)
{
	char *test_src_message = "20170331:121030|r  aspi|C  RM01|0000|010-7202-1516|Seoul|end";
	int rc;
	char *dst_msg=NULL;
	fq_logger_t *l=NULL;

	if( ac != 3 ) {
		printf("Usage: $ %s [src.map] [dst.map] <enter>\n", av[0]);
		printf("       $ ./ConvMessage SRC.MSG.map DST.MSG.list <enter>\n");
		return(0);
	}

	rc = fq_open_file_logger(&l, "/tmp/ConvMessage.log", FQ_LOG_TRACE_LEVEL);
	CHECK(rc=TRUE);

	rc = do_msg_conv( l, test_src_message, '|', av[1], av[2], &dst_msg);
	CHECK( rc > 0 );

	printf("dst_msg=[%s]\n", dst_msg);

	if(dst_msg) free(dst_msg);

	fq_close_file_logger(&l);

	return(rc);
}
