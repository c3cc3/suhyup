#include "fqueue.h"
#include "fq_manage.h"
#include "fq_linkedlist.h"
#include "shm_queue.h"

static bool my_CB_function( size_t value_sz, void *value)
{
	queue_obj_node_t *tmp;
	tmp = (queue_obj_node_t *) value;
	fqueue_obj_t *qobj=tmp->obj;
	fq_logger_t *l = NULL;

	printf("[%s][%s] : [%ld][%ld]-[%f]\n", qobj->path, qobj->qname, qobj->h_obj->h->de_sum, qobj->h_obj->h->de_sum, qobj->on_get_usage(l, qobj));

	return true;
}

int main()
{
	all_queue_scan_CB_n_times(1, 0, my_CB_function );
	// all_queue_scan_CB_server(5, my_CB_function );
	return 0;
}
