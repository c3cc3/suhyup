/*
 * TestFQ.c
 */
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include "fq_logger.h"
#include "fq_sequence.h"

#define TESTFQ_C_VERSION "1.0.1"

/* last update: 2013/05/21 */
/* 2013/05/23: change: line 216, from CHECK( rc < 0 ) to CHECK( rc == TRUE );  */
/* 2013/10/02: found a bug. memory leak */

#include "fqueue.h"
#include "shm_queue.h"
#include "fq_common.h"
#include "fq_timer.h"

extern struct timeval tp1, tp2;
long	old_seq = -1;
long 	old_data_seq = -1;
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
	char*  XA_flag; /* on or off */
	int threads;
	int shared_mem_q_flag;
} input_param_t;

typedef struct _thread_param {
    int             Qindex;
	input_param_t	*in_param;
} thread_param_t;

fq_logger_t *l=NULL;
fqueue_obj_t *obj=NULL;

void enFQ(fq_logger_t *l, fqueue_obj_t *obj, input_param_t in_params);
void deFQ( fq_logger_t *l, fqueue_obj_t *obj, input_param_t in_params );
void deFQ_XA( fq_logger_t *l, fqueue_obj_t *obj, input_param_t in_params );
void *deQ_thread( void *arg );
void *deQ_XA_thread( void *arg );
void *enQ_thread(void *arg);

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

	printf("\n\nUsage  : $ %s [-V] [-h] [-p path] [-q qname] [-l logname] [-a action] [-b buffer] [-m msglen] [-u usleep] [-P Print] [-o loglevel] [-x on] [-t 1] <enter>\n", p);
	printf("\t	-V: version \n");
	printf("\t	-h: help \n");
	printf("\t	-l: logfilename \n");
	printf("\t	-p: full path \n");
	printf("\t	-q: qname \n");
	printf("\t	-a: action_flag ( EN | DE )\n");
	printf("\t	-m: msglen ( for enQ )\n"); 
	printf("\t	-b: buffer size ( for deQ )\n");
	printf("\t	-u: usleep time ( 1000000 -> 1 second,  )\n");
	printf("\t	-P: Print msg to screen ( 0 | 1 )\n");
	printf("\t	-o: log level ( trace|debug|info|error|emerg )\n");
	printf("\t  -x: XA on or off ( on | off ) \n");
	printf("\t  -t: number of threads ( 1-256 ) \n");
	printf("\t  -s: SHMQ (shard memory queue ) \n");
	printf("enQ example: $ %s -l /tmp/%s.en.log -p %s -q TST -a EN -m 4092 -u 1000000 -P 0 -o error -t 1 <enter>\n", p, p, data_home);
	printf("enQ example: $ %s -l /tmp/%s.en.log -p %s -q TST -a EN -m 4092 -u 1000000 -P 1 -o debug -t 1 <enter>\n", p, p, data_home);
	printf("deQ example: $ %s -l /tmp/%s.de.log -p %s -q TST -a DE -b 65536 -u 1000000 -P 0 -o error -x off<enter>\n", p, p, data_home);
	printf("deQ example: $ %s -l /tmp/%s.de.log -p %s -q TST -a DE -b 65536 -u 1000000 -P 1 -o debug -x on<enter>\n", p, p, data_home);
	printf("deQ example: $ %s -l /tmp/%s.de.log -p %s -q TST -a DE -b 65536 -u 1000000 -P 0 -o error -x off<enter>\n", p, p, data_home);
	printf("deQ example: $ %s -l /tmp/%s.de.log -p %s -q TST -a DE -b 65536 -u 1000000 -P 1 -o debug -x on<enter>\n", p, p, data_home);
	printf("deQ example: $ %s -l /tmp/%s.de.log -p %s -q TST -a DE -b 65536 -u 1000000 -P 1 -o debug -x on -t 10 <enter>\n", p, p, data_home);
	printf("deQ example: $ %s -l /tmp/%s.de.log -p %s -q TST -a DE -b 65536 -u 1000000 -P 0 -o error -x on -t 10 <enter>\n", p, p, data_home);
	printf("deQ example: $ %s -l /tmp/%s.de.log -p %s -q TST -a DE -b 65536 -u 1000000 -P 0 -o error -x off -t 10 <enter>\n", p, p, data_home);
	return;
}
void print_version(char *p)
{
    printf("\nversion: %s.\n\n", TESTFQ_C_VERSION);
    return;
}

