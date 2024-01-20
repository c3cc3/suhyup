/******************************************************************
 ** fq_manage.c
 */

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdbool.h>

#include "fqueue.h"
#include "fq_manage.h"
#include "fq_common.h"
#include "fq_logger.h"
#include "fq_manage.h"
#include "shm_queue.h"
#include "fq_file_list.h"
#include "fq_info_param.h"
#include "fq_info.h"
#include "fq_conf.h"

#include "parson.h"

#define FQ_MANAGER_C_VERSION "1.0.1"

/* 1.0.1: add ExtendFileQueue()  */

/******************************************************************
**
*/
int fq_manage_one(fq_logger_t *l, char *path, char *qname, fq_cmd_t cmd )
{
	int rc=FALSE;

	FQ_TRACE_ENTER(l);

	switch(cmd) {
		case FQ_GEN_INFO:
			rc = GenerateInfoFileByQueuy(l, path, qname);
			break;
		case FQ_CREATE:
			rc = CreateFileQueue(l, path, qname);
			break;
		case FQ_UNLINK:
			printf("[%s][%s].\n", path, qname);
			rc = UnlinkFileQueue(l, path, qname);
			rc = TRUE;
			break;
		case FQ_DISABLE:
			rc = DisableFileQueue(l, path, qname);
			break;
		case FQ_ENABLE:
			rc = EnableFileQueue(l, path, qname);
			break;
		case FQ_RESET:
			rc = ResetFileQueue(l, path, qname);
			break;
		case FQ_FLUSH:
			rc = FlushFileQueue(l, path, qname);
			break;
		case FQ_INFO:
			rc = InfoFileQueue(l, path, qname);
			break;
		case FQ_FORCE_SKIP:
			rc = ForceSkipFileQueue(l, path, qname);
			break;
		case FQ_DIAG:
			rc = DiagFileQueue(l, path, qname);
			break;
		case FQ_EXTEND:
			rc = ExtendFileQueue(l, path, qname);
			break;
		case SHMQ_CREATE:
			rc = CreateShmQueue(l, path, qname);
			break;
		case SHMQ_UNLINK:
			printf("[%s][%s].\n", path, qname);
			rc = UnlinkShmQueue(l, path, qname);
			rc = TRUE;
			break;
		case SHMQ_RESET:
			rc = ResetShmQueue(l, path, qname);
			break;
		case SHMQ_FLUSH:
			rc = FlushShmQueue(l, path, qname);
			break;
		case FQ_EXPORT:
			rc = ExportFileQueue(l, path, qname);
			break;
		case FQ_IMPORT:
			rc = ImportFileQueue(l, path, qname);
			break;
		default:
			rc = FALSE;
			fq_log(l, FQ_LOG_ERROR, "unsupporting cmd.");
			break;
	}
	
	FQ_TRACE_EXIT(l);
	return(rc);
}

int fq_manage_all(fq_logger_t *l, char *path, fq_cmd_t cmd )
{
	int		rc=FALSE;
	FILE	*fp=NULL;
	char	filename[128];
	char	qname[80];

	FQ_TRACE_ENTER(l);

	sprintf(filename, "%s/%s", path, FQ_LIST_FILE_NAME);

	fp = fopen(filename, "r");
	if(fp == NULL) {
		fq_log(l, FQ_LOG_ERROR, "fopen() error. reason=[%s]", strerror(errno));
		goto return_FALSE;
	}

	while( fgets( qname, 80, fp) ) {
		str_lrtrim(qname);
		if( qname[0] == '#' ) continue;
		if( strlen(qname) < 2) continue;
		rc = fq_manage_one(l, path, qname, cmd);
		if( rc == FALSE) {
			fq_log(l, FQ_LOG_ERROR, ">>>>>>>>  managing result : failed: %s, %s\n", path, qname);
			goto return_FALSE;
		}
		else {
			fq_log(l, FQ_LOG_DEBUG, ">>>>>>>>  managing result : success: %s, %s\n", path, qname);
		}
	}

	if(fp) fclose(fp);

	return(TRUE);

return_FALSE:
	if(fp) fclose(fp);

	FQ_TRACE_EXIT(l);
	return(FALSE);
}

/*
**
*/
fq_node_t
*fq_node_new(char* qname, int msglen, int mapping_num, int multi_num, char *desc, int XA_flag, int Wait_mode_flag)
{
    fq_node_t *t;

    t = (fq_node_t*)calloc(1, sizeof(fq_node_t));

    t->qname   = (char *)strdup(qname);
    t->msglen = msglen;
    t->mapping_num = mapping_num;
    t->multi_num = multi_num;
	t->desc = (char *)strdup(desc);
    t->XA_flag = XA_flag;
    t->Wait_mode_flag = Wait_mode_flag;
	t->status;
    t->next = NULL;
    return (t);
}

void
fq_node_free(fq_node_t *t)
{
    SAFE_FREE(t->qname);
    SAFE_FREE(t->desc);
    SAFE_FREE(t);
}

dir_list_t*
dir_list_new(char* dir_name)
{
    dir_list_t *t;

    t = (dir_list_t*)calloc(1, sizeof(dir_list_t));

    t->dir_name  = (char *)strdup(dir_name);
    t->length = 0;
    t->head = t->tail = NULL;
    t->next = NULL;

    return (t);
}

/* It puts a new q_node to a directory */
fq_node_t*
dir_list_put(dir_list_t *t, char* qname, int msglen, int mapping_num, int multi_num, char *desc, int XA_flag, int Wait_mode_flag)
{
	fq_node_t *p;

	p = fq_node_new(qname, msglen, mapping_num, multi_num, desc, XA_flag, Wait_mode_flag);
	if ( !t->tail )
		t->head = t->tail = p;
	else {
		t->tail->next = p;
		t->tail = p;
	}
	(t->length)++;
	return (p);
}

void
dir_list_free(dir_list_t **pt)
{
    fq_node_t *p, *q;

    if ( !(*pt) )
        return;

    p = (*pt)->head;

    while ( p ) {
        q = p;
        p = p->next;
        fq_node_free(q);
    }

    SAFE_FREE( (*pt)->dir_name );
    SAFE_FREE( *pt );
}

void
fq_node_print(fq_node_t *t, FILE* fp)
{
	fprintf(fp, "\t qname='%s', msglen=[%d], mapping_num=[%d], multi_num=[%d], desc=[%s], XA_flag=[%d], Wait_mode_flag=[%d]\n",
		t->qname,
		t->msglen,
		t->mapping_num,
		t->multi_num,
		t->desc,
		t->XA_flag,
		t->Wait_mode_flag);

	return;
}

void
dir_list_print(dir_list_t *t, FILE* fp)
{
	fq_node_t *p;

	if ( !t )
		return;

	fprintf(fp, " fq_list '%s'  contains %d elements\n",
			t->dir_name,  t->length);
	for ( p=t->head; p != NULL ; p = p->next ) {
		// fprintf(fp, "  qname='%s', msglen=[%d]\n", p->qname, p->msglen);
		fq_node_print(p, fp);
		fflush(fp);
	}
	return;
}

void
fq_container_print(fq_container_t *t, FILE* fp)
{
	dir_list_t *p;
	if ( !t )
			return;
	fprintf(fp, "container '%s' contains %d elements\n", t->name, t->length);
	for ( p=t->head; p != NULL ; p = p->next )
			dir_list_print(p, fp);

	return;
}

