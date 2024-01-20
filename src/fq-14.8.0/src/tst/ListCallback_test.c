#include "fq_common.h"
#include "fq_delimiter_list.h"
#include <stdbool.h>

bool usr_f( char *item)
{
	printf("item=[%s]\n", item);
	return true;
}
int main()
{

	delimiter_list_obj_t  *obj=NULL;
	char *src = "kt,skt,lgt,sn,wiseu,mnwise";
	int rc;
	bool tf;

	rc = open_delimiter_list(0, &obj, src, ',');
	CHECK(rc==TRUE);

	tf = ListCallback( obj, usr_f);
	if(tf == false) printf("error.\n");

	close_delimiter_list( 0, &obj);

	return 0;
}
