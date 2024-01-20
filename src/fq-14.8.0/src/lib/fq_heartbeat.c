/*
** fq_heartbeat.c
*/
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

// #include "fqueue_sub.h"
#include "fqueue.h"
#include "fq_logger.h"
#include "fq_common.h"
#include "fq_heartbeat.h"
#include "fq_rlock.h"
#include "fq_scanf.h"

static decide_returns_t on_decide_Master_Slave( fq_logger_t *l , heartbeat_obj_t *obj);
static int on_timestamp( fq_logger_t *l, heartbeat_obj_t *obj, int master_slave_flag);
static int on_print_table( fq_logger_t *l, heartbeat_obj_t *obj);
static int on_getlist( fq_logger_t *l, heartbeat_obj_t *obj, int *count, char **all_string);
static int on_change( fq_logger_t *l, heartbeat_obj_t *obj, char *dist_name, char *prog_name);
static int on_add_table( fq_logger_t *l, heartbeat_obj_t *obj, char *dist_name, char *prog_name, char *host_name);
static int on_del_table( fq_logger_t *l, heartbeat_obj_t *obj, char *dist_name, char *prog_name, char *host_name);
static int on_set_status( fq_logger_t *l, heartbeat_obj_t *obj, char *dist_name, char *prog_name, char *host_name, p_status_t status);
static int on_get_status( fq_logger_t *l, heartbeat_obj_t *obj, char *dist_name, char *prog_name, char *host_name, p_status_t *status);
static int is_expired( time_t stamp);
static int add_kill_list( fq_logger_t *l, heartbeat_obj_t *obj, char *hostname, pid_t pid);

#define MAX_LIST_ITEMS 3
#define DELIMITER_CHAR '|'

int init_heartbeat_table( fq_logger_t *l, fqlist_t *t, char *path, char *DB_filename, char *lock_filename )
{
	char DB_pathfile[128];
	char dot_DB_filename[128];
	int	DB_fd = -1;
	heartbeat_t *h=NULL;
	node_t *p=NULL;
	

	FQ_TRACE_ENTER(l);

	if ( !t ) {
		fq_log(l, FQ_LOG_ERROR, "There is no value of list. illegal request.");
		goto return_FALSE;
	}
	if ( !path || !DB_filename || !lock_filename ) {
		fq_log(l, FQ_LOG_ERROR, "There is no values of path/filename s. illegal request.");
		goto return_FALSE;
	}

	if ( create_lock_table(path, lock_filename, MAX_HEARTBEAT_NODES) < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "create_lock_table('%s') error. \n", lock_filename);
		goto return_FALSE;
	}
	fq_log(l, FQ_LOG_INFO, "Lock file for heartbeat was made successfull in %s/%s.",  path, lock_filename);

	sprintf(DB_pathfile, "%s/.%s", path, DB_filename);
	sprintf(dot_DB_filename, ".%s", DB_filename);

	if( is_file(DB_pathfile) ) {
		unlink(DB_pathfile);
	}

	if( !can_access_file(l, DB_pathfile)) {
		int rc;

		fq_log(l, FQ_LOG_DEBUG, "There is no heartbeat DB file.  can not access to '%s'.", DB_pathfile);

		if( (DB_fd=create_dummy_file(l, path, dot_DB_filename, sizeof(heartbeat_t)))  < 0) {
			fq_log(l, FQ_LOG_ERROR, "create_dummy_file() error. path=[%s] DB_filename=[%s]", path, dot_DB_filename);
			goto return_FALSE;
		}

		fq_log(l, FQ_LOG_INFO, "new heartbeat DB file is created. [%s]", DB_pathfile);
		close(DB_fd);
	}

	if( (DB_fd=open(DB_pathfile, O_RDWR|O_SYNC, 0666)) < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "DB file[%s] can not open.", DB_pathfile);
		goto return_FALSE;
	}

	if( (h = (heartbeat_t *)fq_mmapping(l, DB_fd, sizeof(heartbeat_t), (off_t)0)) == NULL ) {
		fq_log(l, FQ_LOG_ERROR, "fq_mmapping('%s') error.", DB_pathfile);
		goto return_FALSE;
	}

	if( strcmp(t->list->key, "PROC_LIST") != 0 ) {
		fq_log(l, FQ_LOG_ERROR, "illegal request. cache value error. key is not PROC_LIST. [%s]", t->list->key);
		goto return_FALSE;
	}
	
	fq_log(l, FQ_LOG_INFO, "In the heartbeat Total elements is [%d].", t->list->length);
	
	h->registed_process_cnt = 0;

	for ( p=t->list->head; p != NULL ; p = p->next ) {
		int index;
		char 	*pp = NULL;
		char	*qq = NULL;
		char	distname[DIST_NAME_LEN+1];
		char	progname[PROG_NAME_LEN+1];
		char	hostname[HOST_NAME_LEN+1];
		int		i;
		char	*dst[MAX_LIST_ITEMS];
		int		rc;
		
		pp = p->value;

		index = atoi(p->key);

		rc = delimiter_parse(p->value, DELIMITER_CHAR, MAX_LIST_ITEMS, dst);
		if( rc == FALSE ) {
			fq_log(l, FQ_LOG_ERROR, "cache data error.");
			goto return_FALSE;
		}

		sprintf(distname, "%s", dst[0]);
		sprintf(progname, "%s", dst[1]);
		sprintf(hostname, "%s", dst[2]);
		for(i=0; i<MAX_LIST_ITEMS; i++) {
			if(dst[i]) free(dst[i]);
		}

		fq_log(l, FQ_LOG_INFO, "[%03d]-th: distname=[%s], progname=[%s], hostname[%s]", index, distname, progname, hostname);

		/* regist process in mmap table */
		sprintf( h->nodes[index].dist_name, "%s", distname);
		sprintf( h->nodes[index].prog_name, "%s", progname);
		sprintf( h->nodes[index].master_host_name, "%s", hostname);

		/* master area init */
		h->nodes[index].m.pid = -1;
		h->nodes[index].m.time = 0L;
		h->nodes[index].m.status = INIT_STATUS;
		h->nodes[index].m.hostname[0] = 0x00;

		/* slave area init */
		h->nodes[index].s.pid = -1;
		h->nodes[index].s.time = 0L;
		h->nodes[index].s.status = INIT_STATUS;
		h->nodes[index].s.hostname[0] = 0x00;

		h->registed_process_cnt++;
	}


	if( DB_fd > 0 ) close(DB_fd);
	if( h ) fq_munmap(l, (void *)h, sizeof(heartbeat_t));

	FQ_TRACE_EXIT(l);

	fq_log(l, FQ_LOG_INFO, "init_heartbeat_table() finished successfully.");
	return(TRUE);

