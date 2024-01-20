/*
** subQ_distrbute.c
*/
#include "fq_common.h"
#include "fq_codemap.h"
#include "fq_manage.h"
#include "fq_linkedlist.h"
#include "fqueue.h"
#include "fq_file_list.h"

#include "subQ_dist_conf.h"
#include "ums_common_conf.h"
#include "queue_ctl_lib.h"
#include "fq_manage.h"
#include "fq_cache.h"
#include "fq_tci.h"
#include "carrier_msg_lib.h"
#include "fq_timer.h"

int main(int ac, char **av)
{
	/* touch timer */
	double passed_time;

	/* ums common config */
	ums_common_conf_t *cm_conf = NULL;

	/* my_config */
	subQ_dist_conf_t *my_conf=NULL;
	char *errmsg=NULL;

	/* logging */
	fq_logger_t *l=NULL;
	char log_pathfile[256];

	/* common */
	int rc;

	/* dequeue object */
	fqueue_obj_t *deq_obj=NULL;
	
	if( ac != 2 ) {
		printf("Usage: $ %s [your config file] <enter>\n", av[0]);
		return 0;
	}

	if(Load_ums_common_conf(&cm_conf, &errmsg) == false) {
		printf("Load_ums_common_conf() error. reason='%s'\n", errmsg);
		return(-1);
	}

	if(LoadConf(av[1], &my_conf, &errmsg) == false) {
		printf("LoadMyConf('%s') error. reason='%s'\n", av[1], errmsg);
		return(-1);
	}

	sprintf(log_pathfile, "%s/%s", cm_conf->ums_common_log_path, my_conf->log_filename); 
	rc = fq_open_file_logger(&l, log_pathfile, my_conf->i_log_level);
	CHECK(rc==TRUE);

	bool tf;
	tf = Open_takeout_Q(l, my_conf, &deq_obj);
	CHECK(tf);

	linkedlist_t *subQ_obj_ll = NULL;
	subQ_obj_ll = linkedlist_new("subQ_obj_list");
	tf = Make_sub_Q_obj_list( l, my_conf, subQ_obj_ll);
	CHECK(tf);
	CHECK(subQ_obj_ll);

	qformat_t *q=NULL;
	q = new_qformat(my_conf->qformat_file, &errmsg);
	if( !q ) {
		fq_log(l, FQ_LOG_ERROR, "new_qformat('%s') error. reason='%s'.", my_conf->qformat_file, errmsg);
		return(-1);
	}

	rc=read_qformat( q, my_conf->qformat_file, &errmsg);
	if(rc<0) {
		fq_log(l, FQ_LOG_ERROR, "read_qformat('%s') error. reason='%s'.", my_conf->qformat_file, errmsg);
		return(-1);
	}

	fqcache_t  *cache=NULL;
	cache = cache_new('Q', "SEND");
	CHECK(cache);


	pid_t	mypid = getpid();

	if( my_conf->fqpm_use_flag == true ) {
        tf = check_duplicate_me_on_fq_framework( l, av[1]);
        CHECK(tf);
	
		tf = regist_me_on_fq_framework(l, NULL, mypid, true, 60 ); // NULL : use env=FQ_DATA_PATH
		CHECK(tf);
	}
	
	fprintf(stdout, "This program will be logging to '%s'. pid='%d'.\n", log_pathfile, mypid);

	fq_log(l, FQ_LOG_EMERG, "Program(%s) start.\n", av[0]);

	start_timer();
	while(1) {
		int rc;
		ums_msg_t	*ums_msg=NULL;
		char	*unlink_filename = NULL;
		long	l_seq;
		rc = takeout_ums_msg_from_Q(l, my_conf, deq_obj, &ums_msg, &unlink_filename, &l_seq);
		if(rc < 0) { /* fatal error */
			break;
		} 
		else if( rc == 0 ) { /* queue is empty */
			passed_time = end_timer();
			if( passed_time > 5.0 ) {
				fq_log(l, FQ_LOG_DEBUG, "empty: I am waiting for a message to arrive in the queue('%s').", deq_obj->qname);
				if( my_conf->fqpm_use_flag == true ) {
					touch_me_on_fq_framework( l, NULL, mypid );
				}
				start_timer();
			}
			sleep(1);
			continue;
		}
		fq_log(l, FQ_LOG_DEBUG, "ums_msg->svc_code=[%s].", ums_msg->svc_code);

		fqueue_obj_t *subQ=NULL;
		while(1) {
			subQ = Choose_a_subQ(l, subQ_obj_ll);
			if(subQ == NULL) {
				fq_log(l, FQ_LOG_EMERG, "Failed to select subqueue.");
				usleep(1000);
			}
			break;
		}

		bool tf;
		tf = Make_a_carrier_msg_with_umsmsg(l, ums_msg, q, cache);
		CHECK(tf);

		rc = put_carrier_msg_to_subQ(l, q->data, q->datalen+1, q->datalen, subQ);
		if( rc < 0 ){
			cancel_Q(l, deq_obj, unlink_filename, l_seq);
			break;
		}
		else  {
			commit_Q(l, deq_obj, unlink_filename, l_seq);
		}

		passed_time = end_timer();
		if( passed_time > 3.0 ) {
			if( my_conf->fqpm_use_flag == true ) {
				touch_me_on_fq_framework( l, NULL, mypid ); 
			}
			start_timer();
		}
		
		if(ums_msg) free(ums_msg);
	}

	if(q) free_qformat(&q);

	fq_log(l, FQ_LOG_EMERG, "processing rusult is failure. Good bye.");

	return 0;
}
