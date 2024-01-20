/* vi: set sw=4 ts=4: */
/* 
 * fq_monitor.c
 */
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h> 
#include <unistd.h> 
#include <pthread.h>
#include <sys/types.h>
#include <libgen.h>

#include "config.h"
#include "fq_common.h"
#include "fq_semaphore.h"
#include "fq_monitor.h"


#include "shm_queue.h"
#include "fq_linkedlist.h"
#include "fq_manage.h"
#include "fq_external_env.h"
#include "fq_hashobj.h"
#include "fq_delimiter_list.h"
#include "fq_file_list.h"
#include "fq_scanf.h"

#define FQ_MONITOR_C_VERSION "1.0.0"

static int on_send_action_status(fq_logger_t *l, char *appID, char *path, char *qname, char *appDesc, action_flag_t flag,  monitor_obj_t *obj);
static int on_touch(fq_logger_t *l, char *appID, action_flag_t flag,  monitor_obj_t *obj);
static int on_getlist_mon(fq_logger_t *l, monitor_obj_t *obj, int *use_rec_count,  char **all_string);
static int on_get_app_status( fq_logger_t *l, monitor_obj_t *obj, char *appID, char *status);
static char * get_str_from_action_flag(action_flag_t action_flag, char *dst);
static int on_delete_app_in_monitortable(fq_logger_t *l, char *appID, monitor_obj_t *obj);

int Shb_Start_Mon(char *Progname, char *Desc)
{
	monitor_obj_t	*mon_obj=NULL;
	int rc;

	rc = open_monitor_obj( NULL, &mon_obj);
	if( rc  == FALSE ) {
		fq_log(NULL, FQ_LOG_ERROR, "open_monitor_obj('%s', '%s') error.");
		return(FALSE);
	}
	rc = mon_obj->on_send_action_status(NULL, Progname, NULL, NULL, Desc, FQ_EN_ACTION, mon_obj);
	if( rc == FALSE ) {
		fq_log(NULL, FQ_LOG_ERROR, "on_send_action_status() error.");
		close_monitor_obj( NULL, &mon_obj);
		return(FALSE);
	}
	return(TRUE);
}

int RegistMon( fq_logger_t *l, char *Progname, char *Desc, char *path, char *qname, action_flag_t flag)
{
	monitor_obj_t	*mon_obj=NULL;
	int rc;

	rc = open_monitor_obj( l, &mon_obj);
	if( rc  == FALSE ) {
		fq_log(l, FQ_LOG_ERROR, "open_monitor_obj('%s', '%s') error.");
		return(FALSE);
	}
	rc = mon_obj->on_send_action_status(l, Progname,path, qname, Desc, flag, mon_obj);
	if( rc == FALSE ) {
		fq_log(l, FQ_LOG_ERROR, "on_send_action_status() error.");
		close_monitor_obj( l, &mon_obj);
		return(FALSE);
	}
	return(TRUE);
}

