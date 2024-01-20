/* vi: set sw=4 ts=4: */

/* 
 * or add below contents to .vimrc 
** - set tabstop=4
** - set shiftwidth=4
** 
** shm_queue.c
*/
#define SHM_QUEUE_C_VERSION "1.0.1"

#if 0
#define AFTER_DEQ_DELETE  
#endif

/*
** version history
** 1.0.0: 2015/04/28: first version
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#if 0
#include <sys/types.h>
#else
#include <sys/sysmacros.h>
#endif

#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "fq_common.h"
#include "fq_logger.h"
#include "fq_info.h"
#include "fq_info_param.h"
#include "fq_flock.h"

#include "fq_eventlog.h"
#include "fq_external_env.h"

#include "fqueue.h"
#include "shm_queue.h"

static float on_get_usage( fq_logger_t *l, fqueue_obj_t *obj);
static time_t on_check_competition( fq_logger_t *l, fqueue_obj_t *obj, action_flag_t en_de_flag);
static int on_de_bundle_struct(fq_logger_t *l, fqueue_obj_t *obj, int fqdata_cnt, int MaxDataLen, fqdata_t array[], long *run_time);
static int on_reset(fq_logger_t *l, fqueue_obj_t *obj);
static int on_en_bundle_struct( fq_logger_t *l, fqueue_obj_t *obj, int src_cnt, fqdata_t src[], long *run_time);
static int on_de_bundle_array(fq_logger_t *l, fqueue_obj_t *obj, int array_cnt, int MaxDataLen, unsigned char *array[], long *run_time);
static int on_disable(fq_logger_t *l, fqueue_obj_t *obj);
static int on_enable(fq_logger_t *l, fqueue_obj_t *obj);
static int on_force_skip(fq_logger_t *l, fqueue_obj_t *obj);
static int on_diag(fq_logger_t *l, fqueue_obj_t *obj);
static int on_set_master( fq_logger_t *l, fqueue_obj_t *obj, int on_off_flag, char *hostname);
static bool_t on_do_not_flow( fq_logger_t *l, fqueue_obj_t *obj, time_t waiting_time);
static int on_get_big( fq_logger_t *l, fqueue_obj_t *obj);

static int 
set_bodysize(unsigned char* header, int size, int value)
{
	register int i;
	unsigned char* ptr = header;

	for (i=0; i < size; i++) {
		*ptr = (value >> 8*(size-i-1)) & 0xff;
		ptr++;
	}
	return(0);
}

static int 
get_bodysize(unsigned char* header, int size, int header_type)
{
	register int i, k, value=0;
	unsigned char* ptr = header;

	if ( header_type !=0 ) { /* ASCII */
			header[size] = '\0';
			return(atoi((char*)header));
	}
	else {
		for (i=0; i < size; i++) {
			k = (int)(*ptr);
			value |= ( (k & 0xff) << 8*(size-i-1) );
			ptr++;
		}
		return(value);
	}
}

/* dummy functions */
static int
on_de_XA(fq_logger_t *l, fqueue_obj_t *obj, unsigned char *dst, int dstlen, long *seq, long *run_time, char *unlink_filename)
{
	
	fq_log(l, FQ_LOG_ERROR, "on_de_XA() is not support.");
	return(0);
}
static int
on_commit_XA(fq_logger_t *l, fqueue_obj_t *obj, long seq, long *run_time, char *unlink_filename)
{
	fq_log(l, FQ_LOG_ERROR, "on_commit_XA() is not support.");
	return(0);
}
static int
on_cancel_XA(fq_logger_t *l, fqueue_obj_t *obj, long seq, long *run_time)
{
	fq_log(l, FQ_LOG_ERROR, "on_cancel_XA() is not support.");
	return(0);
}
static long
on_get_diff( fq_logger_t *l, fqueue_obj_t *obj)
{
    long rc;

    FQ_TRACE_ENTER(l);

    obj->h_obj->de_flock->on_flock(obj->h_obj->de_flock);

    rc = obj->h_obj->h->en_sum - obj->h_obj->h->de_sum;

    obj->h_obj->de_flock->on_funlock(obj->h_obj->de_flock);

    FQ_TRACE_EXIT(l);
    return(rc);
}
static int
on_set_QOS( fq_logger_t *l, fqueue_obj_t *obj, int usec)
{
    long rc=0;

    FQ_TRACE_ENTER(l);

    obj->h_obj->de_flock->on_flock(obj->h_obj->de_flock);

    obj->h_obj->h->QOS = usec;

    obj->h_obj->de_flock->on_funlock(obj->h_obj->de_flock);

    obj->evt_obj->on_en_eventlog(l, obj->uq_obj, EVENT_FQ_SETQOS, obj->path, obj->qname, TRUE, getpid() );
    /* en_eventlog(l, EVENT_FQ_SETQOS, path, qname, TRUE, getpid()); */
    FQ_TRACE_EXIT(l);
    return(rc);
}
static int
on_view(fq_logger_t *l, fqueue_obj_t *obj, unsigned char *dst, int dstlen, long *seq, long *run_time, bool *big_flag)
{
	fq_log(l, FQ_LOG_ERROR, "on_view() is not support.");
	return(TRUE);
}

int 
create_dummy_shm(fq_logger_t *l, const char *fname, size_t size)
{
	int		fd;
	int		rc = -1;

	/* shm_opem() parameters */
	int     o_flag=O_CREAT|O_EXCL|O_RDWR;
	mode_t	mode=S_IRUSR | S_IWUSR;
	int		name_len=0;


	FQ_TRACE_ENTER(l);

	
	name_len = strlen(fname);
	if( (name_len < 2) || (name_len > NAME_MAX)) {
		fq_log(l, FQ_LOG_ERROR, "illgal shared memory name rule. length error. len=[%d] NAME_MAX=[%d]\n", name_len, NAME_MAX); 
        FQ_TRACE_EXIT(l);
        return(FALSE);
	}
	if( fname[0] != '/') {
        fq_log(l, FQ_LOG_ERROR, "illgal shared memory name rule.  first charactor is not '/'. [%s]", fname);
        FQ_TRACE_EXIT(l);
        return(FALSE);
	}

	if( (fd = shm_open(fname, o_flag, mode ))<0) {
		fq_log(l, FQ_LOG_ERROR, "shm_open(%s) error. errno=[%d] reason=[%s].", fname, errno, strerror(errno));
		if( errno != 17 ) /* Already File exist */
			goto L0;
		else {
			fq_log(l, FQ_LOG_INFO, "continue...");
			if( (fd=shm_open(fname, O_RDWR|O_SYNC, S_IRUSR | S_IWUSR)) < 0 ) {
				fq_log(l, FQ_LOG_ERROR, "shm_open(%s) error. errno=[%d] reason=[%s].", fname, errno, strerror(errno));
				goto L0;
			}
		}
	}
	fq_log(l, FQ_LOG_INFO, "dummy shared memory open(create) OK.[%s]", fname); 


	if( ftruncate(fd, size) < 0 ) {
		fq_log(l,FQ_LOG_ERROR,"shared memory ftruncate() error. [%s] size=[%ld].", strerror(errno), size);
		goto L0;
	}
	fq_log(l, FQ_LOG_INFO, "dummy shared memory ftruncate OK.[%s]", fname); 

	rc = fd;
L0:
	FQ_TRACE_EXIT(l);
	return(rc);
}

int is_shm( char *fname)
{
    struct stat stbuf;

    if ( stat(fname, &stbuf) != 0 ) {
        return(0); /* doesn't exist */
    }
    else {
		return(1); /* exist */
    }
}

