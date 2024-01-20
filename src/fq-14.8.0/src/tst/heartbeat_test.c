/*
** heartbeat_test.c
*/
#include "fq_logger.h"
#include "fq_common.h"
#include "fq_heartbeat.h"

volatile sig_atomic_t signal_received = 0;

void sighandler (int signum) {
        signal_received = 1;
}

int main(int ac, char **argv)
{
	int rc;
	int my_index = -1;
	fq_logger_t *l=NULL;
	char	logname[256];
	char	hostname[HOST_NAME_LEN+1];
	int time_stamp_count=1;
	pid_t	pid=getpid();

	heartbeat_obj_t *obj=NULL;

	if( ac != 2 ) {
		printf("Usage: $ %s [hostname]\n", argv[0]);
		printf("$ %s server-1 \n", argv[0]);
		printf("$ %s server-2 \n", argv[0]);
		return(0);
	}

	if (signal (SIGUSR1, sighandler) == SIG_ERR) {
          perror ("signal(SIGUSR1) failed");
    }

	sprintf(hostname, "%s", argv[1]);

	sprintf(logname, "/tmp/%s.log", "heartbeat_test");

	// rc = fq_open_file_logger(&l, logname, FQ_LOG_TRACE_LEVEL);
	rc = fq_open_file_logger(&l, logname, FQ_LOG_INFO_LEVEL);
	CHECK(rc==TRUE);

	my_index =  open_heartbeat_obj( l, hostname, "dist_1", "prog_1", "/home/pi/data" , "heartbeat.DB", "heartbeat", &obj);
	if( my_index < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "open_heartbeat_obj() failed.");
		return(0);
	}
	printf("my_index=[%d]\n", my_index);

	while(1) {
		decide_returns_t  ret;

		ret = obj->on_decide_Master_Slave(l, obj);
		printf("on_decide_Master_Slave() ret = [%d]\n", ret);

        if(ret == BE_MASTER) {
			printf("[%d] I will be a master from now.\n", pid );
			break;
        }
        else if( ret == BE_SLAVE ) {
			signal_received = 0; /* slave ignore signal */
			printf("[%d] Already Master process is running in another server. I'll be slave and check status of Master.\n", pid );
			sleep(1);
			continue;
        }
		else if( ret == ALREADY_MASTER_EXIST ) {
			printf("[%d] Already Master process is running in same server. I'll stop.\n", pid );
			exit(0);
		}
		else if( ret == ALREADY_SLAVE_EXIST ) {
			printf("[%d] Already Slave process is running in same server. I'll stop.\n", pid );
			exit(0);
		}
		else if( ret == DUP_PROCESS ) {
			printf("[%d] same process is running in same server. I'll stop.\n", pid );
			exit(0);
		}
        else {
            printf("[%d] Status error.\n", pid);
			exit(0);
        }
    }

	while(1) {
		if (signal_received) {
			printf("I recevied USR1 signal(restart command). After %d second,I'll restart myself.\n", CHANGING_WAIT_TIME+5);
			signal_received = 0;
			sleep( CHANGING_WAIT_TIME + 5);
			restart_process (argv);
			sleep(1);
		}

		time_stamp_count++; // for HANG simulation.

		printf("[%d] I'm doing with Master.\n", pid);

		if( (time_stamp_count % 80) == 0 ) {
			printf("HANG start...\n");
			sleep(30); /* HANG test */
			printf("WAKEUP ...\n");
		}

		rc = obj->on_timestamp(l, obj, MASTER_TIME_STAMP);
		if( rc == FALSE ) {
			printf("on_timestamp() failed. I'll stop.\n");
			restart_process (argv);
		}
		sleep(1);
	}

	close_heartbeat_obj(l, &obj);

	printf("all success!\n");

	return(0);
}
