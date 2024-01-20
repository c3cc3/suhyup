#include <stdio.h>
#include <stdbool.h>
#include <libgen.h>

#include "fq_common.h"
#include "fqueues.h"

int main(int ac, char **av)
{
	fq_logger_t *l=NULL;
	fq_objs_t *objs = NULL;
	bool tf;
	char group_initial[2];

	tf = OpenFileQueues(l, "./co_queue.map", &objs);
	CHECK(tf == true);

	fqueue_obj_t *qobj=NULL;

loop:
	/* get a queue object by same group_initial */
	get_seed_ratio_rand_str( group_initial, sizeof(group_initial), "KLSN");
	qobj = objs->on_get_key_qobj(objs, group_initial[0]);
	CHECK(qobj);
	printf("group=[%c] path=%s, qname=%s\n", group_initial[0], qobj->path, qobj->qname);

	/* get a queue object with least diff */
	qobj = objs->on_get_least_qobj(objs);
	CHECK(qobj);
	printf("least -> path=%s, qname=%s\n", qobj->path, qobj->qname);

	/* get a queue object with least diff */
	qobj = objs->on_find_qobj(objs, "/home/gwisangchoi/enmq", "TST4");
	CHECK(qobj);
	printf("found -> path=%s, qname=%s\n", qobj->path, qobj->qname);

 	usleep(1000);
 	goto loop;

	tf = CloseFileQueues(l, objs);
	CHECK(tf == true);
	return(0);
}