static int 
create_header_shm(fq_logger_t *l, 
			char *qname, 
			int msglen, 
			int mapping_num, 
			int multi_num, 
			char *desc,
			char *xa_mode_on_off,
			char *wait_mode_on_off)
{
	mmap_header_t  *h=NULL;
	int 	rtn=-1;
	char	filename[1024];
	int		fd=0;
	
	FQ_TRACE_ENTER(l);

	sprintf(filename, "/%s_h", qname);

	if( is_file(filename) ) { /* checks whether the file exists. */
		fq_log(l, FQ_LOG_ERROR, "Already shared memory[%s] exists.", filename);
		step_print("- Check if header shared memory already does exist ", "NOK");
		goto L0;
	}
	step_print("- Check if header shared memory already does exist ", "OK");

	if( (fd=create_dummy_shm(l, filename, sizeof(mmap_header_t))) == FALSE) {
		fq_log(l, FQ_LOG_ERROR, "create_dummy_shm() error. name=[%s]", filename);
		step_print("- create header dummy shared memory.", "NOK");
		goto L0;
	}
	step_print("- create header dummy shared memory.", "OK");

	if( (h = (mmap_header_t *)fq_mmapping(l, fd, sizeof(mmap_header_t), (off_t)0)) == NULL) {
        fq_log(l, FQ_LOG_ERROR, "fq_mmapping() error.");
        step_print("- header file mmapping.", "NOK");
        goto L0;
    }
	step_print("- header mmapping.", "OK");

	h->q_header_version = HEADER_VERSION;

	// sprintf(h->path, "%s", "/"); /* shared memory always uses  '/' */
    // sprintf(h->qname, "%s", qname);
    // sprintf(h->desc, "%s", desc);

	strcpy(h->path, "/"); /* shared memory always uses  '/' */
    strcpy(h->qname, qname);
    strcpy(h->desc, desc);

	h->msglen = msglen;
	h->multi_num = multi_num;
	h->mapping_num = mapping_num;

	if( h->msglen <= getpagesize() ) {
		h->mapping_len = (getpagesize() * mapping_num);
		h->file_size = (size_t)((size_t)getpagesize() * (size_t)multi_num * (size_t)mapping_num);
	}
	else {
		h->mapping_len = h->msglen * mapping_num;;
		h->file_size = (size_t)((size_t)h->msglen * (size_t)multi_num * (size_t)mapping_num);
	}

	
	/*
	** mathematical formula for getting max records .
	** max_rec_cnt = (pagesize/msglen))*mmapping_num*multi_num;
	** 
	** real size = h->max_rec_cnt = h->file_size / h->msglen;
	*/
	h->max_rec_cnt = h->file_size / h->msglen;

	if( STRCCMP(xa_mode_on_off, "ON") == 0 ) h->XA_flag = XA_ON;
	else h->XA_flag = XA_OFF;

	if( STRCCMP(wait_mode_on_off, "ON") == 0 ) h->mode = WAIT_MODE;
	else h->mode = NO_WAIT_MODE;

	h->status = QUEUE_ENABLE; 
	h->made_big_dir = 0; 
	h->current_big_files = 0;
	h->big_sum = 0L;
	/* h->en_q_map_offset = 0L; */
	/* h->de_q_map_offset = 0L; */
	h->pagesize = getpagesize();
	h->en_sum = 0L;
	h->de_sum = 0L;
	h->limit_file_size = (h->msglen * h->max_rec_cnt);

	h->create_time = time(0);
	h->last_en_time = time(0);
	h->last_de_time = time(0);

	h->latest_en_time = 0L;
	h->latest_en_time_date = time(0);
	h->latest_de_time = 0L;
	h->latest_de_time_date = time(0);

	h->last_en_row = -1;
	h->oldest_en_row = -1;

	h->QOS = DEFAULT_QOS_VALUE;

	h->en_available_from_offset = 0L;
	h->en_available_to_offset = 0L;
	h->de_available_from_offset = 0L;
	h->de_available_to_offset = 0L;

	step_print("- initialize header shared memory.", "OK");

	rtn = 0;

L0:
	SAFE_FD_CLOSE(fd);
	step_print("- close all files safely.", "OK");

	if((fd > 0) && (rtn != 0) ) {
		if( unlink(filename) < 0 ) {
			fq_log(l, FQ_LOG_ERROR, "unlink([%s]) error.", filename);
		}
	}
	step_print("- Finalize", "OK");

	if(h && h->mapping_len > 0)  fq_munmap(l, (void *)h, sizeof(mmap_header_t));
	
	FQ_TRACE_EXIT(l);

	return(rtn);
}

static int
create_data_shm(fq_logger_t *l, headfile_obj_t *h_obj)
{
	int fd;
	char	filename[256], name[256];

	FQ_TRACE_ENTER(l);

	sprintf(name, "/%s_r", h_obj->h->qname);

	if( is_shm(name) ) {
		fq_log(l, FQ_LOG_ERROR, "Already the shared memory[%s] exists.", filename);
		step_print("- Check if data file already does exist ", "NOK");
		goto L0;
	}
	step_print("- Check if data shared memory already does exist ", "OK");
	
	if( (fd=create_dummy_shm(l, name, h_obj->h->file_size )) < 0) { 
		fq_log(l, FQ_LOG_ERROR, "create_dummy_shm() error. file=[%s]", name);
		step_print("- create dummy data shared memory.", "NOK");
		goto L0;
	}
	step_print("- create dummy data shared memory.", "OK");

	FQ_TRACE_EXIT(l);
    return(TRUE);
	
L0:
	FQ_TRACE_EXIT(l);
	return(FALSE);
}

static int
unlink_data_shm(fq_logger_t *l, char *qname)
{
	char filename[256];

	FQ_TRACE_ENTER(l);

	sprintf(filename, "/%s_r", qname);

	if( shm_unlink(filename) >= 0 ) {
		fq_log(l, FQ_LOG_DEBUG, "shm unlink OK.[%s]", filename);
		FQ_TRACE_EXIT(l);
		return(TRUE);
	}
    fq_log(l, FQ_LOG_DEBUG, "data_shm unlink failed.[%s] errno=[%d] reason=[%s]", filename, errno, strerror(errno));

	FQ_TRACE_EXIT(l);
    return(FALSE);
}

static int
unlink_head_shm(fq_logger_t *l, char *qname)
{
	char filename[256];

	FQ_TRACE_ENTER(l);
	sprintf(filename, "/%s_h", qname);

	if( shm_unlink(filename) >= 0 ) {
		fq_log(l, FQ_LOG_DEBUG, "shm unlink OK.[%s]", filename);
		FQ_TRACE_EXIT(l);
		return(TRUE);
	}
    fq_log(l, FQ_LOG_DEBUG, "header_shm unlink failed.[%s] errno=[%d] reason=[%s]", filename, errno, strerror(errno));

	FQ_TRACE_EXIT(l);
    return(FALSE);
}

#ifdef OS_SOLARIS
#include <sys/types.h>
#include <sys/mkdev.h>
#endif
static int 
on_head_print( fq_logger_t *l, headfile_obj_t *h_obj)
{
	FQ_TRACE_ENTER(l);

	fprintf(stdout, "en_sum = [%ld]\n", h_obj->h->en_sum);
	fprintf(stdout, "de_sum = [%ld]\n", h_obj->h->de_sum);

	printf("File Type:          %s\n", get_str_typeOfFile(h_obj->st.st_mode));

    if (((h_obj->st.st_mode & S_IFMT) != S_IFCHR) &&
        ((h_obj->st.st_mode & S_IFMT) != S_IFBLK)) {
        printf("File Size:          %ld bytes, %ld blocks\n", h_obj->st.st_size,
               h_obj->st.st_blocks);
        printf("Optimum I/O Unit:   %ld bytes\n", (long)h_obj->st.st_blksize);
    }
    else {
        printf("Device Numbers:     Major: %u   Minor: %u\n",
               (unsigned int)major(h_obj->st.st_rdev), (unsigned int)minor(h_obj->st.st_rdev));
    }

    printf("Permission Bits:    %s (%04o)\n", permOfFile(h_obj->st.st_mode),
           (unsigned int)h_obj->st.st_mode & 07777);

    printf("Inode Number:       %u\n", (unsigned int) h_obj->st.st_ino);
    printf("Owner User-Id:      %d\n", (int)h_obj->st.st_uid);
    printf("Owner Group-Id:     %d\n", (int)h_obj->st.st_gid);
    printf("Link Count:         %d\n", (int) h_obj->st.st_nlink);

    printf("File System Device: Major: %u   Minor: %u\n",
           (unsigned int)major(h_obj->st.st_dev), (unsigned int)minor(h_obj->st.st_dev));

    printf("Last Access:        %s", ctime(&h_obj->st.st_atime));
    printf("Last Modification:  %s", ctime(&h_obj->st.st_mtime));
    printf("Last I-Node Change: %s", ctime(&h_obj->st.st_ctime));
	

	FQ_TRACE_EXIT(l);
	return(TRUE);
}

static int 
open_headshm_obj( fq_logger_t *l, char *path, char *qname, headfile_obj_t **obj)
{
	headfile_obj_t *rc=NULL;

	FQ_TRACE_ENTER(l);

	if( !HASVALUE(qname) )  {
        fq_log(l, FQ_LOG_ERROR, "illgal function call.");
		FQ_TRACE_EXIT(l);
		return(FALSE);
    }

	rc = (headfile_obj_t *)calloc(1, sizeof(headfile_obj_t));
	if(rc) {
		char filename[256];
		char flock_path[256];

		sprintf(filename, "/%s_h", qname);
		rc->name = strdup(filename);

		rc->fd = 0;
		if( (rc->fd=shm_open(rc->name, O_RDWR|O_SYNC, S_IRUSR | S_IWUSR)) < 0 ) {
			fq_log(l, FQ_LOG_ERROR, "Queue head shared memory [%s] can not open. reason=[%s]", rc->name, strerror(errno));
			goto return_FALSE;
		}

		if(( fstat(rc->fd, &rc->st)) < 0 ) {
			fq_log(l, FQ_LOG_ERROR, "fstat() error. reason=[%s]", strerror(errno));
			goto return_FALSE;
		}

		rc->h = NULL;
		if( (rc->h = (mmap_header_t *)fq_mmapping(l, rc->fd, sizeof(mmap_header_t), (off_t)0)) == NULL ) {
			fq_log(l, FQ_LOG_ERROR, "header file fq_mmapping() error.");
			goto return_FALSE;
		}

		rc->en_flock = NULL;
		rc->de_flock = NULL;

		if( open_flock_obj(l, path, qname, EN_FLOCK, &rc->en_flock) != TRUE ) {
			fq_log(l, FQ_LOG_ERROR, "open_flock_obj(en) error.");
			goto return_FALSE;
		}

		if( open_flock_obj(l, path, qname, DE_FLOCK, &rc->de_flock) != TRUE ) {
			fq_log(l, FQ_LOG_ERROR, "open_flock_obj(de) error.");
			goto return_FALSE;
		}

		rc->l = l;
		rc->on_head_print = on_head_print;

		*obj = rc;
		FQ_TRACE_EXIT(l);
		return(TRUE);
	}

return_FALSE:
	if(rc->fd) close(rc->fd);
	if(rc->h)  {
		// fq_munmap(l, (void *)rc->h, rc->h->mapping_len);
		rc->h = NULL;
	}

	*obj = NULL;
	FQ_TRACE_EXIT(l);
	return(FALSE);
}

