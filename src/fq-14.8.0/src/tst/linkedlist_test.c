#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "fq_linkedlist.h"
#include "fq_common.h"
#include "fqueue.h"

#define MEM_LEAK_TEST (0)

typedef struct _mon mon_t;
struct _mon {
	int i;
	char *a;
	fqueue_obj_t	*obj;
};

bool my_scan_function( size_t value_sz, void *value)
{
	mon_t *tmp;
	tmp = (mon_t *) value;
	printf("value size =[%zu] tmp.i=[%d], tmp.a=[%s], tmp->obj->h->msglen=[%zu]\n", 
		value_sz, tmp->i, tmp->a, tmp->obj->h_obj->h->msglen);

	return(true);
}

int main()
{
	linkedlist_t	*ll=0;
	ll_node_t *ll_node=NULL;
	fqueue_obj_t *obj=NULL;
	int rc;
	char *path="/home/gwisangchoi/enmq";
	char *qname="TST";

	rc =  OpenFileQueue(0, NULL, NULL, NULL, NULL, path, qname, &obj);
	CHECK(rc==TRUE);

#ifdef MEM_LEAK_TEST
while(1) {
#endif

	ll = linkedlist_new("mon");

	int i;
	for(i=0; i<100; i++) {
		mon_t	*tmp=NULL;
		char key[8];
		sprintf(key, "%07d", i);
		tmp = calloc(1, sizeof(mon_t));
		tmp->i = i;
		tmp->a = "12345678901234567890123456789012345678901234567890";

		tmp->obj = obj;
		ll_node = linkedlist_put(ll, key, (void *)tmp, sizeof(int)+50+sizeof(fqueue_obj_t));
		free(tmp);
	}

	linkedlist_sort(ll);

	// size_t	value_sz;
	// void *value;
	linkedlist_callback( ll, my_scan_function);

#if 0
  	linkedlist_scan(ll, stdout);

	ll_node_t *p;
	for(p=ll->head; p!=NULL; p=p->next) {
		// mon_t tmp;
		// memcpy(&tmp, p->value, p->value_sz);
		// printf("tmp->i=[%d], tmp->a=[%s].\n", tmp.i, tmp.a);
		mon_t *tmp;
		size_t   value_sz;
		tmp = (mon_t *) p->value;
		value_sz = p->value_sz;
		printf("value size =[%d] tmp.i=[%d], tmp.a=[%s]\n", value_sz, tmp->i, tmp->a);
	}
#endif

	printf("\nused\n");
	system("free -m");

	if(ll) linkedlist_free(&ll);

	sleep(1);
	printf("\nafter\n");

	system("free -m");

#ifdef MEM_LEAK_TEST
	sleep(1);
}
#endif
	return(0);
}
