#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "fq_common.h"
#include "fq_utl.h"

int main(int ac, char **av)
{
	int rc;

	/* for saving argv */
	char *fn=NULL;
	int	lines;
	int	msglen;
	char	*data_home=NULL;
	/* argv end */

	printf("Compiled on %s %s\n", __TIME__, __DATE__);

	data_home = getenv("FQ_DATA_HOME");
    if( !data_home ) {
        data_home = getenv("HOME");
        if( !data_home ) {
            data_home = strdup("/data");
        }
    }

	if( ac != 4 ) {
		printf("Usage: $ %s [filename] [lines] [msglen]\n", av[0]);
		return(0);
	}

	fn = av[1];
	lines = atoi(av[2]);
	msglen = atoi(av[3]);

	printf("\n\n Summary ***\n");
	printf("\t- filename: %s\n", fn);
	printf("\t- lines   : %d\n", lines);
	printf("\t- msglen : %d\n", msglen);

	rc = MakeTestData(fn, lines, msglen);
	CHECK(rc>=0);

	printf("Success.\n");
	return(0);
}