int open_monitor_obj( fq_logger_t *l, monitor_obj_t **obj)
{
	monitor_obj_t *rc=NULL;

	FQ_TRACE_ENTER(l);

	rc  = (monitor_obj_t *)malloc(sizeof(monitor_obj_t));
	if(rc) {
		int i, ret=-1;
		int shm_id=-1;
		key_t   shmkey;
		void	*shm_p=NULL;

		shmkey = ftok(FQ_MONITORING_KEY_PATH, FQ_MONITORING_SHM_CHAR_KEY);

		fq_log(l, FQ_LOG_DEBUG, "Shared memory key for monitoring is [0x%08x].", shmkey);

		if ((shm_id = shmget(shmkey, sizeof(monitor_t), IPC_CREAT|IPC_EXCL|0666 )) == -1) {
			if( errno == EEXIST || errno == 0 || errno == 2 ) {
				fq_log(l, FQ_LOG_DEBUG, "Shared memory key[0x%08x] already exist.", shmkey);
				if ((shm_id = shmget(shmkey, sizeof(monitor_t),  0)) == -1) {
					fq_log(l, FQ_LOG_ERROR, "shmget() error. reason=[%s]. Remove it and Re-try shmget().", strerror(errno));
					goto return_FALSE;
				}
				else {
					if ((shm_p = shmat( shm_id, (void *)0, 0)) == (void *)-1 ) {
						fq_log(l, FQ_LOG_ERROR, "shmat() error. reason=[%s]", strerror(errno));
						goto return_FALSE;
					}
					fq_log(l, FQ_LOG_DEBUG, "Successfully, attached to shared memory [0x%x].", shmkey);
					rc->m = (monitor_t *)shm_p;
				}
			}
			else {
				fq_log(l, FQ_LOG_ERROR, "shmget() error. errno=[%d]", errno);
				goto return_FALSE;
			}
		}
		else { /*  creation success. */
			fq_log(l, FQ_LOG_INFO, "New shared memory was created for monitoring.");
			if ((shm_p = shmat( shm_id, (void *)0, 0)) == (void *)-1 ) {
				fq_log(l, FQ_LOG_ERROR, "shmat() error. reason=[%s]", strerror(errno));
				goto return_FALSE;
			}
			rc->m = (monitor_t *)shm_p;
			rc->m->current = 0;

			for(i=0; i<MAX_MON_RECORDS; i++) {
				memset( rc->m->r[i].appID, 0x00, sizeof(rc->m->r[i].appID));
				memset( rc->m->r[i].path, 0x00, sizeof(rc->m->r[i].appID));
				memset( rc->m->r[i].qname, 0x00, sizeof(rc->m->r[i].appID));
				memset( rc->m->r[i].appDesc, 0x00, sizeof(rc->m->r[i].appID));
				rc->m->r[i].heartbeat_time = time(0);
				rc->m->r[i].action_flag = FQ_NO_ACTION;
				rc->m->r[i].pid = getpid();
			}
		}
		
		ret = open_semaphore_obj(l, FQ_MONITORING_KEY_PATH, FQ_MONITORING_SHM_CHAR_KEY, &rc->sema_obj);
		if(ret == FALSE) {
			fq_log(l, FQ_LOG_ERROR, "open_semaphore_obj() error.");
			goto return_FALSE;
		}

		rc->on_send_action_status = on_send_action_status;
		rc->on_touch = on_touch;
		rc->on_getlist_mon = on_getlist_mon;
		rc->on_get_app_status = on_get_app_status;
		rc->on_delete_app_in_monitortable = on_delete_app_in_monitortable;

		rc->l = l;
		*obj = rc;

		fq_log(l, FQ_LOG_INFO, "monitoring object open success!!.");
		FQ_TRACE_EXIT(l);
		return(TRUE); 
	}

return_FALSE:

	SAFE_FREE(*obj);

	return(FALSE);
}

static int
on_send_action_status(fq_logger_t *l, char *appID, char *path, char *qname, char *appDesc, action_flag_t action_flag,  monitor_obj_t *obj)
{
	int i;
	int found_flag=0;

	FQ_TRACE_ENTER(l);

	fq_log(l, FQ_LOG_DEBUG, "------[%s] last_send_time: [%ld] : current=[%ld]", appID, obj->last_send_time, time(0));

	if( obj->last_send_time == time(0) &&  strcmp(appID, obj->last_send_appID) == 0 ) {
		fq_log(l, FQ_LOG_DEBUG, "update skipped. (%s)", appID);
		goto END;
	}

	obj->sema_obj->on_sem_lock(obj->sema_obj);

	obj->last_send_time = time(0);

	/*
	 - 검색해서 동일한 레코드가 있으면 그 인덱스에 업데이트한다.
	 - 검색해서 없으면 새로운 인덱스를 할당하고 그곳에 업데이트한다.
	*/

	/* memcmp 와 memcpy로 바꾸어야 하는가? */
	for(i=0; i<MAX_MON_RECORDS; i++) {
		if(HASVALUE(obj->m->r[i].appID) && STRCMP( obj->m->r[i].appID, appID) == 0 ) {
			time_t t;
			pid_t	current_pid;

			fq_log(obj->l, FQ_LOG_DEBUG, "'%s' was found in shared memory.", appID);
			obj->m->r[i].action_flag = action_flag;
			t = time(0);
			obj->m->r[i].heartbeat_time = t; /* update heartbeat_time; */
			current_pid = getpid();
			if( current_pid != obj->m->r[i].pid ) {
				fq_log(obj->l, FQ_LOG_EMERG, "Process was restarted. curent_pid=[%d]", current_pid);
			}
			obj->m->r[i].pid = current_pid;
			obj->m->r[i].write_count++;
			sprintf(obj->last_send_appID, "%s", appID);

			if( HASVALUE(appDesc) ) { /* appDesc값이 들어오면 매번 변경한다 */
				memset(obj->m->r[i].appDesc, 0x00, sizeof(obj->m->r[i].appDesc));
				sprintf(obj->m->r[i].appDesc, "%s", appDesc);
			}

			found_flag = 1;
			fq_log(obj->l, FQ_LOG_DEBUG, "Already registed. write_count=[%ld].", obj->m->r[i].write_count);
			break;
		}
	}

	if( !found_flag ) {
		fq_log(obj->l, FQ_LOG_DEBUG, "'%s' was not found in shared memory.", appID);
		for(i=0; i<MAX_MON_RECORDS; i++) {
			if( !HASVALUE(obj->m->r[i].appID) ) {
				time_t t;

				/* new registration */
				sprintf(obj->m->r[i].appID, "%s", appID);
				sprintf(obj->m->r[i].path, "%s", path);
				sprintf(obj->m->r[i].qname, "%s", qname);
				sprintf(obj->m->r[i].appDesc, "%s", appDesc);
				obj->m->r[i].action_flag = action_flag;

				obj->m->r[i].write_count = 1;
				obj->m->r[i].pid = getpid();

				t = time(0);
				obj->m->r[i].heartbeat_time = t;  /* update heartbeat_time; */

				found_flag = 1;
				obj->m->current++;
				sprintf(obj->last_send_appID, "%s", appID);

				fq_log(obj->l, FQ_LOG_DEBUG, "New registed. write_count=[%ld].", obj->m->r[i].write_count);
				break;
			}
		}
		if(!found_flag) {
			fq_log(obj->l, FQ_LOG_ERROR, "Table is full.");
			obj->sema_obj->on_sem_unlock(obj->sema_obj);
			FQ_TRACE_EXIT(l);
			return(FALSE);
		}
	}
	fq_log(obj->l, FQ_LOG_DEBUG, "current use_rec_count is [%d] found_flag is [%d].", obj->m->current, found_flag);

	obj->sema_obj->on_sem_unlock(obj->sema_obj);

END:
	FQ_TRACE_EXIT(l);
    return(TRUE);
}