return_FALSE:

	if( DB_fd > 0 ) close(DB_fd);
	if( h ) fq_munmap(l, (void *)h, sizeof(heartbeat_t));

	FQ_TRACE_EXIT(l);
	return(FALSE);
}

static int find_index( heartbeat_t *h, char *dist_name, char *prog_name)
{
	int index;

	for(index=0; index<MAX_HEARTBEAT_NODES; index++) {
		if( (strcmp( h->nodes[index].prog_name, prog_name) == 0) &&
			(strcmp( h->nodes[index].dist_name, dist_name) == 0) ) {
			return(index); /* found */
		}
	}
	return(-1); /* not found */
}

static int 
is_expired( time_t stamp)
{
	time_t current_bintime;

	current_bintime = time(0);
	if( (stamp + CHANGING_WAIT_TIME) < current_bintime) {
		return(TRUE);
	}
	else {
		return(FALSE);
	}
}
	
int open_heartbeat_obj( fq_logger_t *l, char* hostname, char *dist_name, char *prog_name, char *path, char *db_filename, char *lock_filename, heartbeat_obj_t **obj)
{
	heartbeat_obj_t *rc=NULL;

	FQ_TRACE_ENTER(l);

	if( !HASVALUE(hostname) || !HASVALUE(dist_name) || !HASVALUE(prog_name) ) {
		fq_log(l, FQ_LOG_ERROR, "illgal function call. There is no hostname, dist_name or prog_name");
		goto return_MINUS;

	}
	if( !HASVALUE(path) || !HASVALUE(db_filename) || !HASVALUE(lock_filename) ) {
		fq_log(l, FQ_LOG_ERROR, "illgal function call. There is no path or db_filename or lock_filename");
		goto return_MINUS;
    }

	rc = (heartbeat_obj_t *)calloc(1, sizeof(heartbeat_obj_t));
	if(rc) {
		int i;
		char	tmp[256];
		int my_pid = getpid();

		rc->path = strdup(path);
		rc->db_filename = strdup(db_filename);
		sprintf(tmp, "%s/.%s", path, db_filename);
		rc->db_pathfile = strdup(tmp);

		rc->lock_filename = strdup(lock_filename);
		sprintf(tmp, "%s/.%s.lock", path, lock_filename);
        rc->lock_pathfile = strdup(tmp);

		if( (rc->lockfd = open_lock_table(rc->lock_pathfile) ) < 0 ) {
			fq_log(l, FQ_LOG_ERROR, "open_lock_table('%s') error. \n", rc->lock_pathfile);
			goto return_MINUS;
		}

		if( !can_access_file(l, rc->db_pathfile)) {
			fq_log(l, FQ_LOG_ERROR, "There is no DB_file. [%s]", rc->db_pathfile);
			close(rc->fd);
		}

		rc->fd = 0;
        if( (rc->fd=open(rc->db_pathfile, O_RDWR|O_SYNC, 0666)) < 0 ) {
            fq_log(l, FQ_LOG_ERROR, "DB file[%s] can not open.", rc->db_pathfile);
            goto return_MINUS;
        }

		if( (rc->h = (heartbeat_t *)fq_mmapping(l, rc->fd, sizeof(heartbeat_t), (off_t)0)) == NULL ) {
			fq_log(l, FQ_LOG_ERROR, "fq_mmapping('%s') error.", rc->db_pathfile);
			goto return_MINUS;
        }

		rc->my_index = find_index( rc->h,  dist_name, prog_name);

		if( rc->my_index < 0 ) {
			fq_log(l, FQ_LOG_ERROR, "Unregistered process name. dist_name[%s], prog_name[%s]", dist_name, prog_name);
			goto return_MINUS;
		}

		r_lock_table( rc->lockfd,  rc->my_index );


		rc->hostname = strdup(hostname);
		rc->last_timestamp = time(0);
		rc->h->nodes[rc->my_index].m.pid = getpid();
		rc->h->nodes[rc->my_index].m.status = WORK_STATUS;
		rc->h->nodes[rc->my_index].m.time = rc->last_timestamp;

		rc->l = l;
		rc->on_decide_Master_Slave = on_decide_Master_Slave;
		rc->on_timestamp = on_timestamp;
		rc->on_change = on_change;
		rc->on_add_table = on_add_table;
		rc->on_del_table = on_del_table;
		rc->on_set_status = on_set_status;
		rc->on_get_status = on_get_status;
		rc->on_print_table = on_print_table;
		rc->on_getlist = on_getlist; 
	
		*obj = rc;

		r_unlock_table( rc->lockfd, rc->my_index);

		FQ_TRACE_EXIT(l);
        return(rc->my_index);
    }
	else { /* There is no rc so need not free */
		FQ_TRACE_EXIT(l);
		return(-1);
	}

return_MINUS:

	SAFE_FREE( rc->path );
	SAFE_FREE( rc->db_filename );
	SAFE_FREE( rc->db_pathfile );
	SAFE_FREE( rc->lock_filename );
	SAFE_FREE( rc->lock_pathfile );

	SAFE_FREE( rc->hostname );

	if(rc->fd) close(rc->fd);
	if(rc->lockfd) close(rc->lockfd);

	if(rc->h)  {
		fq_munmap(l, (void *)rc->h, sizeof(heartbeat_t));
		rc->h = NULL;
    }

	SAFE_FREE(*obj);
	
    *obj = NULL;

    FQ_TRACE_EXIT(l);
    return(-1);
}