int
fq_node_manage(fq_logger_t *l, char *dir_name, fq_node_t *t, int CMD)
{
	int rc=-1;

	FQ_TRACE_ENTER(l);

	fprintf(stdout, "CMD:[%d] path=[%s] qname=[%s] ...\n", CMD, dir_name, t->qname);

	switch(CMD) {
		case FQ_GEN_INFO:
			rc = GenerateInfoFileByQueuy(l, dir_name, t->qname);
			break;
		case FQ_PRINT:
			fprintf(stdout, "\t qname='%s', msglen=[%d], mapping_num=[%d], multi_num=[%d], desc=[%s], XA_flag=[%d], Wait_mode_flag=[%d]\n",
				t->qname,
				t->msglen,
				t->mapping_num,
				t->multi_num,
				t->desc,
				t->XA_flag,
				t->Wait_mode_flag);
			break;
		case FQ_UNLINK:
			rc = UnlinkFileQueue(l, dir_name, t->qname);
			break;
		case FQ_CREATE:
			rc = CreateFileQueue(l, dir_name, t->qname);
			break;
		case FQ_RESET:
			rc = ResetFileQueue(l, dir_name, t->qname);
			break;
		case FQ_ENABLE:
			rc = EnableFileQueue(l, dir_name, t->qname);
			break;
		case FQ_DISABLE:
			rc = DisableFileQueue(l, dir_name, t->qname);
			break;
		case FQ_FLUSH:
			rc = FlushFileQueue(l, dir_name, t->qname);
			break;
		case FQ_INFO:
			rc = InfoFileQueue(l, dir_name, t->qname);
			break;
		case FQ_FORCE_SKIP:
			rc = ForceSkipFileQueue(l, dir_name, t->qname);
			break;
		case FQ_EXTEND:
			rc = ExtendFileQueue(l, dir_name, t->qname);
			break;
		case FQ_DIAG:
			rc = DiagFileQueue(l, dir_name, t->qname);
			break;
		case SHMQ_CREATE:
			rc = CreateShmQueue(l, dir_name, t->qname);
			break;
		case SHMQ_UNLINK:
			rc = UnlinkShmQueue(l, dir_name, t->qname);
			break;
		case SHMQ_RESET:
			rc = ResetShmQueue(l, dir_name, t->qname);
			break;
		case SHMQ_FLUSH:
			rc = FlushShmQueue(l, dir_name, t->qname);
			break;
		case FQ_EXPORT:
			rc = ExportFileQueue(l, dir_name, t->qname);
			break;
		case FQ_IMPORT:
			rc = ImportFileQueue(l, dir_name, t->qname);
			break;
		default:
			fprintf(stderr, "illegal command.\n");
			FQ_TRACE_EXIT(l);
			return(-1);
	}

	FQ_TRACE_EXIT(l);
	return(rc);
}
int
fq_node_manage_with_queues_list_t(fq_logger_t *l, queues_list_t *qlist, int CMD)
{
	int rc=-1;

	FQ_TRACE_ENTER(l);

	fprintf(stdout, "CMD:[%d] path=[%s] qname=[%s] ...\n", CMD, qlist->qpath, qlist->qname);

	switch(CMD) {
		case FQ_UNLINK:
			rc = UnlinkFileQueue(l, qlist->qpath, qlist->qname);
			break;
		case FQ_CREATE:
			rc = CreateFileQueue(l, qlist->qpath, qlist->qname);
			break;
		case SHMQ_CREATE:
			rc = CreateShmQueue(l, qlist->qpath, qlist->qname);
			break;
		case SHMQ_UNLINK:
			rc = UnlinkShmQueue(l, qlist->qpath, qlist->qname);
			break;
		default:
			fq_log(l, FQ_LOG_ERROR, "We do not support CMD=[%d].", CMD);
			FQ_TRACE_EXIT(l);
			return(-1);
	}
	FQ_TRACE_EXIT(l);
	return(rc);
}

int 
fq_queue_manage( fq_logger_t *l, fq_container_t *t, char *dirname, char *qname, int CMD)
{
	dir_list_t *d;
	fq_node_t *n;
	int node_count=0;

	FQ_TRACE_ENTER(l);

	if ( !t ) {
		FQ_TRACE_EXIT(l);
        return(-1);
	}

	for ( d=t->head; d != NULL ; d = d->next ) {
		if( strcmp(d->dir_name, dirname) == 0 ) {
			for ( n=d->head; n != NULL ; n = n->next ) {
				if( strcmp(n->qname, qname) == 0 ) {
					node_count++;
					fq_node_manage(l, d->dir_name,  n, CMD);
					FQ_TRACE_EXIT(l);
					return(0);
				}
			}
		}
	}
	if( node_count == 0 ) {
		fprintf(stderr, "Can't find a queue name[%s] in [%s] directory.\n", qname, dirname);
	}

	FQ_TRACE_EXIT(l);
	return(-1);
}
int 
fq_queue_manage_with_queues_list_t( fq_logger_t *l, queues_list_t *qlist, int CMD)
{
	dir_list_t *d;
	fq_node_t *n;
	int node_count=0;
	int rc = 0;

	FQ_TRACE_ENTER(l);

	if ( !qlist ) {
		FQ_TRACE_EXIT(l);
        return(-1);
	}

	rc = fq_node_manage_with_queues_list_t(l, qlist, CMD);
	if( rc ==  FALSE ) {
		fq_log(l, FQ_LOG_ERROR, "fq_node_manage_with_queues_list_t( '%s', '%s' ) error." ,
			qlist->qpath, qlist->qname);
		FQ_TRACE_EXIT(l);
		return(-2);
	}

	FQ_TRACE_EXIT(l);
	return(0);
}

int
dir_list_manage( fq_logger_t *l, dir_list_t *t, int CMD)
{
	fq_node_t *p;

	FQ_TRACE_ENTER(l);

	if ( !t ) {
		FQ_TRACE_EXIT(l);
        return(-1);
	}

	for ( p=t->head; p != NULL ; p = p->next ) {
		fq_node_manage(l, t->dir_name,  p, CMD);
	}

	FQ_TRACE_EXIT(l);
	return(0);
}

int
fq_directory_manage( fq_logger_t *l, fq_container_t *t, char *dirname, int CMD)
{
	dir_list_t *p;
	int dir_count=0;

	FQ_TRACE_ENTER(l);
    if ( !t ) {
		FQ_TRACE_EXIT(l);
        return(-1);
	}

    for ( p=t->head; p != NULL ; p = p->next ) {
			if( strcmp(p->dir_name, dirname) == 0 ) {
				dir_count++;
				dir_list_manage(l, p, CMD);
				FQ_TRACE_EXIT(l);
				return(0);
			}
	}
	if( dir_count == 0 ) {
		fprintf(stderr, "Can't find a directiory name.[%s]\n", dirname);
	}
	FQ_TRACE_EXIT(l);
    return(-1);
}

int
fq_container_manage( fq_logger_t *l, fq_container_t *t,  int CMD)
{
	dir_list_t *p;

	FQ_TRACE_ENTER(l);
    if ( !t ) {
		FQ_TRACE_EXIT(l);
        return(-1);
	}

    for ( p=t->head; p != NULL ; p = p->next ) {
            dir_list_manage(l, p, CMD);
	}

	FQ_TRACE_EXIT(l);
    return(0);
}

fq_node_t*
dir_list_find_fq(dir_list_t *t, char *qname)
{
	fq_node_t *p;

	if( !t ) return(NULL);

	p = t->head;

	while ( p ) {
		if( !(p->qname) ) return(NULL);
		if( !HASVALUE(qname))  return(NULL);

		if ( (STRCCMP(p->qname, qname)) == 0 )
			return (p);

		p = p->next;
	}
	return (NULL);
}

dir_list_t*
fq_container_find_dir(fq_container_t *t, char *dir_name)
{
        dir_list_t *p=NULL;

        ASSERT(t);

        for ( p=t->head; (p != NULL) ; p = p->next ) {
                if ( STRCCMP((void *)p->dir_name, (void *)VALUE(dir_name)) == 0 ) {
                        return (p);
                }
		}
        return (NULL);
}

int 
fq_container_search_one(fq_container_t *t, fq_node_t **fq,  char *dir, char *qname)
{
	dir_list_t *p=NULL;
	fq_node_t *q=NULL;

	p = fq_container_find_dir( t, dir);
	if( !p ) {
			return FALSE;
	}

	q = dir_list_find_fq( p , qname);
	if( !q ) {
			return FALSE;
	}

	*fq = q;

	return TRUE;
}

dir_list_t*
fq_container_put(fq_container_t *t, char* key)
{
        dir_list_t *p;

        ASSERT(t);

        p = dir_list_new(key);
        if ( !t->tail )
                t->head = t->tail = p;
        else {
                t->tail->next = p;
                t->tail = p;
        }
        (t->length)++;

        return(p);
}

fq_container_t*
fq_container_new( char *name)
{
	fq_container_t *t;

	t = (fq_container_t*)calloc(1, sizeof(fq_container_t));
	ASSERT(t);

	t->name = (char *)strdup(name);
	t->length = 0;
	t->head = t->tail = NULL;

	return(t);
}

void
fq_container_free(fq_container_t **pt)
{
        dir_list_t *p, *q;

        if ( !(*pt) )
                return;

        p = (*pt)->head;
        while ( p ) {
                q = p;
                p = p->next;
                dir_list_free(&q);
        }
        SAFE_FREE( (*pt)->name );
        SAFE_FREE( *pt );
}