static int
on_touch(fq_logger_t *l, char *appID, action_flag_t action_flag,  monitor_obj_t *obj)
{
	int i;
	int found_flag=0;

	FQ_TRACE_ENTER(l);

	fq_log(l, FQ_LOG_DEBUG, "------[%s] last_send_time: [%ld] : current=[%ld]", appID, obj->last_send_time, time(0));

	if( obj->last_send_time == time(0) &&  strcmp(appID, obj->last_send_appID) == 0 ) {
		fq_log(l, FQ_LOG_DEBUG, "update skipped. (%s)", appID);
		goto END;
	}

	obj->sema_obj->on_sem_lock(obj->sema_obj);

	obj->last_send_time = time(0);

	/*
	 - 검색해서 동일한 레코드가 있으면 그 인덱스에 업데이트한다.
	 - 검색해서 없으면 새로운 인덱스를 할당하고 그곳에 업데이트한다.
	*/

	/* memcmp 와 memcpy로 바꾸어야 하는가? */
	for(i=0; i<MAX_MON_RECORDS; i++) {
		if(HASVALUE(obj->m->r[i].appID) && STRCMP( obj->m->r[i].appID, appID) == 0 ) {
			time_t t;
			pid_t	current_pid;

			fq_log(obj->l, FQ_LOG_DEBUG, "'%s' was found in shared memory.", appID);
			obj->m->r[i].action_flag = action_flag;
			t = time(0);
			obj->m->r[i].heartbeat_time = t; /* update heartbeat_time; */
			current_pid = getpid();
			if( current_pid != obj->m->r[i].pid ) {
				fq_log(obj->l, FQ_LOG_EMERG, "Process was restarted. curent_pid=[%d]", current_pid);
			}
			obj->m->r[i].pid = current_pid;
			obj->m->r[i].write_count++;
			sprintf(obj->last_send_appID, "%s", appID);

			found_flag = 1;
			fq_log(obj->l, FQ_LOG_DEBUG, "Already registed. write_count=[%ld].", obj->m->r[i].write_count);
			break;
		}
	}

	fq_log(obj->l, FQ_LOG_DEBUG, "current use_rec_count is [%d] found_flag is [%d].", obj->m->current, found_flag);

	obj->sema_obj->on_sem_unlock(obj->sema_obj);

END:
	FQ_TRACE_EXIT(l);
    return(TRUE);
}

