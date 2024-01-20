/*
** make_threads_from_filelist.c
*/

# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <unistd.h>
# include <pthread.h>

#include "fq_defs.h"
#include "fqueue.h"
#include "fq_file_list.h"
#include "fq_logger.h"
#include "fq_common.h"
#include "fq_multi_config.h"

void *enQ_thread(void *arg);


typedef struct _thread_param {
    int         id;
	char		*company;
	char		*conf_filename;
	char		*path;
	char		*qname;
	int			msglen;
	SZ_toDefine_t conf;
} thread_param_t;

fq_logger_t *l=NULL;

int main()
{
	file_list_obj_t	*obj=NULL;
	FILELIST *this_entry=NULL;
	int rc;
	pthread_t *tids=NULL;
	thread_param_t *t_params=NULL;
	void *thread_rtn=NULL;
	int t_no;
	
	rc = fq_open_file_logger(&l, "/tmp/make_threads_from_filelist.log", FQ_LOG_TRACE_LEVEL);
	CHECK(rc == TRUE);

	rc = open_file_list(l, &obj, "./thread.list");
	CHECK( rc == TRUE );

	// printf("count = [%d]\n", obj->count);
	// obj->on_print(obj);

	t_params = calloc( obj->count, sizeof(thread_param_t));
	CHECK(t_params);

	tids = calloc ( obj->count, sizeof(pthread_t));
	CHECK(tids);

	this_entry = obj->head;

	for( t_no=0;  (this_entry->next && this_entry->value); t_no++ ) {
		thread_param_t *p;

		p = t_params+t_no;
		p->id = t_no;
		p->company = this_entry->value;
		p->path = strdup("/home/pi/data");
		p->qname = strdup(p->company);
		p->msglen = 1024;
		
		if( pthread_create( tids+t_no, NULL, &enQ_thread, p) ) {
			printf("pthread_create() error.");
			return(-1);
		}

		printf("this_entry->value=[%s]\n", this_entry->value);

        this_entry = this_entry->next;
    }

	for(t_no=0; t_no < obj->count ; t_no++) {
        if( pthread_join( *(tids+t_no) , &thread_rtn)) {
			printf("pthread_join() error.");
			return(-1);
		}
	}

	close_file_list(l, &obj);

	fq_close_file_logger(&l);

	return(0);
}

void *enQ_thread(void *arg)
{
	thread_param_t *tp = (thread_param_t *)arg;
	fqueue_obj_t *obj=NULL;
	int rc;

	
	rc =  OpenFileQueue(l, NULL, NULL, NULL, NULL, tp->path, tp->qname, &obj);
    CHECK(rc==TRUE);

	while(1) {
		int rc;
		unsigned char *buf=NULL;
		long l_seq=0L;
		long run_time=0L;
		int databuflen = tp->msglen + 1;
		int datasize;

		buf = calloc(databuflen, sizeof(char));

		memset(buf, 0x00, databuflen);
		memset(buf, 'D', databuflen-1);
		
		datasize = strlen((char *)buf);

		rc =  obj->on_en(l, obj, EN_NORMAL_MODE, buf, databuflen, datasize, &l_seq, &run_time );
		if( rc == EQ_FULL_RETURN_VALUE )  {
			printf("[%d][%s]-thread: full.\n", tp->id, tp->company);
		}
		else if( rc < 0 ) { 
			if( rc == MANAGER_STOP_RETURN_VALUE ) {
				printf("[%d][%s]-thread: Manager asked to stop a processing.\n", tp->id, tp->company);
			}
			printf("error.\n");
		}
		else {
			printf("[%d][%s]-thread: enFQ OK. rc=[%d]\n", tp->id, tp->company,  rc);
			usleep(1000000);
		}

		if(buf) free(buf);

	}

	pthread_exit( (void *)0);
}
