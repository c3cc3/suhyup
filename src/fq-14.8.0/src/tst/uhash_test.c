/*
** qsort.c:17:5: error: 'for' loop initial declarations are only allowed in C99 mode
** qsort.c:17:5: note: use option -std=c99 or -std=gnu99 to compile your code
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fq_common.h"

unsigned long 
fq_msgid_hash(const char *s, int msgid_len, int mod )
{
    unsigned long i;
	unsigned long sum = 0;

    for(i = 0;  i < msgid_len; i++) {
		sum = sum + s[i];
    }
	return ( sum % mod ); 
}

int main(int argc, char* argv[]) 
{
	int r;
	int max_thread_numbers = 10;
	char msgid[20+1+1];
	char date_time_msec[20];

	get_date_time_ms( date_time_msec );

	sprintf(msgid, "K%s", date_time_msec);

	printf("[%s] msgid = [%s] len = [%ld]\n", date_time_msec,  msgid, strlen(msgid) );

	r = fq_msgid_hash( msgid, 21, max_thread_numbers  );	

	printf("return =[%d]\n", r);

    return 0;
}
