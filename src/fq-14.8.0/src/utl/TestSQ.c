/*
 * TestSQ.c
 */
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>

#define TESTSQ_C_VERSION "1.0.1"

/* last update: 2013/05/21 */
/* 2013/05/23: change: line 216, from CHECK( rc < 0 ) to CHECK( rc == TRUE );  */
/* 2013/10/02: found a bug. memory leak */

#include "fqueue.h"
#include "fq_common.h"
#include "fq_timer.h"
#include "shm_queue.h"

extern struct timeval tp1, tp2;
long    count=0;
double  e;

typedef struct _input_param_t {
    char progname[64];
    char *logname;
    char *path;
    char *qname;
    char *action; /* EN_ACTION, DE_ACTION */
    char *log_level; /* trace, debug, info, error, emerg  */
    int  msglen;
    int  bufsize;
    long usleep_time;
    int  print_flag; /* 1 or 0 */
} input_param_t;

void enSQ(fq_logger_t *l, fqueue_obj_t *obj, input_param_t in_params);
void deSQ( fq_logger_t *l, fqueue_obj_t *obj, input_param_t in_params );

void print_help(char *p)
{
	char    *data_home=NULL;

	printf("Compiled on %s %s\n", __TIME__, __DATE__);

    data_home = getenv("FQ_DATA_HOME");
    if( !data_home ) {
        data_home = getenv("HOME");
        if( !data_home ) {
            data_home = strdup("/data");
        }
    }

	printf("\n\nUsage  : $ %s [-V] [-h] [-p path] [-q qname] [-l logname] [-a action] [-b buffer] [-m msglen] [-u usleep] [-P Print] [-o loglevel] <enter>\n", p);
	printf("\t	-V: version \n");
	printf("\t	-h: help \n");
	printf("\t	-l: logfilename \n");
	printf("\t	-p: full path \n");
	printf("\t	-q: qname \n");
	printf("\t	-a: action_flag ( EN | DE )\n");
	printf("\t	-m: msglen ( for enQ )\n"); 
	printf("\t	-b: buffer size ( for deQ )\n");
	printf("\t	-u: usleep time ( 1000000 -> 1√  )\n");
	printf("\t	-P: Print msg to screen ( 0 | 1 )\n");
	printf("\t	-o: log level ( trace|debug|info|error|emerg )\n");
	printf("enQ example: $ %s -l /tmp/%s.en.log -p %s -q STST -a EN -m 2044 -u 1000 -P 0 -o error <enter>\n", p, p, data_home);
	printf("enQ example: $ %s -l /tmp/%s.en.log -p %s -q STST -a EN -m 2044 -u 1000 -P 1 -o debug <enter>\n", p, p, data_home);
	printf("deQ example: $ %s -l /tmp/%s.de.log -p %s -q STST -a DE -b 2048 -u 1000 -P 0 -o error <enter>\n", p, p, data_home);
	printf("deQ example: $ %s -l /tmp/%s.de.log -p %s -q STST -a DE -b 2048 -u 1000 -P 1 -o error <enter>\n", p, p, data_home);
	printf("deQ example: $ %s -l /tmp/%s.de.log -p %s -q STST -a DE -b 2048 -u 1000 -P 0 -o debug <enter>\n", p, p, data_home);
	printf("deQ example: $ %s -l /tmp/%s.de.log -p %s -q STST -a DE -b 2048 -u 1000 -P 1 -o debug <enter>\n", p, p, data_home);
	return;
}
void print_version(char *p)
{
    printf("\nversion: %s.\n\n", TESTSQ_C_VERSION);
    return;
}

