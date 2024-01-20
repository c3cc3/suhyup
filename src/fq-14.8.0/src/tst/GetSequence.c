/*
 * $(HOME)/src/utl/GetSequence.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include "fq_common.h"

int main(int ac, char **av)  
{  
	int rc;
	fq_logger_t *l=NULL;
	char *logname = "/tmp/GetSequence.log";
	char SEQ[21];

	rc = fq_open_file_logger(&l, logname, FQ_LOG_ERROR_LEVEL);
	CHECK( rc > 0 );

	/*
    ** l: logger_t  *pointer: If you are not necessary, Put NULL.
    ** "ESS", : prefix
    ** 20 : total length of sequence value
    ** SEQ: buffer name for string value
    ** buffersize
    */
	rc = GetSequeuce( l,  "ESS",  20,  SEQ, sizeof(SEQ) );
	CHECK( rc == TRUE );

	printf("SEQ=[%s] len=[%ld]\n", SEQ, strlen(SEQ));

	fq_close_file_logger(&l);

	printf("Good bye !\n");
	return(rc);
}
