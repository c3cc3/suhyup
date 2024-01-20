#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "fq_common.h"

#define ITEMS 3
#define DELIMITER_CHAR '|'
#define DELIMITER_CHAR_1 '`'
#define DELIMITER_CHAR_2 ' '

int main(int ac, char **av)
{
	char *src="distname|program|hostname";
	char *src1="distname1`program1`hostname1";
	char *src2="distname2 program2 hostname2";
	char *err_src="distname2  program2 hostname2"; // insert two space 
	char *err_src1="distname2program2 hostname2"; // lack of 1 space 
	char *dst[ITEMS]; /* distname|program|hostnam -> ITEMS is 3.*/
	int i;
	int rc;

	/* test-1 */
	rc = delimiter_parse(src,DELIMITER_CHAR, ITEMS, dst);
	CHECK(rc==TRUE);
	printf("results: %s,%s,%s\n", dst[0], dst[1], dst[2]);
	for(i=0; i<ITEMS; i++)
		if(dst[i]) free(dst[i]);

	/* test-2 */
	rc = delimiter_parse(src1,DELIMITER_CHAR_1, ITEMS, dst);
	CHECK(rc==TRUE);
	printf("results: %s,%s,%s\n", dst[0], dst[1], dst[2]);
	for(i=0; i<ITEMS; i++)
		if(dst[i]) free(dst[i]);

	/* test-3 */
	rc = delimiter_parse(src2,DELIMITER_CHAR_2, ITEMS, dst);
	CHECK(rc==TRUE);
	printf("results: %s,%s,%s\n", dst[0], dst[1], dst[2]);
	for(i=0; i<ITEMS; i++)
		if(dst[i]) free(dst[i]);

	/* error data test */
	rc = delimiter_parse(err_src1, DELIMITER_CHAR_2, ITEMS, dst);
	CHECK(rc==TRUE);
	printf("results: %s,%s,%s\n", dst[0], dst[1], dst[2]);
	for(i=0; i<ITEMS; i++)
		if(dst[i]) free(dst[i]);

	printf("The end.\n");
}