int 
get_heartbeat_obj( fq_logger_t *l, char *hostname,  char *path, char *db_filename, char *lock_filename, heartbeat_obj_t **obj)
{
	heartbeat_obj_t *rc=NULL;

	FQ_TRACE_ENTER(l);

	if( !HASVALUE(path) || !HASVALUE(db_filename) || !HASVALUE(lock_filename) ) {
		fq_log(l, FQ_LOG_ERROR, "illgal function call. There is no path or db_filename or lock_filename");
		goto return_MINUS;
    }

	rc = (heartbeat_obj_t *)calloc(1, sizeof(heartbeat_obj_t));
	if(rc) {
		int i;
		char	tmp[256];

		rc->fd = 0;
		rc->lockfd = 0;
		rc->h = NULL;
		rc->path = strdup(path);
		rc->db_filename = strdup(db_filename);
		sprintf(tmp, "%s/.%s", path, db_filename);
		rc->db_pathfile = strdup(tmp);

		rc->hostname = strdup(hostname);

		rc->lock_filename = strdup(lock_filename);
		sprintf(tmp, "%s/.%s.lock", path, lock_filename);
        rc->lock_pathfile = strdup(tmp);

		if( (rc->lockfd = open_lock_table(rc->lock_pathfile) ) < 0 ) {
			fq_log(l, FQ_LOG_ERROR, "open_lock_table('%s') error. \n", rc->lock_pathfile);
			goto return_MINUS;
		}

		if( !can_access_file(l, rc->db_pathfile)) {
			fq_log(l, FQ_LOG_ERROR, "There is no DB_file. [%s]", rc->db_pathfile);
			close(rc->fd);
		}

        if( (rc->fd=open(rc->db_pathfile, O_RDWR|O_SYNC, 0666)) < 0 ) {
            fq_log(l, FQ_LOG_ERROR, "DB file[%s] can not open.", rc->db_pathfile);
            goto return_MINUS;
        }

		if( (rc->h = (heartbeat_t *)fq_mmapping(l, rc->fd, sizeof(heartbeat_t), (off_t)0)) == NULL ) {
			fq_log(l, FQ_LOG_ERROR, "fq_mmapping('%s') error.", rc->db_pathfile);
			goto return_MINUS;
        }


		rc->last_timestamp = time(0);
		rc->l = l;
		rc->on_decide_Master_Slave = on_decide_Master_Slave;
		rc->on_timestamp = on_timestamp;
		rc->on_change = on_change;
		rc->on_add_table = on_add_table;
		rc->on_del_table = on_del_table;
		rc->on_set_status = on_set_status;
		rc->on_get_status = on_get_status;
		rc->on_print_table = on_print_table;
		rc->on_getlist = on_getlist;
	
		*obj = rc;

		FQ_TRACE_EXIT(l);
        return(0);
    }
	else { /* There is no rc so need not free */
		FQ_TRACE_EXIT(l);
		return(-1);
	}

return_MINUS:

	SAFE_FREE( rc->path );
	SAFE_FREE( rc->db_filename );
	SAFE_FREE( rc->db_pathfile );
	SAFE_FREE( rc->lock_filename );
	SAFE_FREE( rc->lock_pathfile );

	SAFE_FREE( rc->hostname );

	if(rc->fd) close(rc->fd);

	if(rc->h)  {
		fq_munmap(l, (void *)rc->h, sizeof(heartbeat_t));
		rc->h = NULL;
    }

	SAFE_FREE(*obj);
	
    *obj = NULL;

    FQ_TRACE_EXIT(l);
    return(-1);
}

int regist_controller( fq_logger_t *l, char *hostname, char *progname, int c1c2 )
{
	heartbeat_obj_t *obj=NULL;
	int rc;
	int ret=FALSE;

	FQ_TRACE_ENTER(l);
	
	rc = get_heartbeat_obj( l, hostname,  HB_DB_PATH, HB_DB_NAME, HB_LOCK_FILENAME, &obj);
	if( rc == FALSE ) {
		fq_log(l, FQ_LOG_ERROR, "get_heartbeat_obj() failed.");
		goto return_FALSE;
	}
	
	if( c1c2 == 1 ) {
		obj->h->c1.pid  = getpid();
		sprintf(obj->h->c1.progname, "%s", progname);
	}
	else {
		obj->h->c2.pid  = getpid();
		sprintf(obj->h->c2.progname, "%s", progname);
	}
	ret = TRUE;

return_FALSE:
	FQ_TRACE_EXIT(l);
	return(ret);
}