static int 
open_datashm_obj( fq_logger_t *l, char *path, char *qname, datafile_obj_t **obj)
{
	datafile_obj_t *rc=NULL;

	FQ_TRACE_ENTER(l);

	if( !HASVALUE(path) || !HASVALUE(qname) )  {
        fq_log(l, FQ_LOG_ERROR, "illgal function call.");
		FQ_TRACE_EXIT(l);
		return(FALSE);
    }

	rc = (datafile_obj_t *)calloc( 1, sizeof(datafile_obj_t));
	if(rc) {
		char filename[256];

		sprintf(filename, "/%s_r", qname);
		rc->name = strdup(filename);

		rc->fd = 0;
		if( (rc->fd=shm_open(rc->name, O_RDWR|O_SYNC, 0666)) < 0 ) {
			fq_log(l, FQ_LOG_ERROR, "Queue head shared memory[%s] can not open.", rc->name);
			goto return_FALSE;
		}

		if(( fstat(rc->fd, &rc->st)) < 0 ) {
			fq_log(l, FQ_LOG_ERROR, "fstat() error. reason=[%s]", strerror(errno));
			goto return_FALSE;
		}

		rc->l = l;

		*obj = rc;
		FQ_TRACE_EXIT(l);
		return(TRUE);
	}

return_FALSE:
	if(rc->fd) close(rc->fd);

	SAFE_FREE(rc);

	SAFE_FREE(*obj);

	FQ_TRACE_EXIT(l);
	return(FALSE);
}

#define DELAY_MICRO_SECOND 1000000 /* 1초 */

static int 
on_en( fq_logger_t *l, fqueue_obj_t *obj, enQ_mode_t en_mode, const unsigned char *data, int bufsize, size_t len, long *seq, long *run_time)
{
	int ret=-1;
	unsigned char header[FQ_HEADER_SIZE+1];
	register off_t   record_no;
	register off_t   p_offset_from;
	register off_t   mmap_offset;
	register off_t   add_offset;
	long    nth_mapping;
	int		msync_flag = 0; /* sysc 가 느려져서 deQ에서 zeor byte 가 리턴되는 경우 생긴 신한은행  */

	stopwatch_micro_t p;
    long sec=0L, usec=0L, total_micro_sec=0L;

	FQ_TRACE_ENTER(l);

	on_stopwatch_micro(&p);
	if( !obj ) {
		fq_log(l, FQ_LOG_ERROR, "Object is null.");
		FQ_TRACE_EXIT(l);
		return(-1);
	}

	if( (en_mode != EN_NORMAL_MODE) && (en_mode != EN_CIRCULAR_MODE) ) {
		fq_log(l, FQ_LOG_ERROR, "en_mode error. Use EN_NORMAL_MODE or EN_CIRCULAR_MODE");
		FQ_TRACE_EXIT(l);
		return(-1);
	}

	if( !data ) {
		fq_log(l, FQ_LOG_ERROR, "data is null.");
		FQ_TRACE_EXIT(l);
		return(-1);
	}

	if( (bufsize < 1) || (bufsize > 2048000000) ) {
		fq_log(l, FQ_LOG_ERROR, "data is null.");
		FQ_TRACE_EXIT(l);
		return(-1);
	}

	if( (len <= 0) || (len > 2048000000) ) {
		fq_log(l, FQ_LOG_ERROR, "data length is zero(0) or minus or so big. Please check your buffer. len=[%ld]", len);
		FQ_TRACE_EXIT(l);
		return(-1);
	}

	if( seq == NULL ) {
		fq_log(l, FQ_LOG_ERROR, "seq is null");
		FQ_TRACE_EXIT(l);
		return(-1);
	}
	if( run_time == NULL ) {
		fq_log(l, FQ_LOG_ERROR, "runtime is null");
		FQ_TRACE_EXIT(l);
		return(-1);
	}

	if( len >= bufsize ) {
		fq_log(l, FQ_LOG_ERROR, "data len(%d) is greater than buffer size(%d).", len, bufsize);
		FQ_TRACE_EXIT(l);
		return(-1);
	}

	if( len > obj->h_obj->h->msglen-FQ_HEADER_SIZE ) { /* big message is not support */
		fq_log(l, FQ_LOG_ERROR, "big data len(%d) is not support in shared memory queue.", len);
		FQ_TRACE_EXIT(l);
		return(-1);
	}

	if( obj->stop_signal == 1) { /* By Admin */
		fq_log(l, FQ_LOG_ERROR, "obj was already closed by admin.");
		FQ_TRACE_EXIT(l);
		return(MANAGER_STOP_RETURN_VALUE);
	} 

	if( obj->h_obj->h->status != QUEUE_ENABLE ) {
		fq_log(l, FQ_LOG_ERROR, "This queue is disabled by Admin.");
		FQ_TRACE_EXIT(l);
		return(MANAGER_STOP_RETURN_VALUE);
	} 

	if( obj->h_obj->h->QOS != 0 ) { /* QoS: Quality of Service. you can control enQ speed as adjusting this value. */
		usleep(obj->h_obj->h->QOS); /* min is 1000 */
	}

	pthread_mutex_lock(&obj->mutex);
	obj->h_obj->en_flock->on_flock(obj->h_obj->en_flock);

	if( (obj->h_obj->h->en_sum - obj->h_obj->h->de_sum) >= obj->h_obj->h->max_rec_cnt) {
		if( en_mode == EN_CIRCULAR_MODE ) { /* When queue is full, We flush a data automatically and do */
			obj->evt_obj->on_en_eventlog(l, obj->uq_obj, EVENT_FQ_DE_AUTO, obj->path, obj->qname, TRUE, getpid(), obj->h_obj->h->de_sum, 0L );
			obj->h_obj->h->de_sum++;
		}
		else { /* EN_NORMAL_MODE */
			fq_log(l, FQ_LOG_DEBUG, "en_sum[%ld] de_sum[%ld] max_rec_cnt[%ld]", 
				obj->h_obj->h->en_sum, obj->h_obj->h->de_sum, obj->h_obj->h->max_rec_cnt);
			ret = EQ_FULL_RETURN_VALUE;
			goto unlock_return;
		}
	}

	record_no = obj->h_obj->h->en_sum % obj->h_obj->h->max_rec_cnt;
	p_offset_from = record_no * obj->h_obj->h->msglen;
	/*
    p_offset_to = p_offset_from + obj->h_obj->h->msglen;
	*/

	if( obj->h_obj->h->msglen <= obj->h_obj->h->pagesize) {
		ldiv_t  ldiv_value;

		ldiv_value = ldiv(p_offset_from, (obj->h_obj->h->pagesize * obj->h_obj->h->mapping_num) );
		nth_mapping = ldiv_value.quot;
		mmap_offset = nth_mapping * obj->h_obj->h->pagesize * obj->h_obj->h->mapping_num;
	}
	else {
		ldiv_t  ldiv_value;

        ldiv_value = ldiv(p_offset_from, (obj->h_obj->h->msglen * obj->h_obj->h->mapping_num));
        nth_mapping = ldiv_value.quot;
		mmap_offset = nth_mapping * obj->h_obj->h->msglen * obj->h_obj->h->mapping_num;
	}

	if(obj->en_p == NULL) {
		if( (obj->en_p = fq_mmapping(l, obj->d_obj->fd, obj->h_obj->h->mapping_len , mmap_offset)) == NULL ) {
			fq_log(l, FQ_LOG_ERROR, "first fq_mmapping(for body) error.");
			goto unlock_return;
		}
		obj->h_obj->h->en_available_from_offset = mmap_offset;
		obj->h_obj->h->en_available_to_offset = mmap_offset + obj->h_obj->h->mapping_len;
	}
	else { /* already have a mapping pointer */
		if( p_offset_from != obj->h_obj->h->en_available_from_offset ) { /* out of range */
			if( fq_munmap(l, (void *)obj->en_p, obj->h_obj->h->mapping_len) < 0 ) { /* un-mapping for re-mapping */
				fq_log(obj->l, FQ_LOG_ERROR, "fq_munmap(for body) error. reason=[%s].", strerror(errno));
				obj->en_p = NULL;
				goto unlock_return;
			}
			if( (obj->en_p = fq_mmapping(l, obj->d_obj->fd, obj->h_obj->h->mapping_len, mmap_offset)) == NULL ) { /* re-mapping */
				fq_log(l, FQ_LOG_ERROR, "re-fq_mmapping(for body) error.");
				goto unlock_return;
			}
			obj->h_obj->h->en_available_from_offset = mmap_offset;
			obj->h_obj->h->en_available_to_offset = mmap_offset + obj->h_obj->h->mapping_len;
		}
	}

	/* set length of data */
	set_bodysize(header, FQ_HEADER_SIZE, len);
	add_offset = p_offset_from - obj->h_obj->h->en_available_from_offset;

	memset( (void *)((off_t)obj->en_p + add_offset), 0x00, obj->h_obj->h->msglen); /*clean old data */
	memcpy( (void *)((off_t)obj->en_p + add_offset), header, FQ_HEADER_SIZE); /* write length of data */

	/* normal message */
	memcpy( (void *)((off_t)obj->en_p + add_offset + FQ_HEADER_SIZE), (void *)data, len);

	*seq = obj->h_obj->h->en_sum; 
	obj->h_obj->h->en_sum++;

	obj->h_obj->h->last_en_time = time(0);
	ret = len;

	if( msync_flag ) { 
			msync((void *)obj->en_p, obj->h_obj->h->mapping_len, MS_SYNC|MS_INVALIDATE); /*  MS_INVALIDATE */
	}
unlock_return:

	obj->h_obj->en_flock->on_funlock(obj->h_obj->en_flock);

	off_stopwatch_micro(&p);
    get_stopwatch_micro(&p, &sec, &usec);

    total_micro_sec = sec * 1000000;
    total_micro_sec =  total_micro_sec + usec;

	*run_time = total_micro_sec;

	/*
	** 최대 지연시간을 갱신한다.
	*/
	if( total_micro_sec > obj->h_obj->h->latest_en_time ) {
		obj->h_obj->h->latest_en_time = total_micro_sec;
		obj->h_obj->h->latest_en_time_date = time(0);
	} 

	/*
	** 지연시간이 DELAY_MICRO_SECOND 이상이면 에러로그를 (EMERG)로 남긴다.
	*/
    if( total_micro_sec > DELAY_MICRO_SECOND) {
        fq_log(l, FQ_LOG_EMERG, "on_en() delay time : %ld micro second.", total_micro_sec);
    }

	obj->evt_obj->on_en_eventlog(l, obj->uq_obj, EVENT_FQ_EN, obj->path, obj->qname, ((ret>=0)?TRUE:FALSE), getpid(), *seq, total_micro_sec );

	pthread_mutex_unlock(&obj->mutex);
	FQ_TRACE_EXIT(l);
	return(ret);
}

