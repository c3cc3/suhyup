#include "fqueue.h"
#include "fq_manage.h"
#include "fq_linkedlist.h"
#include "shm_queue.h"
#include "fq_param.h"
#include "fq_config.h"
#include "fq_logger.h"
#include "fq_common.h"

fq_logger_t *l;
char *snapshot_folder;
FILE *fp;

static bool my_CB_function( size_t value_sz, void *value)
{
	queue_obj_node_t *tmp;
	tmp = (queue_obj_node_t *) value;
	fqueue_obj_t *qobj=tmp->obj;

	printf("[%s][%s] : [%ld][%ld]-[%f]\n", qobj->path, qobj->qname, qobj->h_obj->h->de_sum, qobj->h_obj->h->de_sum, qobj->on_get_usage(l, qobj));

	CloseFileQueue(l, &qobj);
	if(tmp) free(tmp);

	return true;
}

int main(int ac, char **av)
{
	int rc;
	int i_loglevel;
	int snapshot_cycle_sec;
	char *logname=NULL;
	char *loglevel=NULL;
	char    buffer[128];
	char snapshot_filename[256];
	char opened_date[9], now_date[9];
	char time[7];

	if(ac != 2) {
		printf("Usage: %s [config_file] <ENTER>\n", av[0]);
		return(0);
	}

	char *config_file = av[1];
	config_t *t = new_config(config_file);
	if(load_param(t, config_file) < 0 ) {
		fprintf(stderr, "load_param() error.\n");
		return(0);
	}
	
	get_config(t, "LOG_NAME", buffer);
	logname = strdup(buffer);

	get_config(t, "LOG_LEVEL", buffer);
	loglevel = strdup(buffer);

	get_config(t, "SNAPSHOT_CYCLE_SEC", buffer);
	snapshot_cycle_sec = atoi(buffer);

	get_config(t, "SNAPSHOT_FOLDER", buffer);
	snapshot_folder = strdup(buffer);

	i_loglevel = get_log_level(loglevel);
	rc = fq_open_file_logger(&l, logname, i_loglevel);
	CHECK( rc > 0 );

re_fopen:
	get_time(opened_date, time);
	sprintf( snapshot_filename, "%s/%s.snapshot", snapshot_folder, opened_date);
	fp = fopen( snapshot_filename, "w");
	if( fp == NULL ) {
		fq_log(l, FQ_LOG_ERROR, "fopen('%s') error.", snapshot_filename);
		return(0);
	}

	while(1) {
		get_time( now_date, time);
		if( strcmp(opened_date, now_date) != 0 ) {
			printf("re open.\n");
			fclose(fp);
			goto re_fopen;
		}
		all_queue_scan_CB_n_times(1, 0, my_CB_function );

		sleep(snapshot_cycle_sec);
	}

	free_config(&t);
	fq_close_file_logger(&l);

	return 0;
}