int main(int ac, char **av)  
{  
	int rc;
	int     ch;
	fq_logger_t *l=NULL;
	fqueue_obj_t *obj=NULL;
	input_param_t   in_params;

	printf("Compiled on %s %s\n", __TIME__, __DATE__);

	if( ac < 2 ) {
		print_help(av[0]);
        return(0);
    }

	in_params.logname = NULL;
	in_params.path = NULL;
	in_params.qname = NULL;
	in_params.action = NULL;
	in_params.log_level = NULL;
	in_params.msglen = -1;
	in_params.bufsize = -1;
	in_params.usleep_time = 0;	
	in_params.print_flag = 0;	
	
	while(( ch = getopt(ac, av, "VvHh:l:p:q:a:m:b:u:P:o:")) != -1) {
		switch(ch) {
			case 'H':
            case 'h':
                print_help(av[0]);
                return(0);
            case 'V':
            case 'v':
                print_version(av[0]);
                return(0);
            case 'l':
                in_params.logname = strdup(optarg);
				break;
			case 'p':
				in_params.path = strdup(optarg);
				break;
			case 'q':
				in_params.qname = strdup(optarg);
				break;
			case 'a':
				in_params.action = strdup(optarg);
				break;
			case 'm':
				in_params.msglen = atoi(optarg);
				break;
			case 'b':
				in_params.bufsize = atoi(optarg);
				break;
			case 'u':
				in_params.usleep_time = atol(optarg);
				break;
			case 'P':
				in_params.print_flag = atoi(optarg);
				break;
			case 'o':
				in_params.log_level = strdup(optarg);
				break;
			default:
				printf("ch=[%c]\n", ch);
				print_help(av[0]);
                return(0);
		}
	}

	if( !in_params.logname || !in_params.path || !in_params.qname || !in_params.action || !in_params.log_level) {
		fprintf(stderr, "ERROR: logname[%s], path[%s], qname[%s], action[%s] log_level[%s] are mandatory.\n", 
			in_params.logname, in_params.path, in_params.qname, in_params.action, in_params.log_level );
		print_help(av[0]);
		return(-1);
	}

	rc = fq_open_file_logger(&l, in_params.logname, FQ_LOG_ERROR_LEVEL);
	CHECK( rc > 0 );


	rc =  OpenShmQueue(l, in_params.path, in_params.qname, &obj);
	CHECK(rc==TRUE);

	/*buffer_size = obj->h_obj->h->msglen-4+1; */
	printf("Shm Queue msglen is [%ld]\n", obj->h_obj->h->msglen);

	if( strncmp(in_params.action, "EN", 2) == 0 ) {
		enSQ( l, obj, in_params);
	}
	else if( strncmp(in_params.action, "DE", 2) == 0 )  {
		deSQ( l, obj, in_params);
	}
	else {
        print_help(av[0]);
    }

	rc = CloseShmQueue(l, &obj);
	CHECK(rc==TRUE);

	fq_close_file_logger(&l);
	printf("OK.\n");

	return(rc);
}

void enSQ(fq_logger_t *l, fqueue_obj_t *obj, input_param_t in_params)
{
	printf("enQ() start\n");

	while(1) {
		int rc;
		char	*buf=NULL;
		long l_seq=0L;
		long run_time=0L;

		buf = calloc(in_params.msglen + 1, sizeof(char));
		memset(buf, 'D', in_params.msglen);

		while(1) {
			rc =  obj->on_en(l, obj, EN_NORMAL_MODE, (unsigned char *)buf, in_params.msglen+1, in_params.msglen, &l_seq, &run_time );
			if( rc == EQ_FULL_RETURN_VALUE )  {
				if( in_params.print_flag ) printf("full...\n");
				usleep(in_params.usleep_time);
				continue; /* retry */
			}
			else if( rc < 0 ) {
				if( rc == MANAGER_STOP_RETURN_VALUE ) {
					fq_log(l, FQ_LOG_EMERG, "Manager asked to stop a processing.\n");
				}
				else {
					fq_log(l, FQ_LOG_ERROR, "enQ() error.\n");
				}
				SAFE_FREE(buf);
				return; /* go out from function */
			}
			else {
				if( in_params.print_flag ) {
					fprintf(stdout, "MSG:%s seq=[%ld]\n", buf, l_seq);
				}
				usleep( in_params.usleep_time );	
				SAFE_FREE(buf);
				break; /* next message processing */
			}
		}
	}
	return;
}

void deSQ( fq_logger_t *l, fqueue_obj_t *obj, input_param_t in_params )
{
	while(1) {
		int rc;
		long l_seq=0L;
		long run_time=0L;
		unsigned char *buf=NULL;

		buf = calloc(in_params.bufsize, sizeof(char));

		rc = obj->on_de(l, obj, buf, in_params.bufsize, &l_seq, &run_time);

		if( rc == DQ_EMPTY_RETURN_VALUE ) {
			printf("empty..\n");
			SAFE_FREE(buf);
			usleep(100000);
			continue;
		}
		else if( rc < 0 ) {
			if( rc == DQ_FATAL_SKIP_RETURN_VALUE ) { /* -88 */
				printf("This msg was crashed. I'll continue.\n");
				continue;
			}
			if( rc == MANAGER_STOP_RETURN_VALUE ) { /* -99 */
				printf("Manager asked to stop a processing.\n");
				break;
			}
			printf("error..\n");
			break;
		}
		else {
			if( in_params.print_flag ) {
				fprintf(stdout, "MSG:%s seq=[%ld]\n", buf, l_seq);
			}
			else {
				fprintf(stdout, "DE: seq=[%ld]\n", l_seq);
			}
			SAFE_FREE(buf);
		}
	}
	return;
}