#if 0 /* only file queue version */
int load_fq_container(fq_logger_t *l, fq_container_t **fq_c )
{
	int rc, i;
	char *fq_home_path=NULL;
    FILELIST *this_entry=NULL;
	file_list_obj_t *dirobj=NULL;
	fq_container_t *c=NULL;
	char fq_dirlist_file[256];

	FQ_TRACE_ENTER(l);

	fq_home_path = getenv("FQ_HOME");
	if( fq_home_path == NULL ) {
		fq_home_path = getenv("FQ_DATA_HOME");
		if( fq_home_path == NULL ) {
			fq_log(l, FQ_LOG_ERROR, "Undefined FQ_HOME or FQ_DATA_HOME  in env.");
			goto return_FALSE;
		}
	}

	c = fq_container_new( fq_home_path );
	if( !c ) {
		fq_log(l, FQ_LOG_ERROR, "fq_container_new('%s') error.", fq_home_path);
		goto return_FALSE;
	}

	sprintf(fq_dirlist_file, "%s/%s", fq_home_path, FQ_DIRECTORIES_LIST); 
	rc = open_file_list(l, &dirobj, fq_dirlist_file);
	if( rc != TRUE ) {
		fq_log(l, FQ_LOG_ERROR, "open_file_list('%s') error. There is no '%s'", fq_dirlist_file, FQ_DIRECTORIES_LIST);
		goto return_FALSE;
	}

	this_entry = dirobj->head;
	for( i=0;  (this_entry->next && this_entry->value); i++ ) {
		char dirname[256];
		dir_list_t *d=NULL;
		FILELIST *this_entry_sub=NULL;
		file_list_obj_t *fqlist_obj=NULL;
		char fq_qlist_file[256];
		int rc;
		int j;

		sprintf(dirname, "%s", this_entry->value);
		d = fq_container_put( c, dirname);

		sprintf(fq_qlist_file, "%s/%s", dirname, FQ_LIST_FILE_NAME);
		rc = open_file_list(l, &fqlist_obj, fq_qlist_file);
		if( rc != TRUE ) {
				fq_log(l, FQ_LOG_ERROR, "open_file_list('%s') error.", fq_qlist_file);
				goto return_FALSE;
		}
		this_entry_sub = fqlist_obj->head;
		for( j=0;  (this_entry_sub->next && this_entry_sub->value); j++ ) {
			char qname[MAX_FQ_NAME_LEN+1];
			char fq_info_file[256];
			FILE *fq=NULL;
			fq_info_t *t=NULL;

			char i_qname[MAX_FQ_NAME_LEN+1];
			char i_desc[MAX_DESC_LEN+1];
			int i_msglen, i_mmapping_num, i_multi_num, i_xa_mode_on_off;
			int i_wait_mode_on_off;

			sprintf(qname, "%s", this_entry_sub->value);
			
			sprintf(fq_info_file, "%s/%s.Q.info", dirname, qname);

			t = new_fq_info(fq_info_file);
			if( !t ) {
				fq_log(l, FQ_LOG_ERROR, "new_fq_info('%s') error.", fq_info_file);
				goto return_FALSE;
			}

			rc = load_info_param(t, fq_info_file);
			if(rc < 0 ) {
				fq_log(l, FQ_LOG_ERROR, "load_info_param('%s') error.", fq_info_file);
				goto return_FALSE;
			}

			get_fq_info(t, "QNAME", i_qname);
			i_msglen = get_fq_info_i(t, "MSGLEN");
			i_mmapping_num = get_fq_info_i(t, "MMAPPING_NUM");
			i_multi_num = get_fq_info_i(t, "MULTI_NUM");
			get_fq_info(t, "DESC", i_desc);
			i_xa_mode_on_off = get_fq_info_i(t, "XA_MODE_ON_OFF");
			i_wait_mode_on_off = get_fq_info_i(t, "WAIT_MODE_ON_OFF");

			dir_list_put(d, i_qname, i_msglen, i_mmapping_num, 
				i_multi_num, i_desc, i_xa_mode_on_off, i_wait_mode_on_off);
			
			if( t ) {
				free_fq_info( &t );
			}

			this_entry_sub = this_entry_sub->next;
		}

		close_file_list(NULL, &fqlist_obj);

		this_entry = this_entry->next;
	}

	// fq_container_print(c, stdout);

	// if(dirobj) close_file_list(NULL, &dirobj);

	// if(c) fq_container_free(c);

	*fq_c = c;

	FQ_TRACE_EXIT(l);
	return(TRUE);

return_FALSE:

	if(c) fq_container_free(&c);

	FQ_TRACE_EXIT(l);
	return(FALSE);
}

#else /* both file and shared memory queue  version */
int load_fq_container(fq_logger_t *l, fq_container_t **fq_c )
{
	int rc, i;
	char *fq_home_path=NULL;
    FILELIST *this_entry=NULL;
	file_list_obj_t *dirobj=NULL;
	fq_container_t *c=NULL;
	char fq_dirlist_file[256];

	FQ_TRACE_ENTER(l);

	fq_home_path = getenv("FQ_HOME");
	if( fq_home_path == NULL ) {
		fq_home_path = getenv("FQ_DATA_HOME");
		if( fq_home_path == NULL ) {
			fq_log(l, FQ_LOG_ERROR, "Undefined FQ_HOME or FQ_DATA_HOME  in env.");
			goto return_FALSE;
		}
	}

	c = fq_container_new( fq_home_path );
	if( !c ) {
		fq_log(l, FQ_LOG_ERROR, "fq_container_new('%s') error.", fq_home_path);
		goto return_FALSE;
	}

	sprintf(fq_dirlist_file, "%s/%s", fq_home_path, FQ_DIRECTORIES_LIST); 
	rc = open_file_list(l, &dirobj, fq_dirlist_file);
	if( rc != TRUE ) {
		fq_log(l, FQ_LOG_ERROR, "open_file_list('%s') error.", fq_dirlist_file);
		goto return_FALSE;
	}

	this_entry = dirobj->head;
	for( i=0;  (this_entry->next && this_entry->value); i++ ) {
		char dirname[256];
		dir_list_t *d=NULL;
		FILELIST *this_entry_sub=NULL;
		file_list_obj_t *fqlist_obj=NULL;
		file_list_obj_t *shmqlist_obj=NULL;
		char fq_qlist_file[1024];
		char shmq_qlist_file[2048];
		int rc;
		int j;

		sprintf(dirname, "%s", this_entry->value);
		d = fq_container_put( c, dirname);

		/*
		** Load a list of File Queue
		*/
		sprintf(fq_qlist_file, "%s/%s", dirname, FQ_LIST_FILE_NAME);
		rc = open_file_list(l, &fqlist_obj, fq_qlist_file);
		if( rc != TRUE ) {
				fq_log(l, FQ_LOG_ERROR, "open_file_list('%s') error.", fq_qlist_file);
				goto return_FALSE;
		}
		this_entry_sub = fqlist_obj->head;
		for( j=0;  (this_entry_sub->next && this_entry_sub->value); j++ ) {
			char qname[MAX_FQ_NAME_LEN+1];
			char fq_info_file[1024];
			FILE *fq=NULL;
			fq_info_t *t=NULL;

			char i_qname[MAX_FQ_NAME_LEN+1];
			char i_desc[MAX_DESC_LEN+1];
			int i_msglen, i_mmapping_num, i_multi_num, i_xa_mode_on_off;
			int i_wait_mode_on_off;

			sprintf(qname, "%s", this_entry_sub->value);
			
			sprintf(fq_info_file, "%s/%s.Q.info", dirname, qname);

			t = new_fq_info(fq_info_file);
			if( !t ) {
				fq_log(l, FQ_LOG_ERROR, "new_fq_info('%s') error.", fq_info_file);
				goto return_FALSE;
			}

			rc = load_info_param(l, t, fq_info_file);
			if(rc < 0 ) {
				fq_log(l, FQ_LOG_ERROR, "load_info_param('%s') error.", fq_info_file);
				goto return_FALSE;
			}

			get_fq_info(t, "QNAME", i_qname);
			i_msglen = get_fq_info_i(t, "MSGLEN");
			i_mmapping_num = get_fq_info_i(t, "MMAPPING_NUM");
			i_multi_num = get_fq_info_i(t, "MULTI_NUM");
			get_fq_info(t, "DESC", i_desc);
			i_xa_mode_on_off = get_fq_info_i(t, "XA_MODE_ON_OFF");
			i_wait_mode_on_off = get_fq_info_i(t, "WAIT_MODE_ON_OFF");

			dir_list_put(d, i_qname, i_msglen, i_mmapping_num, 
				i_multi_num, i_desc, i_xa_mode_on_off, i_wait_mode_on_off);
			
			if( t ) {
				free_fq_info( &t );
			}

			this_entry_sub = this_entry_sub->next;
		}
		close_file_list(NULL, &fqlist_obj);

		/*
		** Load a list of shared memory Queue
		*/
		sprintf(shmq_qlist_file, "%s/%s", dirname, SHMQ_LIST_FILE_NAME);
		rc = open_file_list(l, &shmqlist_obj, shmq_qlist_file);
		if( rc == TRUE ) {
			this_entry_sub = shmqlist_obj->head;
			for( j=0;  (this_entry_sub->next && this_entry_sub->value); j++ ) {
				char qname[MAX_FQ_NAME_LEN+1];
				char fq_info_file[1024];
				FILE *fq=NULL;
				fq_info_t *t=NULL;

				char i_qname[MAX_FQ_NAME_LEN+1];
				char i_desc[MAX_DESC_LEN+1];
				int i_msglen, i_mmapping_num, i_multi_num, i_xa_mode_on_off;
				int i_wait_mode_on_off;

				sprintf(qname, "%s", this_entry_sub->value);
				
				sprintf(fq_info_file, "%s/%s.Q.info", dirname, qname);

				t = new_fq_info(fq_info_file);
				if( !t ) {
					fq_log(l, FQ_LOG_ERROR, "new_fq_info('%s') error.", fq_info_file);
					goto return_FALSE;
				}

				rc = load_info_param(l, t, fq_info_file);
				if(rc < 0 ) {
					fq_log(l, FQ_LOG_ERROR, "load_info_param('%s') error.", fq_info_file);
					goto return_FALSE;
				}

				get_fq_info(t, "QNAME", i_qname);
				i_msglen = get_fq_info_i(t, "MSGLEN");
				i_mmapping_num = get_fq_info_i(t, "MMAPPING_NUM");
				i_multi_num = get_fq_info_i(t, "MULTI_NUM");
				get_fq_info(t, "DESC", i_desc);
				i_xa_mode_on_off = get_fq_info_i(t, "XA_MODE_ON_OFF");
				i_wait_mode_on_off = get_fq_info_i(t, "WAIT_MODE_ON_OFF");

				dir_list_put(d, i_qname, i_msglen, i_mmapping_num, 
					i_multi_num, i_desc, i_xa_mode_on_off, i_wait_mode_on_off);
				
				if( t ) {
					free_fq_info( &t );
				}

				this_entry_sub = this_entry_sub->next;
			}
			close_file_list(NULL, &shmqlist_obj);
		}
		this_entry = this_entry->next;
	}

	//  fq_container_print(c, stdout);

	if(dirobj) close_file_list(NULL, &dirobj);

	// if(c) fq_container_free(c);

	*fq_c = c;

	FQ_TRACE_EXIT(l);
	return(TRUE);

return_FALSE:

	if(c) fq_container_free(&c);

	FQ_TRACE_EXIT(l);
	return(FALSE);
}
#endif
static char  *cmd_table[] = {
	"FQ_CREATE",
	"FQ_UNLINK",
	"FQ_DISABLE",
	"FQ_ENABLE",
	"FQ_RESET",
	"FQ_FLUSH",
	"FQ_INFO",
	"FQ_FORCE_SKIP",
	"FQ_DIAG",
	"FQ_EXTEND",
	"FQ_PRINT",
	"SHMQ_CREATE",
	"SHMQ_UNLINK",
	"SHMQ_RESET",
	"SHMQ_FLUSH",
	"FQ_EXPORT",
	"FQ_IMPORT",
	"FQ_GEN_INFO",
	"SHMQ_GEN_INFO",
	"FQ_DATA_VIEW",
	0x00 
};