static int 
on_de(fq_logger_t *l, fqueue_obj_t *obj, unsigned char *dst, int dstlen, long *seq, long *run_time)
{
	int ret=-1;
	unsigned char header[FQ_HEADER_SIZE+1];
	register off_t   record_no;
	register off_t   p_offset_from;
	register off_t   mmap_offset;
	register off_t   add_offset;
	register long    nth_mapping;
	int     data_len=0;

	stopwatch_micro_t p;
    long sec=0L, usec=0L, total_micro_sec=0L;

	FQ_TRACE_ENTER(l);

	if( !obj ) {
		fq_log(l, FQ_LOG_ERROR, "Object is null.");
		FQ_TRACE_EXIT(l);
		return(-1);
	}
	if( !dst ) {
		fq_log(l, FQ_LOG_ERROR, "data is null.");
		FQ_TRACE_EXIT(l);
		return(-1);
	}

	if( (dstlen <= 0) || (dstlen > 2048000000) ) {
		fq_log(l, FQ_LOG_ERROR, "data length is zero(0) or minus or so big. Please check your buffer. len=[%ld]", dstlen);
		FQ_TRACE_EXIT(l);
		return(-1);
	}

	if( seq == NULL ) {
		fq_log(l, FQ_LOG_ERROR, "seq is null");
		FQ_TRACE_EXIT(l);
		return(-1);
	}

	if( run_time == NULL ) {
		fq_log(l, FQ_LOG_ERROR, "runtime is null");
		FQ_TRACE_EXIT(l);
		return(-1);
	}

	/* This is a sample to use on_get_extenv() function. */
	/*
	obj->ext_env_obj->on_get_extenv(l, obj->ext_env_obj, "LOCK_USLEEP_TIMES", buf );
	printf("LOCK_USLEEP_TIMES=[%s]\n", buf);
	usleep(atoi(buf));
	*/
	on_stopwatch_micro(&p);
	pthread_mutex_lock(&obj->mutex);

	obj->h_obj->de_flock->on_flock(obj->h_obj->de_flock);

	if( obj->stop_signal == 1) {
		fq_log(l, FQ_LOG_ERROR, "Administrator sent stopping signal.");
		obj->h_obj->de_flock->on_funlock(obj->h_obj->de_flock);
		pthread_mutex_unlock(&obj->mutex);
		FQ_TRACE_EXIT(l);
		return(MANAGER_STOP_RETURN_VALUE);
	} 

	if( obj->h_obj->h->status != QUEUE_ENABLE ) {
		fq_log(l, FQ_LOG_ERROR, "This queue is disabled by Admin.");
		obj->h_obj->de_flock->on_funlock(obj->h_obj->de_flock);
		pthread_mutex_unlock(&obj->mutex);
		FQ_TRACE_EXIT(l);
		return(MANAGER_STOP_RETURN_VALUE);
	} 

	if( obj->h_obj->h->QOS != 0 ) {
		usleep(obj->h_obj->h->QOS); /* min is 1000 */
	}

	if( dstlen < (obj->h_obj->h->msglen-FQ_HEADER_SIZE) ) {
		fq_log(obj->l, FQ_LOG_ERROR, "Your buffer size is short. available size is %d.",
			(obj->h_obj->h->msglen-FQ_HEADER_SIZE));
		goto unlock_return;
	}

	if( obj->h_obj->h->de_sum == obj->h_obj->h->en_sum) {
		ret = DQ_EMPTY_RETURN_VALUE ;
		goto unlock_return;
	}

	record_no = obj->h_obj->h->de_sum % obj->h_obj->h->max_rec_cnt;
	p_offset_from = record_no * obj->h_obj->h->msglen;
	/*
    p_offset_to = p_offset_from + obj->h_obj->h->msglen;
	*/

	if( obj->h_obj->h->msglen <= obj->h_obj->h->pagesize) {
		ldiv_t  ldiv_value;

		ldiv_value = ldiv(p_offset_from, (obj->h_obj->h->pagesize * obj->h_obj->h->mapping_num) );
		nth_mapping = ldiv_value.quot;
		mmap_offset = nth_mapping * obj->h_obj->h->pagesize * obj->h_obj->h->mapping_num;
	}
	else {
		ldiv_t  ldiv_value;

        ldiv_value = ldiv(p_offset_from, (obj->h_obj->h->msglen * obj->h_obj->h->mapping_num));
        nth_mapping = ldiv_value.quot;
		mmap_offset = nth_mapping * obj->h_obj->h->msglen * obj->h_obj->h->mapping_num;
	}

	if( obj->de_p == NULL) {
		if( (obj->de_p = fq_mmapping(l, obj->d_obj->fd, obj->h_obj->h->mapping_len , mmap_offset)) == NULL ) {
			fq_log(l, FQ_LOG_ERROR, "first fq_mmapping(body) error.");
			goto unlock_return;
		}
		obj->h_obj->h->de_available_from_offset = mmap_offset;
		obj->h_obj->h->de_available_to_offset = mmap_offset + obj->h_obj->h->mapping_len;
	}
	else {
		if( p_offset_from != obj->h_obj->h->de_available_from_offset ) {

			if( fq_munmap(l, (void *)obj->de_p, obj->h_obj->h->mapping_len) < 0 ) {
				fq_log(obj->l, FQ_LOG_ERROR, "fq_munmap() error. reason=[%s].", strerror(errno));
				obj->de_p = NULL;
				goto unlock_return;
			}
			if( (obj->de_p = fq_mmapping(l, obj->d_obj->fd, obj->h_obj->h->mapping_len, mmap_offset)) == NULL ) {
				fq_log(l, FQ_LOG_ERROR, "re-fq_mmapping(body) error.");
				goto unlock_return;
			}
			obj->h_obj->h->de_available_from_offset = mmap_offset;
			obj->h_obj->h->de_available_to_offset = mmap_offset + obj->h_obj->h->mapping_len;
		}
	}

	add_offset = p_offset_from - obj->h_obj->h->de_available_from_offset;

	memcpy( header, (void *)((off_t)obj->de_p + add_offset), FQ_HEADER_SIZE);
	data_len = get_bodysize( header, FQ_HEADER_SIZE, 0);

	if( data_len <= 0 ) { /* check read message */
		fq_log(l, FQ_LOG_ERROR, "length of data is zero or minus. [%d]. In this case, We return EMPTY to skip this data automatically", data_len);
		obj->h_obj->h->de_sum++; /* error data is skipped. */
		// ret = -9999;
		ret = DQ_EMPTY_RETURN_VALUE ;
		goto unlock_return;
	}

	if( data_len > dstlen-1) { /* overflow */
		fq_log(l, FQ_LOG_ERROR, "Your buffer size is less than size of data(%d)-overflow. In this case, We return EMPTY to skip this data automatically", data_len);
		obj->h_obj->h->de_sum++; /* error data is skipped. */
		// ret = -9999;
		ret = DQ_EMPTY_RETURN_VALUE ;
		goto unlock_return;
	}

	memcpy( dst, (void *)((off_t)obj->de_p + add_offset + FQ_HEADER_SIZE), data_len );
#ifdef AFTER_DEQ_DELETE
	memset( (void *)((off_t)obj->de_p + add_offset + FQ_HEADER_SIZE), 0x00, data_len); /* 1.0.6  After deQ, delete data */
#endif

	*seq = obj->h_obj->h->de_sum;
	obj->h_obj->h->de_sum++;
	obj->h_obj->h->last_de_time = time(0);

	ret = data_len;

unlock_return:
	obj->h_obj->de_flock->on_funlock(obj->h_obj->de_flock);

	off_stopwatch_micro(&p);
    get_stopwatch_micro(&p, &sec, &usec);
    total_micro_sec = sec * 1000000;
    total_micro_sec =  total_micro_sec + usec;

	*run_time = total_micro_sec;

	if( total_micro_sec > obj->h_obj->h->latest_de_time ) {
		obj->h_obj->h->latest_de_time = total_micro_sec;
		obj->h_obj->h->latest_de_time_date = time(0);
	}

    if( total_micro_sec > DELAY_MICRO_SECOND) {
        fq_log(l, FQ_LOG_EMERG, "de_en() delay time : %ld micro second.", total_micro_sec);
    }

	obj->evt_obj->on_en_eventlog(l, obj->uq_obj, EVENT_FQ_DE, obj->path, obj->qname, ((ret>=0)?TRUE:FALSE), getpid(), *seq, total_micro_sec );

	pthread_mutex_unlock(&obj->mutex);
	FQ_TRACE_EXIT(l);
	return(ret);
}

