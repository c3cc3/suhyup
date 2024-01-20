#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "fq_linkedlist.h"
#include "fq_common.h"
#include "fqueue.h"
#include "shm_queue.h"
#include "fq_manage.h"

#define MEM_LEAK_TEST (1)

typedef struct _mon mon_t;
struct _mon {
	int i;
	fqueue_obj_t	*obj;
};
static bool OpenFileQueues_and_MakeLinkedlist(fq_logger_t *l, fq_container_t *fq_c, linkedlist_t *ll);

bool my_scan_function( size_t value_sz, void *value)
{
	mon_t *tmp;
	tmp = (mon_t *) value;
	printf("value size =[%zu] tmp.i=[%d], tmp->obj->h->msglen=[%zu]\n", 
		value_sz, tmp->i, tmp->obj->h_obj->h->msglen);

	return(true);
}

int get_all_fqueue_info( fq_logger_t *l, linkedlist_t *ll, int *count)
{
	int rc;
	fq_container_t *fq_c=NULL;

	rc = load_fq_container(l, &fq_c);
	if( rc != TRUE ) {
		fq_log(l, FQ_LOG_ERROR, "load_fq_container() error.");
		return -1;
	}
		
	bool tf;
	tf = OpenFileQueues_and_MakeLinkedlist(l, fq_c, ll);
	if( tf != true ) {
		fq_log(l, FQ_LOG_ERROR, "OpenFileQueues_and_MakeLinkedlist() error.");
		return -2;
	}
	
	*count = ll->length;

	return(0);
}

int main()
{

	linkedlist_t	*ll=0;
	ll_node_t *ll_node=NULL;
	fqueue_obj_t *obj=NULL;
	int rc;
	char *path="/home/gwisangchoi/enmq";
	char *qname="TST";
	fq_logger_t *l=NULL;

	rc = fq_open_file_logger(&l, "./all_queue_info.log", FQ_LOG_ERROR_LEVEL);
	CHECK(rc == TRUE);

	ll = linkedlist_new("fqueue_info");
	CHECK(ll);

	int counts;
	rc =  get_all_fqueue_info( l, ll, &counts);
	if( rc < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "get_all_fqueue_info() error.");
	}
	printf("rc=[%d]\n", counts);

	ll_node_t *p;
	for(p=ll->head; p!=NULL; p=p->next) {
		mon_t *tmp;
		fqueue_obj_t *fobj = NULL;

		size_t   value_sz;
		tmp = (mon_t *) p->value;
		value_sz = p->value_sz;
		fobj = tmp->obj;
		printf("value size =[%ld] tmp.i=[%d], %s/%s, diff=[%ld] \n", 
			value_sz, tmp->i, fobj->path, fobj->qname, tmp->obj->on_get_diff(0, tmp->obj));
	}

	if(ll) linkedlist_free(&ll);

	return(0);
}

static bool OpenFileQueues_and_MakeLinkedlist(fq_logger_t *l, fq_container_t *fq_c, linkedlist_t *ll)
{
	dir_list_t *d=NULL;
	fq_node_t *f;
	ll_node_t *ll_node = NULL;
	int sn=1;
	int opened = 0;

	for( d=fq_c->head; d!=NULL; d=d->next) {
		for( f=d->head; f!=NULL; f=f->next) {
			fqueue_obj_t *obj=NULL;
			int rc;
			mon_t *tmp = NULL;

			obj = calloc(1, sizeof(fqueue_obj_t));

			fq_log(l, FQ_LOG_DEBUG, "OPEN: [%s/%s].", d->dir_name, f->qname);
			
			char *p=NULL;
			if( (p= strstr(f->qname, "SHM_")) == NULL ) {
				rc =  OpenFileQueue(l, NULL, NULL, NULL, NULL, d->dir_name, f->qname, &obj);
			} 
			else {
				rc =  OpenShmQueue(l, d->dir_name, f->qname, &obj);
			}
			if( rc != TRUE ) {
				fq_log(l, FQ_LOG_ERROR, "OpenFileQueue('%s') error.", f->qname);
				return(false);
			}
			fq_log(l, FQ_LOG_DEBUG, "%s:%s open success.", d->dir_name, f->qname);

			char key[36];
			sprintf(key, "%s/%s", d->dir_name, f->qname);
			tmp = calloc(1, sizeof(mon_t));
			tmp->i = sn++;
			tmp->obj = obj;

			size_t value_size = sizeof(int)+sizeof(fqueue_obj_t);

			ll_node = linkedlist_put(ll, key, (void *)tmp, value_size);

			if(!ll_node) {
				fq_log(l, FQ_LOG_ERROR, "linkedlist_put('%s', '%s') error.", d->dir_name, f->qname);
				return(false);
			}

			// printf("tmp.i=[%d], name->[%s/%s], tmp->obj->h_obj->h->msglen=[%zu]\n", 
			// 	tmp->i, tmp->obj->path, tmp->obj->qname, tmp->obj->h_obj->h->msglen);

			if(tmp) free(tmp);

			opened++;
		}
	}

	fq_log(l, FQ_LOG_INFO, "Number of opened filequeue is [%d].", opened);

	return(true);
}