int 
close_heartbeat_obj( fq_logger_t *l,  heartbeat_obj_t **obj)
{
	FQ_TRACE_ENTER(l);

	SAFE_FREE((*obj)->path);
	SAFE_FREE((*obj)->db_filename);
	SAFE_FREE((*obj)->db_pathfile);
	SAFE_FREE((*obj)->lock_filename);
	SAFE_FREE((*obj)->lock_pathfile);

	SAFE_FREE((*obj)->hostname);

	if( (*obj)->fd > 0 ) {
        close( (*obj)->fd );
        (*obj)->fd = -1;
    }
	if( (*obj)->lockfd > 0 ) {
        close( (*obj)->lockfd );
        (*obj)->lockfd = -1;
    }

	if( (*obj)->h ) {
        fq_munmap(l, (void *)(*obj)->h, sizeof(heartbeat_t));
        (*obj)->h = NULL;
    }

	SAFE_FREE(*obj);

	FQ_TRACE_EXIT(l);
    return(TRUE);
}

static int decide_action( p_status_t status, time_t stamp)
{
	time_t current_bintime;

	if( status == STOP_STATUS || INIT_STATUS ) {
		return(BE_MASTER);
	}
	
	current_bintime = time(0);
	if( (stamp + CHANGING_WAIT_TIME) < current_bintime) {
		return(BE_MASTER);
	}
	else {
		return(BE_SLAVE);
	}
}

static decide_returns_t 
on_decide_Master_Slave( fq_logger_t *l, heartbeat_obj_t *obj)
{
	pid_t my_pid = getpid();
	decide_returns_t action;

	FQ_TRACE_ENTER(l);

	r_lock_table( obj->lockfd, obj->my_index);

	if( obj->h->nodes[obj->my_index].m.status == INIT_STATUS || obj->h->nodes[obj->my_index].m.status == STOP_STATUS || obj->h->nodes[obj->my_index].m.status == PASS_STATUS) {

		if( (obj->h->nodes[obj->my_index].m.status == STOP_STATUS) && (obj->h->nodes[obj->my_index].m.pid > 2) ) {
			int rc;
			rc=add_kill_list( l, obj, obj->h->nodes[obj->my_index].m.hostname, obj->h->nodes[obj->my_index].m.pid);
			if( rc == FALSE) {
				fq_log(l, FQ_LOG_EMERG, "May be killed Heartbeat Controller. Check it");
			}
		}

		action = BE_MASTER;
		sprintf( obj->h->nodes[obj->my_index].m.hostname, "%s", obj->hostname);
		obj->h->nodes[obj->my_index].m.pid = my_pid;
		obj->h->nodes[obj->my_index].m.time = time(0);
		obj->h->nodes[obj->my_index].m.status = WORK_STATUS;

		/* In case Slave becomes Master, Slave and Master PID is same. so clear slave pid/status */
		if( my_pid == obj->h->nodes[obj->my_index].s.pid ) {
			obj->h->nodes[obj->my_index].s.pid = -1;
			obj->h->nodes[obj->my_index].s.status = STOP_STATUS;
			obj->h->nodes[obj->my_index].s.hostname[0] = 0x00;
		}

		r_unlock_table( obj->lockfd, obj->my_index);
		FQ_TRACE_EXIT(l);
		return(action);
	}
	else {
		if( obj->h->nodes[obj->my_index].m.status == WORK_STATUS ) {
			if( is_expired( obj->h->nodes[obj->my_index].m.time) ) {
				int rc;

				rc=add_kill_list( l, obj, obj->h->nodes[obj->my_index].m.hostname, obj->h->nodes[obj->my_index].m.pid);
				if( rc == FALSE) {
					fq_log(l, FQ_LOG_EMERG, "May be killed Heartbeat Controller. Check it");
				}

				action = BE_MASTER;
				sprintf( obj->h->nodes[obj->my_index].m.hostname, "%s", obj->hostname);
				obj->h->nodes[obj->my_index].m.pid = my_pid;
				obj->h->nodes[obj->my_index].m.time = time(0);
				obj->h->nodes[obj->my_index].m.status = WORK_STATUS;

				/* In case Slave becomes Master, Slave and Master PID is same. so clear slave pid/status */
				if( my_pid == obj->h->nodes[obj->my_index].s.pid ) {
					obj->h->nodes[obj->my_index].s.pid = -1;
					obj->h->nodes[obj->my_index].s.status = STOP_STATUS;
					obj->h->nodes[obj->my_index].s.hostname[0] = 0x00;
				}

				r_unlock_table( obj->lockfd, obj->my_index);
				FQ_TRACE_EXIT(l);
				return(action);
			}
			else {  /* master is alive */
				if( strcmp(obj->h->nodes[obj->my_index].m.hostname, obj->hostname) == 0 ) { // dup in same server.
					action = DUP_PROCESS;
					r_unlock_table( obj->lockfd, obj->my_index);
					FQ_TRACE_EXIT(l);
					return(action);
				}
				else {
					if( obj->h->nodes[obj->my_index].s.status == READY_STATUS && (obj->h->nodes[obj->my_index].s.pid != my_pid) && 
							!is_expired( obj->h->nodes[obj->my_index].s.time) ) {

						action = ALREADY_SLAVE_EXIST;
						r_unlock_table( obj->lockfd, obj->my_index);
						FQ_TRACE_EXIT(l);
						return(action);
					}
					else {  /* There is no slave process or myself check */
						if( is_expired(obj->h->nodes[obj->my_index].m.time) ) { // Master is HANG.
							int rc;

							rc=add_kill_list( l, obj, obj->h->nodes[obj->my_index].m.hostname, obj->h->nodes[obj->my_index].m.pid);
							if( rc == FALSE) {
								fq_log(l, FQ_LOG_EMERG, "May be killed Heartbeat Controller. Check it");
							}
								
							action = BE_MASTER;
							sprintf( obj->h->nodes[obj->my_index].m.hostname, "%s", obj->hostname);
							obj->h->nodes[obj->my_index].m.pid = my_pid;
							obj->h->nodes[obj->my_index].m.time = time(0);
							obj->h->nodes[obj->my_index].m.status = WORK_STATUS;

							/* In case Slave becomes Master, Slave and Master PID is same. so clear slave pid/status */
							if( my_pid == obj->h->nodes[obj->my_index].s.pid ) {
								obj->h->nodes[obj->my_index].s.pid = -1;
								obj->h->nodes[obj->my_index].s.status = STOP_STATUS;
								obj->h->nodes[obj->my_index].s.hostname[0] = 0x00;
							}
							
							r_unlock_table( obj->lockfd, obj->my_index);
							FQ_TRACE_EXIT(l);
							return(action);
						}
						else { /* Master is normal */
							action = BE_SLAVE;
							sprintf( obj->h->nodes[obj->my_index].s.hostname, "%s", obj->hostname);
							obj->h->nodes[obj->my_index].s.pid = my_pid;
							obj->h->nodes[obj->my_index].s.time = time(0);
							obj->h->nodes[obj->my_index].s.status = READY_STATUS;
							r_unlock_table( obj->lockfd, obj->my_index);
							FQ_TRACE_EXIT(l);
							return(action);
						}
					}
				}
			}
		}
		else { /* master is not working */
			// if( (obj->h->nodes[obj->my_index].s.pid > 2) && !is_alive_pid_in_Linux( obj->h->nodes[obj->my_index].s.pid) ) {
			if( (obj->h->nodes[obj->my_index].s.pid > 2) && is_expired( obj->h->nodes[obj->my_index].s.time) ) {
				action = BE_SLAVE;
				sprintf( obj->h->nodes[obj->my_index].s.hostname, "%s", obj->hostname);
				obj->h->nodes[obj->my_index].s.pid = my_pid;
				obj->h->nodes[obj->my_index].s.time = time(0);
				obj->h->nodes[obj->my_index].s.status = READY_STATUS;
				r_unlock_table( obj->lockfd, obj->my_index);
				FQ_TRACE_EXIT(l);
				return(action);
			}
			else {
				action = ALREADY_SLAVE_EXIST;
				r_unlock_table( obj->lockfd, obj->my_index);
				FQ_TRACE_EXIT(l);
				return(action);
			}
		}
	}

	r_unlock_table( obj->lockfd, obj->my_index);
	FQ_TRACE_EXIT(l);
	return(-1);
}

