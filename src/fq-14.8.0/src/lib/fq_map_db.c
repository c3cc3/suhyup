/*
** fq_map_db.c
*/
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include "fq_logger.h"
#include "fq_common.h"
#include "fq_map_db.h"
#include "fq_flock.h"
#include "fqueue.h"

static	int on_insert(fq_logger_t *l, map_db_obj_t *obj, char *key, char *value);
static	int on_delete(fq_logger_t *l, map_db_obj_t *obj, char *key);
static	int on_update(fq_logger_t *l, map_db_obj_t *obj, char *key, char *value);
static	int on_retrieve(fq_logger_t *l, map_db_obj_t *obj, char *key, char *value);
static	int on_getlist(fq_logger_t *l, map_db_obj_t *obj, int *cnt, char **all_string);
static	int on_show(fq_logger_t *l, map_db_obj_t *obj);

static void *test()
{
	void *p;

	p = calloc(1, sizeof(char));
	return(p);
}

static void *local_mmapping(fq_logger_t *l, int fd, size_t space_size, off_t from_offset)
{

	void *p;
	register int	prot = PROT_WRITE|PROT_READ;
#ifdef OS_HPUX
	// register int flag = MAP_SHARED|MAP_IO; /* header file must be share */
	register int flag = MAP_SHARED|MAP_SHLIB; /* header file must be share */
	/* It(MAP_SHLIB) tells the kernel to use large-pages for the shared-library's
	* text. Results in fewer TLB misses. Just don't use it for private
	* pages!
	*/
#elif OS_AIX
	register int flag = MAP_SHARED; 
#else
	register int flag = MAP_SHARED|MAP_NORESERVE; /* header file must be share */
#endif

	FQ_TRACE_ENTER(l);

	prot = PROT_WRITE|PROT_READ;

	p = mmap(NULL, space_size, prot,  flag, fd, from_offset);
	if( p == MAP_FAILED ) {
		fq_log(l, FQ_LOG_ERROR, "mmap() error.[%s] space_size=[%ld] from_offset=[%ld]", 
			strerror(errno), space_size, from_offset);
		FQ_TRACE_EXIT(l);
		return(NULL);
	}
	
	FQ_TRACE_EXIT(l);
	return(p);
}
static int 
local_munmap(fq_logger_t *l, void *addr, size_t length)
{
	int rc;

	FQ_TRACE_ENTER(l);
	rc = munmap(addr, length);
	addr = (void *)0;
	if(rc < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "munmap() error. reason=[%s]", strerror(errno));
		FQ_TRACE_EXIT(l);
		return(rc);
	}
	FQ_TRACE_EXIT(l);
	return(rc);
}

int 
open_map_db_obj( fq_logger_t *l, char *path, char *name, map_db_obj_t **obj)
{
	map_db_obj_t *rc=NULL;

	FQ_TRACE_ENTER(l);
	
	if( !HASVALUE(path) || !HASVALUE(name) )  {
		fq_log(l, FQ_LOG_ERROR, "illgal function call.");
        FQ_TRACE_EXIT(l);
        return(FALSE);
    }

	rc  = (map_db_obj_t *)calloc(1, sizeof(map_db_obj_t));
	if( rc ) {
		int ret;
		char filename[128-1];

		rc->path = strdup(path);
		rc->name = strdup(name);

		ret = open_flock_obj( l, path, name, ETC_FLOCK, &rc->flobj);
		if( ret == FALSE )  {
			fq_log(l, FQ_LOG_ERROR, "open_flock_obj('%s', '%s') error." , path, name);
			goto return_FALSE;
		}

		sprintf(filename, "%s.map.DB", name);
		sprintf(rc->pathfile, "%s/%s", path, filename);
		
		if( !is_file(rc->pathfile) ) {
			void *p;
			int n;

			if( (rc->fd=create_dummy_file(l, rc->path, filename, sizeof(map_db_table_t)))  < 0) {
				fq_log(l, FQ_LOG_ERROR, "create_dummy_file('%s','%s') error. ", rc->path, rc->name);
				goto return_FALSE;
			}
			close(rc->fd);
			rc->fd = -1;
		}

		if( (rc->fd=open(rc->pathfile, O_RDWR|O_SYNC, 0666)) < 0 ) {
			fq_log(l, FQ_LOG_ERROR, "DB file[%s] can not open.", rc->pathfile );
			goto return_FALSE;
		}

		rc->mmap_len = sizeof(map_db_table_t);

		rc->t = (map_db_table_t *)local_mmapping(l, rc->fd, rc->mmap_len, (off_t)0);

		if( !rc->t  ) {
			fq_log(l, FQ_LOG_ERROR, "fq_mmapping('%s') error.", rc->pathfile);
			goto return_FALSE;
		}

		rc->l = l;
		rc->on_insert = on_insert;
		rc->on_delete = on_delete;
		rc->on_update = on_update;
		rc->on_retrieve = on_retrieve;
		rc->on_getlist = on_getlist;
		rc->on_show = on_show;

		*obj = rc;

		FQ_TRACE_EXIT(l);
		return(TRUE);
	}
return_FALSE:
	SAFE_FREE( rc->path );
	SAFE_FREE( rc->name );

	close_flock_obj( l, &rc->flobj ) ;

	SAFE_FREE(*obj);
	*obj = NULL;

	FQ_TRACE_EXIT(l);
	return(FALSE);
}

