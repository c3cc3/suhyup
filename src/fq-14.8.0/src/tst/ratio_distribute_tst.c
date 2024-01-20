/*
** This program is a sample for using ratio_distributor object.
** ratio_distribute.c
**
** A: 50 %
** B: 30 %
** C: 10 %
** D: 10 %
*/
#include <stdio.h>
#include <stdbool.h>
#include "fq_common.h"
#include "fq_linkedlist.h"
#include "fq_delimiter_list.h"
#include "fq_ratio_distribute.h"

#define TOTAL_CNT	1000000

bool my_scan_function( size_t value_sz, void *value)
{
    ratio_node_t *tmp;
    tmp = (ratio_node_t *) value;
    printf("value size =[%zu] tmp.initial=[%c], tmp.ratio=[%d]\n",
        value_sz, tmp->initial, tmp->ratio);

    return(true);
}

int main()
{
	fq_logger_t *l=NULL;
	// char *ratio_string="K:10,L:10,S:20,N:60"; 
	// char *ratio_string="K:49,L:10,S:40,N:1";
	// char *ratio_string="K:0,L:0,S:0,N:100";
	char *ratio_string="K:0,L:1,S:1,N:98";
	char delimiter = ',';
	char sub_delimiter = ':';
	ratio_obj_t	*obj=NULL;

	bool tf;

	tf =  open_ratio_distributor( l, ratio_string, delimiter, sub_delimiter, &obj);
	CHECK(tf);
	
	float K=0, L=0, S=0, N=0;
	int i;
	for(i=0; i<TOTAL_CNT; i++) {
		char buf[2];
		buf[0] = obj->on_select(l, obj);
	
		/* We have to select a queue with initial */
		/* using map ( initial <-> queue ) */	
		switch(buf[0]) {
			case 'K':
				K++;
				break;
			case 'L':
				L++;
				break;
			case 'S':
				S++;
				break;
			case 'N':
				N++;
				break;
		}
	}
	printf("------------Result------------\n");
	printf("K:L:S:N = %f:%f:%f:%f\n", K, L, S, N);
	printf("K : [%f] \n", (100.0*K/(float)TOTAL_CNT));
	printf("L : [%f] \n", (100.0*L/(float)TOTAL_CNT));
	printf("S : [%f] \n", (100.0*S/(float)TOTAL_CNT));
	printf("N : [%f] \n", (100.0*N/(float)TOTAL_CNT));

	close_ratio_distributor(l, &obj);

	return 0;
}