int get_cmd( char *cmd )
{
	int i;

	for( i=0; cmd_table[i]; i++) {
		if( strcmp(cmd, cmd_table[i]) == 0 ) {
			return(i);
		}
	}
	return(-1);
}

int load_shm_container(fq_logger_t *l, fq_container_t **fq_c )
{
	int rc, i;
	char *fq_home_path=NULL;
    FILELIST *this_entry=NULL;
	file_list_obj_t *dirobj=NULL;
	fq_container_t *c=NULL;
	char fq_dirlist_file[256];

	FQ_TRACE_ENTER(l);

	fq_home_path = getenv("FQ_HOME");
	if( fq_home_path == NULL ) {
		fq_home_path = getenv("FQ_DATA_HOME");
		if( fq_home_path ) {
			fq_log(l, FQ_LOG_ERROR, "Undefined FQ_HOME or FQ_DATA_HOME  in env.");
			goto return_FALSE;
		}
	}

	c = fq_container_new( fq_home_path );
	if( !c ) {
		fq_log(l, FQ_LOG_ERROR, "fq_container_new('%s') error.", fq_home_path);
		goto return_FALSE;
	}

	sprintf(fq_dirlist_file, "%s/%s", fq_home_path, FQ_DIRECTORIES_LIST); 
	rc = open_file_list(l, &dirobj, fq_dirlist_file);
	if( rc != TRUE ) {
		fq_log(l, FQ_LOG_ERROR, "open_file_list('%s') error.", fq_dirlist_file);
		goto return_FALSE;
	}

	this_entry = dirobj->head;
	for( i=0;  (this_entry->next && this_entry->value); i++ ) {
		char dirname[256];
		dir_list_t *d=NULL;
		FILELIST *this_entry_sub=NULL;
		file_list_obj_t *fqlist_obj=NULL;
		char fq_qlist_file[1024];
		int rc;
		int j;

		sprintf(dirname, "%s", this_entry->value);
		d = fq_container_put( c, dirname);

		sprintf(fq_qlist_file, "%s/%s", dirname, SHMQ_LIST_FILE_NAME);
		rc = open_file_list(l, &fqlist_obj, fq_qlist_file);
		if( rc != TRUE ) {
				fq_log(l, FQ_LOG_ERROR, "open_file_list('%s') error.", fq_qlist_file);
				goto return_FALSE;
		}
		this_entry_sub = fqlist_obj->head;
		for( j=0;  (this_entry_sub->next && this_entry_sub->value); j++ ) {
			char qname[MAX_FQ_NAME_LEN+1];
			char fq_info_file[1024];
			FILE *fq=NULL;
			fq_info_t *t=NULL;

			char i_qname[MAX_FQ_NAME_LEN+1];
			char i_desc[MAX_DESC_LEN+1];
			int i_msglen, i_mmapping_num, i_multi_num, i_xa_mode_on_off;
			int i_wait_mode_on_off;

			sprintf(qname, "%s", this_entry_sub->value);
			
			sprintf(fq_info_file, "%s/%s.Q.info", dirname, qname);

			t = new_fq_info(fq_info_file);
			if( !t ) {
				fq_log(l, FQ_LOG_ERROR, "new_fq_info('%s') error.", fq_info_file);
				goto return_FALSE;
			}

			rc = load_info_param(l, t, fq_info_file);
			if(rc < 0 ) {
				fq_log(l, FQ_LOG_ERROR, "load_info_param('%s') error.", fq_info_file);
				goto return_FALSE;
			}

			get_fq_info(t, "QNAME", i_qname);
			i_msglen = get_fq_info_i(t, "MSGLEN");
			i_mmapping_num = get_fq_info_i(t, "MMAPPING_NUM");
			i_multi_num = get_fq_info_i(t, "MULTI_NUM");
			get_fq_info(t, "DESC", i_desc);
			i_xa_mode_on_off = get_fq_info_i(t, "XA_MODE_ON_OFF");
			i_wait_mode_on_off = get_fq_info_i(t, "WAIT_MODE_ON_OFF");

			dir_list_put(d, i_qname, i_msglen, i_mmapping_num, 
				i_multi_num, i_desc, i_xa_mode_on_off, i_wait_mode_on_off);
			
			if( t ) {
				free_fq_info( &t );
			}

			this_entry_sub = this_entry_sub->next;
		}

		close_file_list(NULL, &fqlist_obj);

		this_entry = this_entry->next;
	}

	// fq_container_print(c, stdout);

	// if(dirobj) close_file_list(NULL, &dirobj);

	// if(c) fq_container_free(c);

	*fq_c = c;

	FQ_TRACE_EXIT(l);
	return(TRUE);

return_FALSE:

	if(c) fq_container_free(&c);

	FQ_TRACE_EXIT(l);
	return(FALSE);
}

bool fq_container_CB( fq_logger_t *l, fq_container_t *fqc, fqCBtype userCB)
{
	dir_list_t *p;

	for ( p=fqc->head; p != NULL ; p = p->next ) {
		fq_node_t *n;
		for ( n=p->head; n != NULL ; n = n->next ) {
			int usr_rc;
			usr_rc = userCB(l, p->dir_name, n);
			if(usr_rc  == false ) return(false);
		}
	}
	return true;
}

/*
** FQPM(File Queue Process Manager)
**
** Process management
*/
#include <sys/types.h>
#include <unistd.h>
#include <libgen.h>

#include "fq_cache.h"
#include "fq_dir.h"

#include <sys/stat.h>
#include <fcntl.h>

fq_logger_t *gl=NULL;

/*
** It gets execute filename of running process with pid.
*/
int get_exe_for_pid(pid_t pid, char *buf, size_t bufsize) 
{
    char path[32];
    sprintf(path, "/proc/%d/exe", pid);
    return readlink(path, buf, bufsize);
}

/*
** cwd: Current Working Directory
*/
int get_cwd_for_pid(pid_t pid, char *buf, size_t bufsize) 
{
    char path[32];
    sprintf(path, "/proc/%d/cwd", pid);
    return readlink(path, buf, bufsize);
}

bool IsValidPidNumber(char *pid_string)
{
	int i;
   for(i = 0; i < strlen( pid_string ); i ++)
   {
      //ASCII value of 0 = 48, 9 = 57. So if value is outside of numeric range then fail
      //Checking for negative sign "-" could be added: ASCII value 45.
      if (pid_string[i] < 48 || pid_string[i] > 57)
         return FALSE;
   }

   return TRUE;
}
bool get_progname_argus_for_pid(pid_t pid, char *progname, fqlist_t *li, int *argu_count) 
{
	char path_cmdline[128];
	int fd_cmdline;
	char whole_cmd_arguments[PATH_MAX];

	snprintf(path_cmdline, sizeof(path_cmdline), "/proc/%d/cmdline", pid);
	fd_cmdline = open(path_cmdline, O_RDONLY);
	if (fd_cmdline < 0) {
		printf("There is no pid(%d) in list of process(system).\n", pid);
		return(false);
	} 

	/*
	** whole_cmd_arguments has cmd and arguments .
	** it is seperated by 0x00.
	*/
	int n;
	if ( (n=read(fd_cmdline, whole_cmd_arguments, PATH_MAX)) < 0) {
		printf("Can't read '/proc/%d' in list of process(system).\n", pid);
		close(fd_cmdline);
		return(false);
	} else {
		int progname_len;

		// printf("read gets %d characters.\n", n); 
		progname_len = strlen(whole_cmd_arguments);
		sprintf(progname, "%s", whole_cmd_arguments);
		// printf("progname = '%s'\n", progname);

		if( n > progname_len) {
			char delimiter=0x00;
			int count;
			*argu_count = MakeListFromDelimiterMsg( li, &whole_cmd_arguments[progname_len+1], delimiter, 0);
			
/*
			memcpy(argus, &whole_cmd_arguments[progname_len+1], 
				(n-(progname_len+1)));
			printf("arguments = '%s'\n", argus);
*/
			
		}
		close(fd_cmdline);
		return true;
	}
}

