/*
** shmq_callback_test.c
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include "shm_queue_callback.h"
#include "fq_logger.h"

int my_CB( unsigned char *msg, int msglen, long seq)
{
	printf("msglen=[%d], seq=[%ld].\n", msglen, seq);
	usleep(10000); /* job_time */
	if(msg) free(msg); /* Don't forgot it */

	return(0);
}

int main(int ac, char **av)
{
	int rc;

#if 1
	rc = make_thread_shmq_CB_server("/tmp/shmq_callback_svr.log", FQ_LOG_TRACE_LEVEL, "/home/ums/fq/enmq", "SHM_TST", true, 1000, my_CB); 
	if(rc == false) {
		return(-1);
	}
#else
	rc = deSHMQ_CB_server("/home/logclt/enmq", "TST", "/tmp/deFQ_CB_TST.log", FQ_LOG_INFO_LEVEL, false, 0, my_CB); 
	if(rc == false) {
		return(-1);
	}
#endif
	pause();
	return(0);
}