int main(int ac, char **av)  
{  
	int rc;
	int     ch;
	input_param_t   in_params;

	printf("Compiled on %s %s\n", __TIME__, __DATE__);

	if( ac < 2 ) {
		print_help(av[0]);
        return(0);
    }

	/* default setting */
	in_params.logname = NULL;
	in_params.path = NULL;
	in_params.qname = NULL;
	in_params.action = NULL;
	in_params.log_level = NULL;
	in_params.msglen = -1;
	in_params.bufsize = -1;
	in_params.usleep_time = 0;	
	in_params.print_flag = 0;	
	in_params.threads = 1;
	in_params.shared_mem_q_flag = 0;
	
	while(( ch = getopt(ac, av, "VvHh:l:p:q:a:m:b:u:P:o:x:t:s:")) != -1) {
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
			case 'x':
				in_params.XA_flag = strdup(optarg);
				break;
			case 't':
				in_params.threads = atoi(optarg);
				break;
			case 's':
				in_params.shared_mem_q_flag = atoi(optarg);
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

	int i_log_level;
	i_log_level = get_log_level(in_params.log_level);
	rc = fq_open_file_logger(&l, in_params.logname, i_log_level);
	CHECK( rc > 0 );


	if( in_params.shared_mem_q_flag == 1) {
		rc =  OpenShmQueue(l, in_params.path, in_params.qname, &obj);
		CHECK(rc==TRUE);
	}
	else {
		rc =  OpenFileQueue(l, NULL, NULL, NULL, NULL, in_params.path, in_params.qname, &obj);
		CHECK(rc==TRUE);
	}

	/*buffer_size = obj->h_obj->h->msglen-4+1; */
	printf("File Queue msglen is [%ld]\n", obj->h_obj->h->msglen);

	if( in_params.threads ==  1) {
		if( strncmp(in_params.action, "EN", 2) == 0 ) {
			enFQ( l, obj, in_params);
		}
		else if( (strncmp(in_params.action, "DE", 2) == 0) && (strncmp(in_params.XA_flag, "off", 2) == 0) )  {
			deFQ( l, obj, in_params);
		}
		else if( (strncmp(in_params.action, "DE", 2) == 0) && (strncmp(in_params.XA_flag, "on", 2) == 0) )  {
			deFQ_XA( l, obj, in_params);
		}
		else {
			print_help(av[0]);
		}
	}
	else { /* thread */
		pthread_t *tids=NULL;
		thread_param_t *t_params=NULL;
		void *thread_rtn=NULL;
		int t_cnt;
		int t_no;

		t_cnt = in_params.threads;

		t_params = calloc( t_cnt, sizeof(thread_param_t));
		CHECK(t_params);

		tids = calloc ( t_cnt, sizeof(pthread_t));
		CHECK(tids);

		/* enFQ_thread */
		if( strncmp(in_params.action, "EN", 2) == 0 ) {
			for(t_no=0; t_no < t_cnt ; t_no++) {
				thread_param_t *p;

				p = t_params+t_no;
				p->Qindex = t_no;
				p->in_param = &in_params;
				if( pthread_create( tids+t_no, NULL, &enQ_thread, p)) {
					perror("pthread_create");
					exit(0);
				}
					
			}

		}
		/* deFQ_thread */
		else if( (strncmp(in_params.action, "DE", 2) == 0) && (strncmp(in_params.XA_flag, "off", 2) == 0) )  {
			for(t_no=0; t_no < t_cnt ; t_no++) {
				thread_param_t *p;

				p = t_params+t_no;
				p->Qindex = t_no;
				p->in_param = &in_params;
				pthread_create( tids+t_no, NULL, &deQ_thread, p);
			}
		}
		/* deFQ_XA_thread */
		else if( (strncmp(in_params.action, "DE", 2) == 0) && (strncmp(in_params.XA_flag, "on", 2) == 0) )  {
			for(t_no=0; t_no < t_cnt ; t_no++) {
				thread_param_t *p;

				p = t_params+t_no;
				p->Qindex = t_no;
				p->in_param = &in_params;
				pthread_create( tids+t_no, NULL, &deQ_XA_thread, p);
			}
		}
		else {
			print_help(av[0]);
		}

		for(t_no=0; t_no < t_cnt ; t_no++) {
			pthread_join( *(tids+t_no) , &thread_rtn);
		}
	}
	if( in_params.shared_mem_q_flag == 1) {
		CloseShmQueue(l, &obj);
		CHECK(rc==TRUE);
	}
	else {
		rc = CloseFileQueue(l, &obj);
		CHECK(rc==TRUE);
	}

	fq_close_file_logger(&l);
	printf("OK.\n");

	return(rc);
}

void enFQ(fq_logger_t *l, fqueue_obj_t *obj, input_param_t in_params)
{
	printf("enQ() start\n");

	sequence_obj_t *sobj=NULL;
	int rc;
	rc = open_sequence_obj(l, ".", "test", "A", 10, &sobj);
	CHECK(rc==TRUE);

	while(1) {
		int rc;
		char	*buf=NULL;
		long l_seq=0L;
		long run_time=0L;

		buf = calloc(in_params.msglen + 1, sizeof(char));
		memset(buf, 'D', in_params.msglen);
		char data_seq[11];
		memset(data_seq, 0x00, sizeof(data_seq));
		
		rc = sobj->on_get_sequence(sobj, data_seq, sizeof(data_seq));
		CHECK(rc==TRUE);
		memcpy(buf, data_seq, 10);

		while(1) {
			rc =  obj->on_en(l, obj, EN_NORMAL_MODE, (unsigned char *)buf, in_params.msglen+1, in_params.msglen, &l_seq, &run_time );
			if( rc == EQ_FULL_RETURN_VALUE )  {
				if( in_params.print_flag ) printf("full...\n");

				if( in_params.usleep_time ) {
					printf("%ld sleep\n", in_params.usleep_time);
					usleep(in_params.usleep_time);
				}
				else {
					// printf("default sleep (100000)\n");
				}
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

				if( in_params.usleep_time ) {
					printf("%ld sleep\n", in_params.usleep_time);
					usleep(in_params.usleep_time);
				}
				else {
					// printf("default sleep (100000)\n");
				}

				SAFE_FREE(buf);
				break; /* next message processing */
			}
		}
	}
	return;
}

void deFQ( fq_logger_t *l, fqueue_obj_t *obj, input_param_t in_params )
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
			if( in_params.usleep_time ) {
				printf("%ld sleep\n", in_params.usleep_time);
				usleep(in_params.usleep_time);
			}
			else {
				printf("default sleep (100000)\n");
			}
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
			/* first time */
			if( old_seq == -1 ) {
				old_seq = l_seq;
			}
			else {
				if( old_seq+1 != l_seq ) {
					fprintf(stderr, "Sequence error: old=%ld, now=%ld\n", old_seq, l_seq);
					break;
				}
				else {
					old_seq = l_seq;
				}
			}
			char data_seq[10];
			memset(data_seq, 0x00, sizeof(data_seq));
			memcpy(data_seq, &buf[1], 9);
			
			long now_data_seq = atol(data_seq);
			if( old_data_seq == -1 ) {
				old_data_seq = now_data_seq;
			}
			else {
				if( old_data_seq + 1 != now_data_seq ) {
					fprintf(stderr, "Sequence error: old=%ld, now=%ld\n", old_data_seq, now_data_seq);
					break;
				}
				else {
					old_data_seq = now_data_seq;
				}
			}	
				
			if( in_params.print_flag ) {
				fprintf(stdout, "MSG:%s seq=[%ld]\n", buf, l_seq);
			}
			else {
				fprintf(stdout, "DE: seq=[%ld]\n", l_seq);
			}

			if( in_params.usleep_time ) {
				printf("%ld sleep\n", in_params.usleep_time);
				usleep(in_params.usleep_time);
			}
			else {
				// printf("default sleep (100000)\n");
			}

			SAFE_FREE(buf);
		}
	}
	return;
}
void deFQ_XA( fq_logger_t *l, fqueue_obj_t *obj, input_param_t in_params )
{
	while(1) {
		int rc;
		long l_seq=0L;
		long run_time=0L;
		unsigned char *buf=NULL;
		char    unlink_filename[256];

		buf = calloc(in_params.bufsize, sizeof(char));

		rc = obj->on_de_XA(l, obj, buf, in_params.bufsize, &l_seq, &run_time, unlink_filename);

		if( rc == DQ_EMPTY_RETURN_VALUE ) {
			printf("empty..\n");
			SAFE_FREE(buf);
			if( in_params.usleep_time ) {
				printf("%ld sleep\n", in_params.usleep_time);
				usleep(in_params.usleep_time);
			}
			else {
				printf("default sleep (100000)\n");
			}
			continue;
		}
		else if( rc < 0 ) {
			if( rc == DQ_FATAL_SKIP_RETURN_VALUE ) { /* -88 */
				printf("This msg was crashed. I'll continue.\n");
				continue;
			}
			if( rc == EXIST_UN_COMMIT ) {
				printf("un-committed file exist. I'll continue.\n");
				SAFE_FREE(buf);
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

			rc = obj->on_commit_XA(l, obj, l_seq, &run_time, unlink_filename);
			if( rc < 0 ) {
				printf("XA commit error.\n");
				SAFE_FREE(buf);
				break;
			}
			SAFE_FREE(buf);
		}
		if( in_params.usleep_time ) {
			printf("%ld sleep\n", in_params.usleep_time);
			usleep(in_params.usleep_time);
		}
		else {
			// printf("default sleep (100000)\n");
		}
	}
	return;
}

void *deQ_thread( void *arg )
{
	thread_param_t *tp = (thread_param_t *)arg;

	while(1) {
		int rc;
		long l_seq=0L;
		unsigned char *buf=NULL;
		long run_time=0L;

		buf = calloc(tp->in_param->bufsize, sizeof(char));

		rc = obj->on_de(l, obj, buf, tp->in_param->bufsize, &l_seq, &run_time);

		if( rc == DQ_EMPTY_RETURN_VALUE ) {
			printf("[%d]-thread: empty..\n", tp->Qindex);
			SAFE_FREE(buf);
			usleep(100000);
		}
		else if( rc < 0 ) {
			if( rc == MANAGER_STOP_RETURN_VALUE ) {
				printf("[%d]-thread: Manager asked to stop a processing.\n", tp->Qindex);
				break;
			}
			printf("error..\n");
			break;
		}
		else {
			/* success */
			printf("[%d]-thread: rc = [%d] seq=[%ld]-th\n", tp->Qindex, rc, l_seq );
			SAFE_FREE(buf);
		}
	}
	pthread_exit(0);
}

void *deQ_XA_thread( void *arg )
{
	thread_param_t *tp = (thread_param_t *)arg;

	while(1) {
		int rc;
		long l_seq=0L;
		unsigned char *buf=NULL;
		long run_time=0L;
		char    unlink_filename[256];

		buf = calloc(tp->in_param->bufsize, sizeof(char));

		rc = obj->on_de_XA(l, obj, buf, tp->in_param->bufsize, &l_seq, &run_time, unlink_filename);

		if( rc == DQ_EMPTY_RETURN_VALUE ) {
			printf("[%d]-thread: empty..\n", tp->Qindex);
			SAFE_FREE(buf);
			usleep(100000);
		}
		else if( rc < 0 ) {
			if( rc == MANAGER_STOP_RETURN_VALUE ) {
				printf("[%d]-thread: Manager asked to stop a processing.\n", tp->Qindex);
				break;
			}
			printf("error..\n");
			break;
		}
		else {
			/* first time */
			if( old_seq == -1 ) {
				old_seq = l_seq;
			}
			else {
				if( old_seq+1 != l_seq ) {
					fprintf(stderr, "Sequence error: old=%ld, now=%ld\n", old_seq, l_seq);
					break;
				}
				else {
					old_seq = l_seq;
				}
			}

			printf("[%d]-XA-thread: rc = [%d] seq=[%ld]-th\n", tp->Qindex, rc, l_seq );
			rc = obj->on_commit_XA(l, obj, l_seq, &run_time, unlink_filename);
			if( rc < 0 ) {
				printf("[%d]-thread: XA commit error. \n", tp->Qindex);
				SAFE_FREE(buf);
				exit(0);
			}
			/* success */
			SAFE_FREE(buf);
		}
	}
	pthread_exit(0);
}

void *enQ_thread(void *arg)
{
	thread_param_t *tp = (thread_param_t *)arg;

	while(1) {
		int rc;
		unsigned char *buf=NULL;
		long l_seq=0L;
		long run_time=0L;
		int databuflen = tp->in_param->msglen + 1;
		int datasize;

		buf = calloc(databuflen, sizeof(char));

		memset(buf, 0x00, databuflen);
		memset(buf, 'D', databuflen-1);
		
		datasize = strlen((char *)buf);

		rc =  obj->on_en(l, obj, EN_NORMAL_MODE, buf, databuflen, datasize, &l_seq, &run_time );
		if( rc == EQ_FULL_RETURN_VALUE )  {
			printf("[%d]-thread: full.\n", tp->Qindex);
		}
		else if( rc < 0 ) { 
			if( rc == MANAGER_STOP_RETURN_VALUE ) {
				printf("[%d]-thread: Manager asked to stop a processing.\n", tp->Qindex);
			}
			printf("error.\n");
		}
		else {
			printf("[%d]-thread: enFQ OK. rc=[%d]\n", tp->Qindex, rc);
			usleep(10000);
		}

		if(buf) free(buf);

	}

	pthread_exit( (void *)0);
}