bool touch_me_on_fq_framework( fq_logger_t *l, const char *path, pid_t pid)
{
	int ret_tf = false;
	FILE *fp=NULL;
	char path_filename[512];
	char *fq_home_path = NULL;

	FQ_TRACE_ENTER(l);

	if( !path ) {
		fq_home_path = getenv("FQ_DATA_HOME");
		if( !fq_home_path ) {
			fq_log(l, FQ_LOG_ERROR, "There is no FQ_DATA_HOME in env of server.");
			FQ_TRACE_EXIT(l);
			return false;			
		}
		sprintf(path_filename, "%s/FQPM/%d", fq_home_path, pid);
	}
	else {
		sprintf(path_filename, "%s/%d", path, pid);
	}

	char cmd[1024];

	sprintf(cmd, "touch %s", path_filename);
	system(cmd);

	return true;
} 

bool is_block_process( fq_logger_t *l, const char *path, pid_t pid, int health_check_term_sec)
{
	int ret_tf = false;
	FILE *fp=NULL;
	char path_filename[512];
	char *fq_home_path = NULL;

	FQ_TRACE_ENTER(l);

	if( !path ) {
		fq_home_path = getenv("FQ_DATA_HOME");
		if( !fq_home_path ) {
			fq_log(l, FQ_LOG_ERROR, "There is no FQ_DATA_HOME in env of server.");
			FQ_TRACE_EXIT(l);
			return false;			
		}
		sprintf(path_filename, "%s/FQPM/%d", fq_home_path, pid);
	}
	else {
		sprintf(path_filename, "%s/%d", path, pid);
	}

    struct stat attr;
	int rc;
    rc = stat(path_filename, &attr);
	if( rc < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "Can't get last update time of '%s'.", path_filename);
		FQ_TRACE_EXIT(l);
		return false;
	}

	time_t current = time(0); 
	double diff;
	diff = difftime( current, attr.st_mtime);
	if( diff > health_check_term_sec ) {
		fq_log(l, FQ_LOG_ERROR, "[%ld] was not checked for [%d] seconds.",
				pid, health_check_term_sec);
		return true;
	}

	return false;
} 

/* If the path value is 0, we use the value defined in the environment variable.  */
bool regist_me_on_fq_framework( fq_logger_t *l, const char *path, pid_t pid, bool restart_flag, int heartbeat_sec)
{
	int ret_tf = false;
	FILE *fp=NULL;
	char path_filename[512];
	char *fq_home_path = NULL;

	FQ_TRACE_ENTER(l);

	if( !path ) {
		fq_home_path = getenv("FQ_DATA_HOME");
		if( !fq_home_path ) {
			fq_log(l, FQ_LOG_ERROR, "There is no FQ_DATA_HOME in env of server.");
			FQ_TRACE_EXIT(l);
			return false;			
		}

		/* fqpm: filequeue process manage */
		char fqpm_dir[PATH_MAX];
		sprintf(fqpm_dir, "%s/FQPM", fq_home_path);

		if( 0 != access(fqpm_dir, F_OK) ) { /* create FQPM directory newly */
			mkdir(fqpm_dir, 0777);
		}
		sprintf(path_filename, "%s/FQPM/%d", fq_home_path, pid);
	}
	else {
		sprintf(path_filename, "%s/%d", path, pid);
	}

	fp = fopen( path_filename, "w");
	if( fp == NULL ) {
		fq_log(l, FQ_LOG_ERROR, "Can't fopen('%s'). Ckeck your path or filename.", path_filename);
		FQ_TRACE_EXIT(l);
		return false;
	}

	/*
	** print PID
	*/
	fprintf(fp,"PID=%d\n", pid);

	/*
	** print path and exe name
	*/
	int rc;

	char run_name[PATH_MAX];
	rc = get_exe_for_pid(pid, run_name, sizeof(run_name));
	if( rc > 0 ){
		fprintf(fp, "RUN_NAME=\"%s\"\n", run_name); 

		char *ts1 = strdup(run_name);
		char *ts2 = strdup(run_name);

		char *dir=dirname(ts1);
		char *exe_name=basename(ts2);

		fprintf(fp, "EXE_NAME=\"%s\"\n", exe_name); 

		if(ts1) free(ts1);
		if(ts2) free(ts2);
	} 


	/*
	** printf Current Working Directory
	*/
	char cwd[PATH_MAX];
	rc = get_cwd_for_pid(pid, cwd, sizeof(cwd));
	if( rc > 0 ){
		fprintf(fp, "CWD=\"%s\".\n", cwd); 
	} 

	/*
	** print run_cmd and arguments
	*/
	char user_cmd[PATH_MAX];
	fqlist_t *argu_list = NULL;
	argu_list = fqlist_new('C', "arguments");
	int argu_cnt;
	int tf;
	tf = get_progname_argus_for_pid(pid, user_cmd, argu_list, &argu_cnt);
	if(tf == false) {
		ret_tf = false;
	}
	else {
		ret_tf = true;
	}
	fprintf(fp,"USER_CMD=\"%s\"\n", user_cmd);
	
	node_t *node=argu_list->list->head;
	int i;
	for ( i=0; node != NULL ; i++, node=node->next ) {
		fprintf(fp, "ARG-%d=\"%s\"\n", i, node->value);
	}
	fqlist_free( &argu_list);

	/*
	** printf up time
	*/
	fprintf(fp,"UPTIME_BIN=%ld\n", time(0));
	char cur_date[9], cur_time[7];
	get_time(cur_date, cur_time);
	fprintf(fp,"UPTIME_ASCII=%s:%s\n", cur_date, cur_time);
	fprintf(fp, "RESTART_FLAG=%d\n", restart_flag);
	fprintf(fp, "HEARTBEAT_SEC=%d\n", heartbeat_sec);
	
	fflush(fp);
	if(fp) fclose(fp);

	FQ_TRACE_EXIT(l);

	return ret_tf;
} 
/* Check_duplicated_running
** We can't run with same config_file.
*/
bool check_duplicate_me_on_fq_framework( fq_logger_t *l, char *config_filename)
{
	pslist_t *pslist=NULL;
	int ret_tf = false;
	char *path = NULL;

	FQ_TRACE_ENTER(l);

	pslist = pslist_new();
	make_ps_list( l, path, pslist);
	
	ps_t *p=NULL;
	for( p=pslist->head; p != NULL; p=p->next) {
		if( strcmp(p->psinfo->arg_0, config_filename) == 0 ) {
			fq_log(l, FQ_LOG_ERROR, "This environment(config) file(%s) is already in use by another process(%d)", 
				config_filename, p->psinfo->pid);
			fprintf(stderr, "This environment(config)(%s) file is already in use by another process(%d).\n",
				config_filename, p->psinfo->pid);
			FQ_TRACE_EXIT(l);
			return false;
		}
	}

	FQ_TRACE_EXIT(l);

	return true;
} 
bool touch_on_fq_framework( fq_logger_t *l, const char *path, pid_t pid)
{
	int ret_tf = false;
	FILE *fp=NULL;
	char path_filename[512];
	char touch_filename[512];
	char *fq_home_path = NULL;

	FQ_TRACE_ENTER(l);

	if( !path ) {
		fq_home_path = getenv("FQ_DATA_HOME");
		if( !fq_home_path ) {
			fq_log(l, FQ_LOG_ERROR, "There is no FQ_DATA_HOME in env of server.");
			FQ_TRACE_EXIT(l);
			return false;			
		}
		sprintf(path_filename, "%s/FQPM/%d", fq_home_path, pid);
	}
	else {
		sprintf(path_filename, "%s/%d", path, pid);
	}

	char touch_cmd[1024];
	sprintf(touch_cmd, "touch %s", path_filename);
	system(touch_cmd);

	FQ_TRACE_EXIT(l);
	return true;
} 

bool delete_me_on_fq_framework( fq_logger_t *l, const char *path, pid_t pid)
{
	char path_filename[512];
	char *fq_home_path=NULL;

	if( !path ) {
		fq_home_path = getenv("FQ_DATA_HOME");
		if( !fq_home_path ) {
			fq_log(l, FQ_LOG_ERROR, "There is no FQ_DATA_HOME in env of server.");
			FQ_TRACE_EXIT(l);
			return false;			
		}
		/* fqpm: filequeue process manage */
		sprintf(path_filename, "%s/FQPM/%d", fq_home_path, pid);
	}
	else {
		sprintf(path_filename, "%s/%d", path, pid);
	}

	unlink(path_filename);
	return true;
} 

