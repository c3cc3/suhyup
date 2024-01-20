/*
** fqueue_callback.c
*/
#include "fqueue.h"
#include "fq_common.h"
#include "fq_logger.h"
#include "fqueue_callback.h"
#include <limits.h>
#include <stdbool.h>
#include <sys/resource.h>

void *deCB_thread( void *arg )
{
	de_msg_t *m = (de_msg_t *)arg;
	int usr_rc;


	usr_rc = m->user_fp(m->msg, m->msglen, m->msgseq);
	
	if(m) free(m);

	pthread_exit((void *)0);
}

bool deFQ_CB_server( char *qpath, char *qname, bool xa_flag, char *logname, int log_level, bool multi_flag, int max_threads, deCB userFP)
{
	int rc;
	fq_logger_t *l=NULL;
	fqueue_obj_t *obj=NULL;
	int buffer_size;
	int thread_cnt = 0;

	rc = fq_open_file_logger(&l, logname, log_level);
	if( rc != FQ_TRUE ) {
		fprintf(stderr, "fq_open_file_logger('%s') failed.", logname);
		return false;
	}

	fq_log(l, FQ_LOG_EMERG, "File queue callback server start start.('%s', '%s', xa_flag='%d', log_level='%d', multi_flag='%d').", qpath, qname, xa_flag, log_level, multi_flag);

	/* check arguments */
	if( (multi_flag == true) && (max_threads > 0 ) ) {
		struct rlimit rlim;
		getrlimit(RLIMIT_NOFILE, &rlim);
		if( max_threads > rlim.rlim_cur) {
			fq_log(l, FQ_LOG_ERROR, "number of thread is too big, in this system, max is [%d].", rlim.rlim_cur);
			goto stop;
		}
	}

	fq_log(l, FQ_LOG_DEBUG, "Checking arguments -> OK.");

	rc = OpenFileQueue(l, 0, 0, 0,0, qpath, qname, &obj);
	if(rc != TRUE) {
		fq_log(l, FQ_LOG_ERROR, "OpenFileQueue('%s', '%s') error.", qpath, qname);
		goto stop;
	}

	buffer_size = 65536;

	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	int empty_count = 0;

	while(1) {
		long l_seq = 0L;
		unsigned char *buf = NULL;
		long run_time=0L;

		buf = calloc(buffer_size, sizeof(unsigned char));
		if( xa_flag == false ) {
			rc = obj->on_de(l, obj, buf, buffer_size, &l_seq, &run_time);
			if( rc == DQ_EMPTY_RETURN_VALUE) {
				if(empty_count == 6000 ) {
					fq_log(l, FQ_LOG_INFO, "There is no en message during 60 seconds.");
					empty_count = 0;
				}
				empty_count++;
				SAFE_FREE(buf);
				usleep(10000);
				continue;
			}
			else if(rc < 0 ) {	
				if(rc == MANAGER_STOP_RETURN_VALUE) {
					fq_log(l, FQ_LOG_EMERG, "Manager asked to stop a processing.");
					break;
				}
				else {
					fq_log(l, FQ_LOG_ERROR, "on_de() error.");
					break;
				}
			}
			else {
				empty_count = 0;
				if( multi_flag == false ) {
					int user_rc;
					user_rc = userFP(buf, rc, l_seq);
					if( user_rc < 0 ) {
						fq_log(l, FQ_LOG_ERROR, "User callback function	error. We will stop server.");
						break;
					}
				}
				else {
					de_msg_t *de;
					pthread_t tid;

					de = calloc(1, sizeof(de_msg_t));
					de->msg = buf;
					de->msglen = rc;
					de->msgseq = l_seq;
					de->user_fp = userFP;

					int thread_rc;

					thread_rc = pthread_create(&tid, &attr, deCB_thread, de);
					if(thread_rc != 0) {
						fq_log(l, FQ_LOG_ERROR, "pthread_create() error. fatal, We'll stop the server.");
						break;
					}
					if( thread_cnt > max_threads ) {
						thread_cnt = 0;
						usleep(100000);
					}
				}
			}
		}
		else {
			char unlink_filename[256];

			rc = obj->on_de_XA(l, obj, buf, buffer_size, &l_seq, &run_time, unlink_filename);
			if( rc == DQ_EMPTY_RETURN_VALUE) {
				if(empty_count == 6000 ) {
					fq_log(l, FQ_LOG_INFO, "There is no en message during 60 seconds.");
					empty_count = 0;
				}
				empty_count++;

				SAFE_FREE(buf);
				usleep(10000);
				continue;
			}
			else if(rc < 0 ) {	
				if(rc == MANAGER_STOP_RETURN_VALUE) {
					fq_log(l, FQ_LOG_EMERG, "Manager asked to stop a processing.");
					break;
				}
				else {
					fq_log(l, FQ_LOG_ERROR, "on_de() error.");
					break;
				}
			}
			else {
				empty_count = 0;
				if( multi_flag == false ) {
					int user_rc;
					user_rc = userFP(buf, rc, l_seq);
					if( user_rc < 0 ) {
						obj->on_cancel_XA(l, obj, l_seq, &run_time);
						fq_log(l, FQ_LOG_ERROR, "User callback function	error. We will stop server.");
						break;
					}
					obj->on_commit_XA(l, obj, l_seq, &run_time, unlink_filename);
				}
				else {
					de_msg_t *de;
					pthread_t tid;

					de = calloc(1, sizeof(de_msg_t));
					de->msg = buf;
					de->msglen = rc;
					de->msgseq = l_seq;
					de->user_fp = userFP;

					int thread_rc;

					thread_rc = pthread_create(&tid, &attr, deCB_thread, de);
					if(thread_rc != 0) {
						obj->on_cancel_XA(l, obj, l_seq, &run_time);
						fq_log(l, FQ_LOG_ERROR, "pthread_create() error. fatal, We'll stop the server.");
						break;
					}
					obj->on_commit_XA(l, obj, l_seq, &run_time, unlink_filename);
					if( thread_cnt > max_threads ) {
						thread_cnt = 0;
						usleep(100000);
					}
				}
			}

		}
	} /* end while(1) */
stop:
	fq_log(l, FQ_LOG_EMERG, "File Queue callback server stop.");
	if(obj) {
		CloseFileQueue(l, &obj);
	}
	if(l) {
		fq_close_file_logger(&l);
	}
	return(false);
}

