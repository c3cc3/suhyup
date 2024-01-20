/*
** setQos.c
** Author: gwisang.choi@gmail.com
** Suhyup UMS Project: 2023.3 ~ 2023.11
** NNwise Corperation.
**********************************************************************
*/
#include <stdio.h>
#include <stdbool.h>
#include <libgen.h>

#include "fq_common.h"
#include "fq_delimiter_list.h"

#include "fq_codemap.h"
#include "fqueue.h"
#include "fq_file_list.h"
#include "fq_timer.h"
#include "fq_hashobj.h"
#include "fq_tci.h"
#include "fq_cache.h"
#include "fq_conf.h"
#include "fq_gssi.h"
#include "fq_mem.h"
#include "parson.h"
#include "fq_linkedlist.h"
#include "hash_manage.h"
#include "fq_monitor.h"
#include "fq_scanf.h"

bool Open_takeout_Q( fq_logger_t *l, char *qpath, char *qname, fqueue_obj_t **deq_obj)
{
	int rc;
	fqueue_obj_t *obj=NULL;

	rc =  OpenFileQueue(l, NULL, NULL, NULL, NULL, qpath, qname, &obj);
	if(rc==FALSE) return false;
	else {
		*deq_obj = obj;
		return true;
	}
}

int main( int ac, char **av)
{
	if( ac != 4 ) {
		printf("Usage: $ %s [qpath] [qname] [QoS usec] <enter> \n", av[0]);
		exit(0);
	}

	int rc;
	char *qpath = av[1];
	char *qname = av[2];
	int	QoS_usec = atoi(av[3]);

	/* logging */
	fq_logger_t *l=NULL;
	char log_pathfile[256];

	/* logging */
	sprintf(log_pathfile, "%s", "setQoS.log");

	rc = fq_open_file_logger(&l, log_pathfile, 0);
	CHECK(rc==TRUE);

	if( FQ_IS_ERROR_LEVEL(l) ) { // in real, We do not print to STDOUT
		fclose(stdout);
	}

	fqueue_obj_t *set_q_obj;
	/* Open Queue */
	bool tf;
	tf = Open_takeout_Q(l, qpath, qname, &set_q_obj);
	if(tf == false) {
		fq_log(l, FQ_LOG_ERROR, "Can't open queue(path='%s', qname='%s'. Please check queue.", qpath, qname);
		return(0);
	}
	fq_log(l, FQ_LOG_INFO, "ums takeout Queue(%s/%s) Open ok.", qpath, qname);

	rc = set_q_obj->on_set_QOS(l, set_q_obj, QoS_usec);

	printf("QoS set OK.\n");

	fq_close_file_logger(&l);
	CloseFileQueue(l, &set_q_obj);
	// close_ratio_distributor(l, &obj);

	return 0;
}
