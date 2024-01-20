#include <stdio.h>
#include "fq_scanf.h"
#include "fq_common.h"

int main(int ac, char **av)
{
	int delimiter = 0x26;

	char *p=NULL;
	char buf1[16];
	char buf2[16];
	char buf3[16];
	char buf4[16];
	int cnt;

	FILE *fp=NULL;
	char buf[512];

	memset(buf1, 0x00, sizeof(buf1));
	memset(buf2, 0x00, sizeof(buf2));
	memset(buf3, 0x00, sizeof(buf3));
	memset(buf4, 0x00, sizeof(buf4));

	printf("delimiter = [%c]\n", '~');
	p = "1111~ ~3333~4444";

	delimiter = (int)'~';
	cnt = fq_sscanf(delimiter, p, "%s%s%s%s", buf1, buf2, buf3, buf4);
	printf("cnt=[%d] [%s] [%s] [%s] [%s] \n\n",  cnt, buf1, buf2, buf3, buf4);


	printf("delimiter = [%c]\n", '&');
	p = "1111&2^%()_!@ /.,';:][#$'`&3333&4444";
	cnt = fq_sscanf(delimiter, p, "%s%s%s%s%s", buf1, buf2, buf3, buf4);
	printf("cnt=[%d]:  [%s] [%s] [%s] [%s] \n\n", cnt, buf1, buf2, buf3, buf4);

	printf("delimiter = [%c]\n", '|');
	p = "1111|2222|3333|4444";
	delimiter = (int)'|';
	cnt = fq_sscanf(delimiter, p, "%s%s%s%s", buf1, buf2, buf3, buf4);
	printf("cnt=[%d] [%s] [%s] [%s] [%s] \n\n",  cnt, buf1, buf2, buf3, buf4);

	printf("delimiter = [%c]\n", ',');
	p = "1111,2222,3333,4444";
	delimiter = (int)',';
	cnt = fq_sscanf(delimiter, p, "%s%s%s%s", buf1, buf2, buf3, buf4);
	printf("cnt=[%d] [%s] [%s] [%s] [%s] \n\n",  cnt, buf1, buf2, buf3, buf4);

	printf("delimiter = [%c]\n", '-');
	p = "1111-2222-3333-4444";
	delimiter = (int)'-';
	cnt = fq_sscanf(delimiter, p, "%s%s%s%s", buf1, buf2, buf3, buf4);
	printf("cnt=[%d] [%s] [%s] [%s] [%s] \n\n",  cnt, buf1, buf2, buf3, buf4);

	printf("delimiter = [%c]\n", '/');
	p = "1111/2222/3333/4444";
	delimiter = (int)'/';
	cnt = fq_sscanf(delimiter, p, "%s%s%s%s", buf1, buf2, buf3, buf4);
	printf("cnt=[%d] [%s] [%s] [%s] [%s] \n\n",  cnt, buf1, buf2, buf3, buf4);

	printf("delimiter = [%c]\n", '^');
	p = "1111^2222^3333^4444";
	delimiter = (int)'^';
	cnt = fq_sscanf(delimiter, p, "%s%s%s%s", buf1, buf2, buf3, buf4);
	printf("cnt=[%d] [%s] [%s] [%s] [%s] \n\n",  cnt, buf1, buf2, buf3, buf4);


	printf("delimiter = [%c]\n", '+');
	p = "1111+2222+3333+4444";
	delimiter = (int)'+';
	cnt = fq_sscanf(delimiter, p, "%s%s%s%s", buf1, buf2, buf3, buf4);
	printf("cnt=[%d] [%s] [%s] [%s] [%s] \n\n",  cnt, buf1, buf2, buf3, buf4);

	printf("delimiter = [%c]\n", '%');
	p = "1111%2222%3333%4444";
	delimiter = (int)'%';
	cnt = fq_sscanf(delimiter, p, "%s%s%s%s", buf1, buf2, buf3, buf4);
	printf("cnt=[%d] [%s] [%s] [%s] [%s] \n\n",  cnt, buf1, buf2, buf3, buf4);

	printf("delimiter = [%c]\n", ':');
	p = "1111:2222:3333:4444";
	delimiter = (int)':';
	cnt = fq_sscanf(delimiter, p, "%s%s%s%s", buf1, buf2, buf3, buf4);
	printf("cnt=[%d] [%s] [%s] [%s] [%s] \n\n",  cnt, buf1, buf2, buf3, buf4);

	printf("delimiter = [%c]\n", ';');
	p = "1111;2222;3333;4444";
	delimiter = (int)';';
	cnt = fq_sscanf(delimiter, p, "%s%s%s%s", buf1, buf2, buf3, buf4);
	printf("cnt=[%d] [%s] [%s] [%s] [%s] \n\n",  cnt, buf1, buf2, buf3, buf4);

	printf("delimiter = [%c]\n", '=');
	p = "1111=2222=3333=4444";
	delimiter = (int)'=';
	cnt = fq_sscanf(delimiter, p, "%s%s%s%s", buf1, buf2, buf3, buf4);
	printf("cnt=[%d] [%s] [%s] [%s] [%s] \n\n",  cnt, buf1, buf2, buf3, buf4);

	fp = fopen("./test.txt", "r");
	if( fp == NULL ) {
		printf("fopen() error. ./test.txt\n");
		return(0);
	}

	delimiter = (int)':';
	while(fgets(buf, 512, fp)) {
		fq_sscanf(delimiter, buf, "%s%s%s%s\n", buf1, buf2, buf3, buf4);
		str_trim(buf4); /* This removes new-line character. */
		printf("[%s] [%s] [%s] [%s] \n",  buf1, buf2, buf3, buf4);
	}

	if(fp) fclose(fp);

	return(0);
} 
