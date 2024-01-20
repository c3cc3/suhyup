/*
** deQ_server_sample.c
*/

#include "fqueue.h"
#include "fq_conf.h"
#include "fq_tci.h"
#include "fq_logger.h"
#include "fq_common.h"

int main(int ac, char **av)
{
	conf_obj_t	*c=NULL;
	codemap_t	*map=NULL;
	fqueue_obj_t *q=NULL;
	char	logpath[256], logname[256], loglevel[16], logpathfile[512];
	char	svc_codemap_file[256];
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
		
	/* logging */
	c->on_get_str(c, "LOG_PATH", logpath);
	c->on_get_str(c, "LOG_NAME", logname);
	c->on_get_str(c, "LOG_LEVEL", loglevel);
	sprintf(logpathfile, "%s/%s", logpath, logname);
	rc = fq_open_file_logger(&l, logpathfile, get_log_level(loglevel));
	CHECK( rc > 0 );

	/* make service codemap */
	c->on_get_str(c, "SVC_CODEMAP_FILE", svc_codemap_file);
	map = new_codemap(l, "service_code");
	rc=read_codemap(l, map, svc_codemap_file);
	CHECK( rc == 0 );

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

		buf = calloc(buffer_size, sizeof(unsigned char));
		rc = q->on_de_XA(l, q, buf, buffer_size, &l_seq, &run_time, unlink_filename);
		printf("en_de_XA('%s', '%s') rc=[%d]\n", q->path, q->qname, rc);
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
			aformat_t *a=NULL;
			char field_name[32];
			char *p = NULL;

			fq_log(l, FQ_LOG_DEBUG, "buf=[%s]\n", buf);

			a =  parsing_stream_message(l, c, map, buf, buffer_size);
			if( !a ) {
				fq_log(l, FQ_LOG_ERROR, "parsing_stream_message() error.");
				q->on_cancel_XA(l, q, l_seq, &run_time);
				goto stop;
			}

			/* get a data from deQ message. */
			sprintf(field_name, "%s", "NAME");
			p = cache_getval_unlocked(a->cache_short, field_name);

			if(p) {
				rc = q->on_commit_XA(l, q, l_seq, &run_time, unlink_filename);
				if( rc < 0 ) {
					fq_log(l, FQ_LOG_ERROR, "XA commit error.");
					SAFE_FREE(buf);
					break;
				}
				fq_log(l, FQ_LOG_INFO, "NAME=[%s]\n", p);
			}
			else {
				fq_log(l, FQ_LOG_ERROR, "Can not found [%s].\n", field_name);
				rc = q->on_cancel_XA(l, q, l_seq, &run_time);
				if( rc < 0 ) {
					fq_log(l, FQ_LOG_ERROR, "XA commit error.");
					SAFE_FREE(buf);
					break;
				}
			}
			SAFE_FREE(buf);

			if(a) free_aformat(&a);
			continue;
		}
	} /* while */

stop:

	free_codemap(l, &map);

	close_conf_obj(&c);

	rc = CloseFileQueue(l, &q);
	CHECK(rc==TRUE);

	fq_close_file_logger(&l);
}
