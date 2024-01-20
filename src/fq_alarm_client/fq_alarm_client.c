/*
** fq_alarm_client.c
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
#include "ums_common_conf.h"
#include "queue_ctl_lib.h"
#include "fq_timer.h"
#include "fq_hashobj.h"
#include "fq_tci.h"
#include "fq_cache.h"
#include "fq_conf.h"
#include "fq_gssi.h"
#include "fq_mem.h"
#include "parson.h"
#include "fq_linkedlist.h"
#include "fq_alarm_client_conf.h"
#include "hash_manage.h"
#include "fq_monitor.h"
#include "fq_scanf.h"

typedef struct _alarm_msg_items {
	char *hostname;
	char *qpath;
	char *qname;
	char *q_status_msg;
} alarm_msg_items_t;

static bool enQ_alarm_msg(fq_logger_t *l, fqueue_obj_t *alarm_q_obj, char *json_template_pathfile, alarm_msg_items_t *alarm_msg );

int main( int ac, char **av)
{
	/* logging */
	fq_logger_t *l=NULL;
	char log_pathfile[256];

	/* ums common config */
	ums_common_conf_t *cm_conf = NULL;

	/* my_config */
	fq_alarm_client_conf_t	*my_conf=NULL;

	char *errmsg=NULL;

	/* common */
	int rc;
	bool tf;

	/* dequeue objects */
	fqueue_obj_t *alarm_en_q_obj=NULL; 

	if( ac != 2 ) {
		fprintf(stderr, "Usage: $ %s [your config file] <enter>\n", av[0]);
		return 0;
	}

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
	int mypid = getpid();
	fprintf(stdout, "This program will be logging to '%s'. pid='%d'.\n", log_pathfile, mypid);

	if( FQ_IS_ERROR_LEVEL(l) ) { // in real, We do not print to STDOUT
		fclose(stdout);
	}

	rc =  OpenFileQueue(l, NULL, NULL, NULL, NULL, my_conf->qpath, my_conf->qname, &alarm_en_q_obj);
	CHECK(rc == TRUE);

	alarm_msg_items_t *alarm=NULL;
	alarm = (alarm_msg_items_t *)calloc(1, sizeof(alarm_msg_items_t) );

	char	hostname[HOST_NAME_LEN+1];
	gethostname(hostname, sizeof(hostname));

	alarm->hostname = strdup(hostname);
	alarm->qpath = strdup(my_conf->err_qpath);
	alarm->qname = strdup(my_conf->err_qname);
	alarm->q_status_msg = strdup(my_conf->err_msg);

	tf = enQ_alarm_msg(l, alarm_en_q_obj, my_conf->json_template_pathfile, alarm );

	SAFE_FREE(alarm);

	CloseFileQueue(l, &alarm_en_q_obj);

	fq_close_file_logger(&l);

	return 0;
}
static bool enQ_alarm_msg(fq_logger_t *l, fqueue_obj_t *alarm_q_obj, char *json_template_pathfile, alarm_msg_items_t *alarm_msg )
{
	// make a json message for KT or LG.
	char *json_msg = NULL;
	int  json_msg_len = 0;

	cache_t *cache_short_for_gssi=NULL;
	cache_short_for_gssi = cache_new('S', "Short term cache");

	char date[9], time[7];
	get_time(date, time);
	char req_dttm[16];
	sprintf(req_dttm, "%s%s", date, time);

	cache_update(cache_short_for_gssi, "req_dttm", req_dttm, 0);
	cache_update(cache_short_for_gssi, "hostname", alarm_msg->hostname, 0);
	cache_update(cache_short_for_gssi, "qpath", alarm_msg->qpath, 0);
	cache_update(cache_short_for_gssi, "qname", alarm_msg->qname, 0);
	cache_update(cache_short_for_gssi, "q_status_msg", alarm_msg->q_status_msg, 0);

	char str_msglen[20];
	sprintf(str_msglen, "%ld", (long)strlen(alarm_msg->q_status_msg));
	cache_update(cache_short_for_gssi, "msglen", str_msglen, 0);

	fq_log(l, FQ_LOG_DEBUG, "host=[%s], qpath=[%s], qname=[%s] msg=[%s] msglen=[%d]. ", 
		alarm_msg->hostname, alarm_msg->qpath, alarm_msg->qname, alarm_msg->q_status_msg, strlen(alarm_msg->q_status_msg));

	char  *var_data = "Gwisang|Kookmin|57|gwisang.choi@gmail.com|M";
	char	dst[8192];
	int rc;
	int file_len;
	unsigned char *json_template_string=NULL;
	
	fq_log(l, FQ_LOG_DEBUG, "json_template_pathfile: %s.",  json_template_pathfile);
	char *errmsg;
	rc = LoadFileToBuffer( json_template_pathfile, &json_template_string, &file_len,  &errmsg);
	CHECK(rc > 0);

	fq_log(l, FQ_LOG_DEBUG, "JSON Templete: '%s'", json_template_string);

	rc = gssi_proc( l, json_template_string, var_data, "", cache_short_for_gssi, '|', dst, sizeof(dst));
	CHECK(rc==0);

	fq_log(l, FQ_LOG_DEBUG, "gssi_proc() result. [%s], rc=[%d] len=[%ld]\n", dst, rc, strlen(dst));

	bool tf;
	tf  = uchar_enQ(l, dst, strlen(dst), alarm_q_obj);
	CHECK(tf);

	if( cache_short_for_gssi ) cache_free(&cache_short_for_gssi);

	SAFE_FREE(json_template_string);

	cache_free(&cache_short_for_gssi);

	return true;
}
