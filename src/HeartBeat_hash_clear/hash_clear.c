/*
** hash_clear.c
*/
#include <stdio.h>
#include <stdbool.h>
#include <libgen.h>
#include <sys/types.h>
#include <dirent.h>


#include "fq_common.h"
#include "fq_delimiter_list.h"

#include "fq_codemap.h"
#include "fqueue.h"
#include "fq_file_list.h"
#include "ums_common_conf.h"
#include "fq_timer.h"
#include "fq_hashobj.h"
#include "fq_tci.h"
#include "fq_cache.h"
#include "fq_conf.h"
#include "fq_gssi.h"
#include "fq_mem.h"
#include "parson.h"
#include "ratio_dist_conf.h"
#include "hash_manage.h"
#include "fq_monitor.h"
#include "main_subfunc.h"

int is_alive_pid( int mypid );

int main( int ac, char **av)
{
	/* ums common config */
	ums_common_conf_t *cm_conf = NULL;

	/* my_config */
	ratio_dist_conf_t	*my_conf=NULL;
	char *errmsg=NULL;

	/* logging */
	fq_logger_t *l=NULL;
	char log_pathfile[256];

	/* common */
	int rc;
	bool tf;

	hashmap_obj_t *seq_check_hash_obj=NULL;
	hashmap_obj_t *mon_svr_hash_obj=NULL;

	if( ac != 2 ) {
		fprintf(stderr, "Usage: $ %s [your config file] <enter>\n", av[0]);
		return 0;
	}

	fprintf(stdout, "\n\nProcess started.\n");

	/* Loading common configuration */
	if(Load_ums_common_conf(&cm_conf, &errmsg) == false) {
		fprintf(stderr, "Load_ums_common_conf() error. reason='%s'\n", errmsg);
		return(-1);
	}	
	fprintf(stdout, "ums common conf loading ok.\n");

	/* Loading my configuration */
	if(LoadConf(av[1], &my_conf, &errmsg) == false) {
		fprintf(stderr, "LoadMyConf('%s') error. reason='%s'\n", av[1], errmsg);
		return(-1);
	}
	fprintf(stdout, "ums my conf loading ok.\n");

	/* logging */
	sprintf(log_pathfile, "%s/%s", cm_conf->ums_common_log_path, my_conf->log_filename);
	rc = fq_open_file_logger(&l, log_pathfile, my_conf->i_log_level);
	CHECK(rc==TRUE);
	fprintf(stdout, "log file: '%s'\n", log_pathfile);
	
	/* Open Hashmap */
	/* General purpose hashmap for UMS */
	/* For creating, Use /utl/ManageHashMap command */
	/* We do not use it now. */
	hashmap_obj_t *hash_obj=NULL;
	rc = OpenHashMapFiles(l, my_conf->heartbeat_hash_map_path, my_conf->heartbeat_hash_map_name, &hash_obj);
	if(rc == FALSE) {
		fq_log(l, FQ_LOG_ERROR, "Can't open  hashmap(path='%s', name='%s'). Please check hashmap.", 
				my_conf->heartbeat_hash_map_path, my_conf->heartbeat_hash_map_path);
		return(0);
	}
	fq_log(l, FQ_LOG_INFO, "ums HashMap(path='%s', name='%s') open ok.", my_conf->heartbeat_hash_map_path, my_conf->heartbeat_hash_map_path);

	fq_log(l, FQ_LOG_EMERG, "Program(%s) start.", av[0]);

	printf("========================= %s( length = %d, elements=[%d)) ================================\n", hash_obj->name, hash_obj->h->h->table_length, hash_obj->h->h->curr_elements);
	int data_count = 0;
	int index;
	for(index=0; index<hash_obj->h->h->table_length; index++) {

		char  *key=NULL;
		unsigned char *value=NULL;
		unsigned char *p=NULL;

		key = calloc(hash_obj->h->h->max_key_length+1, sizeof(char));
		value = calloc(hash_obj->h->h->max_data_length+1, sizeof(unsigned char));

		rc = hash_obj->on_get_by_index(l, hash_obj, index, key, value);
		if( rc == MAP_MISSING ) {
			SAFE_FREE(key);
			SAFE_FREE(value);
			continue;
		}
		else {
			p = (unsigned char *)value;
			data_count++;
			printf( "key=[%s], value=[%s]\n", key, p);

#if 1
			int kill_pid = atoi(key);
			//if( is_alive_pid_in_general(pid) == 1 ) {
			int rc;
			rc = is_alive_pid_in_Linux(kill_pid);
			if( rc  == 1 ) {
				fprintf(stdout,  "Alive process.('%d').\n", kill_pid);
				fq_log(l, FQ_LOG_INFO, "Alive process.('%d').", kill_pid);
				continue;
			}
			else {
				fprintf(stdout, "dead process.('%d').\n", kill_pid);
				fq_log(l, FQ_LOG_INFO, "We delete it.('%d').", kill_pid);
			}
#else
			time_t stamp_bintime = (time_t)atol(value);
			if( stamp_bintime + 60 < time(0)) {
				fprintf(stdout, "Old process, We delete it.\n");
				rc = DeleteHash( l, hash_obj, key);
				CHECK(rc==TRUE);
			}
			else {
				fprintf(stdout, "Recent process, We skip it.\n");
			}
#endif
		}
	}
	printf("record count=[%d]\n", data_count);

	rc = CloseHashMapFiles(l, &hash_obj);
    CHECK(rc==TRUE);

	fq_close_file_logger(&l);

	// close_ratio_distributor(l, &obj);

	return 0;
}