static int 
close_headshm_obj(fq_logger_t *l,  headfile_obj_t **obj)
{
	FQ_TRACE_ENTER(l);

	SAFE_FREE((*obj)->name);
		
	if( (*obj)->fd > 0 ) {
		close( (*obj)->fd );
		(*obj)->fd = -1;
	}

	if( (*obj)->h ) {
		fq_munmap(l, (void *)(*obj)->h, sizeof(mmap_header_t));
		(*obj)->h = NULL;
	}

	if( (*obj)->en_flock ) {
		close_flock_obj(l, &(*obj)->en_flock);
		(*obj)->en_flock = NULL;
	}
	if( (*obj)->de_flock ) {
		close_flock_obj(l, &(*obj)->de_flock);
		(*obj)->de_flock = NULL;
	}

	SAFE_FREE(*obj);

	FQ_TRACE_EXIT(l);
	return(TRUE);
}

static int 
close_datashm_obj(fq_logger_t *l,  datafile_obj_t **obj, int mmapping_len)
{
	FQ_TRACE_ENTER(l);

	
	SAFE_FREE((*obj)->name);

	if( (*obj)->fd > 0 ) {
		close( (*obj)->fd );
		(*obj)->fd = -1;
	}

#if 0
	if( (*obj)->d && (mmapping_len > 0)) {
		fq_munmap(l, (void *)(*obj)->d, mmapping_len);
		(*obj)->d = NULL;
	}
#endif

	SAFE_FREE(*obj);

	FQ_TRACE_EXIT(l);
	return(TRUE);
}
int 
UnlinkShmQueue(fq_logger_t *l, char *path, char *qname)
{
	int rc;
	char en_flock_path[256];
	char de_flock_path[256];

	FQ_TRACE_ENTER(l);

	rc = unlink_head_shm(l, qname);
	if(rc == FALSE) goto return_FALSE;

	step_print("- Delete header shared memory ", "OK");

	rc = unlink_data_shm(l, qname);
	if(rc == FALSE) goto return_FALSE;
	step_print("- Delete data shared memory ", "OK");
	
	sprintf(en_flock_path, "%s/.%s.en_flock", path, qname);
	rc = unlink_flock( l, en_flock_path);
	if(rc == FALSE) goto return_FALSE;

	sprintf(de_flock_path, "%s/.%s.de_flock", path, qname);
	rc = unlink_flock( l, de_flock_path);
	if(rc == FALSE) goto return_FALSE;

	step_print("- Delete flock file ", "OK");

	rc = unlink_unixQ(l, path, FQ_UNIXQ_KEY_CHAR);
	/* if(rc == FALSE) goto return_FALSE; */
	step_print("- Delete unixQ ", "OK");


	char list_info_file[1024];
	sprintf(list_info_file, "%s/ListSHMQ.info", path);
	int delete_line;
	delete_line = find_a_line_from_file(l, list_info_file, qname);
	if( delete_line > 0 ) {
		int rc;
		rc = delete_a_line_from_file(l, list_info_file, delete_line);
		if( rc == 0 ) {
			step_print("- Delete a shn_queue name in ListSHMQ.info ", "OK");
		}
		else {
			step_print("- Delete a shn_queue name in ListSHMQ.info ", "NOK");
		}
	}

	printf("\n");

	en_eventlog(l, EVENT_FQ_UNLINK, path, qname, TRUE, getpid());
	FQ_TRACE_EXIT(l);
    return(TRUE);

return_FALSE:
	en_eventlog(l, EVENT_FQ_UNLINK, path, qname, FALSE, getpid());
	FQ_TRACE_EXIT(l);
    return(FALSE);
}

int 
CreateShmQueue(fq_logger_t *l, char *path, char *qname)
{
	fq_info_t	*_fq_info=NULL;
	char	info_filename[256];
	headfile_obj_t *h_obj=NULL;

	char	info_qpath[256];
	char	info_qname[256];
	char	info_msglen[16];
	char	info_mmapping_num[8];
	char	info_multi_num[8];
	char	info_desc[256];
	char	info_xa_mode_on_off[4];
	char	info_wait_mode_on_off[4];

	int		i_msglen;
	int		i_mmapping_num;
	int		i_multi_num;

	int		bytes_of_size_t = 8;


	FQ_TRACE_ENTER(l);

	sprintf(info_filename, "%s/%s.Q.info", path, qname);
	if( !can_access_file(l, info_filename)) {
		fq_log(l, FQ_LOG_ERROR, "cannot find info_file[%s]", info_filename);
		goto return_FALSE;
	}

	_fq_info = new_fq_info(info_filename);
	if(!_fq_info) {
		fq_log(l, FQ_LOG_ERROR, "new_fq_info() error.");
		goto return_FALSE;
	}

	if( load_info_param(l, _fq_info, info_filename) < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "load_param() error.");
		goto return_FALSE;
	}
	get_fq_info(_fq_info, "QPATH", info_qpath);
	get_fq_info(_fq_info, "QNAME", info_qname);
	get_fq_info(_fq_info, "MSGLEN", info_msglen);
	get_fq_info(_fq_info, "MMAPPING_NUM", info_mmapping_num);
	get_fq_info(_fq_info, "MULTI_NUM", info_multi_num);
	get_fq_info(_fq_info, "DESC", info_desc);
	get_fq_info(_fq_info, "XA_MODE_ON_OFF", info_xa_mode_on_off);
	get_fq_info(_fq_info, "WAIT_MODE_ON_OFF", info_wait_mode_on_off);

	i_msglen = atoi(info_msglen);
	i_mmapping_num = atoi(info_mmapping_num);
	i_multi_num = atoi(info_multi_num);

	if(bytes_of_size_t == 4) { /* 32 bits */
        unsigned long file_size;

        file_size = i_msglen * i_mmapping_num * i_multi_num;

        if( file_size > 2000000000 ) {
            fq_log(l, FQ_LOG_ERROR, "In the 32-bits machine, Cannot make a file more than 2Gbytes. yours requesting size of file is [%lu]", file_size);
            goto return_FALSE;
        }
    }

	fq_log(l, FQ_LOG_DEBUG, "success to get queue info."); 

	if( create_header_shm(l, info_qname, i_msglen, i_mmapping_num, i_multi_num, info_desc, info_xa_mode_on_off, info_wait_mode_on_off) < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "create_header_shm() error.");
		goto return_FALSE;
	}

	if( open_headshm_obj(l, info_qpath, info_qname, &h_obj) == FALSE ) {
		fq_log(l, FQ_LOG_ERROR, "open_headshm() error.");
		goto return_FALSE;
	}

	if( create_data_shm(l, h_obj ) == FALSE ) {
		fq_log(l, FQ_LOG_ERROR, "create_datashm() error.");
		goto return_FALSE;
	}

	if( h_obj ) close_headshm_obj(l, &h_obj);
	if(_fq_info) free_fq_info(&_fq_info);
	
	en_eventlog(l, EVENT_FQ_CREATE, path, qname, TRUE, getpid());
	FQ_TRACE_EXIT(l);
	return(TRUE);

return_FALSE:
	if( h_obj ) close_headshm_obj(l, &h_obj);
	if(_fq_info) free_fq_info(&_fq_info);

	en_eventlog(l, EVENT_FQ_CREATE, path, qname, FALSE, getpid());
	FQ_TRACE_EXIT(l);
	return(FALSE);
}