static int 
on_getlist_mon(fq_logger_t *l, monitor_obj_t *obj, int *use_rec_count,  char **all_string)
{
	int 	rtn=-1;
	int		i;
	char	one[256];
	char	all[65535];
	int		real_count=0;

	FQ_TRACE_ENTER(l);

	memset(all, 0x00, sizeof(all));

	obj->sema_obj->on_sem_lock(obj->sema_obj);

	fq_log(l, FQ_LOG_DEBUG, "Retrieve all start count=[%d]", *use_rec_count);

	for(i=0; i<MAX_MON_RECORDS; i++) {
		if( HASVALUE(obj->m->r[i].appID )) {
			time_t	now;
			char	heartbeat_result;
			char	buf[32];

			memset(one, 0x00, sizeof(one));
			
			now = time(0);

			if( obj->m->r[i].action_flag == FQ_ERR_ACTION ) {
				heartbeat_result = 'E';
			}
			else {
				if( (now - obj->m->r[i].heartbeat_time) > MAX_MON_DELAY_SECOND ) {
					heartbeat_result = 'F';
				}
				else {
					heartbeat_result = 'T';
				}
			}

			sprintf(one, "%s,%s,%s,%s,%s,%c,%d`", 
				obj->m->r[i].appID,
				obj->m->r[i].path,
				obj->m->r[i].qname,
				obj->m->r[i].appDesc,
				get_str_from_action_flag(obj->m->r[i].action_flag, buf),
				heartbeat_result, 
				obj->m->r[i].pid);

			real_count++;
			
			fq_log(l, FQ_LOG_DEBUG, "[%d]-th: %s", i, one);
			strcat(all, one);
		}
	}

	*all_string = strdup(all);

	fq_log(l, FQ_LOG_DEBUG, "Retrieve all conut=[%d] [%s]\n", obj->m->current, all);

	obj->m->current = real_count;
	*use_rec_count = obj->m->current;

	rtn = real_count;

	obj->sema_obj->on_sem_unlock(obj->sema_obj);

	FQ_TRACE_EXIT(l);
	return(rtn);
}

static
int on_get_app_status( fq_logger_t *l, monitor_obj_t *obj, char *appID, char *status)
{
	int     found_flag = 0;
    int     rtn;
    int     i;

	FQ_TRACE_ENTER(l);

	obj->sema_obj->on_sem_lock(obj->sema_obj);

	for(i=0; i<MAX_MON_RECORDS; i++) {

		if(HASVALUE(obj->m->r[i].appID) && STRCMP( obj->m->r[i].appID, appID) == 0 ) {
            time_t now;

            now = time(0);

            if( obj->m->r[i].action_flag == FQ_ERR_ACTION ) {
                *status = 'E';
            }
            else {
                if( (now - obj->m->r[i].heartbeat_time) > MAX_MON_DELAY_SECOND ) {
                    *status = 'F';
                }
                else {
                    *status = 'T';
                }
            }
            found_flag = 1;
        }
    }

    if( !found_flag ) {
        fprintf(stderr, "Cannot found appID(%s)\n", appID);
        rtn =-1;
    }
    else {
        rtn = found_flag;
    }

	obj->sema_obj->on_sem_unlock(obj->sema_obj);

	FQ_TRACE_EXIT(l);
    return(rtn);
}


int close_monitor_obj( fq_logger_t *l, monitor_obj_t **obj)
{
	int rc;

	FQ_TRACE_ENTER(l);
	
#ifdef OS_SOLARIS
	if( (rc=shmdt((char *)(*obj)->m)) < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "shmde() error. reason=[%s]", strerror(errno));
		FQ_TRACE_EXIT(l);
		return(FALSE);
	}
#else
	if( (rc=shmdt((const void *)(*obj)->m)) < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "shmde() error. reason=[%s]", strerror(errno));
		FQ_TRACE_EXIT(l);
		return(FALSE);
	}
#endif

	SAFE_FREE(*obj);

	FQ_TRACE_EXIT(l);
	return(TRUE);
}

int unlink_monitor( fq_logger_t *l)
{
	key_t key;
	int rc;
	int	shm_id;

	FQ_TRACE_ENTER(l);

	key = ftok(FQ_MONITORING_KEY_PATH, FQ_MONITORING_SHM_CHAR_KEY );

	if ((shm_id = shmget(key, sizeof(monitor_t), 0)) == -1) {
		fq_log(l, FQ_LOG_ERROR, "shmget() error. reason=[%s]", strerror(errno));
		FQ_TRACE_EXIT(l);
		return(FALSE);
	}
	if (shmctl(shm_id, IPC_RMID, NULL) == -1) {
		fq_log(l, FQ_LOG_ERROR, "shmctl( IPC_RMID ) error. reason=[%s]", strerror(errno));
		FQ_TRACE_EXIT(l);
        return(FALSE);
    }
	fq_log(l, FQ_LOG_DEBUG, "Shared memory unlink OK. (for monitor) ");

	rc = unlink_semaphore( l, FQ_MONITORING_KEY_PATH, FQ_MONITORING_SHM_CHAR_KEY);
	if(rc == FALSE) {
		fq_log(l, FQ_LOG_ERROR, "unlink_semaphore() error.");
		 FQ_TRACE_EXIT(l);
        return(FALSE);
    }
	fq_log(l, FQ_LOG_DEBUG, "unlink samephore OK. (for monitor)");

	FQ_TRACE_EXIT(l);
	return(TRUE);
}
action_flag_t 
get_mon_action_flag(char *str_action)
{
	if(STRCMP(str_action, "FQ_DE_ACTION") == 0) {
		return(FQ_DE_ACTION);
	}
	else if( STRCMP(str_action, "FQ_EN_ACTION") == 0) {
		return(FQ_EN_ACTION);
	}
	else if( STRCMP(str_action, "FQ_NO_ACTION") == 0) {
		return(FQ_NO_ACTION);
	}
	else {
		return(FQ_ERR_ACTION);
    } 
}