/*
** controller will send a kill(pid, SIGUSR1).
** and then A process will be restarted atomaticall.
*/
static int 
add_kill_list( fq_logger_t *l, heartbeat_obj_t *obj, char *hostname, pid_t pid)
{
	int i;

	FQ_TRACE_ENTER(l);

	for(i=0; i<MAX_KILL_LIST; i++) {
		if( obj->h->k[i].pid < 2 ) {
			obj->h->k[i].pid = pid;
			sprintf(obj->h->k[i].hostname, "%s", hostname);
			return(TRUE);
		}
	}
	fq_log(l, FQ_LOG_ERROR, "There is no available space for adding kill pid.");
	FQ_TRACE_EXIT(l);
	return(FALSE);
}


static int 
on_timestamp( fq_logger_t *l, heartbeat_obj_t *obj, int master_slave_flag)
{
	pid_t my_pid = getpid();
	FQ_TRACE_ENTER(l);
	time_t	current;

	current = time(0);
	
	if( (obj->last_timestamp + 3) > current ) { /* Prevent to stamp  too frequently */
		FQ_TRACE_EXIT(l);
		return(TRUE);
	}

	// r_lock_table( obj->lockfd, obj->my_index);

	if( master_slave_flag == MASTER_TIME_STAMP ) {
		if( my_pid != obj->h->nodes[obj->my_index].m.pid ) { /* When Master was HANG, Slave became Master. */
			// r_unlock_table( obj->lockfd, obj->my_index);
			FQ_TRACE_EXIT(l);
			return(FALSE);
		}
		fq_log(l, FQ_LOG_DEBUG, "Master stamped.");
		obj->h->nodes[obj->my_index].m.time = current;
		obj->h->nodes[obj->my_index].m.status = WORK_STATUS;
	}
	else {
		obj->h->nodes[obj->my_index].s.time = current;
		obj->h->nodes[obj->my_index].s.status = READY_STATUS;
		fq_log(l, FQ_LOG_DEBUG, "Slave stamped.");
	}
	obj->last_timestamp = current;	

	// r_unlock_table( obj->lockfd, obj->my_index);

	FQ_TRACE_EXIT(l);

	return(TRUE);
}

static int get_host_number( char *hostname )
{
	if( strcmp(hostname, FQ_HEARTBEAT_HOSTNAME_1) == 0 ) return(1);
	else if(strcmp(hostname, FQ_HEARTBEAT_HOSTNAME_2) == 0 ) return(2);
	else {
		return(-1);
	}
}