int 
OpenShmQueue(fq_logger_t *l, char *path, char *qname, fqueue_obj_t **obj)
{
	fqueue_obj_t *rc=NULL;

	FQ_TRACE_ENTER(l);


	if( !HASVALUE(path) || !HASVALUE(qname) )  {
        fq_log(l, FQ_LOG_ERROR, "illgal function call.");
		FQ_TRACE_EXIT(l);
		return(FALSE);
    }

	if( _checklicense( path ) < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "You don't have a license in this system in directory[%s].", path);
		FQ_TRACE_EXIT(l);
		return(FALSE);
	}

	/* before calloc() */
	rc = (fqueue_obj_t *)calloc(1,  sizeof(fqueue_obj_t));
	if(rc) {
		rc->path = strdup(path);
		rc->qname = strdup(qname);
		rc->eventlog_path = strdup(EVENT_LOG_PATH);
		rc->pid = getpid();
		rc->tid = pthread_self();
		rc->l = l;

		rc->stop_signal = 0;
		rc->en_p = NULL;
		rc->de_p = NULL;

		pthread_mutex_init(&rc->mutex, NULL);

		rc->h_obj = NULL;
		if( open_headshm_obj(l, path, qname, &rc->h_obj) == FALSE ) {
			fq_log(l, FQ_LOG_ERROR, "open_headshm() error.");
			goto return_FALSE;
		}

		rc->d_obj = NULL;
		if( open_datashm_obj(l, path, qname, &rc->d_obj) == FALSE ) {
			fq_log(l, FQ_LOG_ERROR, "open_datashm() error.");
			goto return_FALSE;
		}

		rc->uq_obj = NULL;
		if( open_unixQ_obj(l, FQ_UNIXQ_KEY_PATH,  FQ_UNIXQ_KEY_CHAR, &rc->uq_obj) == FALSE ) {
			fq_log(l, FQ_LOG_ERROR, "open_unixQ_obj() error.");
			goto return_FALSE;
		}

		rc->evt_obj=NULL;

#if 1 /* 일별 이벤트 로그를 파일로 남기고자 할 때에는 이 부분을 이용한다. 그리고 on_eventlog() 함수를 사용한다. */
		if(open_eventlog_obj( l, rc->eventlog_path, &rc->evt_obj) == FALSE) {
			fq_log(l, FQ_LOG_ERROR, "open_eventlog_obj() error.");
			goto return_FALSE;
		}
#endif

		rc->ext_env_obj = NULL;
		if( open_external_env_obj(l, path, &rc->ext_env_obj)== FALSE) {
			fq_log(l, FQ_LOG_ERROR, "open_external_env_obj() error.To avoid  this error, Make a FQ_external_env.conf in your queue directory.");
		}

#if 0
		rc->on_en = on_en;
		rc->on_de = on_de;
		rc->on_de_XA = on_de_XA;
		rc->on_commit_XA = on_commit_XA;
		rc->on_cancel_XA = on_cancel_XA;
		rc->on_disable = NULL;
		rc->on_enable = NULL;
		rc->on_reset = NULL;
		rc->on_force_skip = NULL;
		rc->on_diag = NULL;
		rc->on_get_diff = on_get_diff;
		rc->on_set_QOS = on_set_QOS;
		rc->on_view = on_view;
		rc->on_get_usage = on_get_usage;
		rc->on_check_competition = on_check_competition;
		rc->on_de_bundle_struct = on_de_bundle_struct;
#else
		
		rc->on_en = on_en;
		rc->on_en_bundle_struct = on_en_bundle_struct;
		rc->on_de = on_de;
		rc->on_de_bundle_array = on_de_bundle_array;
		rc->on_de_bundle_struct = on_de_bundle_struct;
		rc->on_de_XA = on_de_XA;
		rc->on_commit_XA = on_commit_XA;
		rc->on_cancel_XA = on_cancel_XA;
		rc->on_disable = on_disable;
		rc->on_enable = on_enable;
		rc->on_reset = on_reset;
		rc->on_force_skip = on_force_skip;
		rc->on_diag = on_diag;
		rc->on_get_diff = on_get_diff;
		rc->on_set_QOS = on_set_QOS;
		rc->on_view = on_view;
		rc->on_set_master = on_set_master;
		rc->on_get_usage = on_get_usage;
		rc->on_do_not_flow = on_do_not_flow;
		rc->on_check_competition = on_check_competition;
		rc->on_get_big = on_get_big;
#endif

		rc->stop_signal = 0;

		*obj = rc;

		en_eventlog(l, EVENT_FQ_OPEN, path, qname, TRUE, getpid());

		FQ_TRACE_EXIT(l);
		return(TRUE);
	}

return_FALSE:
	if( rc->d_obj ) {
		close_datashm_obj(l, &rc->d_obj, rc->h_obj->h->mapping_len);
	}
	if( rc->h_obj ) {
		close_headshm_obj(l, &rc->h_obj);
	}

	SAFE_FREE(rc->path);
	SAFE_FREE(rc->qname);
	SAFE_FREE(rc->eventlog_path);
	rc->pid = -1;
	rc->tid = 0L;

#if 0
	rc->on_en = NULL;
	rc->on_de = NULL;
	rc->on_de_XA = NULL;
	rc->on_commit_XA = NULL;
	rc->on_cancel_XA = NULL;
	rc->on_disable = NULL;
	rc->on_enable = NULL;
	rc->on_reset = NULL;
	rc->on_force_skip = NULL;
	rc->on_diag = NULL;
	rc->on_get_diff = NULL;
	rc->on_set_QOS = NULL;
	rc->on_view = NULL;
	rc->on_get_usage = NULL;
	rc->on_check_competition = NULL;
	rc->on_de_bundle_struct = NULL;
#else
	rc->on_en = NULL;
	rc->on_en_bundle_struct = NULL;
	rc->on_de = NULL;
	rc->on_de_bundle_array = NULL;
	rc->on_de_bundle_struct = NULL;
	rc->on_de_XA = NULL;
	rc->on_commit_XA = NULL;
	rc->on_cancel_XA = NULL;
	rc->on_disable = NULL;
	rc->on_enable = NULL;
	rc->on_reset = NULL;
	rc->on_force_skip = NULL;
	rc->on_diag = NULL;
	rc->on_get_diff = NULL;
	rc->on_set_QOS = NULL;
	rc->on_view = NULL;
	rc->on_set_master = NULL;
	rc->on_get_usage = NULL;
	rc->on_do_not_flow = NULL;
	rc->on_check_competition = NULL;
	rc->on_get_big = NULL;
#endif

	SAFE_FREE(rc);

	SAFE_FREE(*obj);

	en_eventlog(l, EVENT_FQ_OPEN, path, qname, FALSE, getpid());

	FQ_TRACE_EXIT(l);
	return(FALSE);
}

int 
CloseShmQueue(fq_logger_t *l, fqueue_obj_t **obj)
{
	int rc;
	char	path[256]; /* for eventlog */
	char	qname[256]; /* for eventlog */

	FQ_TRACE_ENTER(l);

	fq_log(l, FQ_LOG_DEBUG, "path=[%s] qname=[%s] closing start", (*obj)->path, (*obj)->qname);

	sprintf(path, "%s", (*obj)->path);
	sprintf(qname, "%s", (*obj)->qname);

	/* For thread safe */
	/* 더 이상 신규 청을 못하도록 한다.*/
	(*obj)->h_obj->en_flock->on_flock((*obj)->h_obj->en_flock);
	(*obj)->h_obj->de_flock->on_flock((*obj)->h_obj->de_flock);
	(*obj)->stop_signal = 1;
	(*obj)->h_obj->en_flock->on_funlock((*obj)->h_obj->en_flock);
	(*obj)->h_obj->de_flock->on_funlock((*obj)->h_obj->de_flock);

	SAFE_FREE((*obj)->path);
	SAFE_FREE((*obj)->qname);
	SAFE_FREE((*obj)->eventlog_path);

	fq_log(l, FQ_LOG_DEBUG, "close_datashm() start");

	if( (*obj)->d_obj ) {
		rc = close_datashm_obj(l, &(*obj)->d_obj, (*obj)->h_obj->h->mapping_len );
		if( rc != TRUE ) {
			fq_log(l, FQ_LOG_ERROR, "close_datashm_obj() error.");
			goto return_FALSE;
		}
	}
	(*obj)->d_obj = NULL;
	fq_log(l, FQ_LOG_DEBUG, "close_datashm() end");
	

	if( (*obj)->h_obj ) {
		rc = close_headshm_obj(l, &(*obj)->h_obj );
		if( rc != TRUE ) {
			fq_log(l, FQ_LOG_ERROR, "close_headshm_obj() error.");
			goto return_FALSE;
		}
	}
	(*obj)->h_obj = NULL;

	if( (*obj)->uq_obj )  {
		rc = close_unixQ_obj(l, &(*obj)->uq_obj);
		if( rc == FALSE ) {
			fq_log(l, FQ_LOG_ERROR, "close_unixQ_obj() error.");
			goto return_FALSE;
		}
	}
	(*obj)->uq_obj = NULL;

	if( (*obj)->evt_obj ) {
		rc = close_evenlog_obj(l, &(*obj)->evt_obj);
		if( rc == FALSE ) {
			fq_log(l, FQ_LOG_ERROR, "close_evenlog_obj() error.");
			goto return_FALSE;
		}
	}


	SAFE_FREE(*obj);

	en_eventlog(l, EVENT_FQ_CLOSE, path, qname, TRUE, getpid());
	FQ_TRACE_EXIT(l);
	return(TRUE);
return_FALSE:

	en_eventlog(l, EVENT_FQ_CLOSE, path, qname, FALSE, getpid());
	FQ_TRACE_EXIT(l);
	return(FALSE);
}

