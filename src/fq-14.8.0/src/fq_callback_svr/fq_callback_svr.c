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

#include "my_conf.h"

int my_CB( unsigned char *msg, int msglen, long seq)
{
	printf("msglen=[%d], seq=[%ld]. msg=[%s]\n", msglen, seq, msg);
	usleep(10000); /* job_time */
	if(msg) free(msg); /* Don't forgot it */

	return(0);
}

int main(int ac, char **av)
{
	int rc;

#if 0
	rc = make_thread_fq_CB_server("/tmp/fq_callback_svr.log", FQ_LOG_INFO_LEVEL, "/home/logclt/enmq", "TST", true, false, 0, my_CB); 
	if(rc == false) {
		return(-1);
	}
#endif
	bool xa_flag = true;
	char *qpath = "/home/ums/fq/enmq";
	char *qname = "TST";
	char *log = "callback_test.log";
	int log_level = FQ_LOG_INFO_LEVEL;
	bool multi_thread_flag = true;
	int max_multi_threads = 5;

	my_conf_t	*mc = NULL; // mc: my conf
	char	*errmsg = NULL;

	if( ac != 2 ) {
        printf("Usage: $ %s [your config file] <enter>\n", av[0]);
        return 0;
    }

	if(LoadConf(av[1], &mc, &errmsg) == false) {
        printf("LoadMyConf('%s') error. reason='%s'\n", av[1], errmsg);
        return(-1);
    }

	rc = deFQ_CB_server(mc->qpath, mc->qname, xa_flag, mc->log_pathfile, mc->i_log_level, mc->multi_thread_flag, mc->max_multi_threads, my_CB); 
	if(rc == false) {
		printf("failed.\n");
		return(-1);
	}
	pause();

	FreeConf(mc);

	return(0);
}
