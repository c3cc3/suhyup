/*
** /tst/base64_decode.c
*/
#include "fq_base64_std.h"
#include "fq_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/param.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct _test_struct {
	char name[20];
	int	age;
	unsigned char msg[1024];
	float average;
};
typedef struct _test_struct test_struct_t;


int main()
{
	test_struct_t	*p=calloc(1, sizeof(test_struct_t));
	char *target = calloc(2048, sizeof(char));

	sprintf( p->name , "%s", "최귀상");
	p->age = 55;
	sprintf(p->msg, "%s", "Hi, gentleman.");
	p->average = 90.0;

	size_t targsize=2048;
	int datalength;
	datalength = b64_encode((u_char *)p, sizeof(test_struct_t) , target, targsize);
	CHECK(datalength);
	printf("encoding result: target='%s', length=%d\n", target, datalength );


	char *target2 = calloc(4096, sizeof(u_char));

	datalength = b64_decode(target, target2, 4096); 

	test_struct_t *q = calloc(1, sizeof(test_struct_t));
	memcpy((u_char *)q, target2, datalength);
	printf("decoded result: rc=%d\n", datalength);
	printf("\t name: '%s'\n", q->name);
	printf("\t age : '%d'\n", q->age);
	printf("\t msg : '%s'\n", q->msg);
	printf("\t avrg: '%f'\n", q->average);

	return(0);
}

