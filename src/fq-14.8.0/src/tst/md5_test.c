/*
** md5_test.c
*/
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "fq_md5.h"

int main(int argc, char **argv) {
	int i, index=0;
	char buf[20+1];
	fq_hash_t h;
 
    if (argc < 2) {
        printf("usage: %s 'string'\n", argv[0]);
        return 1;
    }
 
    char *msg = argv[1];
    size_t msglen = strlen(msg);
 
	memset(buf, 0x00, sizeof(buf));

	md5(msg, msglen, &h, buf);

	printf("msglen=%zu, result=[%s].\n", msglen, buf);
 
    return 0;
}
	

