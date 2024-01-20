/*
 * $(HOME)/src/tst/var_array_multi_open.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include "fqueue.h"
#include "fq_common.h"


int Opens( fq_logger_t *l, fqueue_obj_t **mobj[], char *path, char *qname, int cnt) 
{
	int i, j, rc;
	int opened=0;

	for(i=0; i<cnt; i++) {
		for(j=0; j<3; j++) {
			rc = OpenFileQueue(l, NULL, NULL, NULL, NULL, path, qname, &mobj[i][j]);
			if( rc == FALSE ) {
				fq_log(l, FQ_LOG_ERROR, "OpenFileQueue('%s', '%s') error.", path, qname);
				return(-1);
			}
			opened++;
			printf("[%d][%d]-th open OK\n", i, j );
		}
	}

	printf("All queues are opened.\n");

	return(0);
}

int Reference( fq_logger_t *l, fqueue_obj_t ***mobj, int cnt)
{
	int i, j;

	for(i=0; i<cnt; i++) {
		for(j=0; j<3; j++) {
			printf("[%d][%d]-th opened qname = [%s]\n", i, j, mobj[i][j]->qname);
		}
	}

	printf("All queues are referenced.\n");
	return(0);
}

int Closes( fq_logger_t *l, fqueue_obj_t ***mobj, int cnt)
{
	int i, j, closed=0;

	for(i=0; i<cnt; i++) {
		for(j=0; j<3; j++) {
			CloseFileQueue(l, &mobj[i][j]);
			printf("[%d][%d]-th closed.\n", i, j);
		}
	}

	printf("All queues are cloesed.\n");

	return(0);
}

int main(int ac, char **av)  
{  
	int rc, i, j;
	fq_logger_t *l=NULL;

	fqueue_obj_t **mobj[3]; /* multi-object */

	char *path="/home/sg/data";
    char *qname="TST";
    char *logname = "var_array_multi_open.log";
	int cnt;

	if( ac != 2 ) {
		printf("Usage: $ %s [cnt] <enter>\n", av[0]);
		return(0);
	}

	cnt = atoi(av[1]);

	if( cnt > MAX_FQ_OPENS ) {
		printf("Too many file_queue open. max=[%d]\n", MAX_FQ_OPENS);
		return(0);
	}

	rc = fq_open_file_logger(&l, logname, FQ_LOG_ERROR_LEVEL);
	CHECK( rc > 0 );

	/* alloc and initialize */
	for(i=0; i<cnt; i++) {
		mobj[i] = calloc(1, sizeof(fqueue_obj_t));
		for(j=0; j<3; j++) {
			mobj[i][j] = calloc(1, sizeof(fqueue_obj_t));
			printf("[%d][%d]-th allocated.\n", i, j);
		}
		mobj[i]++;
	}

	if( Opens(l, mobj, path, qname, cnt ) < 0 ) {
		printf("Opens() error.\n");
		return(-1);
	}

	while (1) {
		Reference(l, mobj, cnt);
		usleep(1000);	
	}

	Closes(l, mobj, cnt);


	fq_close_file_logger(&l);

	printf("Good bye !\n");
	return(rc);
}
