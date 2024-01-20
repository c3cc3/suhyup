/*
 * deFQ_heartbeat.c
 * sample
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include "fqueue.h"
#include "fq_common.h"
#include "fq_monitor.h"

#include "fq_heartbeat.h"

#define BUFFER_SIZE 65000
#define QPATH "/home/sg/data"
#define QNAME "TST"
#define LOG_FILE_NAME "deFQ.log"


char **ARGV;

volatile sig_atomic_t signal_received = 0;

void sighandler (int signum) {
		printf("I received restart signal.\n");
        signal_received = 1;
        // restart_process(ARGV);
}

int main(int ac, char **av)  
{  
	int rc;
	fq_logger_t *l=NULL;
	fqueue_obj_t *obj=NULL;
	char *path=QPATH;
    char *qname=QNAME;
    char *logname = LOG_FILE_NAME;
	int	 buffer_size;
	char *p=NULL;
	monitor_obj_t   *mon_obj=NULL;
	heartbeat_obj_t *hb_obj=NULL;

	ARGV = av;
	int my_index;
	char *hostname=NULL;
	char *distname=NULL;
	char *progname=NULL;
	pid_t	pid = getpid();

	if( ac != 4 ) {
		printf("Usage: $ %s [hostname] [distname] [progname] \n", av[0]);
		printf("ex)    $ %s server-1 dist1 prog1_dist1_test \n", av[0]);
		printf("ex)    $ %s server-2 dist1 prog1_dist1_test \n", av[0]);
		return(0);
	}

	hostname = av[1];
	distname = av[2];
	progname = av[3];

	// if (signal (SIGUSR1, sighandler) == SIG_ERR) {
	if (signal (SIGHUP, sighandler) == SIG_ERR) {
          // perror ("signal(SIGUSR1) failed");
          perror ("signal(SIGHUP) failed");
    }

	my_index =  open_heartbeat_obj( l, hostname, distname, progname, "/tmp" , "heartbeat.DB", "heartbeat", &hb_obj);
	if( my_index < 0 ) {
		printf("open_heartbeat_obj() failed.\n");
        return(0);
    }
	while(1) {
		decide_returns_t  ret;

		ret = hb_obj->on_decide_Master_Slave(l, hb_obj);
		printf("on_decide_Master_Slave() ret = [%d]\n", ret);

        if(ret == BE_MASTER) {
			printf("[%d] I will be a master from now.\n", pid );
			break;
        }
        else if( ret == BE_SLAVE ) {
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

	p = getenv("LD_LIBRARY_PATH");
	printf("Current LD_LIBRARY_PATH=[%s]\n\n", p);
	p = NULL;

	p = getenv("FQ_DATA_HOME");
	printf("Current FQ_DATA_HOME=[%s]\n\n", p);

	if(p ) {
		path = strdup(p);
	}

    printf("Current Program settings:\n");
    printf("\t- recv buffer  size: [%d]\n", BUFFER_SIZE);
    printf("\t- path             : [%s]\n", path);
    printf("\t- qname            : [%s]\n", QNAME);
    printf("\t- log file         : [%s]\n", LOG_FILE_NAME);

	printf("\n Are you sure? Press anykey for continue or Press Ctrl+'C' to stop\n");
    // getc(stdin);

	rc = fq_open_file_logger(&l, logname, FQ_LOG_ERROR_LEVEL);
	CHECK( rc > 0 );

	rc =  OpenFileQueue(l, ARGV, hostname, distname, progname, path, qname, &obj);
	CHECK(rc==TRUE);

	rc =  open_monitor_obj(l,  &mon_obj);
	CHECK(rc==TRUE);


	/* buffer_size = obj->h_obj->h->msglen-4+1; */ /* queue message size */
	buffer_size = BUFFER_SIZE;

	printf("File Queue msglen is [%ld].\n", obj->h_obj->h->msglen);

	while(1) {
		long l_seq=0L;
		unsigned char *buf=NULL;
		long run_time=0L;

		buf = malloc(buffer_size);
		memset(buf, 0x00, sizeof(buffer_size));

		rc = obj->on_de(l, obj, buf, buffer_size, &l_seq, &run_time);
		mon_obj->on_send_action_status(l, progname, obj->path, obj->qname, obj->h_obj->h->desc, FQ_DE_ACTION, mon_obj);

		if( rc == DQ_EMPTY_RETURN_VALUE ) {
			printf("[%d]: empty..\n", pid);
			SAFE_FREE(buf);
			usleep(100000);
		}
		else if( rc < 0 ) {
			if( rc == MANAGER_STOP_RETURN_VALUE ) {
				printf("Manager asked to stop a processing.\n");
				break;
			}
			printf("error..\n");
			break;
		}
		else {
			printf("rc=[%d] seq=[%ld]-th\n", rc, l_seq );
			SAFE_FREE(buf);
		}
	}

	rc = close_monitor_obj(l, &mon_obj);
	CHECK(rc==TRUE);

	rc = CloseFileQueue(l, &obj);
	CHECK(rc==TRUE);

	fq_close_file_logger(&l);

	return(rc);
}
