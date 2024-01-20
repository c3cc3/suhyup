/*
** LogNotifier.c
*/

# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <unistd.h>
# include <pthread.h>
#include <sys/inotify.h>
#include <limits.h>

#include "fq_defs.h"
#include "fqueue.h"
#include "fq_file_list.h"
#include "fq_logger.h"
#include "fq_common.h"
#include "fq_multi_config.h"

#define BUF_LEN (10 * (sizeof(struct inotify_event) + NAME_MAX + 1))

void *inotify_thread(void *arg);

typedef struct _thread_param {
    int         id; /* number of thread */
	char		*LogFileName; /* Monitoring file name */
	fq_logger_t	*l;
} thread_param_t;

fq_logger_t *l=NULL;

static void             /* Display information from inotify_event structure */
displayInotifyEvent(fq_logger_t *l, char *filename, struct inotify_event *i)
{
    fq_log(l, FQ_LOG_INFO, "[%s]: wd =%2d; ", filename, i->wd);
    if (i->cookie > 0)
        fq_log(l, FQ_LOG_INFO, "[%s]: cookie =%4d; ", filename, i->cookie);

    printf("mask = ");
    if (i->mask & IN_ACCESS)        fq_log(l, FQ_LOG_INFO, "IN_ACCESS ");
    if (i->mask & IN_ATTRIB)        fq_log(l, FQ_LOG_INFO, "IN_ATTRIB ");
    if (i->mask & IN_CLOSE_NOWRITE) fq_log(l, FQ_LOG_INFO, "IN_CLOSE_NOWRITE ");
    if (i->mask & IN_CLOSE_WRITE)   fq_log(l, FQ_LOG_INFO, "IN_CLOSE_WRITE ");
    if (i->mask & IN_CREATE)        fq_log(l, FQ_LOG_INFO, "IN_CREATE ");
    if (i->mask & IN_DELETE)        fq_log(l, FQ_LOG_INFO, "IN_DELETE ");
    if (i->mask & IN_DELETE_SELF)   fq_log(l, FQ_LOG_INFO, "IN_DELETE_SELF ");
    if (i->mask & IN_IGNORED)       fq_log(l, FQ_LOG_INFO, "IN_IGNORED ");
    if (i->mask & IN_ISDIR)         fq_log(l, FQ_LOG_INFO, "IN_ISDIR ");
    if (i->mask & IN_MODIFY)        fq_log(l, FQ_LOG_INFO, "IN_MODIFY ");
    if (i->mask & IN_MOVE_SELF)     fq_log(l, FQ_LOG_INFO, "IN_MOVE_SELF ");
    if (i->mask & IN_MOVED_FROM)    fq_log(l, FQ_LOG_INFO, "IN_MOVED_FROM ");
    if (i->mask & IN_MOVED_TO)      fq_log(l, FQ_LOG_INFO, "IN_MOVED_TO ");
    if (i->mask & IN_OPEN)          fq_log(l, FQ_LOG_INFO, "IN_OPEN ");
    if (i->mask & IN_Q_OVERFLOW)    fq_log(l, FQ_LOG_INFO, "IN_Q_OVERFLOW ");
    if (i->mask & IN_UNMOUNT)       fq_log(l, FQ_LOG_INFO, "IN_UNMOUNT ");

    if (i->len > 0)
        fq_log(l, FQ_LOG_INFO, "        name = %s\n", i->name);
}

int main()
{
	file_list_obj_t	*obj=NULL;
	FILELIST *this_entry=NULL;
	int rc;
	pthread_t *tids=NULL;
	thread_param_t *t_params=NULL;
	void *thread_rtn=NULL;
	int t_no;
	
	rc = fq_open_file_logger(&l, "/tmp/LogNotifier.log", FQ_LOG_INFO_LEVEL);
	CHECK(rc == TRUE);

	rc = open_file_list(l, &obj, "./log_files.list");
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
		p->LogFileName = this_entry->value;
		p->l = l;
		
		if( pthread_create( tids+t_no, NULL, &inotify_thread, p) ) {
			fprintf(stderr, "pthread_create() error. \n");
			return(0);
		}

		printf("this_entry->value=[%s]\n", this_entry->value);

        this_entry = this_entry->next;
    }

	for(t_no=0; t_no < obj->count ; t_no++) {
        pthread_join( *(tids+t_no) , &thread_rtn);
	}

	close_file_list(l, &obj);

	fq_close_file_logger(&l);

	return(0);
}

void *inotify_thread(void *arg)
{
	int inotifyFd, wd;
	struct inotify_event *event;
	char buf[BUF_LEN] __attribute__ ((aligned(8)));
	ssize_t numRead;
	char *p;
	thread_param_t *tp = (thread_param_t *)arg;
	int rc;
	fq_logger_t *l = tp->l;

	inotifyFd = inotify_init(); /* Create inotify instance */
	if( inotifyFd == -1) {
		fq_log(l, FQ_LOG_ERROR, "[%d]-thread: inotify_init() error.", tp->id);
		pthread_exit( (void *)0 ) ;
	}

	wd = inotify_add_watch(inotifyFd, tp->LogFileName, IN_ALL_EVENTS);
	if( wd == -1 ) {
		fq_log(l, FQ_LOG_ERROR, "[%d]-thread: inotify_add_watch() error. fiile: [%s]", tp->id, tp->LogFileName);
		pthread_exit( (void *)0 ) ;
	}
	for(;;) {
		numRead = read(inotifyFd, buf, BUF_LEN);
		if (numRead == 0) {
			fq_log(l, FQ_LOG_ERROR, "[%d]-thread:read() from inotify fd returned 0!", tp->id);
			break;
		}
		if(numRead == -1) {
			fq_log(l, FQ_LOG_ERROR, "[%d]-thread: read() error.", tp->id);
			break;
		}

		fq_log(l, FQ_LOG_DEBUG, "[%d]-thread: Read %ld bytes from inotify fd.(%s).", 
				tp->id, (long) numRead,  tp->LogFileName);

		/* Process all of the events in buffer returned by read() */
		for (p = buf; p < buf + numRead; ) {
			event = (struct inotify_event *) p;
			displayInotifyEvent(l, tp->LogFileName, event);
			p += sizeof(struct inotify_event) + event->len;
		}
	}
	pthread_exit( (void *)0);
}
