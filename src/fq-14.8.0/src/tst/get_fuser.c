/*
** get_fuser.c
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fq_common.h"

int main(int argc, char* argv[]) 
{
	char *out = NULL;
	int rc;

	rc = get_fuser( "/home/pi/data/TST.r", &out);
	if( rc > 0 ) {
		printf("rc=[%d], out : %s", rc, out);
		SAFE_FREE(out);
	}

    return 0;
}
