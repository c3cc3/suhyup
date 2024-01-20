/*
** DeleteHash.c
*/

#include "fqueue.h"
#include "fq_conf.h"
#include "fq_tci.h"
#include "fq_logger.h"
#include "fq_common.h"
#include "fq_hashobj.h"


int main(int ac, char **av)
{
	hashmap_obj_t *seq_check_hash_obj=NULL;
	conf_obj_t	*c=NULL;
	fqueue_obj_t *q=NULL;
	char	logpath[256], logname[256], loglevel[16], logpathfile[512];
	char	qpath[256], qname[256];
	int rc;
	fq_logger_t *l=NULL;
	
	if( ac != 2 ) {
		printf("$ %s [config_filename] <enter>\n", av[0]);
		printf("$ %s tst.cfg <enter>\n", av[0]);
		exit(0);
	}
	/* config */
	rc = open_conf_obj( av[1], &c);
	CHECK( rc == TRUE ); 
		
	char *hash_home = NULL;
	hash_home = getenv("FQ_HASH_HOME");
	CHECK(hash_home);

	rc = OpenHashMapFiles(l, hash_home, "history", &seq_check_hash_obj);
    CHECK(rc==TRUE);
	/* logging */
	c->on_get_str(c, "LOG_PATH", logpath);
	c->on_get_str(c, "LOG_NAME", logname);
	c->on_get_str(c, "LOG_LEVEL", loglevel);
	float run_threshold = c->on_get_float(c, "RUN_THRESHOLD");


	sprintf(logpathfile, "%s/%s", logpath, logname);
	rc = fq_open_file_logger(&l, logpathfile, get_log_level(loglevel));
	CHECK( rc > 0 );

	/* make service codemap */
	/* open file queue */
	c->on_get_str(c, "QPATH", qpath);
	c->on_get_str(c, "QNAME", qname);
	rc =  OpenFileQueue(l, NULL, NULL, NULL, NULL, qpath, qname, &q);
	CHECK(rc==TRUE);

	while(1) {
		long l_seq=0L;
		long run_time=0L;
		unsigned char *buf=NULL;
		int buffer_size = q->h_obj->h->msglen;
		char	unlink_filename[256];

		if( q->on_get_usage(l, q) < run_threshold ) {
			sleep(1);
			continue;
		}

		buf = calloc(buffer_size, sizeof(unsigned char));
		rc = q->on_de_XA(l, q, buf, buffer_size, &l_seq, &run_time, unlink_filename);
		printf("en_de_XA('%s', '%s') rc=[%d] buf=[%s]\n", q->path, q->qname, rc, buf);
		fq_log(l, FQ_LOG_DEBUG, "rc = [%d]", rc);
		if( rc == DQ_EMPTY_RETURN_VALUE ) {
			fq_log(l, FQ_LOG_DEBUG, "empty...");
			sleep(1);
			continue;
		}
		else if( rc < 0 ) {
			fq_log(l, FQ_LOG_ERROR, "file queue error.");
			SAFE_FREE(buf);
			break;
		}
		else {
			fq_log(l, FQ_LOG_DEBUG, "buf=[%s]\n", buf);

			int ret;
			ret =  DeleteHash( l, seq_check_hash_obj, buf);
			// CHECK(ret);
			rc = q->on_commit_XA(l, q, l_seq, &run_time, unlink_filename);
			if( rc < 0 ) {
				fq_log(l, FQ_LOG_ERROR, "XA commit error.");
				SAFE_FREE(buf);
				break;
			}
			SAFE_FREE(buf);
			continue;
		}
	} /* while */

stop:
	close_conf_obj(&c);

	 rc = CloseHashMapFiles(l, &seq_check_hash_obj);
    CHECK(rc==TRUE);

	rc = CloseFileQueue(l, &q);
	CHECK(rc==TRUE);

	fq_close_file_logger(&l);
}
