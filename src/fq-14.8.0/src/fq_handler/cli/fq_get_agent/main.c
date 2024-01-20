/*
** fq_callback_test.c
** memory leak checking -> passed (2020/07/20)
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include "fqueue_callback.h"
#include "fq_logger.h"
#include "fq_tcp_client_obj.h"
#include "fq_protocol.h"

#include "my_conf.h"

int uchar_enQ(fq_logger_t *l, unsigned char *data, size_t data_len, fqueue_obj_t *obj);

tcp_client_obj_t *tcpobj=NULL;
char    *skey=NULL;
my_conf_t	*mc = NULL; // mc: my conf
fq_logger_t *l=NULL;
fqueue_obj_t *en_qobj=NULL;

int my_CB( unsigned char *msg, int msglen, long seq)
{
	char    result;
	int try_count;
	int rc;
	printf("msglen=[%d], seq=[%ld]. msg=[%s]\n", msglen, seq, msg);

retry_en:
	rc =  FQ_put(l, tcpobj, (unsigned char *)msg, msglen, skey, mc->en_qpath, mc->en_qname, &result);
	if( rc < 0 ) {
		fprintf( stderr, "en queue error. rc=[%d]. Processing will be stop. See log for detail.\n", rc);
		SAFE_FREE(msg);
		exit(0);
	}
	else if( result == 'F') { /* full */
		try_count++;
		fprintf(stderr, "queue is full. ..retry [%d]-th...\\n", try_count);
		usleep(1000000);
		goto retry_en;
	}
	else { /* success */
		fprintf(stdout, "put OK. rc=[%d]\n", rc);
	}

	if(msg) free(msg); /* Don't forgot it */

	return(0);
}

int main(int ac, char **av)
{
	bool xa_flag = true;
	char *log = "callback_test.log";
	int log_level = FQ_LOG_INFO_LEVEL;
	bool multi_thread_flag = true;
	int max_multi_threads = 5;

	char	*errmsg = NULL;
	int	rc;
	char    result;

	if( ac != 2 ) {
        printf("Usage: $ %s [your config file] <enter>\n", av[0]);
        return 0;
    }

	if(LoadConf(av[1], &mc, &errmsg) == false) {
        printf("LoadMyConf('%s') error. reason='%s'\n", av[1], errmsg);
        return(-1);
    }

	rc = fq_open_file_logger(&l, mc->log_pathfile, mc->i_log_level);
	CHECK( rc > 0 );
	fq_log(l, FQ_LOG_DEBUG, "log open OK.");


	rc =  OpenFileQueue(l, NULL, NULL, NULL, NULL, mc->en_qpath, mc->en_qname, &en_qobj);
	CHECK(rc == TRUE);

	fq_log(l, FQ_LOG_INFO, "Connecting to FQ_server.....\n");
	rc = FQ_link(l, 100, mc->handler_ip1, mc->handler_ip2, mc->handler_port, &tcpobj, mc->remote_de_qpath, mc->remote_de_qname, &result, &skey );
	if( rc < 0 || result != 'Y') {
		fprintf(stderr, "failed to connect at the FQ_server.\n");
		return(0);
	}

	while(1) {
		int rc;
		unsigned char *qdata=NULL;
		int qdata_len=0;

		rc = FQ_get(l, tcpobj, skey, mc->remote_de_qpath, mc->remote_de_qname, &qdata, &qdata_len, &result);
		if( rc < 0 ) {
			fq_log(l, FQ_LOG_ERROR, "error. rc=[%d].", rc);
			break;
		}
		else if( result == 'E' ) {
			printf("%s\n", "empty");
			SAFE_FREE(qdata);
			usleep(1000000); /* 1 sec  */
			continue;
		}
		else {
			int rc_len;
			rc_len = uchar_enQ(l, qdata, qdata_len, en_qobj);
			
			if( rc_len < 0 ) {
				fq_log(l, FQ_LOG_ERROR, "uchar_enQ() error.");
				break;
			}
			else {
				fq_log(l, FQ_LOG_INFO, "enQ. ok. rc_len=[%d]", rc_len);
			}
			SAFE_FREE(qdata);
		}
	} /* while end */


	SAFE_FREE(skey);
	rc = close_tcp_client_obj(l, &tcpobj);
	CHECK(rc==TRUE);

	fq_close_file_logger(&l);
	FreeConf(mc);

	return(0);
}

int uchar_enQ(fq_logger_t *l, unsigned char *data, size_t data_len, fqueue_obj_t *obj)
{
	int rc;
	long l_seq=0L;
    long run_time=0L;

	while(1) {
		rc =  obj->on_en(l, obj, EN_NORMAL_MODE, (unsigned char *)data, data_len+1, data_len, &l_seq, &run_time );
		// rc =  obj->on_en(l, obj, EN_CIRCULAR_MODE, (unsigned char *)data, data_len+1, data_len, &l_seq, &run_time );
		if( rc == EQ_FULL_RETURN_VALUE )  {
			fq_log(l, FQ_LOG_EMERG, "Queue('%s', '%s') is full.\n", obj->path, obj->qname);
			usleep(100000);
			continue;
		}
		else if( rc < 0 ) { 
			if( rc == MANAGER_STOP_RETURN_VALUE ) { // -99
				printf("Manager stop!!! rc=[%d]\n", rc);
				fq_log(l, FQ_LOG_EMERG, "[%s] queue is Manager stop.", obj->qname);
				sleep(1);
				continue;
			}
			fq_log(l, FQ_LOG_ERROR, "Queue('%s', '%s'): on_en() error.\n", obj->path, obj->qname);
			return(rc);
		}
		else {
			fq_log(l, FQ_LOG_INFO, "enqueued ums_msg len(rc) = [%d].", rc);
			break;
		}
	}
	return(rc);
}
