/* fq_container.c */
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include "fqueue.h"
#include "fq_manage.h"

int main(int ac, char **av)
{
	int rc;
	fq_container_t *fq_c=NULL;
	fq_logger_t *l=NULL;

	rc = load_fq_container(l, &fq_c);

	CHECK(rc == TRUE );

	fq_container_print(fq_c, stdout);

	/* access container */
	dir_list_t *d;
	fq_node_t *f;

	for ( d=fq_c->head; d != NULL; d = d->next ) {
		printf("dirname=[%s].\n", d->dir_name);
		for ( f=d->head; f != NULL ; f = f->next ) {
			fprintf(stdout, "\t qname='%s', msglen=[%d], mapping_num=[%d], multi_num=[%d], desc=[%s], XA_flag=[%d], Wait_mode_flag=[%d]\n",
        		f->qname,
        		f->msglen,
        		f->mapping_num,
        		f->multi_num,
        		f->desc,
        		f->XA_flag,
        		f->Wait_mode_flag);
		}
	}

	if(fq_c) fq_container_free(&fq_c);

	return(0);
}