static int my_CB_func(fq_logger_t *l, char *pathfile)
{
	FQ_TRACE_ENTER(l);
	int ret;

	char *ts1 = strdup(pathfile);
	char *ts2 = strdup(pathfile);

	char *dir=dirname(ts1);
	char *fname=basename(ts2);
	

	if( IsValidPidNumber(fname) == false ) {
		fq_log(l, FQ_LOG_EMERG, "'%s' is not number.", fname);
		FQ_TRACE_EXIT(l);
		return false;
	}
	
	if( atoi(fname) < 1000 ) {
		fq_log(l, FQ_LOG_EMERG, "'%s' is not user pid.", fname);
		FQ_TRACE_EXIT(l);
		return false;
	}

	struct stat sts;
	char check_string_pid[256];

	sprintf(check_string_pid, "/proc/%s", fname);
	if (stat(check_string_pid, &sts) == -1 && errno == ENOENT) {
  		fq_log(l, FQ_LOG_EMERG, "process[%s] doesn't exist(in /proc/).", fname);
		unlink(pathfile);
		ret = false;
	}
	else {
		int rc;
		char cmd_line_buf[4096];
		pid_t pid=(pid_t)atoi(fname);

		rc = kill( pid, 0 );
		if( rc == 0 ) {
			fq_log(l, FQ_LOG_INFO, "(%s) is alive.", pathfile);
			ret = true;
		}
		else {
			fq_log(l, FQ_LOG_EMERG, "pid(%d) is not running.( didn't replay to signal)", pid);
			unlink(pathfile);
			ret  = false;
		}
	}

	if(ts1) free(ts1);
	if(ts2) free(ts2);

	FQ_TRACE_EXIT(l);
	return(true);
}

/*
** 프레임워크에 구동중인 모든 프로세스에 signal을 보내어 응답이 없는
** 프로세스의 상태를 로그로 남기고, 프로세스 파일을 삭제한다.
*/
#include <stdint.h>
bool check_fq_framework_process( fq_logger_t *l, const char *path,  pid_t pid, bool all_flag )
{
	int rc;
	char path_filename[4096];
	char Check_path[126];
	char *fq_home_path=NULL;

	FQ_TRACE_ENTER(l);

	if( !path ) {
		fq_home_path = getenv("FQ_DATA_HOME");
		if( !fq_home_path ) {
			fq_log(l, FQ_LOG_ERROR, "There is no FQ_DATA_HOME in env of server.");
			FQ_TRACE_EXIT(l);
			return false;			
		}
		sprintf(Check_path, "%s/FQPM", fq_home_path);
	}
	else {
		sprintf(Check_path, "%s",  path);
		sprintf(path_filename, "%s/%d", path, pid);
	}

	sprintf(path_filename, "%s/%d", Check_path, pid);
	if( all_flag == false ) {
		rc = kill( pid, 0 );

		if( rc == 0 ) {
			fq_log(l, FQ_LOG_INFO, "%s/%d alive\n", Check_path, pid);
			return rc;
		}
		else {
			fq_log(l, FQ_LOG_EMERG, "pid = %ld is not running.", (long) pid);
			sprintf(path_filename, "%s/%d", Check_path, pid);
			unlink(path_filename);
			return rc;
		}
	}
	else {
		/* Use directory callback function */
		recursive_dir_CB( l, Check_path, my_CB_func);
	}

	FQ_TRACE_EXIT(l);
	return true;
}

bool get_ps_info( fq_logger_t *l, const char *path, pid_t pid, fq_ps_info_t *ps_info, bool debug_flag)
{
	char path_filename[4096];
	char Check_path[126];
	char *fq_home_path=NULL;
	char buf[512];
	conf_obj_t *obj=NULL;

	FQ_TRACE_ENTER(l);

	if( !path ) {
		fq_home_path = getenv("FQ_DATA_HOME");
		if( !fq_home_path ) {
			fq_log(l, FQ_LOG_ERROR, "There is no FQ_DATA_HOME in env of server.");
			FQ_TRACE_EXIT(l);
			return false;			
		}
		sprintf(Check_path, "%s/FQPM", fq_home_path);
	}
	else {
		sprintf(Check_path, "%s",  path);
		sprintf(path_filename, "%s/%d", path, pid);
	}
	sprintf(path_filename, "%s/%d", Check_path, pid);


	int rc;

	rc = open_conf_obj( path_filename, &obj);
	if( rc == FALSE) {
		fq_log(l, FQ_LOG_ERROR, "Can't open '%s'.", path_filename);
		if(ps_info) free(ps_info);

		FQ_TRACE_EXIT(l);
		return false;
	}
	ps_info->pid = pid;
	obj->on_get_str(obj, "RUN_NAME", buf);
	ps_info->run_name = strdup(buf);

	obj->on_get_str(obj, "EXE_NAME", buf);
	ps_info->exe_name = strdup(buf);

	obj->on_get_str(obj, "CWD", buf);
	ps_info->cwd = strdup(buf);
	
	obj->on_get_str(obj, "USER_CMD", buf);
	ps_info->user_cmd = strdup(buf);

	rc = obj->on_get_str(obj, "ARG-0", buf);
	if( rc == 1 )
		ps_info->arg_0 = strdup(buf);
	else
		ps_info->arg_0 = strdup("");

	rc = obj->on_get_str(obj, "ARG-1", buf);
	if( rc == 1 )
		ps_info->arg_1 = strdup(buf);
	else
		ps_info->arg_1 = strdup("");

	ps_info->uptime_bin = obj->on_get_long(obj, "UPTIME_BIN");

	obj->on_get_str(obj, "UPTIME_ASCII", buf);
	ps_info->uptime_ascii = strdup(buf);

	int tf;
	tf = obj->on_get_int(obj, "RESTART_FLAG");
	ps_info->restart_flag = (bool)tf;

	int heartbeat_sec;
	heartbeat_sec = obj->on_get_int(obj, "HEARTBEAT_SEC");
	ps_info->heartbeat_sec = heartbeat_sec;

	rc = kill( pid, 0 );
	if( rc == 0 ) {
		ps_info->isLive = true;
	}
	else {
		ps_info->isLive = false;
	}

    struct stat attr;
    rc = stat(path_filename, &attr);
	if( rc < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "Can't get last update time of '%s'.", path_filename);
		FQ_TRACE_EXIT(l);
		close_conf_obj(&obj);
		return false;
	}
	ps_info->mtime = attr.st_mtime;

	if( debug_flag ) {
		obj->on_print_conf(obj);
	}

	close_conf_obj(&obj);

	FQ_TRACE_EXIT(l);
	return true;
}

bool is_live_process(pid_t pid)
{
	int rc;

	rc = kill( pid, 0 );
	if( rc == 0 ) {
		return true;
	}
	else {
		return false;
	}
}



void ps_info_free( fq_ps_info_t *t)
{
	SAFE_FREE(t->run_name);
	SAFE_FREE(t->exe_name);
	SAFE_FREE(t->cwd);
	SAFE_FREE(t->user_cmd);
	SAFE_FREE(t->arg_0);
	SAFE_FREE(t->arg_1);
	SAFE_FREE(t->uptime_ascii);
	SAFE_FREE(t);

	return;
}

static ps_t *
ps_new( fq_ps_info_t *psinfo)
{
	ps_t *t;

	t = (ps_t *)calloc(1, sizeof(ps_t));

	t->psinfo = (fq_ps_info_t *)calloc(1, sizeof(fq_ps_info_t));
	
	t->psinfo->pid = psinfo->pid;
	t->psinfo->run_name = strdup(psinfo->run_name);
	t->psinfo->exe_name = strdup(psinfo->exe_name);
	t->psinfo->cwd = strdup(psinfo->cwd);
	t->psinfo->arg_0 = strdup(psinfo->arg_0);
	t->psinfo->arg_1 = strdup(psinfo->arg_1);
	t->psinfo->uptime_bin = psinfo->uptime_bin;
	t->psinfo->uptime_ascii = strdup(psinfo->uptime_ascii);
	t->psinfo->isLive = psinfo->isLive;
	t->psinfo->mtime = psinfo->mtime;
	t->psinfo->restart_flag = psinfo->restart_flag;
	t->psinfo->heartbeat_sec = psinfo->heartbeat_sec;
	
	t->next = NULL;
	return(t);
}

static void
ps_free( ps_t *t )
{
	SAFE_FREE(t->psinfo->run_name);
	SAFE_FREE(t->psinfo->exe_name);
	SAFE_FREE(t->psinfo->cwd);
	SAFE_FREE(t->psinfo->user_cmd);
	SAFE_FREE(t->psinfo->arg_0);
	SAFE_FREE(t->psinfo->arg_1);
	SAFE_FREE(t->psinfo->uptime_ascii);
	SAFE_FREE(t->psinfo);
	SAFE_FREE(t);
}

pslist_t *
pslist_new()
{
	pslist_t *t;

	t = (pslist_t*) calloc(1, sizeof(pslist_t));

	t->length = 0;
	t->head = t->tail = NULL;
	t->next = NULL;
	return(t);
}

void
pslist_free(pslist_t **t)
{
	ps_t *p, *q;
	if( !(*t) ) return;

	p = (*t)->head;
	while(p) {
		q = p;
		p = p->next;
		ps_free(q);
	}
	SAFE_FREE(*t);
}

ps_t *
pslist_put( pslist_t *t, fq_ps_info_t *ps_info)
{
	ps_t *p;

	p = ps_new( ps_info );
	if( !t->tail ) 
		t->head = t->tail = p;
	else {
		t->tail->next = p;
		t->tail = p;
	}
	(t->length)++;

	return(p);
}