int ResetShmQueue(fq_logger_t *l, char *path, char *qname)
{
	int rc;
	fqueue_obj_t *obj=NULL;

	rc =  OpenShmQueue(l, path, qname, &obj);
	if(rc != TRUE) {
		step_print("- OpenFileQueue() ", "NOK");
		goto return_FALSE;
	}
	step_print("- OpenFileQueue() ", "OK");

	rc = obj->on_reset(l, obj);
	if(rc != TRUE) {
		step_print("- Reset processing. ", "NOK");
        goto return_FALSE;
    }
	step_print("- Reset processing. ", "OK");

	CloseShmQueue(l, &obj);
	en_eventlog(l, EVENT_FQ_RESET, path, qname, TRUE, getpid());

	step_print("- Closing File Queue. ", "OK");
	FQ_TRACE_EXIT(l);
	return(TRUE);

return_FALSE:
	if(obj) {
		CloseShmQueue(l, &obj);
	}
	en_eventlog(l, EVENT_FQ_RESET, path, qname, FALSE, getpid());
	FQ_TRACE_EXIT(l);
	return(FALSE);
}

int FlushShmQueue(fq_logger_t *l, char *path, char *qname)
{
	int rc;
	fqueue_obj_t *obj=NULL;
	int buffer_size;

	rc =  OpenShmQueue(l, path, qname, &obj);
	if(rc != TRUE) {
		step_print("- Open File Queue. ", "NOK");
		goto return_FALSE;
	}
	step_print("- Open File Queue. ", "OK");

	buffer_size = obj->h_obj->h->msglen+1;

	while(1) {
		long l_seq=0L;
		unsigned char *buf=NULL;
		long run_time=0L;

		buf = malloc(buffer_size);
		memset(buf, 0x00, sizeof(buffer_size));

		rc = obj->on_de(l, obj, buf, buffer_size, &l_seq, &run_time);

		if( rc == DQ_EMPTY_RETURN_VALUE ) {
			break;
		}
		else if( rc < 0 ) {
			if( rc == MANAGER_STOP_RETURN_VALUE ) {
				printf("Manager asked to stop a processing.\n");
				goto return_FALSE;
			}
			printf("error..\n");
			goto return_FALSE;
		}
		else {
			SAFE_FREE(buf);
		}
	}
	step_print("- Flush Processing ", "OK");
		
	CloseShmQueue(l, &obj);
	FQ_TRACE_EXIT(l);

	step_print("- Closing ", "OK");
	return(TRUE);

return_FALSE:
	if(obj) {
		CloseShmQueue(l, &obj);
	}
	step_print("- Flush Processing ", "NOK");

	step_print("- Closing ", "OK");

	FQ_TRACE_EXIT(l);
	return(FALSE);
}
static float 
on_get_usage( fq_logger_t *l, fqueue_obj_t *obj)
{
	long diff;
	float usage;

	FQ_TRACE_ENTER(l);

	// obj->h_obj->de_flock->on_flock(obj->h_obj->de_flock);

    diff = obj->h_obj->h->en_sum - obj->h_obj->h->de_sum;
	if( diff == 0 ) usage = 0.0;
	else {
		usage = ((float)diff*100.0)/(float)obj->h_obj->h->max_rec_cnt;
	}

	// obj->h_obj->de_flock->on_funlock(obj->h_obj->de_flock);

	FQ_TRACE_EXIT(l);
	return(usage);
}
static time_t 
on_check_competition( fq_logger_t *l, fqueue_obj_t *obj, action_flag_t en_de_flag)
{
	stopwatch_micro_t p;
	long sec=0L, usec=0L;
	time_t total_micro_sec=0L;

	FQ_TRACE_ENTER(l);

	on_stopwatch_micro(&p);

	if( en_de_flag == FQ_EN_ACTION ) {
		obj->h_obj->en_flock->on_flock(obj->h_obj->en_flock);
	} else if( en_de_flag == FQ_DE_ACTION ) {
		obj->h_obj->de_flock->on_flock(obj->h_obj->de_flock);
	} else {
		fq_log(l, FQ_LOG_ERROR, "en_de_flag error.");
	}	

	off_stopwatch_micro(&p);

	get_stopwatch_micro(&p, &sec, &usec);
	total_micro_sec = sec * 1000000;
	total_micro_sec =  total_micro_sec + usec;

	if( en_de_flag == FQ_EN_ACTION ) {
		obj->h_obj->en_flock->on_funlock(obj->h_obj->en_flock);
	} else if( en_de_flag == FQ_DE_ACTION ) {
		obj->h_obj->de_flock->on_funlock(obj->h_obj->de_flock);
	}
		
	FQ_TRACE_EXIT(l);
	return(total_micro_sec);
}

