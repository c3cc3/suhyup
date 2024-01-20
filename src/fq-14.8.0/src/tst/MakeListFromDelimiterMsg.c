#include <stdio.h>
#include <string.h>
 #include <unistd.h>

#include "fq_cache.h"

int main()
{
	fqlist_t	*li=NULL;
	int Count;
	char delimiter='|';

//	while(1) {
		li = fqlist_new('C', "body");
		char *a="1234|56789|0abc|de|fgh|ijk|lmn|opq|rst|uvw|xyz";

		Count = MakeListFromDelimiterMsg(li, a, delimiter, 0);

		fqlist_print(li, stdout);
		printf("Element ount: %d.\n", Count);

		fqlist_free(&li);
		usleep(1000);
//	}
}

#if 0

/*
** This is a sample when delimiter is null(0x00) 
*/
#include <libgen.h>

#include "fq_common.h"
#include "fq_tokenizer.h"
#include "fq_cache.h"

int main()
{
	bool	tf;
	fqlist_t *li=NULL;
	char	delimiter=0x00;
	int		count;
	unsigned char src_stream[128];

	memset(src_stream, 0x00, sizeof(src_stream));
	memcpy(&src_stream[0], "aaa", 3);
	memcpy(&src_stream[4], "bbb", 3);
	memcpy(&src_stream[8], "ccc", 3);
	
	li = fqlist_new('C', "arguments");
	count = MakeListFromDelimiterMsg( li, src_stream, delimiter, 0); 

	fqlist_print(li, stdout);
	printf("Count of arguments: %d.\n", count);

	return 0;
}
#endif
