/*
** fq_container_CB_test.c
*/


#include <stdio.h>
#include <stdbool.h>
#include "fq_manage.h"
#include "fq_logger.h"
#include "fq_common.h"

bool my_CB_func(fq_logger_t *l, char *dir, fq_node_t *n)
{
	printf("%s %s\n", dir, n->qname);
	return true;
}

int main()
{
	fq_logger_t *l=NULL;
	fq_container_t *fq_c=NULL;
	int rc;

	rc = load_fq_container(l, &fq_c);
	CHECK(rc == TRUE);

	fq_container_CB( l, fq_c, my_CB_func);

	if(fq_c) fq_container_free(&fq_c);

	return(0);
}
