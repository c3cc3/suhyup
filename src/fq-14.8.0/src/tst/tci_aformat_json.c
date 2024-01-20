#include <stdio.h>
#include "fq_tci.h"
#include "fq_common.h"

#include "fq_conf.h"
#include "fq_codemap.h"
#include "fq_cache.h"

int main(int ac, char **av)
{
	int rc, len;
	char	*errmsg=NULL;

	aformat_t *a=NULL;
	char	*aformat_file="aformat.sample";

	char	*body="GWSANG01HWANGROONGM0302172.30.1.15    |11111172.30.1.30    |33333172.30.1.11    |44444011010372451   |100000000012522817400107|900000000020200620041000";

	a = new_aformat(aformat_file, &errmsg);
	CHECK(a);
	step_print( "new_aformat()", "OK");

	rc = read_aformat(a, aformat_file, &errmsg);
	if(rc < 0 ) {
		printf("read_aformat() error. errmsg=[%s]\n", errmsg);
		return -1;
	}
	step_print( "read_aformat()", "OK");

	/*
	** Receive data from other server by TCP or Messaging Queue.
	*/
	a->data = strdup(body);
	a->datalen = strlen(body);;

	len = assoc_aformat(a, &errmsg);
	if( len < 0 ) {
		printf("assoc_aformat() error. errmsg=[%s]\n", errmsg);
		return(-1);
	}
	step_print( "assoc_aformat()", "OK");
	printf("We make a->postdata with a->data by assoc_aformat().\n");
	printf("postdata: %s\n", a->postdata);

	printf("success!!!\n");
	
	char buf[128];
	memset(buf, 0x00, sizeof(buf));
	get_avalue( a, "address", buf);
	printf("buf='%s'\n", buf);

	if( a->data ) {
		free(a->data);
		a->data = NULL;
	}

	if(a) free_aformat(&a);

	printf("good bye...\n");
	return(0);
}