ps_t *
pslist_pop( pslist_t *t )
{
	ps_t *p;

	p = t->head;
	if(!p ) return(NULL);

	t->head = p->next;
	(t->length)-- ;
	return(p);
}

void
pslist_print(pslist_t *t, FILE *fp)
{
	ps_t	*p;

	if( !t ) return;

	fprintf(fp, "pslist contains %d elements\n", t->length);

	for( p=t->head; p != NULL; p=p->next) {
		fprintf(fp, "\t- pid='%d'\n", p->psinfo->pid);
		fprintf(fp, "\t- run_name='%s'\n", p->psinfo->run_name);
		fprintf(fp, "\t- exe_name='%s'\n", p->psinfo->exe_name);
		fprintf(fp, "\t- cwd='%s'\n", p->psinfo->cwd);
		fprintf(fp, "\t- user_cmd='%s'\n", p->psinfo->user_cmd);
		fprintf(fp, "\t- arg_0='%s'\n", p->psinfo->arg_0);
		fprintf(fp, "\t- arg_1='%s'\n", p->psinfo->arg_1);
		fprintf(fp, "\t- uptime_bin='%ld'\n", p->psinfo->uptime_bin);
		fprintf(fp, "\t- uptime_ascii='%s'\n", p->psinfo->uptime_ascii);
		fprintf(fp, "\t- restart_flag='%d'\n", p->psinfo->restart_flag);
		fprintf(fp, "\t- heartbeat_sec='%d'\n", p->psinfo->heartbeat_sec);
		fprintf(fp, "\t- isLive='%d'\n", p->psinfo->isLive);
		fflush(fp);
	}
}

static int get_int_typeOfFile(mode_t mode)
{
    switch (mode & S_IFMT) {
                case S_IFREG: /* normal file */
                        return(1);
                case S_IFDIR: /* directory */
                        return(2);
                case S_IFCHR:
                        return(3);
                case S_IFBLK:
                        return(4);
                case S_IFLNK:
                        return(5);
                case S_IFIFO:
                        return(6);
                case S_IFSOCK:
                        return(7);
                default:
                        return(-1);
    }
}

#include <dirent.h>

/*
** make_ps_list( fq_logger_t *l, char *path, pslist_t *ps_list )
** Description: This function makes linked list with all process registration.
** path: manage directory name for managing. Default is FQ_DATA_HOME in env.
*/
bool make_ps_list( fq_logger_t *l, char *path, pslist_t *ps_list )
{
	int count, rc;

	char Check_path[126];
	char *fq_home_path=NULL;

	FQ_TRACE_ENTER(l);

	if( !path ) {
		fq_home_path = getenv("FQ_DATA_HOME");
		if( !fq_home_path ) {
			fq_log(l, FQ_LOG_ERROR, "There is no FQ_DATA_HOME in env of server.");
			FQ_TRACE_EXIT(l);
			return false;			
		}
		sprintf(Check_path, "%s/FQPM", fq_home_path);
	}
	else {
		sprintf(Check_path, "%s",  path);
	}

	DIR *dir;
	struct dirent *dirent;
	int	ftype;

	fq_log(l, FQ_LOG_DEBUG, "opendir('%s') start.", Check_path);
	if ((dir = opendir( Check_path )) == NULL) {
		fq_log(l, FQ_LOG_ERROR, "opendir('%s') failed.", Check_path);
		return(false);
	}

	fq_log(l, FQ_LOG_DEBUG, "opendir('%s') success.", Check_path);
	while ((dirent = readdir(dir)) != NULL) {
		struct stat fbuf;
		char fullname[512];

		if (strcmp (dirent->d_name,   "." ) == 0 || strcmp (dirent->d_name,   "..") == 0) continue;

		/* filename is not digit(number) */
		if( IsValidPidNumber(dirent->d_name) == false ) {
			continue;
		}

		sprintf(fullname, "%s/%s", Check_path, dirent->d_name);

		if( stat(fullname, &fbuf) < 0 ) {
			fq_log(l, FQ_LOG_ERROR,  "stat() error. resson=[%s].", strerror(errno));
			continue;
		}

		if( (ftype = get_int_typeOfFile(fbuf.st_mode)) == 1 ) { /* is normal file */
			pid_t pid;

			fq_log(l, FQ_LOG_DEBUG, "file: '%s'.", dirent->d_name); 

			pid = (pid_t)atol( dirent->d_name);
			fq_ps_info_t *one_ps_info=NULL;
    		one_ps_info = (fq_ps_info_t *)calloc(1, sizeof(fq_ps_info_t));

			if( l && l->level <= FQ_LOG_DEBUG_LEVEL ) { /* 0: TRACE 1: DEBUG */
				get_ps_info( l, Check_path, pid, one_ps_info, true);
			}
			else {
				get_ps_info( l, Check_path, pid, one_ps_info, false);
			}
			fq_log(l, FQ_LOG_DEBUG, "get_ps_info() end: '%ld'.", pid); 

			ps_t *p=NULL;

			p = pslist_put ( ps_list, one_ps_info );
			if( !p ) {
				fq_log(l, FQ_LOG_ERROR, "pslist_put() error.");
				FQ_TRACE_EXIT(l);
				return(false);
			}

			if(one_ps_info) ps_info_free(one_ps_info);
		}
		else if( ftype == 2) { /* directory */
			continue;
		}
    }

	fq_log(l, FQ_LOG_DEBUG, "read dir('%s') end.", Check_path);

	closedir(dir);

	if(l && l->level <= FQ_LOG_DEBUG_LEVEL) {
		fq_file_logger_t *p = l->logger_private;

		pslist_print(ps_list, p->logfile);
	}

	FQ_TRACE_EXIT(l);

	return(true);
}

bool conv_from_pslist_to_json(fq_logger_t *l, pslist_t *ps_list, JSON_Value *jroot, JSON_Object *jobj, JSON_Array *jary_pids, char **dst, int *list_cnt, bool pretty_flag)
{
	ps_t *p=NULL;

#if 0
	/* Templete of json data */
	"ProcessIDs": [
		"1111",
		"2222",
		"4444"
	],
	"1111": {
		"Run_name": "/aaa/a.exe",
		"Exe_name": "a.exe",
		"Cwd": "/aaa",
		"User_cmd": "./a.exe",
		"Arg_0": "my.cfg",
		"Arg_1": "debug",
		"Uptime_bin": "1621165312",
		"Uptime_ascii": "20210516:204152"
	},
#endif

	// fprintf(stdout, "pslist contains %d elements\n", ps_list->length);
	
	*list_cnt = ps_list->length;

	for( p=ps_list->head; p != NULL; p=p->next) {
		char	dotkey[128], dotval[256];

		json_array_append_number(jary_pids, p->psinfo->pid);

		sprintf(dotkey, "%d.Run_name", p->psinfo->pid);
		json_object_dotset_string(jobj, dotkey, p->psinfo->run_name);

		sprintf(dotkey, "%d.Exe_name", p->psinfo->pid);
		json_object_dotset_string(jobj, dotkey, p->psinfo->exe_name);

		sprintf(dotkey, "%d.Cwd", p->psinfo->pid);
		json_object_dotset_string(jobj, dotkey, p->psinfo->cwd);

		sprintf(dotkey, "%d.User_cmd", p->psinfo->pid);
		json_object_dotset_string(jobj, dotkey, p->psinfo->user_cmd);

		sprintf(dotkey, "%d.Arg_0", p->psinfo->pid);
		json_object_dotset_string(jobj, dotkey, p->psinfo->arg_0);

		sprintf(dotkey, "%d.Arg_1", p->psinfo->pid);
		json_object_dotset_string(jobj, dotkey, p->psinfo->arg_1);

		sprintf(dotkey, "%d.Uptime_bin", p->psinfo->pid);
		json_object_dotset_number(jobj, dotkey, p->psinfo->uptime_bin);

		sprintf(dotkey, "%d.Uptime_ascii", p->psinfo->pid);
		json_object_dotset_string(jobj, dotkey, p->psinfo->uptime_ascii);

		sprintf(dotkey, "%d.Islive", p->psinfo->pid);
		json_object_dotset_boolean(jobj, dotkey, p->psinfo->isLive);

		sprintf(dotkey, "%d.Mtime", p->psinfo->pid);
		json_object_dotset_number(jobj, dotkey, p->psinfo->mtime);

		sprintf(dotkey, "%d.Restart_flag", p->psinfo->pid);
		json_object_dotset_boolean(jobj, dotkey, p->psinfo->restart_flag);

		sprintf(dotkey, "%d.Heartbeat_sec", p->psinfo->pid);
		json_object_dotset_boolean(jobj, dotkey, p->psinfo->heartbeat_sec);
	}

	char *buf=NULL;
	size_t buf_size;

	buf_size = json_serialization_size(jroot) + 1;
	buf = calloc(sizeof(char), buf_size);
	
	if( pretty_flag == true ) {
		buf = json_serialize_to_string_pretty(jroot);
		*dst = buf;
	} else {
		json_serialize_to_buffer(jroot, buf, buf_size);
		*dst = buf;
	}

	//   	json_serialize_to_file_pretty(jroot, json_filename);

	return true;
}
#if 0
/* example for json */
	JSON_Value *rootValue;
	JSON_Object *rootObject;

	rootValue = json_value_init_object();             // JSON_Value 생성 및 초기화
	rootObject = json_value_get_object(rootValue);    // JSON_Value에서 JSON_Object를 얻음

	json_object_set_value(rootObject, "ProcessIDs", json_value_init_array());
	JSON_Array *jary_pids = json_object_get_array(rootObject, "ProcessIDs");

	char *buf=NULL;
	bool pretty_flag = true;
	int	list_count=0;
	conv_from_pslist_to_json(l, pslist, rootValue, rootObject, jary_pids, &buf, &list_count, pretty_flag);

	printf("%s\n", buf);

	if(buf) free(buf);

	json_value_free(rootValue);
