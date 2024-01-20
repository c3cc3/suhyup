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

tcp_client_obj_t *tcpobj=NULL;
char    *skey=NULL;
my_conf_t	*mc = NULL; // mc: my conf
fq_logger_t *l=NULL;

int my_CB( unsigned char *msg, int msglen, long seq)
{
	char    result;
	int try_count;
	int rc;
	printf("msglen=[%d], seq=[%ld]. msg=[%s]\n", msglen, seq, msg);

retry_en:
	rc =  FQ_put(l, tcpobj, (unsigned char *)msg, msglen, skey, mc->remote_en_qpath, mc->remote_en_qname, &result);
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

	fq_log(l, FQ_LOG_INFO, "Connecting to FQ_server.....\n");

	int max_try_count = 100;
	rc = FQ_link(l, max_try_count, mc->handler_ip1, mc->handler_ip2, mc->handler_port, &tcpobj, mc->remote_en_qpath, mc->remote_en_qname, &result, &skey );
	if( rc < 0 || result != 'Y') {
		fprintf(stderr, "failed to connect at the FQ_server.\n");
		return(0);
	}
	fprintf(stdout, "FQ_link OK. skey=[%s]\n", skey);

	rc = deFQ_CB_server(mc->de_qpath, mc->de_qname, xa_flag, mc->log_pathfile, mc->i_log_level, mc->multi_thread_flag, mc->max_multi_threads, my_CB); 
	if(rc == false) {
		printf("failed.\n");
		return(-1);
	}
	pause();

	SAFE_FREE(skey);
	rc = close_tcp_client_obj(l, &tcpobj);
	CHECK(rc==TRUE);

	fq_close_file_logger(&l);
	FreeConf(mc);

	return(0);
}