char *
get_str_from_action_flag(action_flag_t action_flag, char *dst)
{
	if(action_flag == FQ_EN_ACTION) 
		sprintf(dst, "%s", "FQ_EN_ACTION");
	else if(action_flag == FQ_DE_ACTION) 
		sprintf(dst, "%s", "FQ_DE_ACTION");
	else if(action_flag == FQ_NO_ACTION) 
		sprintf(dst, "%s", "FQ_NO_ACTION");
	else
		sprintf(dst, "%s", "FQ_ERR_ACTION");

	return(dst);
}

static int
on_delete_app_in_monitortable(fq_logger_t *l, char *appID, monitor_obj_t *obj)
{
	int i;

	FQ_TRACE_ENTER(l);

	obj->sema_obj->on_sem_lock(obj->sema_obj);

	for(i=0; i<MAX_MON_RECORDS; i++) {
		if(HASVALUE(obj->m->r[i].appID) && STRCMP( obj->m->r[i].appID, appID) == 0 ) {
			time_t t;

			memset(obj->m->r[i].appID, 0x00, sizeof(obj->m->r[i].appID) ); /* clear appID */
			memset(obj->m->r[i].path, 0x00, sizeof(obj->m->r[i].path) ); /* clear path */
			memset(obj->m->r[i].qname, 0x00, sizeof(obj->m->r[i].qname) ); /* clear qname */
			obj->m->r[i].action_flag = FQ_NO_ACTION;
			t = time(0);
			obj->m->r[i].heartbeat_time = t; /* update heartbeat_time; */
			obj->m->r[i].write_count = 0L;

			fq_log(obj->l, FQ_LOG_DEBUG, "Deleted %s.", appID);
			break;
		}
	}
	obj->sema_obj->on_sem_unlock(obj->sema_obj);

	FQ_TRACE_EXIT(l);
    return(TRUE);
}

#if 0
typedef struct _monitor_touch_thread_param {
    fq_logger_t     *l;
	char	*appID;
	char	*qpath;
	char	*qname;
	char	*desc;
	action_flag_t action_flag;
	int		period;
} monitor_touch_thread_param_t;
#endif

void *monitor_touch_thread(void *arg)
{
	monitor_touch_thread_param_t	*env_param = (monitor_touch_thread_param_t *)arg;
	monitor_obj_t   *mobj=NULL;
	int rc;

	rc =  open_monitor_obj(env_param->l,  &mobj);
	CHECK(rc==TRUE);

    while(1) {  
		rc = mobj->on_send_action_status(env_param->l, 
										env_param->appID, 
										env_param->qpath,
										env_param->qname,
										env_param->desc,
										env_param->action_flag,
										mobj);
		if( rc != TRUE ) {
			fq_log(env_param->l, FQ_LOG_ERROR, "ERROR: on_send_action_status.");
			break;
		}

		fq_log(env_param->l, FQ_LOG_DEBUG, "Success! on_send_action_status().");
		sleep(env_param->period);
    }

	close_monitor_obj(env_param->l, &mobj);

	exit(EXIT_FAILURE);
}

bool MakeLinkedList_filequeues(fq_logger_t *l, linkedlist_t *ll  )
{
	fq_container_t *fq_c = NULL;
	
	FQ_TRACE_ENTER(l);

	int rc = load_fq_container(l, &fq_c);
	
    if( rc != TRUE ) {
        fq_log(l, FQ_LOG_ERROR, "load_fq_container() error.");
		FQ_TRACE_EXIT(l);
		return false;
    }

	dir_list_t *d=NULL;

	/* scan cotainer */
	for( d=fq_c->head; d!=NULL; d=d->next) {
		fq_node_t *f;
		for( f=d->head; f!=NULL; f=f->next) {
			fq_log(l, FQ_LOG_DEBUG, "KEY: [%s/%s].", d->dir_name, f->qname);
			// fprintf( stdout, "KEY: [%s/%s].\n", d->dir_name, f->qname);

			fqueue_list_t *tmp=NULL;
			tmp = (fqueue_list_t *)calloc(1, sizeof(fqueue_list_t));

			sprintf(tmp->qpath, "%s", d->dir_name);
			sprintf(tmp->qname, "%s", f->qname);

			char key[512];
			sprintf(key, "%s/%s", d->dir_name, f->qname);

			ll_node_t *ll_node=NULL;
			ll_node = linkedlist_put(ll, key, (void *)tmp, sizeof(char)+sizeof(fqueue_list_t));
			CHECK(ll_node);

			if(tmp) free(tmp);
		}
	}

	if(fq_c) fq_container_free(&fq_c);

	FQ_TRACE_EXIT(l);
		
	return true;
}

