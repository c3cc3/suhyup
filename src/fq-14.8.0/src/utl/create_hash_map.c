#include "fq_common.h"
#include "fq_hashobj.h"
#include "fq_logger.h"


int main(int ac, char **av)
{
	int rc;

	if( ac != 3 ) {
		printf("Usage: $ %s [path] [name]\n", av[0]);
		printf("ex   : $ %s /home/gwisangchoi/enmq/HASH dictionary \n", av[0]);
		return 1;
	}

	rc = CreateHashMapFiles( NULL, av[1], av[2]);
	CHECK(rc == TRUE);

	return 0;
}