#endif

/*
** FQPM process uses it for eventlog.
** Warning: Other can't use it.
*/
#if 0
typedef struct _fqpm_eventlog_obj_t fqpm_eventlog_obj_t;
struct _fqpm_eventlog_obj_t {
	fq_logger_t *l;

	char *path;
	char *fn;
	time_t	opened_time;
	FILE	*fp;

	bool(*on_eventlog)(fq_logger_t *, fqpm_eventlog_obj_t *, char *, char *, ...);
};
#endif

static bool on_eventlog(fq_logger_t *l, fqpm_eventlog_obj_t *obj, char *event_name, char *run_name, char *arg_0, ...);

bool open_fqpm_eventlog_obj( fq_logger_t *l, char *path, fqpm_eventlog_obj_t **obj)
{
	fqpm_eventlog_obj_t	*rc=NULL;
	char	*fq_home_path=NULL;
	char	fqpm_eventlog_path[512];

	FQ_TRACE_ENTER(l);

	if( !path ) {
		fq_home_path = getenv("FQ_DATA_HOME");
		if( !fq_home_path ) {
			fq_log(l, FQ_LOG_ERROR, "There is no FQ_DATA_HOME in env of server.");
			FQ_TRACE_EXIT(l);
			return false;			
		}
		sprintf(fqpm_eventlog_path, "%s/FQPM.EVT.LOG", fq_home_path);
	}
	else {
		sprintf(fqpm_eventlog_path, "%s/FQPM.EVT.LOG", path);
	}

	rc = (fqpm_eventlog_obj_t *)calloc( 1, sizeof(fqpm_eventlog_obj_t));
	if(rc) {
		char datestr[9], timestr[7];
		char fn[1024];

		rc->path = strdup(fqpm_eventlog_path);
		rc->opened_time = time(0);

		get_time(datestr, timestr);
		snprintf(fn, sizeof(fn), "%s/fqpm_%s.eventlog", fqpm_eventlog_path, datestr);
		rc->fn = strdup(fn);

		rc->on_eventlog = on_eventlog; /* put function pointer */

		if( 0 != access(fqpm_eventlog_path, F_OK) ) { /* create FQPM directory newly */
			mkdir(fqpm_eventlog_path, 0777);
		}

		rc->fp = fopen(fn, "a");
		if(!rc->fp) {
			fq_log(l, FQ_LOG_ERROR, "failed to open eventlog file. [%s]", fn);
			goto return_FALSE;
		}

		*obj = rc;
		FQ_TRACE_EXIT(l);
		return true;
	}
return_FALSE:
	SAFE_FREE(rc->path);
	SAFE_FREE(rc->fn);
	if(rc->fp) fclose(rc->fp);
	rc->fp = NULL;
	SAFE_FREE(rc);
	SAFE_FREE(*obj);

	FQ_TRACE_EXIT(l);
	return false;
}

bool close_fqpm_eventlog_obj(fq_logger_t *l, fqpm_eventlog_obj_t **obj)
{
	FQ_TRACE_ENTER(l);

	SAFE_FREE( (*obj)->path );
	SAFE_FREE( (*obj)->fn );
	if((*obj)->fp) fclose((*obj)->fp);
	(*obj)->fp = NULL;
	SAFE_FREE( (*obj) );

	FQ_TRACE_EXIT(l);

	return true;
}

bool on_eventlog(fq_logger_t *l, fqpm_eventlog_obj_t *obj, char *event_name, char *run_name, char *arg_0, ...)
{
	char datestr[9], timestr[7];
	va_list ap;
	char	opened_time[32];
	char	current_time[32];

	FQ_TRACE_ENTER(l);

	char *p=NULL;
	sprintf( current_time, "%s", (p=asc_time(time(0))));
	if(p) free(p);
	get_time(datestr, timestr);

	/*
	** 오픈날짜와 현재날짜가 변경되었는지를 확인한다.
	*/
	if( strncmp(opened_time, current_time, 10) != 0 )  {
		char fn[256];

		fclose(obj->fp);
		obj->fp = NULL;
		snprintf(fn, sizeof(fn), "%s/fqpm_%s.eventlog", obj->path, datestr);
		obj->fp = fopen(fn, "a");

		if(!obj->fp) { 
			fq_log(l, FQ_LOG_ERROR, "failed to open eventlog file. [%s]", fn);
			goto return_FALSE;
		}
		if(obj->fn) free(obj->fn);
		obj->fn = strdup(fn);
		obj->opened_time = time(0);
	}

	va_start(ap, arg_0);

	fprintf(obj->fp, "%s|%s|%s|%s|%s\n", datestr, timestr, event_name, run_name, VALUE(arg_0));

	va_end(ap);

	fflush(obj->fp);
	return true;

return_FALSE:
	fprintf(stderr, "on_eventlog() failed.\n");
	return false;
}

#if 0
static bool OpenFileQueues_and_MakeLinkedlist(fq_logger_t *l, fq_container_t *fq_c, linkedlist_t *ll)
{
	dir_list_t *d=NULL;
	fq_node_t *f;
	ll_node_t *ll_node = NULL;
	int sn=1;
	int opened = 0;

	for( d=fq_c->head; d!=NULL; d=d->next) {
		for( f=d->head; f!=NULL; f=f->next) {
			fqueue_obj_t *obj=NULL;
			int rc;
			queue_obj_node_t *tmp = NULL;

			obj = calloc(1, sizeof(fqueue_obj_t));

			fq_log(l, FQ_LOG_DEBUG, "OPEN: [%s/%s].", d->dir_name, f->qname);
			
			char *p=NULL;
			if( (p= strstr(f->qname, "SHM_")) == NULL ) {
				rc =  OpenFileQueue(l, NULL, NULL, NULL, NULL, d->dir_name, f->qname, &obj);
			} 
			else {
				rc =  OpenShmQueue(l, d->dir_name, f->qname, &obj);
			}
			if( rc != TRUE ) {
				fq_log(l, FQ_LOG_ERROR, "OpenFileQueue('%s') error.", f->qname);
				return(false);
			}
			fq_log(l, FQ_LOG_DEBUG, "%s:%s open success.", d->dir_name, f->qname);

			/*
			** Because of shared memory queue, We change it to continue.
			*/
			// CHECK(rc == TRUE);
			char	key[256];
			sprintf(key, "%s/%s", d->dir_name, f->qname);
			tmp = calloc(1, sizeof(queue_obj_node_t));
			tmp->obj = obj;

			size_t value_size=sizeof(fqueue_obj_t);

			ll_node = linkedlist_put(ll, key, (void *)tmp, value_size);

			if(!ll_node) {
				fq_log(l, FQ_LOG_ERROR, "linkedlist_put('%s', '%s') error.", d->dir_name, f->qname);
				return(false);
			}

			if(tmp) free(tmp);

			opened++;
		}
	}

	fq_log(l, FQ_LOG_INFO, "Number of opened filequeue is [%d].", opened);

	return(true);
}


/* 나를 부를때에는 뒤의 함수형식에 맞춰 함수를 만들고, 그 함수명만을 전달해
** 그러면 너가 만든 함수를 내가 너의 함수 인자에 맞춰서 너함수를 불러줄께.
*/ 
int all_queue_scan_CB_server(int sleep_time, bool(*your_func_name)(size_t value_sz, void *value) )
{
	linkedlist_t    *ll=NULL;
	fq_container_t *fq_c=NULL;
	fq_logger_t *l=NULL;

	int rc = load_fq_container(l, &fq_c);
	ll = linkedlist_new("fqueue_objects");
	bool tf;
    tf = OpenFileQueues_and_MakeLinkedlist(l, fq_c, ll);
    CHECK(tf == true);

	while(1) {
		bool result;
		result = linkedlist_callback(ll, your_func_name );
		if( result == false ) break;
		sleep(sleep_time);
	}
	if(ll) linkedlist_free(&ll);
}
#endif
void log_call_process_info( fq_logger_t *l)
{
	int rc;
	char run_name[PATH_MAX];

	pid_t call_pid = getpid();
	rc = get_exe_for_pid(call_pid, run_name, sizeof(run_name));
	if( rc > 0 ){
		fq_log(l, FQ_LOG_ERROR, "RUN_NAME=\"%s\".", run_name); 

		char *ts1 = strdup(run_name);
		char *ts2 = strdup(run_name);

		char *dir=dirname(ts1);
		char *exe_name=basename(ts2);

		fq_log(l, FQ_LOG_ERROR , "EXE_NAME=\"%s\"\n", exe_name); 

		if(ts1) free(ts1);
		if(ts2) free(ts2);
	} 
	return;
}