bool del_Exclude_Queues_in_LinkedList(fq_logger_t *l, linkedlist_t *ll, char *exclude_filename )
{
	CHECK(ll);
	CHECK(exclude_filename);

	file_list_obj_t *exclude_q_list = NULL;

	int rc;
	rc = open_file_list(l, &exclude_q_list, exclude_filename);
	CHECK( rc == TRUE );

	FILELIST *this_entry = NULL;

	this_entry = exclude_q_list->head;
	while( this_entry->next && this_entry->value ) {
		linkedlist_del(ll , this_entry->value);

		this_entry = this_entry->next;
	}
	close_file_list(l, &exclude_q_list);

	return(true);
}

bool Make_Array_from_Hash_QueueInfo_linkedlist( fq_logger_t *l, hashmap_obj_t *hash_obj,  linkedlist_t *ll, HashQueueInfo_t Array[]  )
{
	FQ_TRACE_ENTER(l);

	ll_node_t *p;
	int	array_index;
	
	for(p=ll->head, array_index=0; p!=NULL; p=p->next, array_index++) {
		fqueue_list_t *tmp;

		tmp = (fqueue_list_t *) p->value;

		char *hash_value = NULL;

		sprintf(Array[array_index].key, "%s",  p->key);

		char *ts1=strdup(p->key);
		char *ts2=strdup(p->key);

		char *qpath=dirname(ts1);
		char *qname=basename(ts2);

		sprintf(Array[array_index].qpath, "%s", qpath);
		sprintf(Array[array_index].qname, "%s", qname);

		free(ts1);
		free(ts2);

		int rc;
		rc = GetHash(l, hash_obj, p->key, &hash_value);
		if( rc == TRUE ) {
			// printf("key=%s, found: value=[%s]\n", p->key, hash_value);

			delimiter_list_obj_t    *obj=NULL;
			delimiter_list_t *this_entry=NULL;
	
			int rc;
			rc = open_delimiter_list(l, &obj, hash_value, '|');
			CHECK(rc == TRUE);

			fq_log(l, FQ_LOG_DEBUG, "items of hash value=[%d].", obj->count);

			CHECK(obj->count == 16);

			this_entry = obj->head;
			int t_no;
			for( t_no=0;  (this_entry->next && this_entry->value); t_no++ ) {
				// printf("[%s], ", this_entry->value);

				switch(t_no) {
					case 0:
						Array[array_index].msglen = atoi(this_entry->value);
						break;
					case 1:
						Array[array_index].max_rec_cnt = atoi(this_entry->value);
						break;
					case 2:
						Array[array_index].gap = atoi(this_entry->value);
						break;
					case 3:
						Array[array_index].usage = atof(this_entry->value);
						break;
					case 4:
						Array[array_index].en_competition = atol(this_entry->value);
						break;
					case 5:
						Array[array_index].de_competition = atol(this_entry->value);
						break;
					case 6:
						Array[array_index].big = atoi(this_entry->value);
						break;
					case 7:
						Array[array_index].en_tps = atoi(this_entry->value);
						break;
					case 8:
						Array[array_index].de_tps = atoi(this_entry->value);
						break;
					case 9:
						Array[array_index].en_sum = atol(this_entry->value);
						break;
					case 10:
						Array[array_index].de_sum = atol(this_entry->value);
						break;
					case 11:
						Array[array_index].status = atoi(this_entry->value);
						break;
					case 12:
						Array[array_index].shmq_flag = atoi(this_entry->value);
						break;
					case 13:
						sprintf(Array[array_index].desc, "%s", this_entry->value);
						break;
					case 14:
						Array[array_index].last_en_time = atol(this_entry->value);
						break;
					case 15:
						Array[array_index].last_de_time = atol(this_entry->value);
						break;
					default:
						break;

				}
				this_entry = this_entry->next;
			}
			// printf("\n");
			close_delimiter_list(l, &obj);
	
		} else {
			fq_log(l, FQ_LOG_ERROR, "not found.\n");
		}
		SAFE_FREE(hash_value);
	}

	FQ_TRACE_EXIT(l);
		
	return true;
}
#if 1
static bool find_a_queue_info( fq_logger_t *l, hashmap_obj_t *hash_obj,  linkedlist_t *ll, char *key, HashQueueInfo_t *dst )
{
	FQ_TRACE_ENTER(l);

	ll_node_t *p;
	int	array_index;
	
	for(p=ll->head, array_index=0; p!=NULL; p=p->next, array_index++) {
		if( strcmp(key, p->key) != 0 ) {
			continue;
		}

		char *ts1=strdup(p->key);
		char *ts2=strdup(p->key);

		char *qpath=dirname(ts1);
		char *qname=basename(ts2);

		sprintf(dst->qpath, "%s", qpath);
		sprintf(dst->qname, "%s", qname);

		free(ts1);
		free(ts2);

		int rc;
		char *hash_value = NULL;
		rc = GetHash(l, hash_obj, p->key, &hash_value);

		if( rc == TRUE ) {
			fqlist_t *ll = fqlist_new('A', "delimiter");
			CHECK(ll);

			int value_count;
			value_count = MakeListFromDelimiterMsg( ll, hash_value, '|', 0 );
			fq_log(l, FQ_LOG_DEBUG, "value count = %d", value_count);

			tlist_t *t = ll->list;
			node_t *p;

			int t_no=0;
			for (  p = t->head ; p != NULL ; p=p->next, t_no++ ) {
				switch(t_no) {
					case 0:
						dst->msglen = atoi(p->value);
						break;
					case 1:
						dst->max_rec_cnt = atoi(p->value);
						break;
					case 2:
						dst->gap = atoi(p->value);
						break;
					case 3:
						dst->usage = atof(p->value);
						break;
					case 4:
						dst->en_competition = atol(p->value);
						break;
					case 5:
						dst->de_competition = atol(p->value);
						break;
					case 6:
						dst->big = atoi(p->value);
						break;
					case 7:
						dst->en_tps = atoi(p->value);
						break;
					case 8:
						dst->de_tps = atoi(p->value);
						break;
					case 9:
						dst->en_sum = atol(p->value);
						break;
					case 10:
						dst->de_sum = atol(p->value);
						break;
					case 11:
						dst->status = atoi(p->value);
						break;
					case 12:
						dst->shmq_flag = atoi(p->value);
						break;
					case 13:
						sprintf(dst->desc, "%s", p->value);
						break;
					case 14:
						dst->last_en_time = atol(p->value);
						break;
					case 15:
						dst->last_de_time = atol(p->value);
						break;
					default:
						break;

				}
			} // for end.
			if(ll) fqlist_free(&ll);
			SAFE_FREE(hash_value);

			break;
		} else {
			fq_log(l, FQ_LOG_ERROR,  "[%s] is not found in hashmap.", p->key);
		}

	}// for end.

	if( dst->msglen > 0 ) {
		FQ_TRACE_EXIT(l);
		return true;
	}
	else {
		FQ_TRACE_EXIT(l);
		return false;
	}
}
#else
static bool find_a_queue_info( fq_logger_t *l, hashmap_obj_t *hash_obj,  linkedlist_t *ll, char *key, HashQueueInfo_t *dst )
{
	FQ_TRACE_ENTER(l);

	ll_node_t *p;
	int	array_index;
	
	for(p=ll->head, array_index=0; p!=NULL; p=p->next, array_index++) {
		fqueue_list_t *tmp;

		tmp = (fqueue_list_t *) p->value;

		if( strcmp(key, p->key) != 0 ) {
			continue;
		}

		char *ts1=strdup(p->key);
		char *ts2=strdup(p->key);

		char *qpath=dirname(ts1);
		char *qname=basename(ts2);

		printf("qpath=[%s]\n", qpath);
		printf("qname=[%s]\n", qname);

		sprintf(dst->qpath, "%s", qpath);
		sprintf(dst->qname, "%s", qname);

		free(ts1);
		free(ts2);

		int rc;
		char *hash_value = NULL;
		rc = GetHash(l, hash_obj, p->key, &hash_value);

		if( rc == TRUE ) {
			printf("found: value=[%s]\n", hash_value);

			int delimiter = '|';
			char buf1[36];
			char buf2[36];
			char buf3[36];
			char buf4[36];
			char buf5[36];
			char buf6[36];
			char buf7[36];
			char buf8[36];
			char buf9[36];
			char buf10[36];
			char buf11[36];
			char buf12[36];
			char buf13[36];
			char buf14[128];

			fq_sscanf(delimiter, hash_value, "%s%s%s%s%s%s%s%s%s%s%s%s%s%s", 
				buf1, buf2, buf3, buf4, buf5, buf6, buf7, buf8, buf9, buf10, buf11, buf12, buf13, buf14);

			dst->msglen = atoi(buf1);
			dst->max_rec_cnt = atoi(buf2);
			dst->gap = atoi(buf3);
			dst->usage = atof(buf4);
			dst->en_competition = atoi(buf5);
			dst->de_competition = atoi(buf6);
			dst->big = atoi(buf7);
			dst->en_tps = atoi(buf8);
			dst->de_tps = atoi(buf9);
			dst->en_sum = atol(buf10);
			dst->de_sum = atol(buf11);
			dst->status = atoi(buf12);
			dst->shmq_flag = atoi(buf13);
			sprintf(dst->desc, "%s", buf14);
			
#if 0
			printf("%d|%d|%d|%f|%ld|%ld|%d|%d|%d|%ld|%ld|%d|%d|%s\n", 
					dst->msglen, dst->max_rec_cnt, dst->gap, dst->usage, dst->en_competition,  dst->de_competition,  dst->big ,
					dst->en_tps, dst->de_tps, dst->en_sum,  dst->de_sum, dst->status,  dst->shmq_flag, dst->desc );
#endif
		} else {
			fq_log(l, FQ_LOG_ERROR,  "[%s] is not found in hashmap.", p->key);
		}
		SAFE_FREE(hash_value);
	}// for end.

	if( dst->msglen > 0 ) {
		FQ_TRACE_EXIT(l);
		return true;
	}
	else {
		FQ_TRACE_EXIT(l);
		return false;
	}
}
#endif