void *FQ_callback_server(void *arg)
{
	FQ_server_t *x = (FQ_server_t *)arg;
	int sleep_cnt = 0;
	int threads_cnt = 0;

	fq_log(x->l, FQ_LOG_EMERG, "FQ_callback_server('%s', '%s') start.", x->qobj->path, x->qobj->qname);

	pthread_attr_t attr;
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	while(1) {
		int rc;
		long l_seq, run_time;
		unsigned char *buf = NULL;
		int buffer_size = 65536;

		buf = calloc(buffer_size, sizeof(unsigned char));
		
		rc = x->qobj->on_de(x->l, x->qobj, buf, buffer_size, &l_seq, &run_time);
		if( rc == DQ_EMPTY_RETURN_VALUE ) {
			if(sleep_cnt == 60 ) {
				fq_log(x->l, FQ_LOG_INFO, "There is no en message during 60 secondes.");
				sleep_cnt = 0;
			}
			SAFE_FREE(buf);
			usleep(1000000);
			sleep_cnt++;
			continue;
		}
		else if(rc<0) {
			if(rc == MANAGER_STOP_RETURN_VALUE ) {
				fq_log(x->l, FQ_LOG_EMERG, "Manager asked stopping a processing.");
				break;
			}
			fq_log(x->l, FQ_LOG_ERROR, "on_de() error.");
			break;
		}
		else {
			sleep_cnt = 0;
			
			if(x->multi_flag == false) {
				int user_rc;
				user_rc = x->fp(buf, rc, l_seq);
				if(user_rc < 0) {
					fq_log(x->l, FQ_LOG_ERROR, "User function error. We will stop server.");
					break;
				}
			}
			else {
				de_msg_t *de;
				pthread_t tid;
				
				de = calloc(1, sizeof(de_msg_t));
				de->msg = buf;
				de->msglen = rc;
				de->msgseq = l_seq;
				de->user_fp = x->fp;

				int thread_rc;
				thread_rc = pthread_create(&tid, &attr, deCB_thread, de);
				if(thread_rc != 0) {
					fq_log(x->l, FQ_LOG_ERROR, "pthread_create() error. fatal, We will stop server.");
					break;
				}

				threads_cnt++;
				if(threads_cnt > x->max_threads) {
					threads_cnt = 0;
					sleep(1);
				}
			}
		}
	}

	fq_log(x->l, FQ_LOG_ERROR, "FQ_callback_server('%s', '%s') exit.", x->qobj->path, x->qobj->qname);
	pthread_exit( (void *)0);
}
void *FQ_callback_server_XA(void *arg)
{
	FQ_server_t *x = (FQ_server_t *)arg;
	int sleep_cnt = 0;
	int threads_cnt = 0;

	fq_log(x->l, FQ_LOG_EMERG, "FQ_callback_server_XA('%s', '%s') start.", x->qobj->path, x->qobj->qname);

	pthread_attr_t attr;
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	while(1) {
		int rc;
		long l_seq, run_time;
		unsigned char *buf = NULL;
		int buffer_size = 65536;
		char unlink_filename[256];

		buf = calloc(buffer_size, sizeof(unsigned char));
		
		rc = x->qobj->on_de_XA(x->l, x->qobj, buf, buffer_size, &l_seq, &run_time, unlink_filename);
		if( rc == DQ_EMPTY_RETURN_VALUE ) {
			if(sleep_cnt == 60 ) {
				fq_log(x->l, FQ_LOG_INFO, "There is no en message during 60 secondes.");
				sleep_cnt = 0;
			}
			SAFE_FREE(buf);
			usleep(1000000);
			sleep_cnt++;
			continue;
		}
		else if(rc<0) {
			if(rc == MANAGER_STOP_RETURN_VALUE ) {
				fq_log(x->l, FQ_LOG_EMERG, "Manager asked stopping a processing.");
				break;
			}
			fq_log(x->l, FQ_LOG_ERROR, "on_de_XA() error.");
			break;
		}
		else {
			sleep_cnt = 0;
			
			if(x->multi_flag == false) {
				int user_rc;
				user_rc = x->fp(buf, rc, l_seq);
				if(user_rc < 0) {
					fq_log(x->l, FQ_LOG_ERROR, "User function error. We will stop server.");
					x->qobj->on_cancel_XA(x->l, x->qobj, l_seq, &run_time);
					break;
				}
				x->qobj->on_commit_XA(x->l, x->qobj, l_seq, &run_time, unlink_filename);
			}
			else {
				de_msg_t *de;
				pthread_t tid;
				
				de = calloc(1, sizeof(de_msg_t));
				de->msg = buf;
				de->msglen = rc;
				de->msgseq = l_seq;
				de->user_fp = x->fp;

				int thread_rc;
				thread_rc = pthread_create(&tid, &attr, deCB_thread, de);
				if(thread_rc != 0) {
					fq_log(x->l, FQ_LOG_ERROR, "pthread_create() error. fatal, We will stop server.");
					x->qobj->on_cancel_XA(x->l, x->qobj, l_seq, &run_time);
					break;
				}

				x->qobj->on_commit_XA(x->l, x->qobj, l_seq, &run_time, unlink_filename);
				threads_cnt++;
				if(threads_cnt > x->max_threads) {
					threads_cnt = 0;
					sleep(1);
				}
			}
		}
	}

	fq_log(x->l, FQ_LOG_ERROR, "FQ_callback_server('%s', '%s') exit.", x->qobj->path, x->qobj->qname);
	pthread_exit( (void *)0);
}

