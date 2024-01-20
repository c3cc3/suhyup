/*
** enQ_server_sample.c
*/

#include "fqueue.h"
#include "fq_cache.h"
#include "fq_conf.h"
#include "fq_tci.h"
#include "fq_logger.h"
#include "fq_common.h"

int make_stream_message(fq_logger_t *l, conf_obj_t *cfg, codemap_t *svc_code_map,  char *svc_code, char *postdata, cache_t *cache_short, unsigned char **buf);


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
	int usleep_msec = 1000; /* default */
	int tps = 1; /* default */
	
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

	/* enQ speed control */
	tps = c->on_get_int(c, "TPS");


	while(1) {
		long l_seq=0L;
		long run_time=0L;
		unsigned char *buf=NULL;
		int buffer_size = q->h_obj->h->msglen;
		int stream_len;
		cache_t *cache_short=0x00;
		char	in_service_code[5], in_name[11], in_age[4];

		cache_short = cache_new('S', "Short term cache");

#if 0
		getprompt("What is a servcice code:(04)", in_service_code, 4);
		getprompt("What is a name         :(10)", in_name, 10);
		getprompt("What is a age          :(03)", in_age, 3);

		cache_update(cache_short, "NAME", in_name, 0);
		cache_update(cache_short, "AGE", in_age, 0);
#else
		sprintf(in_service_code, "%s", "0001");
		cache_update(cache_short, "NAME", "GWisang", 0);
		cache_update(cache_short, "AGE", "51", 0);
#endif

		/* 0001 -> service code */
		stream_len = make_stream_message(l, c, map, in_service_code, NULL, cache_short, &buf);

		if( stream_len <=0 ) {
			fq_log(l, FQ_LOG_ERROR, "make_stream_message() error!!");
			break;
		}

		rc =  q->on_en(l, q, EN_NORMAL_MODE, buf, stream_len, stream_len, &l_seq, &run_time );

		fq_log(l, FQ_LOG_DEBUG, "on_en('%s', '%s'), rc = [%d]", q->path, q->qname, rc);

		printf("on_en('%s', '%s'), rc = [%d]\n", q->path, q->qname, rc);

		if( rc == EQ_FULL_RETURN_VALUE ) {
			fq_log(l, FQ_LOG_DEBUG, "full...");
			sleep(1);
			continue;
		}
		else if( rc < 0 ) {
			fq_log(l, FQ_LOG_ERROR, "file queue error.");
			SAFE_FREE(buf);
			break;
		}
		else {
			fq_log(l, FQ_LOG_DEBUG, "on_en('%s', '%s') ok. rc=[%d]\n", q->path, q->qname, rc);
			cache_free( &cache_short );
			if( buf ) free(buf);
	
			usleep_msec = 1000000/tps;
			usleep(usleep_msec);
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

/*
** Warning : You must free a return pointer(buf) after using.
*/
int make_stream_message(fq_logger_t *l, conf_obj_t *cfg, codemap_t *svc_code_map,  char *svc_code, char *postdata, cache_t *cache_short, unsigned char **buf)
{
	int stream_len=0;
	unsigned char *packet=NULL;
	qformat_t *q=NULL;
	char qformat_path[256];
	char qformat_filename[256];
	char qformat_path_file[512];
	char *errmsg=NULL;
	int rc;
	int len;

	if( !svc_code_map ) {
		fq_log(l, FQ_LOG_ERROR, "There is no a value in svc_code_map parameter.");
		goto stop;
	}

	/* get a qformat filename from service_code_map with service_code */
	rc = get_codeval(l, svc_code_map, svc_code, qformat_filename);
	if( rc != 0 ) {
        fq_log(l, FQ_LOG_ERROR, "service code error.! Can't find a aformat filename in service_code_map.");
        goto stop;
    }

	fq_log(l, FQ_LOG_DEBUG, "qformat_filename = [%s]\n", qformat_filename );

	
	cfg->on_get_str(cfg, "SVC_QFORMAT_PATH", qformat_path);

	sprintf(qformat_path_file, "%s/%s", qformat_path, qformat_filename);
	q = new_qformat(qformat_path_file, &errmsg);
	if( !q ) {
		fq_log(l, FQ_LOG_ERROR, "new_qformat('%s') error. reason=[%s].", qformat_path_file, errmsg);
        goto stop;
    }

	rc=read_qformat( q, qformat_path_file, &errmsg);
	if( rc < 0 ) {
        fq_log(l, FQ_LOG_ERROR, "read_qformat('%s') error. reason=[%s].", qformat_path_file, errmsg);
        goto stop;
    }

	fill_qformat(q, postdata, cache_short, NULL);

	len = assoc_qformat(q, NULL);
	if( len <= 0 ) {
		fq_log(l, FQ_LOG_ERROR, "assoc_qformat('%s') error.", qformat_path_file);
		goto stop;
	}

	packet = calloc(len+1, sizeof(unsigned char));

	memcpy(packet, q->data, q->datalen);

	*buf = packet;

	len = q->datalen;

	if(q) free_qformat(&q);

	return(len);

stop:
	SAFE_FREE(errmsg);

	if(q) free_qformat(&q);

	return(-1);
}