int 
close_map_db_obj( fq_logger_t *l, map_db_obj_t **obj)
{
	FQ_TRACE_ENTER(l);
	
	SAFE_FREE( (*obj)->path );
	SAFE_FREE( (*obj)->name );


	local_munmap(l, (*obj)->t, (*obj)->mmap_len);

	close_flock_obj( l, &(*obj)->flobj);

	if( (*obj)->fd > 0 ) close( (*obj)->fd);

	SAFE_FREE( (*obj) );

	FQ_TRACE_EXIT(l);
	return(TRUE);
}

int 
unlink_map_db( fq_logger_t *l, char *path, char *name) 
{
	char pathfile[128];

	sprintf(pathfile, "%s/.%s.map.DB", path, name);
	unlink(pathfile);

	sprintf(pathfile, "%s/.%s.flock", path, name);
	unlink(pathfile);

	return(0);
}

static int find_index( fq_logger_t *l, map_db_table_t *t, char *key ) 
{
	int i;

	FQ_TRACE_ENTER(l);
	for(i=0; i<MAX_RECORDS; i++) {
		if( strcmp(key, t->R[i].key) == 0 ) {
			return(i);
		}
	}
	FQ_TRACE_EXIT(l);
	return(-1);
}

static int get_index( fq_logger_t *l, map_db_table_t *t, char *key ) 
{
	int i;

	FQ_TRACE_ENTER(l);
	for(i=0; i<MAX_RECORDS; i++) {
		if( HASVALUE(t->R[i].key) ) {
			continue;
		}
		return(i);
	}
	FQ_TRACE_EXIT(l);
	return(-1);
}

static int 
on_insert( fq_logger_t *l, map_db_obj_t *obj, char *key, char *value )
{
	int index;
	int key_len, value_len;

	FQ_TRACE_ENTER(l);

	if( !HASVALUE(key) || !HASVALUE(value)) {
		fq_log( obj->l, FQ_LOG_ERROR, "illegal request.");
		FQ_TRACE_EXIT(l);
		return(FALSE);
	}
	
	key_len = strlen(key);
	value_len = strlen(value);
	if( (key_len > MAX_KEY_LEN) || ( value_len > MAX_VALUE_LEN) ) {
		fq_log( obj->l, FQ_LOG_ERROR, "Too long key or value.");
		FQ_TRACE_EXIT(l);
		return(FALSE);
	}

	obj->flobj->on_flock( obj->flobj );

	index = find_index(l, obj->t, key);
	if( index >= 0 ) {
		fq_log( obj->l, FQ_LOG_ERROR, "already exist key.[%s]", key);
		FQ_TRACE_EXIT(l);	
		obj->flobj->on_funlock( obj->flobj );
		return(FALSE);
	}
	index = get_index(l, obj->t, key);
	if( index < 0 ) {
		fq_log( obj->l, FQ_LOG_ERROR, "There is no empty.");
		FQ_TRACE_EXIT(l);
		obj->flobj->on_funlock( obj->flobj );
		return(FALSE);
    }
	
	sprintf(obj->t->R[index].key, "%s", key);
	sprintf(obj->t->R[index].value, "%s", value);

	obj->flobj->on_funlock( obj->flobj );
	FQ_TRACE_EXIT(l);
	return(TRUE);
}