static int 
on_change( fq_logger_t *l, heartbeat_obj_t *obj, char *dist_name, char *prog_name)
{
	int i;

	FQ_TRACE_ENTER(l);

	if( dist_name && !prog_name) {
		for(i=0; i<MAX_HEARTBEAT_NODES; i++) {
			if( !HASVALUE(obj->h->nodes[i].dist_name) ) continue;
			if( !HASVALUE(obj->h->nodes[i].m.hostname)) continue;

			r_lock_table( obj->lockfd, i);

			if( strcmp( obj->hostname, obj->h->nodes[i].m.hostname) == 0 ) { 
				int rc;

				if( obj->h->nodes[i].s.status != READY_STATUS ) {
					fq_log(l, FQ_LOG_ERROR, "Slave is not READY status.");
					r_unlock_table( obj->lockfd, i);
                    FQ_TRACE_EXIT(l);
                    return(FALSE);
                }
			
				rc = add_kill_list(l, obj, obj->h->nodes[i].m.hostname, obj->h->nodes[i].m.pid);

				if( rc == FALSE ) {
					fq_log(l, FQ_LOG_ERROR, "kill list is full.");
                    r_unlock_table( obj->lockfd, i);
                    FQ_TRACE_EXIT(l);
                    return(FALSE);
                }
			}
			else {
				fq_log(l, FQ_LOG_ERROR, "In this server(%s), I cannot send signal to master(%s).",
					obj->hostname, obj->h->nodes[i].m.hostname);

				r_unlock_table( obj->lockfd, i);
				FQ_TRACE_EXIT(l);
				return(FALSE);
			}
			r_unlock_table( obj->lockfd, i);
		}
	}
	else if( dist_name && prog_name ) {
		int index;

		index = find_index( obj->h, dist_name, prog_name);
		if( index >= 0 ) {
			r_lock_table( obj->lockfd, i);

			if( strcmp( obj->hostname, obj->h->nodes[i].m.hostname) == 0 ) { 
				int rc;
				if( strcmp(dist_name, obj->h->nodes[i].dist_name) == 0 ) {
					if( obj->h->nodes[i].s.status != READY_STATUS ) {
						fq_log(l, FQ_LOG_ERROR, "Slave is not READY status.");
						r_unlock_table( obj->lockfd, i);
						FQ_TRACE_EXIT(l);
						return(FALSE);
					}
                }
				rc = add_kill_list(l, obj, obj->h->nodes[i].m.hostname, obj->h->nodes[i].m.pid);
                if( rc == FALSE ) {
                    fq_log(l, FQ_LOG_ERROR, "kill list is full.");
                    r_unlock_table( obj->lockfd, i);
                    FQ_TRACE_EXIT(l);
                    return(FALSE);
                }
			}
			else {
				fq_log(l, FQ_LOG_ERROR, "In this server(%s), I cannot send signal to master(%s).",
					obj->hostname, obj->h->nodes[i].m.hostname);

				r_unlock_table( obj->lockfd, i);
				FQ_TRACE_EXIT(l);
				return(FALSE);
			}
			r_unlock_table( obj->lockfd, i);
		}
		else {
			fq_log(l, FQ_LOG_ERROR, "Can't find index with '%s','%s'.", dist_name, prog_name);
			FQ_TRACE_EXIT(l);
			return(FALSE);
		}
	}
	else {
			fq_log(l, FQ_LOG_ERROR, "illegal request. There is no arguments.");
			FQ_TRACE_EXIT(l);
			return(FALSE);
	}

    FQ_TRACE_EXIT(l);

    return (TRUE);
}

static int 
on_add_table( fq_logger_t *l, heartbeat_obj_t *obj, char *dist_name, char *prog_name, char *host_name)
{
	int i;

	FQ_TRACE_ENTER(l);

	if( !dist_name || !prog_name || !host_name ) {
		fq_log(l, FQ_LOG_ERROR, "illegal request. There is no function arguments.\n");
		FQ_TRACE_EXIT(l);
		return(FALSE);
	}
		
	if( find_index( obj->h, dist_name, prog_name) >= 0 ) {
		fq_log(l, FQ_LOG_ERROR, "Already registed names. [%s][%s]", dist_name, prog_name);
		FQ_TRACE_EXIT(l);
        return(FALSE);
    }

	for(i=0; i<MAX_HEARTBEAT_NODES; i++) {
		if( HASVALUE(obj->h->nodes[i].dist_name) ) continue;
		if( HASVALUE(obj->h->nodes[i].prog_name) ) continue;

		r_lock_table( obj->lockfd, i);

		sprintf(obj->h->nodes[i].dist_name, "%s", dist_name);
		sprintf(obj->h->nodes[i].prog_name, "%s", prog_name);

		obj->h->nodes[i].m.status = INIT_STATUS;
		obj->h->nodes[i].m.pid = -1;

		obj->h->nodes[i].s.status = INIT_STATUS;
		obj->h->nodes[i].s.pid = -1;

		r_unlock_table( obj->lockfd, i);
		FQ_TRACE_EXIT(l);
		fq_log(l, FQ_LOG_INFO, "Successfully Done. on_add_table().");
		return(TRUE);
	}

	fq_log(l, FQ_LOG_ERROR, "Table is full. There is no empty room.");

    FQ_TRACE_EXIT(l);
    return (FALSE);
}
static int 
on_del_table( fq_logger_t *l, heartbeat_obj_t *obj, char *dist_name, char *prog_name, char *host_name)
{
	int index;

	FQ_TRACE_ENTER(l);

	if( !dist_name || !prog_name || !host_name ) {
		fq_log(l, FQ_LOG_ERROR, "illegal request. There is no function arguments.\n");
		FQ_TRACE_EXIT(l);
		return(FALSE);
	}

	if( (index = find_index(obj->h, dist_name, prog_name) ) < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "Can't found same name in table.\n");
        FQ_TRACE_EXIT(l);
        return(FALSE);
    }

	r_lock_table( obj->lockfd, index);

	obj->h->nodes[index].dist_name[0] = 0x00;
	obj->h->nodes[index].prog_name[0] = 0x00;
	obj->h->nodes[index].master_host_name[0] = 0x00;

	obj->h->nodes[index].m.status = INIT_STATUS;
	obj->h->nodes[index].m.pid = -1;

	obj->h->nodes[index].s.status = INIT_STATUS;
	obj->h->nodes[index].s.pid = -1;

	r_unlock_table( obj->lockfd, index);

	FQ_TRACE_EXIT(l);
	fq_log(l, FQ_LOG_INFO, "Successfully Done. on_del_table().");

	return(TRUE);
}
static int 
on_set_status( fq_logger_t *l, heartbeat_obj_t *obj, char *dist_name, char *prog_name, char *host_name, p_status_t status)
{
	int index;

	FQ_TRACE_ENTER(l);

	if( !dist_name || !prog_name || !host_name ) {
		fq_log(l, FQ_LOG_ERROR, "illegal request. There is no function arguments.\n");
		FQ_TRACE_EXIT(l);
		return(FALSE);
	}

	if( (index = find_index(obj->h, dist_name, prog_name) ) < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "Can't found same name in table.\n");
        FQ_TRACE_EXIT(l);
        return(FALSE);
    }

	r_lock_table( obj->lockfd, index);

	obj->h->nodes[index].m.status = status;

	r_unlock_table( obj->lockfd, index);

	FQ_TRACE_EXIT(l);
	fq_log(l, FQ_LOG_INFO, "Successfully Done. on_set_status().index=[%d]", index);

	return(TRUE);
}