bool make_thread_fq_CB_server(char *logname, int log_level, char *qpath, char *qname, bool xa_flag, bool multi_flag, int max_threads, deCB my_CB)
{
	pthread_t tid;
	pthread_attr_t attr;
	FQ_server_t *x=NULL;
	fq_logger_t *l=NULL;
	int rc;

	rc = fq_open_file_logger(&l, logname, log_level);
	if(rc != FQ_TRUE ) {
		fprintf(stderr, "fq_open_file_logger('%s') failed.", logname);
		return false;
	}

	fqueue_obj_t *obj=NULL;
	rc = OpenFileQueue(l, 0,0,0,0, qpath, qname, &obj);
	if(rc != TRUE ) {
		fq_log(l, FQ_LOG_ERROR, "OpenFileQueue('%s', '%s') error.", qpath, qname);
		goto stop;
	}
	
	x = calloc(1, sizeof(FQ_server_t));
	
	x->l = l;
	x->qobj = obj;
	x->xa_flag = xa_flag;
	x->multi_flag = multi_flag;
	x->max_threads = max_threads;
	x->fp = my_CB;

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	int error;
	if(x->xa_flag == true) {
		error = pthread_create(&tid, &attr, FQ_callback_server_XA, x);
	}
	else {
		error = pthread_create(&tid, &attr, FQ_callback_server, x);
	}
	if( error != 0 ) {
		fq_log(l, FQ_LOG_ERROR, "pthread_create() error.");
		goto stop;
	}
	return true;

stop:
	if(obj) {
		CloseFileQueue(l, &obj);
	}
	if(l) {
		fq_close_file_logger(&l);
	}
	if(x) free(x);

	return false;
}
	
	


 