static int 
on_delete( fq_logger_t *l, map_db_obj_t *obj, char *key )
{
	int index;

	FQ_TRACE_ENTER(l);

	obj->flobj->on_flock( obj->flobj );

	index = find_index(l, obj->t, key);
	if( index < 0 ) {
		fq_log( obj->l, FQ_LOG_ERROR, "Can't find key.[%s]", key);
		FQ_TRACE_EXIT(l);	
		obj->flobj->on_funlock( obj->flobj );
		return(FALSE);
	}

	memset(obj->t->R[index].key, 0x00, sizeof(obj->t->R[index].key));
	memset(obj->t->R[index].value, 0x00, sizeof(obj->t->R[index].value));

	obj->flobj->on_funlock( obj->flobj );
	FQ_TRACE_EXIT(l);
	return(TRUE);
}

static int 
on_update( fq_logger_t *l, map_db_obj_t *obj, char *key, char *value )
{
	int index;

	FQ_TRACE_ENTER(l);

	obj->flobj->on_flock( obj->flobj );

	index = find_index(l, obj->t, key);
	if( index < 0 ) {
		fq_log( obj->l, FQ_LOG_ERROR, "Can't find key.[%s]", key);
		FQ_TRACE_EXIT(l);	
		obj->flobj->on_funlock( obj->flobj );
		return(FALSE);
	}

	/* sprintf(obj->t->R[index].key, "%s", key); */
	sprintf(obj->t->R[index].value, "%s", value);

	obj->flobj->on_funlock( obj->flobj );
	FQ_TRACE_EXIT(l);
	return(TRUE);
}

static int 
on_retrieve( fq_logger_t *l, map_db_obj_t *obj, char *key, char *value )
{
	int index;

	FQ_TRACE_ENTER(l);

	obj->flobj->on_flock( obj->flobj );

	index = find_index(l, obj->t, key);
	if( index < 0 ) {
		fq_log( obj->l, FQ_LOG_ERROR, "Can't find key.[%s]", key);
		FQ_TRACE_EXIT(l);	
		obj->flobj->on_funlock( obj->flobj );
		return(FALSE);
	}

	sprintf( value, "%s", obj->t->R[index].value);

	obj->flobj->on_funlock( obj->flobj );
	FQ_TRACE_EXIT(l);
	return(TRUE);
}

static int 
on_getlist( fq_logger_t *l, map_db_obj_t *obj, int *cnt, char **all_string )
{
	int i;
	char    one[MAX_KEY_LEN+MAX_VALUE_LEN+1];
	char    all[65536];
	int		count=0;

	FQ_TRACE_ENTER(l);

	memset(all, 0x00, sizeof(all));
	obj->flobj->on_flock( obj->flobj );

	for(i=0; i<MAX_RECORDS; i++) {
		if( HASVALUE(obj->t->R[i].key) ) {
			memset(one, 0x00, sizeof(one));
			sprintf(one, "%s,%s", obj->t->R[i].key, obj->t->R[i].value);
			strcat(all, one);
			count++;
		}
	}

	*cnt = count;
	*all_string = strdup(all); 

	obj->flobj->on_funlock( obj->flobj );

	FQ_TRACE_EXIT(l);

	return(TRUE);
}

#define RECORD_DELIMITER_CHAR '`'
static int
on_show( fq_logger_t *l, map_db_obj_t *obj)
{
	int rc, i;
	int count;
	char	*all_string=NULL;
	char	*dst[MAX_RECORDS];

	FQ_TRACE_ENTER(l);

	for(i=0; i<MAX_RECORDS; i++) {
		dst[i] = NULL;
	}

	printf("map_db path: %s\n", obj->path);
	printf("map_db name: %s\n", obj->name);
	printf("map_db real file: %s\n", obj->pathfile);
	printf("map_db lock file: %s\n", obj->flobj->pathfile);

	rc = obj->on_getlist(l, obj, &count, &all_string);
	if( rc == FALSE) {
		fq_log(l, FQ_LOG_ERROR, "on_getlist() error.");
		goto return_FALSE;
	}

	printf("\t - record count is [%d]\n", count);
	all_string[strlen(all_string)-1] = 0x00; /* remove last delimiter */
	printf("\t- all stirng  is [%s]\n", all_string);

	rc = delimiter_parse(all_string, RECORD_DELIMITER_CHAR, count, dst);
	CHECK( rc == TRUE );
	for(i=0; i<count; i++) {
		printf("\t\t- '%s'\n", dst[i]);
		if(dst[i]) free(dst[i]);
	}

	if( all_string ) free(all_string);

	FQ_TRACE_EXIT(l);
	return(TRUE);

return_FALSE:

	FQ_TRACE_EXIT(l);
	return(FALSE);
}