static int 
on_de_bundle_struct(fq_logger_t *l, fqueue_obj_t *obj, int fqdata_cnt, int MaxDataLen, fqdata_t array[], long *run_time)
{
	int ret=-1;
	unsigned char header[FQ_HEADER_SIZE+1];
	register off_t   record_no;
	register off_t   p_offset_from; 
	/*
	register off_t	p_offset_to;
	*/
	register off_t   mmap_offset;
	register off_t   add_offset;
	register long    nth_mapping;
	int     data_len=0;
	int 	diff;
	int		array_index=0;

	stopwatch_micro_t p;
    long sec=0L, usec=0L, total_micro_sec=0L;

	FQ_TRACE_ENTER(l);

#ifdef _USE_HEARTBEAT
#ifdef _USE_PROCESS_CONTROL
	if( signal_received == 1) {
		fq_log(l, FQ_LOG_EMERG, "I received restart signal. from manager.");
		signal_received = 0;
		restart_process(obj->argv);
		sleep(1);
	}
#endif

	if( obj->hostname && obj->hb_obj) {
		int rc;
		rc = obj->hb_obj->on_timestamp(l, obj->hb_obj, MASTER_TIME_STAMP);
		if( rc == FALSE ) {
			FQ_TRACE_EXIT(l);
#ifdef _USE_PROCESS_CONTROL
			signal_received = 0;
			restart_process(obj->argv);
#endif
		}
	}
#endif

	if( !obj ) {
		fq_log(l, FQ_LOG_ERROR, "Object is null.");
		FQ_TRACE_EXIT(l);
		return(-1);
	}
	if( !array ) {
		fq_log(l, FQ_LOG_ERROR, "struct array buffer is null.");
		FQ_TRACE_EXIT(l);
		return(-1);
	}

	if( (MaxDataLen <= 0) || (MaxDataLen > 2048000000) ) {
		fq_log(l, FQ_LOG_ERROR, "Max data length is zero(0) or minus or so big. Please check your buffer. len=[%ld]", MaxDataLen);
		FQ_TRACE_EXIT(l);
		return(-1);
	}

	if( fqdata_cnt > obj->h_obj->h->max_rec_cnt ) {
		fq_log(l, FQ_LOG_ERROR, "Max data length is [%d]. Please check your size of array.", obj->h_obj->h->max_rec_cnt);
		FQ_TRACE_EXIT(l);
		return(-1);
	}

	if( run_time == NULL ) {
		fq_log(l, FQ_LOG_ERROR, "runtime is null");
		FQ_TRACE_EXIT(l);
		return(-1);
	}

#if 0
	/* This is a sample to use on_get_extenv() function. */
	if( obj->ext_env_obj ) {
		obj->ext_env_obj->on_get_extenv(l, obj->ext_env_obj, "LOCK_USLEEP_TIMES", buf );
		printf("LOCK_USLEEP_TIMES=[%s]\n", buf);
		usleep(atoi(buf));
	}
#endif

	if( obj->stopwatch_on_off == STOPWATCH_ON ) {
		on_stopwatch_micro(&p);
	}

	obj->h_obj->de_flock->on_flock(obj->h_obj->de_flock);

	for(array_index=0; array_index<fqdata_cnt; array_index++) {

		if( obj->stop_signal == 1) {
			fq_log(l, FQ_LOG_ERROR, "Administrator sent stopping signal.");
			obj->h_obj->de_flock->on_funlock(obj->h_obj->de_flock);
			FQ_TRACE_EXIT(l);
			return(MANAGER_STOP_RETURN_VALUE);
		} 

		if( obj->h_obj->h->status != QUEUE_ENABLE ) {
			fq_log(l, FQ_LOG_ERROR, "This queue is disabled by Admin.");
			obj->h_obj->de_flock->on_funlock(obj->h_obj->de_flock);
			FQ_TRACE_EXIT(l);
			return(MANAGER_STOP_RETURN_VALUE);
		} 

		if( (obj->h_obj->h->Master_assign_flag == 1) && (obj->h_obj->h->MasterHostName[0] != 0x00) ) {
			fq_log(l, FQ_LOG_DEBUG, "Master checking start.");
			fq_log(l, FQ_LOG_DEBUG, "h->MasterHostName=[%s], obj->hostname=[%s]", obj->h_obj->h->MasterHostName, obj->hostname);

			if( (obj->h_obj->h->MasterHostName[0] != 0x00) && (obj->hostname[0]!=0x00) ) {
				if(strcmp( obj->h_obj->h->MasterHostName, obj->hostname) != 0 ) {
					fq_log(l, FQ_LOG_ERROR, "You can't access to this queue. Accessable server is [%s].", obj->h_obj->h->MasterHostName);
					FQ_TRACE_EXIT(l);
					return(IS_NOT_MASTER);
				}
			}
		}

		diff = obj->h_obj->h->en_sum - obj->h_obj->h->de_sum;
		if( diff == 0 ) {
			break;
		}

		record_no = obj->h_obj->h->de_sum % obj->h_obj->h->max_rec_cnt;
		p_offset_from = record_no * obj->h_obj->h->msglen;
		/*
		p_offset_to = p_offset_from + obj->h_obj->h->msglen;
		*/

		if( obj->h_obj->h->msglen <= obj->h_obj->h->pagesize) {
			ldiv_t  ldiv_value;

			ldiv_value = ldiv(p_offset_from, (obj->h_obj->h->pagesize * obj->h_obj->h->mapping_num) );
			nth_mapping = ldiv_value.quot;
			mmap_offset = nth_mapping * obj->h_obj->h->pagesize * obj->h_obj->h->mapping_num;
		}
		else {
			ldiv_t  ldiv_value;

			ldiv_value = ldiv(p_offset_from, (obj->h_obj->h->msglen * obj->h_obj->h->mapping_num));
			nth_mapping = ldiv_value.quot;
			mmap_offset = nth_mapping * obj->h_obj->h->msglen * obj->h_obj->h->mapping_num;
		}

		if( obj->de_p == NULL) {
			if( (obj->de_p = fq_mmapping(l, obj->d_obj->fd, obj->h_obj->h->mapping_len , mmap_offset)) == NULL ) {
				fq_log(l, FQ_LOG_ERROR, "first fq_mmapping(body) error.");
				goto unlock_return;
			}
			obj->h_obj->h->de_available_from_offset = mmap_offset;
			obj->h_obj->h->de_available_to_offset = mmap_offset + obj->h_obj->h->mapping_len;
		}
		else {
			if( p_offset_from != obj->h_obj->h->de_available_from_offset ) {

#ifdef MSYNC_WHENEVER_UNMAP
				/* 2016/01/07 : for using shared disk(SAN) in ShinhanBank */
				msync((void *)obj->en_p, obj->h_obj->h->mapping_len, MS_SYNC|MS_INVALIDATE); /*  MS_INVALIDATE */
#endif

				if( fq_munmap(l, (void *)obj->de_p, obj->h_obj->h->mapping_len) < 0 ) {
					fq_log(obj->l, FQ_LOG_ERROR, "fq_munmap() error. reason=[%s].", strerror(errno));
					obj->de_p = NULL;
					goto unlock_return;
				}
				if( (obj->de_p = fq_mmapping(l, obj->d_obj->fd, obj->h_obj->h->mapping_len, mmap_offset)) == NULL ) {
					fq_log(l, FQ_LOG_ERROR, "re-fq_mmapping(body) error.");
					goto unlock_return;
				}
				obj->h_obj->h->de_available_from_offset = mmap_offset;
				obj->h_obj->h->de_available_to_offset = mmap_offset + obj->h_obj->h->mapping_len;
			}
		}

		add_offset = p_offset_from - obj->h_obj->h->de_available_from_offset;

		memcpy( header, (void *)((off_t)obj->de_p + add_offset), FQ_HEADER_SIZE);
		data_len = get_bodysize( header, FQ_HEADER_SIZE, 0);

		if( data_len <= 0 ) { /* check read message */
			fq_log(l, FQ_LOG_ERROR, "length of data is zero or minus. [%d]. In this case, We return EMPTY to skip this data automatically", data_len);
			obj->h_obj->h->de_sum++; /* error data is skipped. */
			// ret = -9999;
			ret = DQ_EMPTY_RETURN_VALUE ;
			goto unlock_return;
		}

		if( data_len > MaxDataLen-1) { /* overflow */
			fq_log(l, FQ_LOG_ERROR, "Your buffer size is less than size of data(%d)-overflow. In this case, We return EMPTY to skip this data automatically", data_len);
			obj->h_obj->h->de_sum++; /* error data is skipped. */
			// ret = -9999;
			ret = DQ_EMPTY_RETURN_VALUE ;
			goto unlock_return;
		}

		if( data_len > (obj->h_obj->h->msglen-FQ_HEADER_SIZE) ) { /* big message processing */
			fq_log(obj->l, FQ_LOG_ERROR, "data lenngth(%d) error. We will skip it.", data_len);
		} 
		else { /* normal messages */
			array[array_index].data = calloc(MaxDataLen+1, sizeof(unsigned char));
			memcpy( array[array_index].data, (void *)((off_t)obj->de_p + add_offset + FQ_HEADER_SIZE), data_len );
			array[array_index].len = data_len;
			array[array_index].seq = obj->h_obj->h->de_sum;
#ifdef AFTER_DEQ_DELETE
			memset( (void *)((off_t)obj->de_p + add_offset + FQ_HEADER_SIZE), 0x00, data_len); /* 1.0.6  After deQ, delete data */
#endif
		}
		obj->h_obj->h->de_sum++;
		obj->h_obj->h->last_de_time = time(0);
	} /* for end */

	ret = array_index;

unlock_return:
	obj->h_obj->de_flock->on_funlock(obj->h_obj->de_flock);

	if( obj->stopwatch_on_off == STOPWATCH_ON ) {
		off_stopwatch_micro(&p);
		get_stopwatch_micro(&p, &sec, &usec);
		total_micro_sec = sec * 1000000;
		total_micro_sec =  total_micro_sec + usec;

		*run_time = total_micro_sec;
	}
	else {
		total_micro_sec = 0;
		*run_time = 0;
	}

	if( total_micro_sec > obj->h_obj->h->latest_de_time ) {
		obj->h_obj->h->latest_de_time = total_micro_sec;
		obj->h_obj->h->latest_de_time_date = time(0);
	}

	
	if( obj->stopwatch_on_off == STOPWATCH_ON ) {
		if( total_micro_sec > DELAY_MICRO_SECOND) {
			fq_log(l, FQ_LOG_EMERG, "on_de() delay time : %ld micro second.", total_micro_sec);
		}
	}

	if( obj->monID && obj->mon_obj) {
		obj->mon_obj->on_touch(l, obj->monID, FQ_DE_ACTION, obj->mon_obj);
	}

	FQ_TRACE_EXIT(l);
	return(ret);
}
int 
on_reset(fq_logger_t *l, fqueue_obj_t *obj)
{
	char	cmd[256];
	int 	rc;

	FQ_TRACE_ENTER(l);

	/* For thread safe */
	/* ´õ ÀÌ»ó ½Å±Ô Ã»À» ¸øÇÏµµ·Ï ÇÑ´Ù.*/
	obj->h_obj->en_flock->on_flock(obj->h_obj->en_flock);
	obj->h_obj->de_flock->on_flock(obj->h_obj->de_flock);

	obj->h_obj->h->status = QUEUE_ALL_DISABLE;

	obj->h_obj->h->made_big_dir = 0;
	obj->h_obj->h->current_big_files = 0;
	obj->h_obj->h->big_sum = 0L;
	obj->h_obj->h->en_sum = 0L;
	obj->h_obj->h->de_sum = 0L;
	obj->h_obj->h->last_en_time = time(0);
	obj->h_obj->h->last_de_time = time(0);
	obj->h_obj->h->latest_en_time = 0L;
	obj->h_obj->h->latest_en_time_date = time(0);
	obj->h_obj->h->latest_de_time = 0L;
	obj->h_obj->h->latest_de_time_date = time(0);
	obj->h_obj->h->last_en_row = 0;
	obj->h_obj->h->made_big_dir = 0;
	obj->h_obj->h->en_available_from_offset = 0L;
	obj->h_obj->h->en_available_to_offset = 0L;
	obj->h_obj->h->de_available_from_offset = 0L;
	obj->h_obj->h->de_available_to_offset = 0L;
	
	obj->h_obj->h->status = QUEUE_ENABLE;

	/* delete big files */
	fq_log(l, FQ_LOG_DEBUG, "Deleting BIG data directiories started.");
	sprintf(cmd, "rm -rf %s/BIG_%s", obj->path, obj->qname);
	rc = system(cmd);
	CHECK(rc != -1);

	fq_log(l, FQ_LOG_DEBUG, "Deleting BIG data directiories finished.");

	obj->h_obj->en_flock->on_funlock(obj->h_obj->en_flock);
	obj->h_obj->de_flock->on_funlock(obj->h_obj->de_flock);

	FQ_TRACE_EXIT(l);
	return(TRUE);
}
static int
on_en_bundle_struct( fq_logger_t *l, fqueue_obj_t *obj, int src_cnt, fqdata_t src[], long *run_time)
{
	return(0);
}
static int 
on_de_bundle_array(fq_logger_t *l, fqueue_obj_t *obj, int array_cnt, int MaxDataLen, unsigned char *array[], long *run_time)
{
	return(0);
}
static int
on_disable(fq_logger_t *l, fqueue_obj_t *obj)
{
	return(0);
}
static int
on_enable(fq_logger_t *l, fqueue_obj_t *obj)
{
	return(0);
}
static int
on_force_skip(fq_logger_t *l, fqueue_obj_t *obj)
{
	return(0);
}
static int
on_diag(fq_logger_t *l, fqueue_obj_t *obj)
{
	return(0);
}
static int
on_set_master( fq_logger_t *l, fqueue_obj_t *obj, int on_off_flag, char *hostname)
{
	return(0);
}
static bool_t
on_do_not_flow( fq_logger_t *l, fqueue_obj_t *obj, time_t waiting_time)
{
	return(0);
}
static int
on_get_big( fq_logger_t *l, fqueue_obj_t *obj)
{
	return(0);
}