static int 
on_get_status( fq_logger_t *l, heartbeat_obj_t *obj, char *dist_name, char *prog_name, char *host_name, p_status_t *status)
{
	int index;
	p_status_t st;

	FQ_TRACE_ENTER(l);

	if( !dist_name || !prog_name || !host_name ) {
		fq_log(l, FQ_LOG_ERROR, "illegal request. There is no function arguments.\n");
		FQ_TRACE_EXIT(l);
		return(FALSE);
	}

	if( (index = find_index(obj->h, dist_name, prog_name) ) < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "Can't found same name in table. dist_name=[%s], prog_name=[%s]\n", dist_name, prog_name);
        FQ_TRACE_EXIT(l);
        return(FALSE);
    }

	st = obj->h->nodes[index].m.status;

	if(st == WORK_STATUS) {
		if( is_alive_pid_in_general( obj->h->nodes[index].m.pid) ) {
			fq_log(l, FQ_LOG_DEBUG, "pid[%d] is alive.\n", obj->h->nodes[index].m.pid);
			if( is_expired( obj->h->nodes[index].m.time ) ) {
				*status = HANG_STATUS;
			}
			else {
				*status = WORK_STATUS;
			}
		}
		else {
			*status = STOP_STATUS;
		}
	}
	else {
			*status = st;
	}

	FQ_TRACE_EXIT(l);
	fq_log(l, FQ_LOG_INFO, "Successfully Done. on_set_status(). Status=[%d]", *status);

	return(TRUE);
}



void 
get_status_str( p_status_t status_code, char *dst)
{
	switch(status_code) {
		case 0:
			sprintf(dst, "%s", "INIT");
			break;
		case 1:
			sprintf(dst, "%s", "READY");
			break;
		case 2:
			sprintf(dst, "%s", "WORK");
			break;
		case 3:
			sprintf(dst, "%s", "HANG");
			break;
		case 4:
			sprintf(dst, "%s", "STOP");
			break;
		default:
			sprintf(dst, "%s", "Undefined");
			break;
	}
	return;
}

static int 
on_print_table( fq_logger_t *l, heartbeat_obj_t *obj)
{
	int i;
	char MS[12]; /* Master Status */
	char SS[12]; /* Slave Status */
	p_status_t	st, real_status;
	char	szStatus[16];

	FQ_TRACE_ENTER(l);

	if( l ) {
		fq_log(l, FQ_LOG_INFO, "=================< heartbeat Table >==================");
		fq_log(l, FQ_LOG_INFO, "- Total registerd process: [%d]", obj->h->registed_process_cnt);
	}
	else {
		fprintf(stdout, "=================< heartbeat Table >==================\n");
		fprintf(stdout, "- Total registerd process: [%d]\n", obj->h->registed_process_cnt);
	}
	
	for(i=0; i<MAX_HEARTBEAT_NODES; i++) {
		p_status_t  master_status;
		p_status_t  slave_status;

		if( obj->h->nodes[i].dist_name[0] == 0 ) continue;
		if( obj->h->nodes[i].prog_name[0] == 0 ) continue;

		st = obj->h->nodes[i].m.status;

		if(st == WORK_STATUS) {
			if( is_alive_pid_in_general( obj->h->nodes[i].m.pid) ) {
				fq_log(l, FQ_LOG_DEBUG, "pid[%d] is alive.\n", obj->h->nodes[i].m.pid);
				if( is_expired( obj->h->nodes[i].m.time ) ) {
					real_status = HANG_STATUS;
				}
				else {
					real_status = WORK_STATUS;
				}
			}
			else {
				real_status = STOP_STATUS;
			}
		}
		else {
				real_status = st;
		}
	
		get_status_str( real_status, szStatus);

		printf("distname=[%s], prog_name=[%s], pid=[%d], status=[%s].\n", 
			obj->h->nodes[i].dist_name,
			obj->h->nodes[i].prog_name,
			obj->h->nodes[i].m.pid,
			szStatus);
    }

    FQ_TRACE_EXIT(l);

    return (TRUE);
}