#if 0
HashQueueInfo_t *get_a_queue_info_in_hashmap( fq_logger_t *l, char *key)
{
	FQ_TRACE_ENTER(l);

	linkedlist_t *ll = linkedlist_new("file queue linkedlist.");
	CHECK(ll);

	hashmap_obj_t *hash_obj=NULL;
	int rc;
	rc = OpenHashMapFiles(l, "/home/ums/fq/hash", "ums", &hash_obj);
    CHECK(rc==TRUE);

	bool tf = MakeLinkedList_filequeues(l, ll);
	CHECK(tf);

	HashQueueInfo_t *p;
	p = (HashQueueInfo_t *)calloc(1, sizeof(HashQueueInfo_t));
	CHECK(p);

	tf = find_a_queue_info(l, hash_obj, ll, key, p);
	CHECK(tf);

	if(ll) linkedlist_free(&ll);
	if(hash_obj) CloseHashMapFiles(l, &hash_obj);

	FQ_TRACE_EXIT(l);

	return(p);
}
#else 
HashQueueInfo_t *get_a_queue_info_in_hashmap( fq_logger_t *l, char *hash_path, char *hash_name,  char *key)
{
	HashQueueInfo_t *p=NULL;

	FQ_TRACE_ENTER(l);

	p = calloc(1, sizeof(HashQueueInfo_t));
	linkedlist_t *ll = linkedlist_new("file queue linkedlist.");
	CHECK(ll);

	hashmap_obj_t *hash_obj=NULL;
	int rc;
	rc = OpenHashMapFiles(l, hash_path, hash_name, &hash_obj);
    CHECK(rc==TRUE);

	bool tf = MakeLinkedList_filequeues(l, ll);
	CHECK(tf);

	p = (HashQueueInfo_t *)calloc(1, sizeof(HashQueueInfo_t));
	CHECK(p);

	tf = find_a_queue_info(l, hash_obj, ll, key, p);
	CHECK(tf);
	if(hash_obj) CloseHashMapFiles(l, &hash_obj);

	if(ll) linkedlist_free(&ll);

	FQ_TRACE_EXIT(l);

	return(p);
}
#endif
bool get_a_queue_info_in_hashmap_2( fq_logger_t *l, char *hash_path, char *hash_name,  char *key, HashQueueInfo_t **dst)
{
	HashQueueInfo_t *p=NULL;

	FQ_TRACE_ENTER(l);

	p = calloc(1, sizeof(HashQueueInfo_t));
	linkedlist_t *ll = linkedlist_new("file queue linkedlist.");
	CHECK(ll);

	hashmap_obj_t *hash_obj=NULL;
	int rc;
	rc = OpenHashMapFiles(l, hash_path, hash_name, &hash_obj);
    CHECK(rc==TRUE);

	bool tf = MakeLinkedList_filequeues(l, ll);
	CHECK(tf);

	p = (HashQueueInfo_t *)calloc(1, sizeof(HashQueueInfo_t));
	CHECK(p);

	tf = find_a_queue_info(l, hash_obj, ll, key, p);
	CHECK(tf);
	if(hash_obj) CloseHashMapFiles(l, &hash_obj);

	if(ll) linkedlist_free(&ll);

	*dst = p;
	FQ_TRACE_EXIT(l);

	return(true);
}

