#include <stdio.h>
#include "fq_common.h"
#include "fqueue.h"

int main(int ac, char **av)
{
	fq_logger_t *l=0x00;
	char *buf = 0x00;
	int	return_length;
	long return_seq;
	bool big_flag = false;
	int rc;
	char *logname = "fq_view.log";

	if( ac != 3 ) {
		printf("Usage: $ %s <qpath> <qname> <ENTER>\n", av[0]);
		return 0;
	}

	rc = fq_open_file_logger(&l, logname, FQ_LOG_TRACE_LEVEL);
	CHECK( rc > 0 );

	rc = ViewFileQueue(l, av[1], av[2], &buf, &return_length, &return_seq, &big_flag);
	printf("rc = %d result='%s'\n", rc, (rc>=0) ? "success": "failed");

	if( rc == 0 ) {
		printf("There is no data.(empty)\n");
	}
	else if( rc> 0 ) {
		printf("buf='%s', length='%d', seq='%ld' big_flag =[%d]\n", buf, return_length, return_seq, big_flag);
	}

	SAFE_FREE(buf);
	return 0;
}