static int 
on_getlist( fq_logger_t *l, heartbeat_obj_t *obj, int *count, char **all_string)
{
	int i;
	char	one[256];
	char	all[65536];
	char MS[12]; /* Master Status */
	char SS[12]; /* Slave Status */
	char	m_hostname[HOST_NAME_LEN+1];
	char	s_hostname[HOST_NAME_LEN+1];
	p_status_t st, real_status;
	char	szStatus[16];

	FQ_TRACE_ENTER(l);

	memset(all, 0x00, sizeof(all));

	for(i=0; i<MAX_HEARTBEAT_NODES; i++) {
		p_status_t  master_status;
		p_status_t  slave_status;

		if( obj->h->nodes[i].dist_name[0] == 0 ) continue;
		if( obj->h->nodes[i].prog_name[0] == 0 ) continue;

		st = obj->h->nodes[i].m.status;

		if(st == WORK_STATUS) {
			if( is_alive_pid_in_general( obj->h->nodes[i].m.pid) ) {
				fq_log(l, FQ_LOG_DEBUG, "pid[%d] is alive.\n", obj->h->nodes[i].m.pid);
				if( is_expired( obj->h->nodes[i].m.time ) ) {
					real_status = HANG_STATUS;
				}
				else {
					real_status = WORK_STATUS;
				}
			}
			else {
				real_status = STOP_STATUS;
			}
		}
		else {
				real_status = st;
		}
	
		get_status_str( real_status, szStatus);

		sprintf(one, "%s,%s,%s,%s`", 
				obj->h->nodes[i].dist_name,
				obj->h->nodes[i].prog_name,
				m_hostname,
				szStatus);

		strcat(all, one);
    }

	*count = obj->h->registed_process_cnt;
	*all_string = strdup(all);

    FQ_TRACE_EXIT(l);

    return (TRUE);
}

void *timestamp_thread(void *arg)
{
#if 1
	timestamp_thread_params_t *t_param = (timestamp_thread_params_t *)arg;

	FQ_TRACE_ENTER(t_param->l);

	fq_log(t_param->l, FQ_LOG_INFO, "%s", "timestamp_thread started.");

	while(1) {
		t_param->obj->on_timestamp( t_param->l, t_param->obj, MASTER_TIME_STAMP);
		// t_param->obj->on_timestamp( t_param->l, t_param->obj, SLAVE_TIME_STAMP);
		sleep(1);
	}

	fq_log(t_param->l, FQ_LOG_ERROR, "%s.", "timestamp_thread was stoped.");

	FQ_TRACE_EXIT(t_param->l);
	pthread_exit(0);
#endif
}

int make_timestamp_thread( fq_logger_t *l, heartbeat_obj_t *obj) 
{
	pthread_t	tid;
	int rc;
	timestamp_thread_params_t *params=NULL;

	FQ_TRACE_ENTER(l);

	/* Warning !!!!
	** 이 함수가 전달받은 parameters들을 쓰레드에 넘겨줄 때에는 반드시 calloc 해서 넘겨 줘야 
    ** 이 함수가 종료 되더라도 thread가 그값을 계속해서 참조할 수 있게 된다.
	*/
	params  = (timestamp_thread_params_t *)calloc( 1, sizeof( timestamp_thread_params_t ));
	params->l  = l;
	params->obj = obj;

	if( pthread_create(&tid, NULL, &timestamp_thread, params) != 0 ) {
		fq_log(l, FQ_LOG_ERROR, "pthread_create( 'tiemstamp_thread') error.");
		return(FALSE);
	}

    FQ_TRACE_EXIT(l);
    return (TRUE);
}

void restart_process(char **args){
	int childpid;

	/* Wait until slave becomes master */
	sleep(CHANGING_WAIT_TIME+5);

	childpid = fork ();
	if (childpid < 0) {
		perror ("fork failed");
	} 
	else if (childpid  == 0) {
		// printf ("new process %d\n", getpid());
		int rv = execve (args[0], args, NULL);
		if (rv == -1) {
				perror ("execve");
				exit (EXIT_FAILURE);
		}
	} 
	else {
		sleep(1);
		exit(0); /* Itself must exit. */
	}
}
void print_heartbeat_status( p_status_t status)
{
	switch(status) {
		case 0:
			printf("INIT\n");
			break;
		case 1:
			printf("READY\n");
			break;
		case 2:
			printf("WORK\n");
			break;
		case 3:
			printf("HANG\n");
			break;
		case 4:
			printf("STOP\n");
			break;
		case 5:
			printf("PASS\n");
			break;
		default:
			printf("Undefined.\n");
			break;
	}
	return;
}

#define MAX_LINE 128
/*
** Usage: make a file using delimiter(|)
** ex) dist1|progname1
**     dist1|progmame2
*/
fqlist_t *load_process_list_from_file( char *filename)
{
	fqlist_t *t=NULL;
	FILE	*fp=NULL;
	char	hostname[HOST_NAME_LEN+1];
	char	distname[DIST_NAME_LEN+1];
	char	appID[PROG_NAME_LEN+1];

	int		delimiter = '|';
	char	value[128];
	char	buf[MAX_LINE+1];
	char	key[32];
	int		ikey=0;

	t = fqlist_new('A', "PROC_LIST");
	CHECK(t);

	fp = fopen(filename, "r");
	if( fp == NULL ) {
		printf("Can't open [%s].\n", filename);
		return((fqlist_t *)NULL);
	}

	gethostname(hostname, sizeof(hostname));

	while( fgets( buf, MAX_LINE, fp )) {
		
		buf[strlen(buf)-1] = 0x00; /* remove newline charactor */
		fq_sscanf(delimiter, buf, "%s%s", distname, appID);

		str_lrtrim(distname);
		str_lrtrim(appID);

		printf("distname=[%s], appID(monID)=[%s], hostname=[%s]\n", distname, appID, hostname);
		memset(value, 0x00, sizeof(value));
		sprintf(value, "%s|%s|%s", distname,  appID, hostname);

		sprintf(key, "%03d", ikey++);

		fqlist_add(t, key, value, 0);
	}

	if(fp) fclose(fp);
	return(t);
} 
