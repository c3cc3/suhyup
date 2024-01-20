/*
** hash_config.c
** Descriptions:
	HashMap 을 이용하여 config를 간단하게 관리하는 방법을 구현한 예제이다.
*/

#include <stdio.h>
#include "fq_hashobj.h"
#include "fq_common.h"
#include "fq_logger.h"

struct _thread_param {
	fq_logger_t *l;
	hashmap_obj_t *hash_obj;
};
typedef struct _thread_param thread_param_t;

void *hash_view_thread( void *arg );

int main(int ac, char **av)
{
	int rc;
	fq_logger_t *l=NULL;
	hashmap_obj_t *hash_obj=NULL;
	char *logname = "/tmp/hashobj_test.log";
	char *filename;
	char *value=NULL;
	pthread_t thread_id;
	void  *thread_rtn=NULL;
	thread_param_t tp;
	
	if( ac != 2 ) {
		printf("Usage: $ %s [map_filename] <enter>\n", av[0]);
		printf("Usage: $ %s hashmap.tab <enter>\n", av[0]);
		exit(0);
	}

	rc = fq_open_file_logger(&l, logname, FQ_LOG_DEBUG_LEVEL);
    CHECK( rc > 0 );

	/* For creating, Use /utl/ManageHashMap command */
	/* This file must be created already */
    rc = OpenHashMapFiles(l, "/home/pi/data", "config", &hash_obj);
    CHECK(rc==TRUE);

	filename = av[1];
	rc = LoadingHashMap(l, hash_obj,  filename);
	CHECK(rc > 0);
	printf("items=[%d]\n", rc);

	tp.l = l;
	tp.hash_obj = hash_obj;

	if( pthread_create( &thread_id, NULL, &hash_view_thread, &tp) )
		fq_log(l, FQ_LOG_ERROR, "pthread_create() error.");

	if( pthread_join( thread_id, &thread_rtn) )
		fq_log(l, FQ_LOG_ERROR, "pthread_join() error.");


	rc = CloseHashMapFiles(l, &hash_obj);
    CHECK(rc==TRUE);

    fq_close_file_logger(&l);

	return 0;
}

void *hash_view_thread( void *arg )
{
	thread_param_t *tp = arg;

	fq_logger_t *l = tp->l;
	hashmap_obj_t *hash_obj = tp->hash_obj;
	char	*qpath=NULL, *qname=NULL;

	while(1) {
		int rc;
		rc = GetHash(l, hash_obj, "QPATH", &qpath);
		CHECK(rc);
		rc = GetHash(l, hash_obj, "QNAME", &qname);
		CHECK(rc);

		printf("QPATH=[%s].\n", qpath);
		printf("QNAME=[%s].\n", qname);

		SAFE_FREE(qpath);
		SAFE_FREE(qname);
		usleep(1000);
	}
}
