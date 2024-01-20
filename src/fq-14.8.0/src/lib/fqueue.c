/* or add below contents to .vimrc 
** - set tabstop=4
** - set shiftwidth=4
** 
** fqueue.c
**
*/
#define FQUEUE_C_VERSION "1.0.11"

#if 0
#define AFTER_DEQ_DELETE  /* 1.0.6 */
#endif

#if 0
#define SYNC_FILE_RANGE  /* 1.0.6 */
#endif

// If you use it, Max Speed is about 250 TPS in en-queue.
#if 0
#define FDATASYNC
#endif

/*
** If you select this option, Performance slow down dozens of times.
** about 150 TPS on Raspi.
*/
#if 0
#define MSYNC_WHENEVER_UNMAP 0
#endif

/*
** If you select this option, Performance slow down dozens of times.
** about 200 TPS on Raspi.
*/
#if 0
#define RAW_LSEEK_WRITE 1
#endif

/*
** version history
** 1.0.1: 2013/05/19 : upgrade: add on_view() function
** 1.0.2: 2013/05/19 : bug patch: add on_reset() 함수에 made_big_dir = 0 ; 추가
** 1.0.3: 2013/05/22 : Change locking on_view() to no locking on_view().
** 1.0.4: 2013/05/23 : bug patch In the on_view(),  in case of data is null, for skipping, increase de_sum.
** 1.0.5: 2013/08/09 : When there is no FQ_external_env.conf, change error message. 
** 1.0.6: 2013/08/28 : SAMSUNG want to be deleted after deQ.
** 1.0.7: 2013/09/18 : printing  bug fix. header -> data.
** 1.0.8: 2013/09/30 : Rollback deleting data after deQ in 1.0.6 ( found a bug )
** 1.0.9: 2013/11/08 : Add on_force_skip() method.
** 1.0.10: 2013/11/20: In on_en(), If big_file were proceed, run msync().
** 1.0.11: 2014/03/27: add extend file queue.
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

#include <limits.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "fq_common.h"
#include "fqueue.h"
#include "fq_logger.h"
#include "fq_info.h"
#include "fq_info_param.h"
#include "fq_flock.h"
#include "fq_unixQ.h"

#include "fq_eventlog.h"
#include "fq_external_env.h"

#include "fq_heartbeat.h"
#include "fq_typedef.h"
#include "fq_monitor.h"
#include "fq_base64_std.h"

#include "fq_manage.h"
#include "fq_linkedlist.h"
#include "shm_queue.h"

#include "fq_manage.h"
#include "fq_linkedlist.h"

#include "fq_safe_write.h"
#include "stdbool.h"
#include "fq_common.h"

/*
** It make receive SIGUSR1 from controller and restart.
*/
#ifdef _USE_HEARTBEAT
#ifdef _USE_PROCESS_CONTROL
extern volatile sig_atomic_t signal_received;
#endif
#endif

#define DELAY_MICRO_SECOND 100000 /* 1/10 second */
#define USE_SHM_LOCK  1
//#define USE_FILE_LOCK 1

static int open_headfile_obj( fq_logger_t *l, char *path, char *qname, headfile_obj_t **obj);
static int close_headfile_obj(fq_logger_t *l,  headfile_obj_t **obj);
static int close_datafile_obj(fq_logger_t *l,  datafile_obj_t **obj, int mmapping_len);
static int on_view(fq_logger_t *l, fqueue_obj_t *obj, unsigned char *dst, int dstlen, long *seq, long *run_time, bool *big_flag);
static int on_set_master(fq_logger_t *l, fqueue_obj_t *obj, int on_off_flag, char *hostname );

static int backup_for_XA( fq_logger_t *l, char *path, char *qname, unsigned char *data, int datalen);
static int delete_for_XA( fq_logger_t *l, char *path, char *qname);
static int load_big_Q_file_2_buf(fq_logger_t *l, char *big_qfilename, char**ptr_dst, int *flen, char** ptr_errmsg);

static void save_Q_fatal_error(char *qpath, char *qname, char *msg, char *file, int src_line);

bool_t check_fq_env()
{
	char *fq_home_path = NULL;

	fq_home_path = getenv("FQ_DATA_HOME");
	if( fq_home_path == NULL ) {
		fprintf(stderr, "Set FQ_DATA_HOME value to your .bashrc or .profile or .bash_profile.\n");
		return(False);
	}
	fq_home_path = getenv("FQ_LIB_HOME");
	if( fq_home_path == NULL ) {
		fprintf(stderr, "Set FQ_LIB_HOME value to your .bashrc or .profile or .bash_profile.\n");
		return(False);
	}

	return(True);
}

char *get_fq_home( char *buf )
{
	char *fq_home_path = NULL;

	fq_home_path = getenv("FQ_HOME");
	if( fq_home_path == NULL ) {
		fprintf(stderr, "Set FQ_HOME value to your .bashrc or .profile or .bash_profile.\n");
		return(NULL);
	}
	else {
		sprintf(buf, "%s", fq_home_path);
		return(buf);
	}
}

/*
 * pagesize = sysconf(_SC_PAGE_SIZE);
 * pagesize = getpagesize();
 */
off_t get_aligned_offset_4_mmap( off_t offset, off_t pagesize)
{
	off_t	pa_offset;

	pa_offset = offset & ~(pagesize - 1);
	return(pa_offset);
}

static int 
set_bodysize(unsigned char* header, int size, int value, int header_type)
{
	register int i;
	unsigned char* ptr = header;
	char fmt[40];

	if( header_type != 0 ) { /* ASCII */
		sprintf(fmt, "%c0%dd", '%', size);
		sprintf((char*)header, fmt, value);
		return(0);
	}

	else { /* Binary */
		for (i=0; i < size; i++) {
			*ptr = (value >> 8*(size-i-1)) & 0xff;
			ptr++;
		}
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

int 
create_dummy_file(fq_logger_t *l, const char *path, const char *fname, size_t size)
{
	int		fd;
	int		rc = -1;
	char	filename[256];
	mode_t	mode=S_IRUSR| S_IWUSR| S_IRGRP| S_IWGRP| S_IROTH| S_IWOTH;
	// mode_t	mode=0666;

/*
 * add CFLAGS = -D_GNU_SOURCE
 */
#ifdef O_LARGEFILE
	int		o_flag=O_CREAT|O_EXCL|O_RDWR|O_LARGEFILE|O_SYNC;
#else
	int     o_flag=O_CREAT|O_EXCL|O_RDWR|O_SYNC;
#endif

	FQ_TRACE_ENTER(l);

#ifdef O_LARGEFILE
	fq_log(l, FQ_LOG_EMERG, "This system supports O_LARGEFILE.");
#else
	fq_log(l, FQ_LOG_EMERG, "This system doesn't support O_LARGEFILE.");
#endif

	sprintf(filename, "%s/%s", path, fname);
	if( (fd = open(filename, o_flag, mode ))<0) {
		fq_log(l, FQ_LOG_ERROR, "open(%s) error. [%s]. If permission error, Change mode of dir to 777.", filename, strerror(errno));
		goto L0;
	}
	fq_log(l, FQ_LOG_INFO, "dummy file create open OK.[%s]", filename); 

	if( ftruncate(fd, size) < 0 ) {
		fq_log(l,FQ_LOG_ERROR,"ftruncate() error. [%s] size=[%zu].", strerror(errno), size);
		goto L0;
	}
	fq_log(l, FQ_LOG_INFO, "dummy file ftruncate OK.[%s]", filename); 

	if( fchmod(fd, (mode_t)0666) < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "fchmod() error.[%s]", strerror(errno));
		goto L0;
	}
	fq_log(l, FQ_LOG_INFO, "dummy file chmod OK.[%s]", filename); 

	rc = fd;
L0:
	FQ_TRACE_EXIT(l);
	return(rc);
}

int 
extend_dummy_file(fq_logger_t *l, const char *path, const char *fname, size_t size)
{
	int		fd;
	int		rc = -1;
	char	filename[256];
	mode_t	mode=S_IRUSR;

#ifdef O_LARGEFILE
	int		o_flag=O_RDWR|O_LARGEFILE|O_SYNC;
#else
	int     o_flag=O_EXCL|O_RDWR|O_SYNC;
#endif

	FQ_TRACE_ENTER(l);

#ifdef O_LARGEFILE
	fq_log(l, FQ_LOG_EMERG, "This system supports O_LARGEFILE.");
#else
	fq_log(l, FQ_LOG_EMERG, "This system doesn't support O_LARGEFILE.");
#endif

	sprintf(filename, "%s/%s", path, fname);
	if( (fd = open(filename, o_flag, mode ))<0) {
		fq_log(l, FQ_LOG_ERROR, "open(%s) error. [%s]. If permission error, Change mode of dir to 777.", filename, strerror(errno));
		goto L0;
	}
	fq_log(l, FQ_LOG_INFO, "dummy file open OK.[%s]", filename); 

	if( ftruncate(fd, size) < 0 ) {
		fq_log(l,FQ_LOG_ERROR,"ftruncate() error. [%s] size=[%zu].", strerror(errno), size);
		goto L0;
	}
	fq_log(l, FQ_LOG_INFO, "dummy file re-ftruncate OK.[%s]", filename); 

	rc = fd;
L0:
	FQ_TRACE_EXIT(l);
	return(rc);
}

void *
fq_mmapping(fq_logger_t *l, int fd, size_t space_size, off_t from_offset)
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
		fq_log(l, FQ_LOG_ERROR, "mmap() error.[%s] space_size=[%zu] from_offset=[%jd]", 
			strerror(errno), space_size, from_offset);
		FQ_TRACE_EXIT(l);
		return(NULL);
	}
	
	FQ_TRACE_EXIT(l);
	return(p);
}

int 
fq_munmap(fq_logger_t *l, void *addr, size_t length)
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

static int 
create_headerfile(fq_logger_t *l, 
			char *path, 
			char *qname, 
			int msglen, 
			int mapping_num, 
			int multi_num, 
			char *desc,
			char *xa_mode_on_off,
			char *wait_mode_on_off,
			int master_use_flag,
			char *master_hostname)
{
	mmap_header_t  *h=NULL;
	int 	rtn=-1;
	char	filename[1024];
	char	name[512];
	int		fd=0;
	
	FQ_TRACE_ENTER(l);

	sprintf(name, "%s.h", qname);
	sprintf(filename, "%s/%s", path, name);

	if( is_file(filename) ) { /* checks whether the file exists. */
		fq_log(l, FQ_LOG_ERROR, "Already the file[%s] exists.", filename);
		step_print("- Check if header file already does exist ", "NOK");
		goto L0;
	}
	step_print("- Check if header file already does exist ", "OK");

	if( (fd=create_dummy_file(l, path, name, sizeof(mmap_header_t)))  < 0) {
		fq_log(l, FQ_LOG_ERROR, "create_dummy_file() error. path=[%s] name=[%s]", path, name);
		step_print("- create header dummy file.", "NOK");
		goto L0;
	}
	step_print("- create header dummy file.", "OK");

	if( (h = (mmap_header_t *)fq_mmapping(l, fd, sizeof(mmap_header_t), (off_t)0)) == NULL) {
        fq_log(l, FQ_LOG_ERROR, "fq_mmapping() error.");
        step_print("- header file mmapping.", "NOK");
        goto L0;
    }
	step_print("- header mmapping.", "OK");

	h->q_header_version = HEADER_VERSION;

	//sprintf(h->path, "%s", path);
    //sprintf(h->qname, "%s", qname);
    //sprintf(h->desc, "%s", desc);

	strcpy(h->path, path);
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

	h->XA_ing_flag = 0;
	h->XA_lock_time = 0L;

	h->Master_assign_flag = master_use_flag;
	sprintf( h->MasterHostName, "%s", master_hostname);

	step_print("- initialize header file.", "OK");

	chmod( filename , 0666);
	step_print("- change mode  body file.", "OK");

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
extend_headerfile(fq_logger_t *l, 
			char *path, 
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
	char	name[512];
	int		fd=0;
	
	FQ_TRACE_ENTER(l);

	sprintf(name, "%s.h", qname);
	sprintf(filename, "%s/%s", path, name);

	if( !is_file(filename) ) { /* checks whether the file exists. */
		fq_log(l, FQ_LOG_ERROR, "The file[%s] does not exists.", filename);
		step_print("- Check if header file already does exist ", "NOK");
		goto L0;
	}
	step_print("- Check if header file does exist ", "OK");

	if( (fd=extend_dummy_file(l, path, name, sizeof(mmap_header_t)))  < 0) {
		fq_log(l, FQ_LOG_ERROR, "extend_dummy_file() error. path=[%s] name=[%s]", path, name);
		step_print("- extend header file.", "NOK");
		goto L0;
	}
	step_print("- extend header file.", "OK");

	if( (h = (mmap_header_t *)fq_mmapping(l, fd, sizeof(mmap_header_t), (off_t)0)) == NULL) {
        fq_log(l, FQ_LOG_ERROR, "fq_mmapping() error.");
        step_print("- header file mmapping.", "NOK");
        goto L0;
    }
	step_print("- header mmapping.", "OK");

	h->q_header_version = HEADER_VERSION;

	// sprintf(h->path, "%s", path);
    // sprintf(h->qname, "%s", qname);
    // sprintf(h->desc, "%s", desc);
	strcpy(h->path, path);
    strcpy(h->qname, qname);
    strcpy(h->desc,  desc);

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

	h->XA_ing_flag = 0;
	h->XA_lock_time = 0L;

	step_print("- re-config header file.", "OK");

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
create_datafile(fq_logger_t *l, headfile_obj_t *h_obj)
{
	int fd;
	char	filename[512], name[256];

	FQ_TRACE_ENTER(l);

	sprintf(filename, "%s/%s.r", h_obj->h->path, h_obj->h->qname);
	sprintf(name, "%s.r", h_obj->h->qname);

	if( is_file(filename) ) {
		fq_log(l, FQ_LOG_ERROR, "Already the file[%s] exists.", filename);
		step_print("- Check if data file already does exist ", "NOK");
		goto L0;
	}
	step_print("- Check if data file already does exist ", "OK");
	
	if( (fd=create_dummy_file(l, h_obj->h->path, name, h_obj->h->file_size )) < 0) { 
		fq_log(l, FQ_LOG_ERROR, "create_dummy_file() error. path=[%s] file=[%s]", h_obj->h->path, name);
		step_print("- create dummy data file.", "NOK");
		goto L0;
	}
	step_print("- create dummy data file.", "OK");

	FQ_TRACE_EXIT(l);
    return(TRUE);
	
L0:
	FQ_TRACE_EXIT(l);
	return(FALSE);
}

static int
extend_datafile(fq_logger_t *l, headfile_obj_t *h_obj)
{
	int fd;
	char	filename[512], name[256];

	FQ_TRACE_ENTER(l);

	sprintf(filename, "%s/%s.r", h_obj->h->path, h_obj->h->qname);
	sprintf(name, "%s.r", h_obj->h->qname);

	if( !is_file(filename) ) {
		fq_log(l, FQ_LOG_ERROR, "The file[%s] does not exists.", filename);
		step_print("- Check if data file does exist ", "NOK");
		goto L0;
	}
	step_print("- Check if data file exist ", "OK");
	
	if( (fd=extend_dummy_file(l, h_obj->h->path, name, h_obj->h->file_size )) < 0) { 
		fq_log(l, FQ_LOG_ERROR, "extend_dummy_file() error. path=[%s] file=[%s]", h_obj->h->path, name);
		step_print("- extend dummy data file.", "NOK");
		goto L0;
	}
	step_print("- extend data file.", "OK");

	FQ_TRACE_EXIT(l);
    return(TRUE);
	
L0:
	FQ_TRACE_EXIT(l);
	return(FALSE);
}

static int
unlink_datafile(fq_logger_t *l, char *path, char *qname)
{
	char filename[256];

	FQ_TRACE_ENTER(l);

	sprintf(filename, "%s/%s.r", path, qname);

	if( is_file(filename) ) {
		if( unlink(filename) >= 0 ) {
			fq_log(l, FQ_LOG_DEBUG, "unlink OK.[%s]", filename);
			FQ_TRACE_EXIT(l);
			return(TRUE);
		}
	}
	fq_log(l, FQ_LOG_DEBUG, "unlink failed.[%s] reason=[%s]", filename, strerror(errno));
	FQ_TRACE_EXIT(l);
    return(FALSE);
}

static int
unlink_headfile(fq_logger_t *l, char *path, char *qname)
{
	char filename[256];

	FQ_TRACE_ENTER(l);
	sprintf(filename, "%s/%s.h", path, qname);

    if( is_file(filename) ) {
        if( unlink(filename) >= 0 ) {
            fq_log(l, FQ_LOG_DEBUG, "unlink OK.[%s]", filename);
            FQ_TRACE_EXIT(l);
            return(TRUE);
        }
    }

    fq_log(l, FQ_LOG_DEBUG, "unlink failed.[%s]", filename);

	FQ_TRACE_EXIT(l);
    return(FALSE);
}

/*
 ** typeOfFile - return the english description of the file type.
 **/
char *
get_str_typeOfFile(mode_t mode)
{
    switch (mode & S_IFMT) {
    case S_IFREG:
        return("regular file");
    case S_IFDIR:
        return("directory");
    case S_IFCHR:
        return("character-special device");
    case S_IFBLK:
        return("block-special device");
    case S_IFLNK:
        return("symbolic link");
    case S_IFIFO:
        return("FIFO");
    case S_IFSOCK:
        return("UNIX-domain socket");
    }

    return("???");
}

int get_mode_typeOfFile(mode_t mode)
{
	return(mode & S_IFMT);
}

/*
 ** permOfFile - return the file permissions in an "ls"-like string.
 *   
 **/
char *
permOfFile(mode_t mode)
{
    int i;
    char *p;
    static char perms[10];

    p = perms;
    strcpy(perms, "---------");

    /*
 	** The permission bits are three sets of three
 	** bits: user read/write/exec, group read/write/exec,
 	** other read/write/exec.  We deal with each set
 	** of three bits in one pass through the loop.
 	**/
    for (i=0; i < 3; i++) {
        if (mode & (S_IREAD >> i*3))
            *p = 'r';
        p++;

        if (mode & (S_IWRITE >> i*3))
            *p = 'w';
        p++;

        if (mode & (S_IEXEC >> i*3))
            *p = 'x';
        p++;
    }

    /*
 	** Put special codes in for set-user-id, set-group-id,
 	** and the sticky bit.  (This part is incomplete; "ls"
 	** uses some other letters as well for cases such as
 	** set-user-id bit without execute bit, and so forth.)
 	**/
    if ((mode & S_ISUID) != 0)
        perms[2] = 's';

    if ((mode & S_ISGID) != 0)
        perms[5] = 's';

    if ((mode & S_ISVTX) != 0)
        perms[8] = 't';

    return(perms);
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
#ifndef OS_SOLARIS
        printf("Device Numbers:     Major: %u   Minor: %u\n",
               major(h_obj->st.st_rdev), minor(h_obj->st.st_rdev));
#else
		printf("Device Numbers:     Major: %u   Minor: %u\n",
				(unsigned int)major(h_obj->st.st_rdev), (unsigned int)minor(h_obj->st.st_rdev));
#endif
    }

#ifndef OS_SOLARIS
    printf("Permission Bits:    %s (%04o)\n", permOfFile(h_obj->st.st_mode), h_obj->st.st_mode & 07777);
#else
	printf("Permission Bits:    %s (%04o)\n", permOfFile(h_obj->st.st_mode), (unsigned int)(h_obj->st.st_mode & 07777));
#endif

    printf("Inode Number:       %u\n", (unsigned int) h_obj->st.st_ino);
#ifndef OS_SOLARIS
    printf("Owner User-Id:      %d\n", h_obj->st.st_uid);
    printf("Owner Group-Id:     %d\n", h_obj->st.st_gid);
#else
	printf("Owner User-Id:      %d\n", (int)h_obj->st.st_uid);
    printf("Owner Group-Id:     %d\n", (int)h_obj->st.st_gid);
#endif
    printf("Link Count:         %d\n", (int) h_obj->st.st_nlink);

#ifndef OS_SOLARIS
    printf("File System Device: Major: %u   Minor: %u\n",
           major(h_obj->st.st_dev), minor(h_obj->st.st_dev));
#else
	printf("File System Device: Major: %u   Minor: %u\n",
           (unsigned int)major(h_obj->st.st_dev), (unsigned int)minor(h_obj->st.st_dev));
#endif

    printf("Last Access:        %s", ctime(&h_obj->st.st_atime));
    printf("Last Modification:  %s", ctime(&h_obj->st.st_mtime));
    printf("Last I-Node Change: %s", ctime(&h_obj->st.st_ctime));
	

	FQ_TRACE_EXIT(l);
	return(TRUE);
}


/*
** Delete characters
*/
static int del_char(char* src, char* dst, char ch)
{
	char *p = src;
	char *q = dst;
	register int i, j, k;

	assert(src); 
	assert(dst); 

	k = strlen(p);
	for (i=0; i < k; i++, p++) {
		if ( *p == ch )
			continue;
		else {
			*q = *p; q++;
		}
	}
	*q = '\0';
	return(0);
}

static int 
open_headfile_obj( fq_logger_t *l, char *path, char *qname, headfile_obj_t **obj)
{
	headfile_obj_t *rc=NULL;

	FQ_TRACE_ENTER(l);

	if( !HASVALUE(path) || !HASVALUE(qname) )  {
        fq_log(l, FQ_LOG_ERROR, "illgal function call.");
		FQ_TRACE_EXIT(l);
		return(FALSE);
    }

	rc = (headfile_obj_t *)calloc(1, sizeof(headfile_obj_t));
	if(rc) {
		char filename[256];

		sprintf(filename, "%s/%s.h", path, qname);
		rc->name = strdup(filename);

		if( !can_access_file(l, rc->name)) {
			fq_log(l, FQ_LOG_ERROR, "Queue headfile[%s] can not access.", rc->name);
			goto return_FALSE;
		}

		rc->fd = 0;
		if( (rc->fd=open(rc->name, O_RDWR|O_SYNC, 0666)) < 0 ) {
			fq_log(l, FQ_LOG_ERROR, "Queue headfile[%s] can not open.", rc->name);
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
		rc->en_shmlock = NULL;
		rc->de_shmlock = NULL;

		if( open_flock_obj(l, path, qname, EN_FLOCK, &rc->en_flock) != TRUE ) {
			fq_log(l, FQ_LOG_ERROR, "open_flock_obj(en) error.");
			goto return_FALSE;
		}

		if( open_flock_obj(l, path, qname, DE_FLOCK, &rc->de_flock) != TRUE ) {
			fq_log(l, FQ_LOG_ERROR, "open_flock_obj(de) error.");
			goto return_FALSE;
		}

		
		char shm_en_name[NAME_MAX];
		char org_path[NAME_MAX-20];
		sprintf(org_path, "%s", path);

		char sanitized_path[NAME_MAX-4];
		del_char( org_path, sanitized_path, '/');

		sprintf(shm_en_name, "%s-%s.en", sanitized_path, qname);

		if( open_shmlock_obj(l, shm_en_name, &rc->en_shmlock) != TRUE ) {
			fq_log(l, FQ_LOG_ERROR, "open_shmlock_obj('%s') error. reason='%s.", shm_en_name,  strerror(errno));
			goto return_FALSE;
		}

		char shm_de_name[NAME_MAX];
		sprintf(shm_de_name, "%s-%s.de", sanitized_path, qname);
		if( open_shmlock_obj(l, shm_de_name, &rc->de_shmlock) != TRUE ) {
			fq_log(l, FQ_LOG_ERROR, "open_shmlock_obj('%s') error. reason='%s.", shm_de_name,  strerror(errno));
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
open_datafile_obj( fq_logger_t *l, char *path, char *qname, datafile_obj_t **obj)
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

		sprintf(filename, "%s/%s.r", path, qname);
		rc->name = strdup(filename);

		if( !can_access_file(l, rc->name)) {
			fq_log(l, FQ_LOG_ERROR, "Queue datafile[%s] can not access.", rc->name);
			goto return_FALSE;
		}
		rc->fd = 0;
		if( (rc->fd=open(rc->name, O_RDWR|O_SYNC, 0666)) < 0 ) {
			fq_log(l, FQ_LOG_ERROR, "Queue datafile[%s] can not open.", rc->name);
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

static int 
open_backupfile_obj( fq_logger_t *l, char *path, char *qname, backupfile_obj_t **obj)
{
	backupfile_obj_t *rc=NULL;

	FQ_TRACE_ENTER(l);

	if( !HASVALUE(path) || !HASVALUE(qname) )  {
        fq_log(l, FQ_LOG_ERROR, "illgal function call.");
		FQ_TRACE_EXIT(l);
		return(FALSE);
    }

	rc = (backupfile_obj_t *)calloc( 1, sizeof(backupfile_obj_t));
	if(rc) {
		char filename[256];

		sprintf(filename, "%s/%s.backup", path, qname);
		rc->name = strdup(filename);


		rc->fp = NULL;

		if( (rc->fp=fopen(rc->name, "a+")) == NULL) {
			fq_log(l, FQ_LOG_ERROR, "Queue backupfile[%s] can not open.", filename);
			goto return_FALSE;
		}

		*obj = rc;
		FQ_TRACE_EXIT(l);
		return(TRUE);
	}

return_FALSE:
	if(rc->fp) fclose(rc->fp);

	SAFE_FREE(rc);

	SAFE_FREE(*obj);

	FQ_TRACE_EXIT(l);
	return(FALSE);
}

static int 
close_backupfile_obj( fq_logger_t *l, backupfile_obj_t **obj)
{
	FQ_TRACE_ENTER(l);

	SAFE_FREE((*obj)->name);
		
	if( (*obj)->fp ) {
		fclose( (*obj)->fp );
		(*obj)->fp = NULL;
	}

	SAFE_FREE(*obj);

	FQ_TRACE_EXIT(l);
	return(TRUE);
}

static long 
on_get_diff( fq_logger_t *l, fqueue_obj_t *obj)
{
	long rc;

	FQ_TRACE_ENTER(l);

	// obj->h_obj->de_flock->on_flock(obj->h_obj->de_flock);
	// fq_critical_section(l, obj, L_DE_ACTION, FQ_LOCK_ACTION);

    rc = obj->h_obj->h->en_sum - obj->h_obj->h->de_sum;

	// obj->h_obj->de_flock->on_funlock(obj->h_obj->de_flock);
	// fq_critical_section(l, obj, L_DE_ACTION, FQ_UNLOCK_ACTION);

	FQ_TRACE_EXIT(l);
	return(rc);
}

static float 
on_get_usage( fq_logger_t *l, fqueue_obj_t *obj)
{
	long diff;
	float usage;

	FQ_TRACE_ENTER(l);

	// obj->h_obj->de_flock->on_flock(obj->h_obj->de_flock);
	// fq_critical_section(l, obj, L_DE_ACTION, FQ_LOCK_ACTION);

    diff = obj->h_obj->h->en_sum - obj->h_obj->h->de_sum;
	if( diff == 0 ) usage = 0.0;
	else {
		usage = ((float)diff*100.0)/(float)obj->h_obj->h->max_rec_cnt;
	}

	// obj->h_obj->de_flock->on_funlock(obj->h_obj->de_flock);
	// fq_critical_section(l, obj, L_DE_ACTION, FQ_UNLOCK_ACTION);

	FQ_TRACE_EXIT(l);
	return(usage);
}

static bool_t 
on_do_not_flow( fq_logger_t *l, fqueue_obj_t *obj, time_t waiting_time)
{
	int len1, len2;
	long seq=0L, run_time=0L;
	unsigned char first_data[65536];
	unsigned char second_data[65536];
	int buffer_size = 65536;
	bool_t ret;
	bool	big_flag;
	
	
	FQ_TRACE_ENTER(l);

	len1 = obj->on_view( l, obj, first_data, buffer_size, &seq, &run_time, &big_flag);
	usleep(waiting_time);
	len2 = obj->on_view( l, obj, second_data, buffer_size, &seq, &run_time, &big_flag);

	fq_log(l, FQ_LOG_DEBUG, "len1=[%d], len2=[%d], first_data=[%40.40s],  second_data=[%40.40s].",
		len1, len2, first_data, second_data);
 
	if( (len1 > 0) && (len2 > 0) && (len1 == len2)) {
		if( strncmp( first_data, second_data, len2) == 0 ) {
			ret = True;
		}
		else {
			ret = False;
		}
	}
	else {
		ret = False;
	}

	FQ_TRACE_EXIT(l);
	return(ret);
}


/* here-1 */
#if 0
#define FQ_LOCK_ACTION 1
#define FQ_UNLOCK_ACTION 2
#define L_EN_ACTION 1
#define L_DE_ACTION 2
#endif

bool fq_critical_section(fq_logger_t *l, fqueue_obj_t *obj, int en_de_flag, int lock_unlock_flag)
{
	FQ_TRACE_ENTER(l);

#ifdef USE_FILE_LOCK 
	if( en_de_flag == L_EN_ACTION ) {
		if( lock_unlock_flag == FQ_LOCK_ACTION ) {
			obj->h_obj->en_flock->on_flock(obj->h_obj->en_flock); // file, en, lock
		}
		else if( lock_unlock_flag == FQ_UNLOCK_ACTION ) {
			obj->h_obj->en_flock->on_funlock(obj->h_obj->en_flock); // file, en, unlock
		}
		else {
			fq_log(l, FQ_LOG_ERROR, "undefined locking method.");
			FQ_TRACE_EXIT(l);
			return false;
		}
	}
	else if( en_de_flag == L_DE_ACTION ) { // de
		if( lock_unlock_flag == FQ_LOCK_ACTION ) {
			obj->h_obj->en_flock->on_flock(obj->h_obj->de_flock); // file, de, lock
		}
		else if( lock_unlock_flag == FQ_UNLOCK_ACTION ) {
			obj->h_obj->en_flock->on_funlock(obj->h_obj->de_flock); // file, de, unlock
		}
		else {
			fq_log(l, FQ_LOG_ERROR, "undefined locking method.");
			FQ_TRACE_EXIT(l);
			return false;
		}
	}
	else {
		fq_log(l, FQ_LOG_ERROR, "undefined locking method.");
		FQ_TRACE_EXIT(l);
		return false;
	}
#endif

#ifdef USE_SHM_LOCK
	if( en_de_flag == L_EN_ACTION ) {
		if( lock_unlock_flag == FQ_LOCK_ACTION ) {
			obj->h_obj->en_shmlock->on_mutex_lock(l, obj->h_obj->en_shmlock); // shm, en, lock
		}
		else if( lock_unlock_flag == FQ_UNLOCK_ACTION  ) {
			obj->h_obj->en_shmlock->on_mutex_unlock(l, obj->h_obj->en_shmlock); // shm, en, unlock
		}
		else {
			fq_log(l, FQ_LOG_ERROR, "undefined locking method.");
			FQ_TRACE_EXIT(l);
			return false;

		}
	}
	else if( en_de_flag == L_DE_ACTION ) { // de
		if( lock_unlock_flag == FQ_LOCK_ACTION ) {
			obj->h_obj->de_shmlock->on_mutex_lock(l, obj->h_obj->de_shmlock); // shm, de, lock
		}
		else if( lock_unlock_flag == FQ_UNLOCK_ACTION ) {
			obj->h_obj->de_shmlock->on_mutex_unlock(l, obj->h_obj->de_shmlock); // shm, de, unlock
		}
		else {
			fq_log(l, FQ_LOG_ERROR, "undefined locking method.");
			FQ_TRACE_EXIT(l);
			return false;
		}
	}
	else {
		fq_log(l, FQ_LOG_ERROR, "undefined locking method.");
		FQ_TRACE_EXIT(l);
		return false;
	}
#endif

	FQ_TRACE_EXIT(l);
	
	return true;
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
		// obj->h_obj->en_flock->on_flock(obj->h_obj->en_flock);
		fq_critical_section(l, obj, L_EN_ACTION, FQ_LOCK_ACTION);
	} else if( en_de_flag == FQ_DE_ACTION ) {
		// obj->h_obj->de_flock->on_flock(obj->h_obj->de_flock);
		fq_critical_section(l, obj, L_DE_ACTION, FQ_LOCK_ACTION);
	} else {
		fq_log(l, FQ_LOG_ERROR, "en_de_flag error.");
	}	

	off_stopwatch_micro(&p);

	get_stopwatch_micro(&p, &sec, &usec);
	total_micro_sec = sec * 1000000;
	total_micro_sec =  total_micro_sec + usec;

	if( en_de_flag == FQ_EN_ACTION ) {
		// obj->h_obj->en_flock->on_funlock(obj->h_obj->en_flock);
		fq_critical_section(l, obj, L_EN_ACTION, FQ_UNLOCK_ACTION);
	} else if( en_de_flag == FQ_DE_ACTION ) {
		// obj->h_obj->de_flock->on_funlock(obj->h_obj->de_flock);
		fq_critical_section(l, obj, L_DE_ACTION, FQ_UNLOCK_ACTION);
	}
		
	FQ_TRACE_EXIT(l);
	return(total_micro_sec);
}

static int 
on_get_big( fq_logger_t *l, fqueue_obj_t *obj)
{
	int big_files;

	FQ_TRACE_ENTER(l);

    big_files = obj->h_obj->h->current_big_files;

	FQ_TRACE_EXIT(l);
	return(big_files);
}

static int 
on_set_QOS( fq_logger_t *l, fqueue_obj_t *obj, int usec)
{
	long rc=0;

	FQ_TRACE_ENTER(l);

	// obj->h_obj->de_flock->on_flock(obj->h_obj->de_flock);
	fq_critical_section(l, obj, L_DE_ACTION, FQ_LOCK_ACTION);

    obj->h_obj->h->QOS = usec;

	// obj->h_obj->de_flock->on_funlock(obj->h_obj->de_flock);
	fq_critical_section(l, obj, L_DE_ACTION, FQ_UNLOCK_ACTION);

	if( obj->evt_obj ) {
		obj->evt_obj->on_en_eventlog(l, obj->uq_obj, EVENT_FQ_SETQOS, obj->path, obj->qname, TRUE, getpid() );
	}
	FQ_TRACE_EXIT(l);
	return(rc);
}

static int save_big_Q_file(fq_logger_t *l, const char *data, size_t len, char *big_qfilename)
{
	int fd=-1, n;

	fq_log(l, FQ_LOG_TRACE, "save_big_Q_file(%s)", big_qfilename);

	if(!data || !big_qfilename || (len <= 0) ) {
		fq_log(l, FQ_LOG_ERROR, "illegal function request. Check parameters.");
		return(-3);
	}
		
	/* We don't unlink for speed, so We open to not be O_EXCL. */
	/* if( (fd=open(filename, O_RDWR|O_CREAT|O_EXCL, 0666)) < 0 ) { */
	if( (fd=open(big_qfilename, O_RDWR|O_CREAT, 0666)) < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "file open error. reason=[%s] file=[%s]", strerror(errno), big_qfilename);
		return(-1);
	}
	if( (n=write(fd, data, len)) != len ) {
		fq_log(l, FQ_LOG_ERROR, "file write error. reason=[%s]", strerror(errno));
		SAFE_FD_CLOSE(fd);
		return(-2);
	}

	SAFE_FD_CLOSE(fd);
	fd = -1;

	if( is_file(big_qfilename) ) {
		int FileSize;

		if( (FileSize=getFileSize(l, big_qfilename)) < len ) {
			fq_log(l, FQ_LOG_ERROR, "Saving a big file('%s') was failed. size= [%d].\n", big_qfilename, FileSize);
			return(-3);
		}
		else {
			fq_log(l, FQ_LOG_DEBUG, "Existing of [%s] was confirmed. size=[%d]", big_qfilename, FileSize );
		}
	}
	else {
		fq_log(l, FQ_LOG_TRACE, "Actually, [%s] does not exist.");
		return(-3);
	}

	return(n);
}

static int load_big_Q_file_2_buf(fq_logger_t *l, char *big_qfilename, char**ptr_dst, int *flen, char** ptr_errmsg)
{
	int fdin, rc=-1;
	struct stat statbuf;
	off_t len=0;
	char	*p=NULL;
	int	n;
	char szErrMsg[512];

	if(( fdin = open(big_qfilename ,O_RDONLY)) < 0 ) { /* option : O_RDWR */
		sprintf(szErrMsg, "file[%s] open() error. reason=[%s]", big_qfilename, strerror(errno)); 
		*ptr_errmsg = strdup(szErrMsg);
		goto L0;
	}

	if(( fstat(fdin, &statbuf)) < 0 ) {
		*ptr_errmsg = strdup("stat() error.");
		goto L1;
	}

	len = statbuf.st_size;
	*flen = len;

	p = calloc(len+1, sizeof(char));
	if(!p) {
		*ptr_errmsg = strdup("memory leak!! Check your free memory!");	
		goto L1;
	}

	if(( n = read( fdin, p, len)) != len ) {
		*ptr_errmsg = strdup("read() error.");
		goto L1;
    }

	*ptr_dst = p;

	rc = 0;
L1:
	SAFE_FD_CLOSE(fdin);
L0:
	return(rc);
}

static int 
on_set_master( fq_logger_t *l, fqueue_obj_t *obj, int on_off_flag, char *hostname)
{
	FQ_TRACE_ENTER(l);

	//obj->h_obj->en_flock->on_flock(obj->h_obj->en_flock);
	fq_critical_section(l, obj, L_EN_ACTION, FQ_LOCK_ACTION);

	// obj->h_obj->de_flock->on_flock(obj->h_obj->de_flock);
	fq_critical_section(l, obj, L_DE_ACTION, FQ_LOCK_ACTION);


	obj->h_obj->h->Master_assign_flag = on_off_flag;
	sprintf(obj->h_obj->h->MasterHostName, "%s", hostname);
	
	//obj->h_obj->en_flock->on_funlock(obj->h_obj->en_flock);
	fq_critical_section(l, obj, L_EN_ACTION, FQ_UNLOCK_ACTION);

	// obj->h_obj->de_flock->on_funlock(obj->h_obj->de_flock);
	fq_critical_section(l, obj, L_DE_ACTION, FQ_UNLOCK_ACTION);


	FQ_TRACE_EXIT(l);
	return(TRUE);
}


#ifndef RAW_LSEEK_WRITE 
static int 
on_en( fq_logger_t *l, fqueue_obj_t *obj, enQ_mode_t en_mode, const unsigned char *data, int bufsize, size_t len, long *seq, long *run_time)
{
	int ret=-1;
	unsigned char header[FQ_HEADER_SIZE+1];
	register off_t   record_no;
	register off_t   p_offset_from; 
	/*
	register off_t  p_offset_to;
	*/
	register off_t   mmap_offset;
	register off_t   add_offset;
	long    nth_mapping;
	int		msync_flag = 0; /* sysc 가 느려져서 deQ에서 zero byte 가 리턴되는 경우 생긴 신한은행  */

	stopwatch_micro_t p;
    long sec=0L, usec=0L, total_micro_sec=0L;

	FQ_TRACE_ENTER(l);

#ifdef _USE_HEARTBEAT
#ifdef _USE_PROCESS_CONTROL
	if( signal_received == 1) {
		signal_received = 0;
		restart_process(obj->argv);
		sleep(1);
	}
#endif
	if( obj->hostname && obj->hb_obj ) {
		int rc;
		rc = obj->hb_obj->on_timestamp(l, obj->hb_obj, MASTER_TIME_STAMP);
		if( rc == FALSE ) {
			FQ_TRACE_EXIT(l);
#ifdef _USE_PROCESS_CONTROL
			signal_received = 0;
			restart_process(obj->argv);
#endif
			sleep(1);
		}
	}
#endif

	if( obj->stopwatch_on_off == STOPWATCH_ON ) {
		on_stopwatch_micro(&p);
	}

	if( !obj ) {
		fq_log(l, FQ_LOG_ERROR, "Object is null.");
		FQ_TRACE_EXIT(l);
		return(-1);
	}
	if( (en_mode != EN_NORMAL_MODE) && (en_mode != EN_CIRCULAR_MODE) && (en_mode != EN_FULL_BACKUP_MODE) ) {
		fq_log(l, FQ_LOG_ERROR, "en_mode error. Use EN_NORMAL_MODE or EN_CIRCULAR_MODE or EN_FULL_BACKUP_MODE");
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
		fq_log(l, FQ_LOG_ERROR, "data length is zero(0) or minus or so big. Please check your buffer. len=[%zu]", len);
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

	if( len > bufsize ) {
		fq_log(l, FQ_LOG_ERROR, "length of data(%zu) is greater than buffer size(%d).", len, bufsize);
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

	if( (obj->h_obj->h->Master_assign_flag == 1) && (obj->h_obj->h->MasterHostName[0] != 0x00) ) {
		fq_log(l, FQ_LOG_DEBUG, "Master checking start.");
		fq_log(l, FQ_LOG_DEBUG, "h->MasterHostName=[%s], obj->hostname=[%s]", obj->h_obj->h->MasterHostName, obj->hostname);

		if( (obj->h_obj->h->MasterHostName[0] != 0x00) && (obj->hostname[0]!=0x00) ) {
			if(strcmp( obj->h_obj->h->MasterHostName, obj->hostname) != 0 ) {
				// obj->h_obj->en_flock->on_funlock(obj->h_obj->en_flock);
				// fq_critical_section(l, obj, L_EN_ACTION, FQ_UNLOCK_ACTION);

				fq_log(l, FQ_LOG_ERROR, "You can't access to this queue. Accessable server is [%s].", obj->h_obj->h->MasterHostName);
				FQ_TRACE_EXIT(l);
				return(IS_NOT_MASTER);
			}
		}
	}

	if( obj->h_obj->h->QOS != 0 ) { /* QoS: Quality of Service. you can control enQ speed as adjusting this value. */
		usleep(obj->h_obj->h->QOS); /* min is 1000 */
	}

	// obj->h_obj->en_flock->on_flock(obj->h_obj->en_flock);
	fq_critical_section(l, obj, L_EN_ACTION, FQ_LOCK_ACTION);


	if( (obj->h_obj->h->en_sum - obj->h_obj->h->de_sum) >= obj->h_obj->h->max_rec_cnt) {
		if( en_mode == EN_CIRCULAR_MODE ) { /* When queue is full, We flush a data to BACKUP file automatically */
			if( obj->evt_obj ) {
				obj->evt_obj->on_en_eventlog(l, obj->uq_obj, EVENT_FQ_DE_AUTO, obj->path, obj->qname, TRUE, getpid(), obj->h_obj->h->de_sum, 0L );
			}
			obj->h_obj->h->de_sum++;
		}
		else if( en_mode == EN_FULL_BACKUP_MODE ) { /* EN_FULL_BACKUP_MODE */
			/* encodeing base54 */
			char *dst = calloc( len*2, sizeof(char));
			int data_len = b64_encode(data, len, dst, len*2);
			fprintf(obj->b_obj->fp, "%s\n", (char *)dst);
			fflush(obj->b_obj->fp);
			free(dst);
			ret = len;
			goto unlock_return;
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

#ifdef MSYNC_WHENEVER_UNMAP
			/* 2016/01/07 : for using shared disk(SAN) in ShinhanBank */
			msync((void *)obj->en_p, obj->h_obj->h->mapping_len, MS_SYNC|MS_INVALIDATE); /*  MS_INVALIDATE */
#endif

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
	set_bodysize(header, FQ_HEADER_SIZE, len, 0);
	add_offset = p_offset_from - obj->h_obj->h->en_available_from_offset;

	memset( (void *)((off_t)obj->en_p + add_offset), 0x00, obj->h_obj->h->msglen); /*clean old data */
	memcpy( (void *)((off_t)obj->en_p + add_offset), header, FQ_HEADER_SIZE); /* write length of data */

	if( len > obj->h_obj->h->msglen-FQ_HEADER_SIZE ) { /* big message */
		char big_filename[256];

		if( obj->h_obj->h->made_big_dir == 0 ) { /* make directory for big file */
			char big_dirname[256];

			sprintf(big_dirname, "%s/BIG_%s", obj->path, obj->qname);
			mkdir(big_dirname, 0666);
			chmod( big_dirname , 0777);
			fq_log(obj->l, FQ_LOG_TRACE, "mkdir: '%s'", big_dirname);
			obj->h_obj->h->made_big_dir = 1;
		}

		if( obj->h_obj->h->current_big_files > FQ_LIMIT_BIG_FILES ) {
			fq_log(obj->l, FQ_LOG_ERROR,  "fatal: There are too many big files in %s/BIG_%s. current=[%d], limited=[%d]", 
					obj->path, obj->qname, obj->h_obj->h->current_big_files, FQ_LIMIT_BIG_FILES);
			goto unlock_return;
		}

		/* We set big_filename to record_no and save it to ${qpath}/BIG_${qname} folder.*/
		memset(big_filename, 0x00, sizeof(big_filename));
		sprintf(big_filename, "%s/BIG_%s/B%08ld-th.Q.msg", obj->path, obj->qname, record_no);
		fq_log(l, FQ_LOG_TRACE, "msglen=[%zu]: Big file,'%s' will be save to '%s'.", len, big_filename, obj->path);

		if( save_big_Q_file( l, (char *)data, len, big_filename ) < 0 ) {
			fq_log(obj->l, FQ_LOG_ERROR,  "fatal: save_big_Q_file(%s) error. Check whether directory is created.", big_filename);
			goto unlock_return;
		}

		/* In case of big_file, We write real filename to the data area */
		memcpy( (void *)((off_t)obj->en_p + add_offset + FQ_HEADER_SIZE), big_filename, strlen(big_filename)+1);

		msync_flag=1;

		obj->h_obj->h->current_big_files++; /* 성공시에만 증가 시킨다 */
		obj->h_obj->h->big_sum++;
	}
	else { /* normal message */
		memcpy( (void *)((off_t)obj->en_p + add_offset + FQ_HEADER_SIZE), (void *)data, len);
	}

	*seq = obj->h_obj->h->en_sum; 
	obj->h_obj->h->en_sum++;

	obj->h_obj->h->last_en_time = time(0);
	ret = len;

	if( msync_flag ) { 
		 msync((void *)obj->en_p, obj->h_obj->h->mapping_len, MS_SYNC|MS_INVALIDATE); /*  MS_INVALIDATE */
	}

#ifdef SYNC_FILE_RAMGE 
	int r;

	// SYNC_FILE_RANGE_WAIT_BEFORE | SYNC_FILE_RANGE_WRITE | SYNC_FILE_RANGE_WAIT_AFTER

	r = sync_file_range(obj->d_obj->fd, mmap_offset, obj->h_obj->h->mapping_len, SYNC_FILE_RANGE_WRITE); // async
	if( r != 0 ) {
		fq_log(obj->l, FQ_LOG_ERROR,  "fatal: sync_file_range( datafile ) error.");	
	}
	r = sync_file_range(obj->h_obj->fd, 0, sizeof(mmap_header_t), SYNC_FILE_RANGE_WRITE); // async
	if( r != 0 ) {
		fq_log(obj->l, FQ_LOG_ERROR,  "fatal: sync_file_range( headerfile ) error.");	
	}
#endif

#ifdef FDATASYNC
	int rr;
	// rr = fdatasync(obj->d_obj->fd);	
	rr = fsync(obj->d_obj->fd);	
	if( rr != 0 ) {
		fq_log(obj->l, FQ_LOG_ERROR,  "fatal: fdatasync(datafile) error.");
	}
	// rr = fdatasync(obj->h_obj->fd);	
	rr = fsync(obj->h_obj->fd);	
	if( rr != 0 ) {
		fq_log(obj->l, FQ_LOG_ERROR,  "fatal: fdatasync(headerfile) error.");
	}

#endif

unlock_return:
	if( obj->monID && obj->mon_obj) {
		obj->mon_obj->on_touch(l, obj->monID, FQ_EN_ACTION, obj->mon_obj);
	}

	// obj->h_obj->en_flock->on_funlock(obj->h_obj->en_flock);
	fq_critical_section(l, obj, L_EN_ACTION, FQ_UNLOCK_ACTION);

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

	if( obj->stopwatch_on_off == STOPWATCH_ON ) {
		if( total_micro_sec > DELAY_MICRO_SECOND) {
			fq_log(l, FQ_LOG_EMERG, "on_en() delay time : %ld micro second.", total_micro_sec);
		}
	}

	if( obj->evt_obj ) {
		obj->evt_obj->on_en_eventlog(l, obj->uq_obj, EVENT_FQ_EN, obj->path, obj->qname, ((ret>=0)?TRUE:FALSE), getpid(), *seq, total_micro_sec );
	}

	FQ_TRACE_EXIT(l);
	return(ret);
}

static int 
on_en_bundle_struct( fq_logger_t *l, fqueue_obj_t *obj, int src_cnt, fqdata_t src[], long *run_time)
{
	int ret=-1;
	unsigned char header[FQ_HEADER_SIZE+1];
	register off_t   record_no;
	register off_t   p_offset_from; 
	/*
	register off_t  p_offset_to;
	*/
	register off_t   mmap_offset;
	register off_t   add_offset;
	long    nth_mapping;
	int		msync_flag = 0; /* sysc 가 느려져서 deQ에서 zero byte 가 리턴되는 경우 생긴 신한은행  */
	int 	array_index = 0;

	stopwatch_micro_t p;
    long sec=0L, usec=0L, total_micro_sec=0L;

	FQ_TRACE_ENTER(l);

#ifdef _USE_HEARTBEAT
#ifdef _USE_PROCESS_CONTROL
	if( signal_received == 1) {
		signal_received = 0;
		restart_process(obj->argv);
		sleep(1);
	}
#endif
	if( obj->hostname && obj->hb_obj ) {
		int rc;
		rc = obj->hb_obj->on_timestamp(l, obj->hb_obj, MASTER_TIME_STAMP);
		if( rc == FALSE ) {
			FQ_TRACE_EXIT(l);
#ifdef _USE_PROCESS_CONTROL
			signal_received = 0;
			restart_process(obj->argv);
#endif
			sleep(1);
		}
	}
#endif

	if( obj->stopwatch_on_off == STOPWATCH_ON ) {
		on_stopwatch_micro(&p);
	}

	if( !obj ) {
		fq_log(l, FQ_LOG_ERROR, "Object is null.");
		FQ_TRACE_EXIT(l);
		return(-1);
	}

	if( !src ) {
		fq_log(l, FQ_LOG_ERROR, "src(fqdata_t) is null.");
		FQ_TRACE_EXIT(l);
		return(-1);
	}

	if( src_cnt >  obj->h_obj->h->max_rec_cnt ) {
		fq_log(l, FQ_LOG_ERROR, "src_cnt is too big. Max value is [%d].", obj->h_obj->h->max_rec_cnt);
		FQ_TRACE_EXIT(l);
		return(-1);
	}

	if( run_time == NULL ) {
		fq_log(l, FQ_LOG_ERROR, "runtime is null");
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

	if( (obj->h_obj->h->Master_assign_flag == 1) && (obj->h_obj->h->MasterHostName[0] != 0x00) ) {
		fq_log(l, FQ_LOG_DEBUG, "Master checking start.");
		fq_log(l, FQ_LOG_DEBUG, "h->MasterHostName=[%s], obj->hostname=[%s]", obj->h_obj->h->MasterHostName, obj->hostname);

		if( (obj->h_obj->h->MasterHostName[0] != 0x00) && (obj->hostname[0]!=0x00) ) {
			if(strcmp( obj->h_obj->h->MasterHostName, obj->hostname) != 0 ) {
				// obj->h_obj->en_flock->on_funlock(obj->h_obj->en_flock);
				// fq_critical_section(l, obj, L_EN_ACTION, FQ_UNLOCK_ACTION);

				fq_log(l, FQ_LOG_ERROR, "You can't access to this queue. Accessable server is [%s].", obj->h_obj->h->MasterHostName);
				FQ_TRACE_EXIT(l);
				return(IS_NOT_MASTER);
			}
		}
	}

	// obj->h_obj->en_flock->on_flock(obj->h_obj->en_flock);
	fq_critical_section(l, obj, L_EN_ACTION, FQ_LOCK_ACTION);


	for(array_index=0; array_index < src_cnt; array_index++) {
		if( (obj->h_obj->h->en_sum - obj->h_obj->h->de_sum) >= obj->h_obj->h->max_rec_cnt) {
			fq_log(l, FQ_LOG_DEBUG, "en_sum[%ld] de_sum[%ld] max_rec_cnt[%ld]", 
				obj->h_obj->h->en_sum, obj->h_obj->h->de_sum, obj->h_obj->h->max_rec_cnt);
			break;
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

	#ifdef MSYNC_WHENEVER_UNMAP
				/* 2016/01/07 : for using shared disk(SAN) in ShinhanBank */
				msync((void *)obj->en_p, obj->h_obj->h->mapping_len, MS_SYNC|MS_INVALIDATE); /*  MS_INVALIDATE */
	#endif

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
		set_bodysize(header, FQ_HEADER_SIZE, src[array_index].len, 0);
		add_offset = p_offset_from - obj->h_obj->h->en_available_from_offset;

		memset( (void *)((off_t)obj->en_p + add_offset), 0x00, obj->h_obj->h->msglen); /*clean old data */
		memcpy( (void *)((off_t)obj->en_p + add_offset), header, FQ_HEADER_SIZE); /* write length of data */

		if( src[array_index].len > obj->h_obj->h->msglen-FQ_HEADER_SIZE ) { /* big message */
			char big_filename[256];

			if( obj->h_obj->h->made_big_dir == 0 ) { /* make directory for big file */
				char big_dirname[256];

				sprintf(big_dirname, "%s/BIG_%s", obj->path, obj->qname);
				mkdir(big_dirname, 0666);
				chmod( big_dirname , 0777);
				fq_log(obj->l, FQ_LOG_TRACE, "mkdir: '%s'", big_dirname);
				obj->h_obj->h->made_big_dir = 1;
			}

			if( obj->h_obj->h->current_big_files > FQ_LIMIT_BIG_FILES ) {
				fq_log(obj->l, FQ_LOG_ERROR,  "fatal: There are too many big files in %s/BIG_%s. current=[%d], limited=[%d]", 
						obj->path, obj->qname, obj->h_obj->h->current_big_files, FQ_LIMIT_BIG_FILES);
				goto unlock_return;
			}

			/* We set big_filename to record_no and save it to ${qpath}/BIG_${qname} folder.*/
			memset(big_filename, 0x00, sizeof(big_filename));
			sprintf(big_filename, "%s/BIG_%s/B%08ld-th.Q.msg", obj->path, obj->qname, record_no);
			fq_log(l, FQ_LOG_TRACE, "msglen=[%d]: Big file,'%s' will be save to '%s'.", src[array_index].len, big_filename, obj->path);

			if( save_big_Q_file( l, (char *)src[array_index].data, src[array_index].len, big_filename ) < 0 ) {
				fq_log(obj->l, FQ_LOG_ERROR,  "fatal: save_big_Q_file(%s) error. Check whether directory is created.", big_filename);
				goto unlock_return;
			}

			/* In case of big_file, We write real filename to the data area */
			memcpy( (void *)((off_t)obj->en_p + add_offset + FQ_HEADER_SIZE), big_filename, strlen(big_filename)+1);

			msync_flag=1;

			obj->h_obj->h->current_big_files++; /* 성공시에만 증가 시킨다 */
			obj->h_obj->h->big_sum++;
		}
		else { /* normal message */
			memcpy( (void *)((off_t)obj->en_p + add_offset + FQ_HEADER_SIZE), (void *)src[array_index].data, src[array_index].len);
		}

		obj->h_obj->h->en_sum++;

	} /* for end */

	obj->h_obj->h->last_en_time = time(0);
	ret = array_index;

	if( msync_flag ) { 
		 msync((void *)obj->en_p, obj->h_obj->h->mapping_len, MS_SYNC|MS_INVALIDATE); /*  MS_INVALIDATE */
	}

#ifdef SYNC_FILE_RAMGE 
	int r;
	r = sync_file_range(obj->d_obj->fd, mmap_offset, obj->h_obj->h->mapping_len, SYNC_FILE_RANGE_WRITE); // async
	if( r != 0 ) {
		fq_log(obj->l, FQ_LOG_ERROR,  "fatal: sync_file_range() error.");	
	}
#endif

unlock_return:
	if( obj->monID && obj->mon_obj) {
		obj->mon_obj->on_touch(l, obj->monID, FQ_EN_ACTION, obj->mon_obj);
	}

	// obj->h_obj->en_flock->on_funlock(obj->h_obj->en_flock);
	fq_critical_section(l, obj, L_EN_ACTION, FQ_UNLOCK_ACTION);


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

	if( obj->stopwatch_on_off == STOPWATCH_ON ) {
		if( total_micro_sec > DELAY_MICRO_SECOND) {
			fq_log(l, FQ_LOG_EMERG, "on_en() delay time : %ld micro second.", total_micro_sec);
		}
	}

	FQ_TRACE_EXIT(l);
	return(ret);
}
#else
static int 
on_en( fq_logger_t *l, fqueue_obj_t *obj, enQ_mode_t en_mode, const unsigned char *data, int bufsize, size_t len, long *seq, long *run_time)
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
	long    nth_mapping;
	off_t	offset_of_file = 0L;

	stopwatch_micro_t p;
    long sec=0L, usec=0L, total_micro_sec=0L;

	// obj->h_obj->en_flock->on_flock(obj->h_obj->en_flock);
	fq_critical_section(l, obj, L_EN_ACTION, FQ_LOCK_ACTION);


	FQ_TRACE_ENTER(l);

	/*
	fq_log(l, FQ_LOG_TRACE, "function call params: pointer of obj=[%x], en_mode=[%d], data=[%-20.20s], bufsize=[%d], len=[%ld], pointer for seq=[%x], pointer for run_time=[%x]",
		obj, en_mode, data?data:"null", bufsize, len, seq, run_time);
	*/
		

	on_stopwatch_micro(&p);

	
	if(len <= 0) {
		// obj->h_obj->en_flock->on_funlock(obj->h_obj->en_flock);
		fq_critical_section(l, obj, L_EN_ACTION, FQ_UNLOCK_ACTION);

		fq_log(l, FQ_LOG_ERROR, "data length is zero(0) or minus.");
		FQ_TRACE_EXIT(l);
		return(-1);
	}

	if( obj->stop_signal == 1) {
		// obj->h_obj->en_flock->on_funlock(obj->h_obj->en_flock);
		fq_critical_section(l, obj, L_EN_ACTION, FQ_UNLOCK_ACTION);

		fq_log(l, FQ_LOG_ERROR, "obj was already closed.");
		FQ_TRACE_EXIT(l);
		return(MANAGER_STOP_RETURN_VALUE);
	} 

	if( obj->h_obj->h->status != QUEUE_ENABLE ) {
		// obj->h_obj->en_flock->on_funlock(obj->h_obj->en_flock);
		// obj->h_obj->flock->on_funlock(obj->h_obj->flock);
		fq_critical_section(l, obj, L_EN_ACTION, FQ_UNLOCK_ACTION);
		fq_log(l, FQ_LOG_ERROR, "This queue is disabled by Admin.");
		FQ_TRACE_EXIT(l);
		return(MANAGER_STOP_RETURN_VALUE);
	} 

	if( obj->h_obj->h->QOS != 0 ) {
		usleep(obj->h_obj->h->QOS); /* min is 1000 */
	}

	if( (obj->h_obj->h->en_sum - obj->h_obj->h->de_sum) >= obj->h_obj->h->max_rec_cnt) {
		if( en_mode == EN_CIRCULAR_MODE ) { /* queue is full */
			if( obj->evt_obj ) {
				obj->evt_obj->on_en_eventlog(l, obj->uq_obj, EVENT_FQ_DE_AUTO, obj->path, obj->qname, TRUE, getpid(), obj->h_obj->h->de_sum, 0L );
			}
			obj->h_obj->h->de_sum++;
		}
		else { /* EN_NORMAL_MODE */
			fq_log(l, FQ_LOG_DEBUG, "en_sum[%ld] de_sum[%ld] max_rec_cnt[%ld]", 
				obj->h_obj->h->en_sum, obj->h_obj->h->de_sum, obj->h_obj->h->max_rec_cnt);
			ret = EQ_FULL_RETURN_VALUE;
			goto unlock_return;
		}
	}
	set_bodysize(header, FQ_HEADER_SIZE, len, 0);

	record_no = obj->h_obj->h->en_sum % obj->h_obj->h->max_rec_cnt;
	offset_of_file = record_no * obj->h_obj->h->msglen;
	// fq_log(l, FQ_LOG_ERROR, "offset_of_file=[%ld]", offset_of_file);

	lseek(obj->d_obj->fd, offset_of_file, SEEK_SET);
	write(obj->d_obj->fd, header, FQ_HEADER_SIZE);

	lseek(obj->d_obj->fd, offset_of_file+FQ_HEADER_SIZE, SEEK_SET);
	write(obj->d_obj->fd, data, len);

	*seq = obj->h_obj->h->en_sum;
	obj->h_obj->h->en_sum++;

	obj->h_obj->h->last_en_time = time(0);
	ret = len;

unlock_return:

	// obj->h_obj->en_flock->on_funlock(obj->h_obj->en_flock);
	fq_critical_section(l, obj, L_EN_ACTION, FQ_UNLOCK_ACTION);


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

	if( obj->monID && obj->mon_obj) {
		obj->mon_obj->on_touch(l, obj->monID, FQ_EN_ACTION, obj->mon_obj);
	}

	if( obj->evt_obj ) {
		obj->evt_obj->on_en_eventlog(l, obj->uq_obj, EVENT_FQ_EN, obj->path, obj->qname, ((ret>=0)?TRUE:FALSE), getpid(), *seq, total_micro_sec );
	}

	FQ_TRACE_EXIT(l);
	return(ret);
}
#endif

static int 
on_de(fq_logger_t *l, fqueue_obj_t *obj, unsigned char *dst, int dstlen, long *seq, long *run_time)
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

restart:
	// obj->h_obj->de_flock->on_flock(obj->h_obj->de_flock);
	fq_critical_section(l, obj, L_DE_ACTION, FQ_LOCK_ACTION);

	if( obj->stop_signal == 1) {
		fq_log(l, FQ_LOG_ERROR, "Administrator sent stopping signal.");
		// obj->h_obj->de_flock->on_funlock(obj->h_obj->de_flock);
		fq_critical_section(l, obj, L_DE_ACTION, FQ_UNLOCK_ACTION);

		FQ_TRACE_EXIT(l);
		return(MANAGER_STOP_RETURN_VALUE);
	} 

	if( obj->h_obj->h->status != QUEUE_ENABLE ) {
		fq_log(l, FQ_LOG_ERROR, "This queue is disabled by Admin.");
		// obj->h_obj->de_flock->on_funlock(obj->h_obj->de_flock);
		fq_critical_section(l, obj, L_DE_ACTION, FQ_UNLOCK_ACTION);
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

	if( obj->h_obj->h->QOS != 0 ) {
		usleep(obj->h_obj->h->QOS); /* min is 1000 */
	}

	if( dstlen < (obj->h_obj->h->msglen-FQ_HEADER_SIZE) ) {
		fq_log(obj->l, FQ_LOG_ERROR, "Your buffer size is short. available size is %d.",
			(obj->h_obj->h->msglen-FQ_HEADER_SIZE));
		goto unlock_return;
	}

	/* 20160407 */
	diff = obj->h_obj->h->en_sum - obj->h_obj->h->de_sum;
	if( diff == 0 ) {
		ret = DQ_EMPTY_RETURN_VALUE ;
		goto unlock_return;
	}
	if( diff < 0 ) {
		fq_log(obj->l, FQ_LOG_EMERG, "Fatal error. Automatic recovery.");
		obj->h_obj->h->de_sum = obj->h_obj->h->en_sum;
		ret = DQ_EMPTY_RETURN_VALUE ;

		save_Q_fatal_error(obj->path, obj->qname, "diff is minus value. Automatic recovery.", __FILE__, __LINE__);
		goto unlock_return;
	}

#if 0
	if( obj->h_obj->h->XA_ing_flag == 1 ) {
		time_t	current;
		current = time(0);

		if( (obj->h_obj->h->XA_lock_time + 60) < current ) {
			fq_log(obj->l, FQ_LOG_ERROR, "locking time is so long time.I ignore it.");
			obj->h_obj->h->XA_lock_time = time(0);
		}
		else {
			obj->h_obj->h->XA_ing_flag = 0; /* Add: 2016/01/16  */
			// obj->h_obj->de_flock->on_funlock(obj->h_obj->de_flock);
			fq_critical_section(l, obj, L_DE_ACTION, FQ_UNLOCK_ACTION);

			usleep(1000);
			goto restart;
		}
	}
#endif

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

	if( data_len > dstlen-1) { /* overflow */
		int rc_unlink;
		char	big_Q_filename[256];

		fq_log(l, FQ_LOG_ERROR, "Your buffer size is less than size of data(%d)-overflow. In this case, We return EMPTY to skip this data automatically. We delete this file.", data_len);
		obj->h_obj->h->de_sum++; /* error data is skipped. */
		// ret = -9999;

		sprintf(big_Q_filename, "%s", (char *)((off_t)obj->de_p + add_offset + FQ_HEADER_SIZE) );
		rc_unlink = unlink(big_Q_filename);
		if( rc_unlink == 0) {
			obj->h_obj->h->current_big_files--; /* load를 성공했을 때에만 감소시킨다 */
		}
		else {
			fq_log(obj->l, FQ_LOG_ERROR, "fatal: unlink(%s) failed.", big_Q_filename);
		}
		ret = DQ_EMPTY_RETURN_VALUE ;
		goto unlock_return;
	}

	if( data_len > (obj->h_obj->h->msglen-FQ_HEADER_SIZE) ) { /* big message processing */
		char	big_Q_filename[256];
		char	*big_buf=NULL, *msg=NULL;
		int		file_size;
		int		rc_unlink;

		usleep(10000);
		// msync((void *)obj->d_obj->d, obj->h_obj->h->mapping_len, MS_SYNC|MS_INVALIDATE); /*  MS_INVALIDATE */
		
		sprintf(big_Q_filename, "%s", (char *)((off_t)obj->de_p + add_offset + FQ_HEADER_SIZE) );

		if( strlen(big_Q_filename) < 2) {
			fq_log(obj->l, FQ_LOG_ERROR, "fatal: There is no big_filename.");

			*seq = obj->h_obj->h->de_sum;
			obj->h_obj->h->de_sum++;
			obj->h_obj->h->last_de_time = time(0);
			obj->h_obj->h->current_big_files--;

			ret = DQ_FATAL_SKIP_RETURN_VALUE; /* for continue, You must skip this data. 2013/10/30 */

			goto unlock_return;
		}

		fq_log(l, FQ_LOG_TRACE, "'%s' will be load from '%s' to buffer.", big_Q_filename, obj->path);

		if( load_big_Q_file_2_buf(l, big_Q_filename , &big_buf, &file_size,  &msg) < 0) {
			fq_log(obj->l, FQ_LOG_ERROR, "fatal: load_file_buf(%s) error. errmsg=[%s]", big_Q_filename,  msg);

			*seq = obj->h_obj->h->de_sum;
			obj->h_obj->h->de_sum++;
			obj->h_obj->h->last_de_time = time(0);
			obj->h_obj->h->current_big_files--;

			ret = DQ_FATAL_SKIP_RETURN_VALUE; /* for continue, You must skip this data. 2013/10/30 */

			goto unlock_return;
		}
		else {
			memcpy(dst, big_buf, file_size);
			SAFE_FREE(big_buf);
			SAFE_FREE(msg);

			rc_unlink = unlink(big_Q_filename);
			if( rc_unlink == 0) {
				obj->h_obj->h->current_big_files--; /* load를 성공했을 때에만 감소시킨다 */
			}
			else {
				fq_log(obj->l, FQ_LOG_ERROR, "fatal: unlink(%s) failed.", big_Q_filename);
			}
		}
	}
	else { /* normal messages */
		dst[0] = 0x00;
		int try_count = 0;

		for( try_count=0; try_count < 100; try_count++) {
			memcpy( dst, (void *)((off_t)obj->de_p + add_offset + FQ_HEADER_SIZE), data_len );
			if( dst[0] == 0x00 ) {
				usleep(1000);
				continue;
			}	
			else break;
		}
		fq_log( l, FQ_LOG_DEBUG, "Read try_count = [%d]\n", try_count);

#ifdef AFTER_DEQ_DELETE
		memset( (void *)((off_t)obj->de_p + add_offset + FQ_HEADER_SIZE), 0x00, data_len); /* 1.0.6  After deQ, delete data */
#endif
	}

	*seq = obj->h_obj->h->de_sum;
	obj->h_obj->h->de_sum++;
	obj->h_obj->h->last_de_time = time(0);

	ret = data_len;

unlock_return:
	// obj->h_obj->de_flock->on_funlock(obj->h_obj->de_flock);
	fq_critical_section(l, obj, L_DE_ACTION, FQ_UNLOCK_ACTION);


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

	if( obj->evt_obj ) {
		obj->evt_obj->on_en_eventlog(l, obj->uq_obj, EVENT_FQ_DE, obj->path, obj->qname, ((ret>=0)?TRUE:FALSE), getpid(), *seq, total_micro_sec );
	}

	FQ_TRACE_EXIT(l);
	return(ret);
}
static int 
on_de_bundle_array(fq_logger_t *l, fqueue_obj_t *obj, int array_cnt, int MaxDataLen, unsigned char *array[], long *run_time)
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
	int		array_index;

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
		fq_log(l, FQ_LOG_ERROR, "array buffer is null.");
		FQ_TRACE_EXIT(l);
		return(-1);
	}

	if( (MaxDataLen <= 0) || (MaxDataLen > 2048000000) ) {
		fq_log(l, FQ_LOG_ERROR, "Max data length is zero(0) or minus or so big. Please check your buffer. len=[%ld]", MaxDataLen);
		FQ_TRACE_EXIT(l);
		return(-1);
	}

	if( array_cnt > obj->h_obj->h->max_rec_cnt ) {
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

	// obj->h_obj->de_flock->on_flock(obj->h_obj->de_flock);
	fq_critical_section(l, obj, L_DE_ACTION, FQ_LOCK_ACTION);


	for(array_index=0; array_index<array_cnt; array_index++) {

		if( obj->stop_signal == 1) {
			fq_log(l, FQ_LOG_ERROR, "Administrator sent stopping signal.");
			// obj->h_obj->de_flock->on_funlock(obj->h_obj->de_flock);
			fq_critical_section(l, obj, L_DE_ACTION, FQ_UNLOCK_ACTION);

			FQ_TRACE_EXIT(l);
			return(MANAGER_STOP_RETURN_VALUE);
		} 

		if( obj->h_obj->h->status != QUEUE_ENABLE ) {
			fq_log(l, FQ_LOG_ERROR, "This queue is disabled by Admin.");
			// obj->h_obj->de_flock->on_funlock(obj->h_obj->de_flock);
			fq_critical_section(l, obj, L_DE_ACTION, FQ_UNLOCK_ACTION);
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
			char	big_Q_filename[256];
			char	*big_buf=NULL, *msg=NULL;
			int		file_size;
			int		rc_unlink;

			usleep(10000);
			// msync((void *)obj->d_obj->d, obj->h_obj->h->mapping_len, MS_SYNC|MS_INVALIDATE); /*  MS_INVALIDATE */
			
			sprintf(big_Q_filename, "%s", (char *)((off_t)obj->de_p + add_offset + FQ_HEADER_SIZE) );

			if( strlen(big_Q_filename) < 2) {
				fq_log(obj->l, FQ_LOG_ERROR, "fatal: There is no big_filename.");

				obj->h_obj->h->de_sum++;
				obj->h_obj->h->last_de_time = time(0);
				obj->h_obj->h->current_big_files--;

				ret = DQ_FATAL_SKIP_RETURN_VALUE; /* for continue, You must skip this data. 2013/10/30 */

				goto unlock_return;
			}

			fq_log(l, FQ_LOG_TRACE, "'%s' will be load from '%s' to buffer.", big_Q_filename, obj->path);

			if( load_big_Q_file_2_buf(l, big_Q_filename , &big_buf, &file_size,  &msg) < 0) {
				fq_log(obj->l, FQ_LOG_ERROR, "fatal: load_file_buf(%s) error. errmsg=[%s]", big_Q_filename,  msg);

				obj->h_obj->h->de_sum++;
				obj->h_obj->h->last_de_time = time(0);
				obj->h_obj->h->current_big_files--;

				ret = DQ_FATAL_SKIP_RETURN_VALUE; /* for continue, You must skip this data. 2013/10/30 */

				goto unlock_return;
			}
			else {
				array[array_index] = calloc(MaxDataLen+1, sizeof(unsigned char));
				memcpy( array[array_index], big_buf, MaxDataLen-1);
				SAFE_FREE(big_buf);
				SAFE_FREE(msg);

				rc_unlink = unlink(big_Q_filename);
				if( rc_unlink == 0) {
					obj->h_obj->h->current_big_files--; /* load를 성공했을 때에만 감소시킨다 */
				}
				else {
					fq_log(obj->l, FQ_LOG_ERROR, "fatal: unlink(%s) failed.", big_Q_filename);
				}
			}
		}
		else { /* normal messages */
			array[array_index] = calloc(MaxDataLen+1, sizeof(unsigned char));
			memcpy( array[array_index], (void *)((off_t)obj->de_p + add_offset + FQ_HEADER_SIZE), data_len );
#ifdef AFTER_DEQ_DELETE
			memset( (void *)((off_t)obj->de_p + add_offset + FQ_HEADER_SIZE), 0x00, data_len); /* 1.0.6  After deQ, delete data */
#endif
		}
		obj->h_obj->h->de_sum++;
		obj->h_obj->h->last_de_time = time(0);
	} /* for end */

	ret = array_index;

unlock_return:
	// obj->h_obj->de_flock->on_funlock(obj->h_obj->de_flock);
	fq_critical_section(l, obj, L_DE_ACTION, FQ_UNLOCK_ACTION);

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

	// obj->h_obj->de_flock->on_flock(obj->h_obj->de_flock);
	fq_critical_section(l, obj, L_DE_ACTION, FQ_LOCK_ACTION);


	for(array_index=0; array_index<fqdata_cnt; array_index++) {

		if( obj->stop_signal == 1) {
			fq_log(l, FQ_LOG_ERROR, "Administrator sent stopping signal.");
			// obj->h_obj->de_flock->on_funlock(obj->h_obj->de_flock);
			fq_critical_section(l, obj, L_DE_ACTION, FQ_UNLOCK_ACTION);

			FQ_TRACE_EXIT(l);
			return(MANAGER_STOP_RETURN_VALUE);
		} 

		if( obj->h_obj->h->status != QUEUE_ENABLE ) {
			fq_log(l, FQ_LOG_ERROR, "This queue is disabled by Admin.");
			// obj->h_obj->de_flock->on_funlock(obj->h_obj->de_flock);
			fq_critical_section(l, obj, L_DE_ACTION, FQ_UNLOCK_ACTION);
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
			char	big_Q_filename[256];
			char	*big_buf=NULL, *msg=NULL;
			int		file_size;
			int		rc_unlink;

			usleep(10000);
			// msync((void *)obj->d_obj->d, obj->h_obj->h->mapping_len, MS_SYNC|MS_INVALIDATE); /*  MS_INVALIDATE */
			
			sprintf(big_Q_filename, "%s", (char *)((off_t)obj->de_p + add_offset + FQ_HEADER_SIZE) );

			if( strlen(big_Q_filename) < 2) {
				fq_log(obj->l, FQ_LOG_ERROR, "fatal: There is no big_filename.");

				obj->h_obj->h->de_sum++;
				obj->h_obj->h->last_de_time = time(0);
				obj->h_obj->h->current_big_files--;

				ret = DQ_FATAL_SKIP_RETURN_VALUE; /* for continue, You must skip this data. 2013/10/30 */

				goto unlock_return;
			}

			fq_log(l, FQ_LOG_TRACE, "'%s' will be load from '%s' to buffer.", big_Q_filename, obj->path);

			if( load_big_Q_file_2_buf(l, big_Q_filename , &big_buf, &file_size,  &msg) < 0) {
				fq_log(obj->l, FQ_LOG_ERROR, "fatal: load_file_buf(%s) error. errmsg=[%s]", big_Q_filename,  msg);

				obj->h_obj->h->de_sum++;
				obj->h_obj->h->last_de_time = time(0);
				obj->h_obj->h->current_big_files--;

				ret = DQ_FATAL_SKIP_RETURN_VALUE; /* for continue, You must skip this data. 2013/10/30 */

				goto unlock_return;
			}
			else {
				array[array_index].data = calloc(MaxDataLen+1, sizeof(unsigned char));
				memcpy( array[array_index].data, big_buf, MaxDataLen-1); // truncated
				array[array_index].len = file_size;
				array[array_index].seq = obj->h_obj->h->de_sum;
				SAFE_FREE(big_buf);
				SAFE_FREE(msg);

				rc_unlink = unlink(big_Q_filename);
				if( rc_unlink == 0) {
					obj->h_obj->h->current_big_files--; /* load를 성공했을 때에만 감소시킨다 */
				}
				else {
					fq_log(obj->l, FQ_LOG_ERROR, "fatal: unlink(%s) failed.", big_Q_filename);
				}
			}
		}
		else { /* normal messages */
			array[array_index].data = calloc(MaxDataLen+1, sizeof(unsigned char));
			memcpy( array[array_index].data, (void *)((off_t)obj->de_p + add_offset + FQ_HEADER_SIZE), data_len );
			array[array_index].len = data_len;
			array[array_index].seq = obj->h_obj->h->de_sum;
#ifdef AFTER_DEQ_DELETE
			memset( (void *)((off_t)obj->de_p + add_offset + FQ_HEADER_SIZE), 0x00, data_len); /* 1.0.6  After deQ, delete data */
			fq_log(l, FQ_LOG_TRACE, "here-5");
#endif
		}
		obj->h_obj->h->de_sum++;
		obj->h_obj->h->last_de_time = time(0);
	} /* for end */

	ret = array_index;

unlock_return:
	// obj->h_obj->de_flock->on_funlock(obj->h_obj->de_flock);
	fq_critical_section(l, obj, L_DE_ACTION, FQ_UNLOCK_ACTION);


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

/*
** We support multi-user XA mode from version fq-13.0.x.x
*/
static int 
on_de_XA(fq_logger_t *l, fqueue_obj_t *obj, unsigned char *dst, int dstlen, long *seq, long *run_time, char *unlink_filename)
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
	stopwatch_micro_t p;
    long sec=0L, usec=0L, total_micro_sec=0L;
	char	XA_backup_filename[256];
	int diff;

	FQ_TRACE_ENTER(l);

#ifdef _USE_HEARTBEAT
	/* by heartbeat */
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

	if( obj->stopwatch_on_off == STOPWATCH_ON ) {
		on_stopwatch_micro(&p);
	}

restart:

	// obj->h_obj->de_flock->on_flock(obj->h_obj->de_flock);
	fq_critical_section(l, obj, L_DE_ACTION, FQ_LOCK_ACTION);


	if( obj->stop_signal == 1) {
		fq_log(l, FQ_LOG_ERROR, "Administrator sent stopping signal.");
		// obj->h_obj->de_flock->on_funlock(obj->h_obj->de_flock);
		fq_critical_section(l, obj, L_DE_ACTION, FQ_UNLOCK_ACTION);

		FQ_TRACE_EXIT(l);
		return(MANAGER_STOP_RETURN_VALUE);
	} 

	if( obj->h_obj->h->status != QUEUE_ENABLE ) {
		fq_log(l, FQ_LOG_ERROR, "This queue is disabled by Admin.");
		// obj->h_obj->de_flock->on_funlock(obj->h_obj->de_flock);
		fq_critical_section(l, obj, L_DE_ACTION, FQ_UNLOCK_ACTION);
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

	if( obj->h_obj->h->QOS != 0 ) {
		usleep(obj->h_obj->h->QOS); /* min is 1000 */
	}

	if( dstlen < (obj->h_obj->h->msglen-FQ_HEADER_SIZE) ) {
		fq_log(obj->l, FQ_LOG_ERROR, "Your buffer size is short. available size is %d.",
			(obj->h_obj->h->msglen-FQ_HEADER_SIZE));
		goto unlock_return;
	}

	/* 20160407 */
	diff = obj->h_obj->h->en_sum - obj->h_obj->h->de_sum;
	if( diff == 0 ) {
		ret = DQ_EMPTY_RETURN_VALUE ;
		goto unlock_return;
	}
	if( diff < 0 ) {
		fq_log(obj->l, FQ_LOG_EMERG, "Fatal error. Automatic recovery.");
		obj->h_obj->h->de_sum = obj->h_obj->h->en_sum;
		ret = DQ_EMPTY_RETURN_VALUE ;

		save_Q_fatal_error(obj->path, obj->qname, "diff is minus value. Automatic recovery.", __FILE__, __LINE__);
		goto unlock_return;
	}

#if 0
	if( obj->h_obj->h->XA_ing_flag == 1 ) {
		time_t	current;
		current = time(0);

		if( (obj->h_obj->h->XA_lock_time + 60) < current ) {
			fq_log(obj->l, FQ_LOG_ERROR, "locking time is so long time.I ignore it.");
		}
		else {
			obj->h_obj->h->XA_ing_flag = 0; /* Add: 2016/01/16  */
			// obj->h_obj->de_flock->on_funlock(obj->h_obj->de_flock);
			fq_critical_section(l, obj, L_DE_ACTION, FQ_UNLOCK_ACTION);

			usleep(1000);
			goto restart;
		}
	}
#endif
	
	obj->h_obj->h->XA_ing_flag = 1;
	obj->h_obj->h->XA_lock_time = time(0);

	sprintf(XA_backup_filename, "%s/.%s_XA.bak", obj->path, obj->qname);
	if( is_file( XA_backup_filename ) ) {
		char	*XA_buf=NULL, *msg=NULL;
		int		file_size;

		fq_log(l, FQ_LOG_DEBUG, "un-committed file exist filename=[%s].", XA_backup_filename);
		if( load_big_Q_file_2_buf(l, XA_backup_filename , &XA_buf, &file_size,  &msg) == 0 ) { /* success */
			memcpy(dst, XA_buf, file_size);
			SAFE_FREE(XA_buf);
			SAFE_FREE(msg);
		}

		if( file_size > 0 ) { /* success */
			fq_log(l, FQ_LOG_DEBUG, "un-committed file return seq = [%ld]", obj->h_obj->h->de_sum);
			*seq = obj->h_obj->h->de_sum;
			// obj->h_obj->h->de_sum++;
			obj->h_obj->h->last_de_time = time(0);

			// Don't unlock in here.
			// 2016/09/09:  obj->h_obj->de_flock->on_funlock(obj->h_obj->de_flock);
			FQ_TRACE_EXIT(l);
			return(file_size);
		}
		else {
			fq_log(l, FQ_LOG_ERROR, "file size is zero. We delete it and put dummy data.");

			unlink(XA_backup_filename);
			*seq = obj->h_obj->h->de_sum;
			obj->h_obj->h->last_de_time = time(0);
			memcpy(dst, "dummy data", 10);
			FQ_TRACE_EXIT(l);	
			return(10);
		}
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
		if( p_offset_from != obj->h_obj->h->de_available_from_offset) {

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

	if( data_len > dstlen-1) { /* overflow */
		fq_log(l, FQ_LOG_ERROR, "Your buffer size is less than size of data(%d)-overflow. In this case, We return EMPTY to skip this data automatically", data_len);
		obj->h_obj->h->de_sum++; /* error data is skipped. */
		// ret = -9999;
		ret = DQ_EMPTY_RETURN_VALUE ;
		goto unlock_return;
	}

	if( data_len > (obj->h_obj->h->msglen-FQ_HEADER_SIZE) ) { /* big message processing */
		char	big_Q_filename[256];
		char	*big_buf=NULL, *msg=NULL;
		int		file_size;

		usleep(10000);

		// msync((void *)obj->d_obj->d, obj->h_obj->h->mapping_len, MS_SYNC|MS_INVALIDATE); /*  MS_INVALIDATE */

		sprintf(big_Q_filename, "%s", (char *)((off_t)obj->de_p + add_offset + FQ_HEADER_SIZE) );

		if( strlen(big_Q_filename) < 2) {
			fq_log(obj->l, FQ_LOG_ERROR, "fatal: There is no big_filename.");

			*seq = obj->h_obj->h->de_sum;
			obj->h_obj->h->de_sum++;
			obj->h_obj->h->last_de_time = time(0);
			obj->h_obj->h->current_big_files--;

			ret = DQ_FATAL_SKIP_RETURN_VALUE; /* for continue, You must skip this data. 2013/10/30 */

			goto unlock_return;
		}

		sprintf(unlink_filename, "%s", big_Q_filename);

		/* This logic has a bug, It needs retry routine */
		if( load_big_Q_file_2_buf(l, big_Q_filename , &big_buf, &file_size,  &msg) < 0) {
			fq_log(obj->l, FQ_LOG_ERROR, "fatal: load_file_buf(%s) error. errmsg=[%s]", big_Q_filename,  msg);

			*seq = obj->h_obj->h->de_sum;
			obj->h_obj->h->de_sum++;
			obj->h_obj->h->last_de_time = time(0);
			obj->h_obj->h->current_big_files--;

			ret = DQ_FATAL_SKIP_RETURN_VALUE; /* for continue, You must skip this data. 2013/10/30 */

			goto unlock_return;
		}

		dst[0] = 0x00;
		int try_count = 0;

		for( try_count=0; try_count < 100; try_count++) {
			memcpy(dst, big_buf, file_size);
			if( dst[0] == 0x00 ) {
				usleep(1000);
				continue;
			}	
		}
		fq_log( l, FQ_LOG_DEBUG, "Read try_count = [%d]\n", try_count);

		SAFE_FREE(big_buf);
		SAFE_FREE(msg);

		/* in XA_mode, commit() do to unlink. */
		/* unlink(big_Q_filename); */
	}
	else { /* normal messages */
		unlink_filename[0] = 0x00;

		dst[0] = 0x00;
		int try_count = 0;

		for( try_count=0; try_count < 100; try_count++) {
			memcpy( dst, (void *)((off_t)obj->de_p + add_offset + FQ_HEADER_SIZE), data_len );
			if( dst[0] == 0x00 ) {
				usleep(1000);
				continue;
			}	
			else  break;

		}
		fq_log( l, FQ_LOG_DEBUG, "Read try_count = [%d]\n", try_count);
	}

	*seq = obj->h_obj->h->de_sum;
	
	fq_log(obj->l, FQ_LOG_DEBUG, "XA Data copy end. seq=[%d]", obj->h_obj->h->de_sum);


#if 0
	obj->h_obj->h->de_sum++; /* Do not increase de_sum, In the commit step, It will do. */
#endif

	ret = data_len;

	backup_for_XA(l, obj->path, obj->qname, dst, data_len);

	/* Don't unlock in here */
	// obj->h_obj->de_flock->on_funlock(obj->h_obj->de_flock);

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
			fq_log(l, FQ_LOG_EMERG, "on_de_XA() delay time : %ld micro second.", total_micro_sec);
		}
	}

	if ( obj->evt_obj ) {
		obj->evt_obj->on_en_eventlog(l, obj->uq_obj, EVENT_FQ_DE_XA, obj->path, obj->qname, TRUE, getpid(), *seq, total_micro_sec );
	}

	FQ_TRACE_EXIT(l);
	return(ret);

unlock_return:
	// obj->h_obj->de_flock->on_funlock(obj->h_obj->de_flock);
	fq_critical_section(l, obj, L_DE_ACTION, FQ_UNLOCK_ACTION);


	if( obj->evt_obj ) {
		obj->evt_obj->on_en_eventlog(l, obj->uq_obj, EVENT_FQ_DE_XA, obj->path, obj->qname, FALSE, getpid(), 0, 0L);
	}
	FQ_TRACE_EXIT(l);
	return(ret);
}

static int 
on_commit_XA(fq_logger_t *l, fqueue_obj_t *obj, long seq, long *run_time, char *unlink_filename)
{
	int ret=-1;
	stopwatch_micro_t p;
    long sec=0L, usec=0L, total_micro_sec=0L;

	FQ_TRACE_ENTER(l);

	if( obj->stopwatch_on_off == STOPWATCH_ON ) {
		on_stopwatch_micro(&p);
	}

	if( obj->stop_signal == 1) {
		fq_log(l, FQ_LOG_ERROR, "Administrator sent stopping signal.");
		// obj->h_obj->de_flock->on_funlock(obj->h_obj->de_flock);
		fq_critical_section(l, obj, L_DE_ACTION, FQ_UNLOCK_ACTION);

		FQ_TRACE_EXIT(l);
		return(MANAGER_STOP_RETURN_VALUE);
	} 

	if(seq != obj->h_obj->h->de_sum) {
		fq_log(obj->l, FQ_LOG_ERROR, "illegal commit request. seq=[%ld] de_sum=[%d]", seq, obj->h_obj->h->de_sum);
		// obj->h_obj->de_flock->on_funlock(obj->h_obj->de_flock);
		fq_critical_section(l, obj, L_DE_ACTION, FQ_UNLOCK_ACTION);
		FQ_TRACE_EXIT(l);
		return(-1);
	}

	if( HASVALUE(unlink_filename) ) { /* big file 경우에만 unlink와 current_big_file--를 수행한다 */
		int rc;

		fq_log(obj->l, FQ_LOG_DEBUG, "unlink file for big message is [%s].", unlink_filename);
		rc = unlink(unlink_filename);

		if(rc >= 0) /* 실제로 unlink가 성공했을 때에만 감소 시킨다 */ 
			obj->h_obj->h->current_big_files--;
	}
#ifdef AFTER_DEQ_DELETE
	else { /* 2013/08/28 : After commit, delete data */
		register off_t   record_no;
		register off_t   p_offset_from; 
		/*
		register off_t		p_offset_to;
		*/
		register off_t   mmap_offset;
		register off_t   add_offset;
		register long    nth_mapping;


		record_no = seq % obj->h_obj->h->max_rec_cnt;
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
				goto skip_delete_routine;
			}
			obj->h_obj->h->de_available_from_offset = mmap_offset;
			obj->h_obj->h->de_available_to_offset = mmap_offset + obj->h_obj->h->mapping_len;
		}
		else {
			if( p_offset_from != obj->h_obj->h->de_available_from_offset) {

#ifdef MSYNC_WHENEVER_UNMAP
				/* 2016/01/07 : for using shared disk(SAN) in ShinhanBank */
				msync((void *)obj->en_p, obj->h_obj->h->mapping_len, MS_SYNC|MS_INVALIDATE); /*  MS_INVALIDATE */
#endif

				if( fq_munmap(l, (void *)obj->de_p, obj->h_obj->h->mapping_len) < 0 ) {
					fq_log(obj->l, FQ_LOG_ERROR, "fq_munmap() error. reason=[%s].", strerror(errno));
					obj->de_p = NULL;
					goto skip_delete_routine;
				}
				if( (obj->de_p = fq_mmapping(l, obj->d_obj->fd, obj->h_obj->h->mapping_len, mmap_offset)) == NULL ) {
					fq_log(l, FQ_LOG_ERROR, "re-fq_mmapping(body) error.");
					goto skip_delete_routine;
				}
				obj->h_obj->h->de_available_from_offset = mmap_offset;
				obj->h_obj->h->de_available_to_offset = mmap_offset + obj->h_obj->h->mapping_len;
			}
		}


		add_offset = p_offset_from - obj->h_obj->h->de_available_from_offset;
		memset( (void *)((off_t)obj->de_p + add_offset + FQ_HEADER_SIZE), 0x00,  (obj->h_obj->h->msglen-FQ_HEADER_SIZE) ); /* 1.0.6 : After deQ, delete data */
	}
skip_delete_routine:
#endif

	obj->h_obj->h->de_sum++;
	obj->h_obj->h->last_de_time = time(0);

	ret = 0;

	/* delete backup_XA file */
	delete_for_XA(l, obj->path, obj->qname);

	obj->h_obj->h->XA_ing_flag = 0;
	obj->h_obj->h->XA_lock_time = 0L;

	// obj->h_obj->de_flock->on_funlock(obj->h_obj->de_flock);
	fq_critical_section(l, obj, L_DE_ACTION, FQ_UNLOCK_ACTION);

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

	if( obj->stopwatch_on_off == STOPWATCH_ON ) {
		if( total_micro_sec > DELAY_MICRO_SECOND) {
			fq_log(l, FQ_LOG_EMERG, "on_commit_XA() delay time : %ld micro second.", total_micro_sec);
		}
	}

	if( obj->monID && obj->mon_obj) {
		obj->mon_obj->on_touch(l, obj->monID, FQ_DE_ACTION, obj->mon_obj);
	}

	if( obj->evt_obj ) {
		obj->evt_obj->on_en_eventlog(l, obj->uq_obj, EVENT_FQ_COMMIT, obj->path, obj->qname, ((ret>=0)?TRUE:FALSE), getpid(), seq, total_micro_sec );
	}

	FQ_TRACE_EXIT(l);
	return(ret);
}

/* Description: rollback */
static int 
on_cancel_XA(fq_logger_t *l, fqueue_obj_t *obj, long seq, long *run_time)
{
	int ret=-1;
	stopwatch_micro_t p;
    long sec=0L, usec=0L, total_micro_sec=0L;

	FQ_TRACE_ENTER(l);

	on_stopwatch_micro(&p);

	if( obj->stop_signal == 1) {
		fq_log(l, FQ_LOG_ERROR, "Administrator sent stopping signal.");
		// obj->h_obj->de_flock->on_funlock(obj->h_obj->de_flock);
		fq_critical_section(l, obj, L_DE_ACTION, FQ_UNLOCK_ACTION);

		FQ_TRACE_EXIT(l);
		return(MANAGER_STOP_RETURN_VALUE);
	} 

	// obj->h_obj->de_flock->on_funlock(obj->h_obj->de_flock);
	fq_critical_section(l, obj, L_DE_ACTION, FQ_UNLOCK_ACTION);

	off_stopwatch_micro(&p);
    get_stopwatch_micro(&p, &sec, &usec);
    total_micro_sec = sec * 1000000;
    total_micro_sec =  total_micro_sec + usec;

	*run_time = total_micro_sec;

    if( total_micro_sec > DELAY_MICRO_SECOND) {
        fq_log(l, FQ_LOG_EMERG, "on_commit_XA() delay time : %ld micro second.", total_micro_sec);
    }

	if( obj->evt_obj ) {
		obj->evt_obj->on_en_eventlog(l, obj->uq_obj, EVENT_FQ_CANCEL, obj->path, obj->qname, ((ret>=0)?TRUE:FALSE), getpid(), seq, total_micro_sec );
	}

	ret = 0;
	FQ_TRACE_EXIT(l);
	
	return(ret);
}

int 
on_disable(fq_logger_t *l, fqueue_obj_t *obj)
{

	FQ_TRACE_ENTER(l);

	/* For thread safe */
	/* 더 이상 신규 청을 못하도록 한다.*/
	// obj->h_obj->en_flock->on_flock(obj->h_obj->en_flock);
	// obj->h_obj->de_flock->on_flock(obj->h_obj->de_flock);


	fq_critical_section(l, obj, L_EN_ACTION, FQ_LOCK_ACTION);
	fq_critical_section(l, obj, L_DE_ACTION, FQ_LOCK_ACTION);


	obj->stop_signal = 1;
	obj->h_obj->h->status = QUEUE_ALL_DISABLE;

	// obj->h_obj->en_flock->on_funlock(obj->h_obj->en_flock);
	// obj->h_obj->de_flock->on_funlock(obj->h_obj->de_flock);
	fq_critical_section(l, obj, L_EN_ACTION, FQ_UNLOCK_ACTION);
	fq_critical_section(l, obj, L_DE_ACTION, FQ_UNLOCK_ACTION);


	FQ_TRACE_EXIT(l);
	return(TRUE);
}

int 
on_enable(fq_logger_t *l, fqueue_obj_t *obj)
{

	FQ_TRACE_ENTER(l);

	/* For thread safe */
	/* 더 이상 신규 청을 못하도록 한다.*/
	// obj->h_obj->en_flock->on_flock(obj->h_obj->en_flock);
	// obj->h_obj->de_flock->on_flock(obj->h_obj->de_flock);

	fq_critical_section(l, obj, L_EN_ACTION, FQ_LOCK_ACTION);
	fq_critical_section(l, obj, L_DE_ACTION, FQ_LOCK_ACTION);



	obj->h_obj->h->status = QUEUE_ENABLE;


	// obj->h_obj->en_flock->on_funlock(obj->h_obj->en_flock);
	// obj->h_obj->de_flock->on_funlock(obj->h_obj->de_flock);

	fq_critical_section(l, obj, L_EN_ACTION, FQ_UNLOCK_ACTION);
	fq_critical_section(l, obj, L_DE_ACTION, FQ_UNLOCK_ACTION);


	FQ_TRACE_EXIT(l);
	return(TRUE);
}

int 
on_reset(fq_logger_t *l, fqueue_obj_t *obj)
{
	char	cmd[256];
	int 	rc;

	FQ_TRACE_ENTER(l);

	/* For thread safe */
	/* 더 이상 신규 청을 못하도록 한다.*/
	// obj->h_obj->en_flock->on_flock(obj->h_obj->en_flock);
	// obj->h_obj->de_flock->on_flock(obj->h_obj->de_flock);

	fq_critical_section(l, obj, L_EN_ACTION, FQ_LOCK_ACTION);
	fq_critical_section(l, obj, L_DE_ACTION, FQ_LOCK_ACTION);


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
	fq_log(l, FQ_LOG_DEBUG, "Deleting BIG data directiories started.('%s', '%s')", obj->path, obj->qname);
	sprintf(cmd, "rm -rf %s/BIG_%s", obj->path, obj->qname);
	fq_log(l, FQ_LOG_DEBUG, "system call -> shell command: '%s'", cmd);
	rc = system(cmd);
	CHECK(rc != -1);

	fq_log(l, FQ_LOG_DEBUG, "Deleting BIG data directiories finished.");

	// obj->h_obj->en_flock->on_funlock(obj->h_obj->en_flock);
	// obj->h_obj->de_flock->on_funlock(obj->h_obj->de_flock);

	fq_critical_section(l, obj, L_EN_ACTION, FQ_UNLOCK_ACTION);
	fq_critical_section(l, obj, L_DE_ACTION, FQ_UNLOCK_ACTION);


	FQ_TRACE_EXIT(l);
	return(TRUE);
}
int 
on_force_skip(fq_logger_t *l, fqueue_obj_t *obj)
{

	FQ_TRACE_ENTER(l);

		
	fq_log(l, FQ_LOG_DEBUG, "Force de-queueing..by manager ...start.");

	if( obj->h_obj->h->de_sum == obj->h_obj->h->en_sum ) {
		fq_log(l, FQ_LOG_DEBUG, "There is no data for skipping.");
		FQ_TRACE_EXIT(l);
		return(FALSE);
	}

	/* For thread safe */
	/* 더 이상 신규 청을 못하도록 한다.*/
	// obj->h_obj->de_flock->on_flock(obj->h_obj->de_flock);
	fq_critical_section(l, obj, L_DE_ACTION, FQ_LOCK_ACTION);

	obj->h_obj->h->status = QUEUE_ALL_DISABLE;

	obj->h_obj->h->de_sum++;

	obj->h_obj->h->status = QUEUE_ENABLE;

	// obj->h_obj->de_flock->on_funlock(obj->h_obj->de_flock);
	fq_critical_section(l, obj, L_DE_ACTION, FQ_UNLOCK_ACTION);

	fq_log(l, FQ_LOG_DEBUG, "Force de-queueing..by manager ...end.");

	FQ_TRACE_EXIT(l);
	return(TRUE);
}

int 
on_diag(fq_logger_t *l, fqueue_obj_t *obj)
{

	FQ_TRACE_ENTER(l);

	/* For thread safe */
	/* 더 이상 신규 청을 못하도록 한다.*/

	step_print("- File Locking start. ", "OK");
	// obj->h_obj->de_flock->on_flock(obj->h_obj->de_flock);
	fq_critical_section(l, obj, L_DE_ACTION, FQ_LOCK_ACTION);

	step_print("- File Locking end. ", "OK");

	obj->h_obj->h->last_en_time = time(0);
	obj->h_obj->h->last_de_time = time(0);

	step_print("- File Unlocking start. ", "OK");
	// obj->h_obj->de_flock->on_funlock(obj->h_obj->de_flock);
	fq_critical_section(l, obj, L_DE_ACTION, FQ_UNLOCK_ACTION);

	step_print("- File Unlocking end. ", "OK");

	FQ_TRACE_EXIT(l);
	return(TRUE);
}

static int 
close_headfile_obj(fq_logger_t *l,  headfile_obj_t **obj)
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
	if( (*obj)->en_shmlock ) {
		close_shmlock_obj(l, &(*obj)->en_shmlock);
		(*obj)->en_shmlock = NULL;
	}
	if( (*obj)->de_flock ) {
		close_shmlock_obj(l, &(*obj)->de_shmlock);
		(*obj)->de_shmlock = NULL;
	}

	SAFE_FREE(*obj);

	FQ_TRACE_EXIT(l);
	return(TRUE);
}

static int 
close_datafile_obj(fq_logger_t *l,  datafile_obj_t **obj, int mmapping_len)
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
UnlinkFileQueue(fq_logger_t *l, char *path, char *qname)
{
	int rc;
	char cmd[256];
	char en_flock_pathfile[256];
	char de_flock_pathfile[256];
	char back_pathfile[256];

	FQ_TRACE_ENTER(l);

	rc = unlink_headfile(l, path, qname);
	if(rc == FALSE) goto return_FALSE;

	step_print("- Delete header file ", "OK");

	

	rc = unlink_datafile(l, path, qname);
	if(rc == FALSE) goto return_FALSE;
	step_print("- Delete data file ", "OK");
	
	sprintf(back_pathfile, "%s/%s.backup", path, qname);
	if( is_file(back_pathfile ) ) {
		unlink( back_pathfile);
	}

	sprintf(en_flock_pathfile, "%s/.%s.en_flock", path, qname);
	rc = unlink_flock( l, en_flock_pathfile);
	if(rc == FALSE) goto return_FALSE;

	sprintf(de_flock_pathfile, "%s/.%s.de_flock", path, qname);
	rc = unlink_flock( l, de_flock_pathfile);
	if(rc == FALSE) goto return_FALSE;

	step_print("- Delete flock file(en/de) ", "OK");

	char shm_en_name[512];
	char org_path[256];
	char sanitized_path[256];
	del_char( org_path, sanitized_path, '/');
	sprintf(shm_en_name, "%s-%s.en", sanitized_path, qname);
	unlink_shmlock_obj(l, shm_en_name);

	char shm_de_name[512];
	sprintf(shm_de_name, "%s-%s.de", sanitized_path, qname);
	unlink_shmlock_obj(l, shm_de_name);
	step_print("- Delete shared memory lock files(en/de) ", "OK");

	rc = unlink_unixQ(l, path, FQ_UNIXQ_KEY_CHAR);
	/* if(rc == FALSE) goto return_FALSE; */
	step_print("- Delete unixQ ", "OK");

	/* delete big files */
	sprintf(cmd, "rm -rf %s/BIG_%s", path, qname);
	rc = system(cmd);
	CHECK(rc != -1);

	step_print("- Delete BIG files ", "OK");

	char list_info_file[1024];
	sprintf(list_info_file, "%s/ListFQ.info", path);
	int delete_line;
	delete_line = find_a_line_from_file(l, list_info_file, qname);
	if( delete_line > 0 ) {
		int rc;
		rc = delete_a_line_from_file(l, list_info_file, delete_line);
		if( rc == 0 ) {
			step_print("- Delete a queue name in ListFQ.info ", "OK");
		}
		else {
			step_print("- Delete a queue name in ListFQ.info ", "NOK");
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
GenerateInfoFileByQueuy(fq_logger_t *l, char *path, char *qname)
{

	fq_info_t *t = NULL;

	FQ_TRACE_ENTER(l);

	if( generate_fq_info_file( l, path, qname, t) == false ) {
		FQ_TRACE_EXIT(l);
		return(FALSE);
	}

	FQ_TRACE_EXIT(l);
	return (TRUE);
}

int 
CreateFileQueue(fq_logger_t *l, char *path, char *qname)
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

	char 	info_master_use_flag[2];
	char	info_master_hostname[HOST_NAME_LEN];

	int		i_msglen;
	int		i_mmapping_num;
	int		i_multi_num;
	int		i_info_master_use_flag;

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
		fq_log(l, FQ_LOG_ERROR, "load_info_param() error.");
		goto return_FALSE;
	}

	get_fq_info(_fq_info, "QPATH", info_qpath);
	get_fq_info(_fq_info, "QNAME", info_qname);

	i_msglen = get_fq_info_i(_fq_info, "MSGLEN");
	if( !isPowerOfTwo(i_msglen) ) {
		fq_log(l, FQ_LOG_ERROR, "MSGLEN(%d) is not power of two.\n", i_msglen);
		goto return_FALSE;
	}
		
	i_mmapping_num = get_fq_info_i(_fq_info, "MMAPPING_NUM");
	i_multi_num = get_fq_info_i(_fq_info, "MULTI_NUM");
	i_info_master_use_flag = get_fq_info_i(_fq_info, "MASTER_USE_FLAG");

	fq_log(l, FQ_LOG_DEBUG, "i_msglen=[%d]", i_msglen);
	fq_log(l, FQ_LOG_DEBUG, "i_mmapping_num=[%d]", i_mmapping_num);
	fq_log(l, FQ_LOG_DEBUG, "i_multi_num=[%d]", i_multi_num);
	fq_log(l, FQ_LOG_DEBUG, "i_info_master_use_flag=[%d]", i_info_master_use_flag);

	get_fq_info(_fq_info, "DESC", info_desc);
	get_fq_info(_fq_info, "XA_MODE_ON_OFF", info_xa_mode_on_off);
	get_fq_info(_fq_info, "WAIT_MODE_ON_OFF", info_wait_mode_on_off);
	get_fq_info(_fq_info, "MASTER_HOSTNAME", info_master_hostname);

	if(bytes_of_size_t == 4) { /* 32 bits */
        unsigned long file_size;

        file_size = i_msglen * i_mmapping_num * i_multi_num;

        if( file_size > 2000000000 ) {
            fq_log(l, FQ_LOG_ERROR, "In the 32-bits machine, Cannot make a file more than 2Gbytes. yours requesting size of file is [%lu]", file_size);
            goto return_FALSE;
        }
    }

	fq_log(l, FQ_LOG_DEBUG, "success to get queue info."); 

	if( create_headerfile(l, info_qpath, info_qname, i_msglen, i_mmapping_num, i_multi_num, info_desc, info_xa_mode_on_off, info_wait_mode_on_off, i_info_master_use_flag, info_master_hostname) < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "create_headerfile() error.");
		goto return_FALSE;
	}

	if( open_headfile_obj(l, info_qpath, info_qname, &h_obj) == FALSE ) {
		fq_log(l, FQ_LOG_ERROR, "open_headfile_obj() error.");
		goto return_FALSE;
	}

	if( create_datafile(l, h_obj ) == FALSE ) {
		fq_log(l, FQ_LOG_ERROR, "create_datafile() error.");
		goto return_FALSE;
	}

	if( h_obj ) close_headfile_obj(l, &h_obj);
	if(_fq_info) free_fq_info(&_fq_info);
	
	en_eventlog(l, EVENT_FQ_CREATE, path, qname, TRUE, getpid());
	FQ_TRACE_EXIT(l);
	return(TRUE);

return_FALSE:
	if( h_obj ) close_headfile_obj(l, &h_obj);
	if(_fq_info) free_fq_info(&_fq_info);

	en_eventlog(l, EVENT_FQ_CREATE, path, qname, FALSE, getpid());
	FQ_TRACE_EXIT(l);
	return(FALSE);
}

int 
ExtendFileQueue(fq_logger_t *l, char *path, char *qname)
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
		step_print("- Access Test ", "NOK");
		fq_log(l, FQ_LOG_ERROR, "cannot find info_file[%s]", info_filename);
		goto return_FALSE;
	}
	step_print("- Access Test ", "OK");

	_fq_info = new_fq_info(info_filename);
	if(!_fq_info) {
		fq_log(l, FQ_LOG_ERROR, "new_fq_info() error.");
		goto return_FALSE;
	}

	if( load_info_param(l, _fq_info, info_filename) < 0 ) {
		step_print("- Get queue infomation ", "NOK");
		fq_log(l, FQ_LOG_ERROR, "load_param() error.");
		goto return_FALSE;
	}
	step_print("- Get queue infomation ", "OK");

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
			step_print("- Extension size checking ", "NOK");
            goto return_FALSE;
        }
    }
	step_print("- Extension size checking ", "OK");

	fq_log(l, FQ_LOG_DEBUG, "success to get queue info."); 

	if( extend_headerfile(l, info_qpath, info_qname, i_msglen, i_mmapping_num, i_multi_num, info_desc, info_xa_mode_on_off, info_wait_mode_on_off) < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "extend_headerfile() error.");
		step_print("- File Queue Header file Extension. ", "NOK");
		goto return_FALSE;
	}
	step_print("- File Queue Header file Extension. ", "OK");

	if( open_headfile_obj(l, info_qpath, info_qname, &h_obj) == FALSE ) {
		fq_log(l, FQ_LOG_ERROR, "open_headfile_obj() error.");
		step_print("- File Queue Header file Open checking. ", "NOK");
		goto return_FALSE;
	}
	step_print("- File Queue Header file Open checking. ", "OK");

	if( extend_datafile(l, h_obj ) == FALSE ) {
		fq_log(l, FQ_LOG_ERROR, "extend_datafile() error.");
		step_print("- File Queue Data file Extension. ", "NOK");
		goto return_FALSE;
	}
	step_print("- File Queue Data file Extension. ", "OK");

	if( h_obj ) close_headfile_obj(l, &h_obj);
	if(_fq_info) free_fq_info(&_fq_info);
	
	en_eventlog(l, EVENT_FQ_EXTEND, path, qname, TRUE, getpid());

	step_print("- File Queue Closing. ", "OK");
	FQ_TRACE_EXIT(l);
	return(TRUE);

return_FALSE:
	if( h_obj ) close_headfile_obj(l, &h_obj);
	if(_fq_info) free_fq_info(&_fq_info);

	en_eventlog(l, EVENT_FQ_EXTEND, path, qname, FALSE, getpid());
	FQ_TRACE_EXIT(l);
	return(FALSE);
}

int 
OpenFileQueue(fq_logger_t *l, char **argv, char *hostname, char *distname, char *monID,  char *path, char *qname, fqueue_obj_t **obj)
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
		char szHostName[HOST_NAME_LEN+1];

		rc->argv = argv;
		rc->path = strdup(path);
		rc->qname = strdup(qname);
		rc->eventlog_path = strdup(EVENT_LOG_PATH);
		rc->pid = getpid();
		rc->tid = pthread_self();
		rc->l = l;

		rc->stop_signal = 0;
		rc->stopwatch_on_off = STOPWATCH_ON;
		rc->en_p = NULL;
		rc->de_p = NULL;

#if 0	
		if( monID ) {
			rc->monID = strdup(monID);
			if( open_monitor_obj(l, &rc->mon_obj) == FALSE) {
				fq_log(l, FQ_LOG_ERROR, "open_monitor_obj() error.");
				goto return_FALSE;
			}
		}
		else {
			rc->monID = NULL;
			rc->mon_obj=NULL;
		}
#else
		rc->monID = NULL;
		rc->mon_obj = NULL;
#endif

#ifdef _USE_HEARTBEAT
		if( argv && hostname && monID && distname) {
			int my_index;

			rc->hostname = strdup(hostname);

			my_index = open_heartbeat_obj( l, hostname, distname, monID, HB_DB_PATH , HB_DB_NAME , HB_LOCK_FILENAME, &rc->hb_obj);
			if(  my_index < 0 ) {
				fq_log(l, FQ_LOG_ERROR, "open_heartbeat_obj('%s', '%s', '%s' ...) failed.", hostname, distname, monID);
				goto return_FALSE;
			}
			else {
				fq_log(l, FQ_LOG_DEBUG, "open_heartbeat_obj() success.");
			}
		}
		else {
			gethostname(szHostName, HOST_NAME_LEN);
			rc->hostname = strdup(szHostName);
			rc->hb_obj = NULL;
		}
#else
		gethostname(szHostName, HOST_NAME_LEN);
		rc->hostname = strdup(szHostName);

		rc->monID = NULL;
		rc->mon_obj = NULL;
		rc->hb_obj = NULL;
#endif

		/* header object */
		rc->h_obj = NULL;
		if( open_headfile_obj(l, path, qname, &rc->h_obj) == FALSE ) {
			fq_log(l, FQ_LOG_ERROR, "open_headfile_obj() error.");
			goto return_FALSE;
		}

		/* data object */
		rc->d_obj = NULL;
		if( open_datafile_obj(l, path, qname, &rc->d_obj) == FALSE ) {
			fq_log(l, FQ_LOG_ERROR, "open_datafile_obj() error.");
			goto return_FALSE;
		}

		/* backup object */
		rc->b_obj = NULL;
		if( open_backupfile_obj(l, path, qname, &rc->b_obj) == FALSE ) {
			fq_log(l, FQ_LOG_ERROR, "open_backupfile_obj() error.");
			goto return_FALSE;
		}

		rc->uq_obj = NULL;
		if( open_unixQ_obj(l, FQ_UNIXQ_KEY_PATH,  FQ_UNIXQ_KEY_CHAR, &rc->uq_obj) == FALSE ) {
			fq_log(l, FQ_LOG_ERROR, "open_unixQ_obj() error.");
			goto return_FALSE;
		}

		rc->evt_obj=NULL;

#ifdef _USE_EVENTLOG
		if(open_eventlog_obj( l, rc->eventlog_path, &rc->evt_obj) == FALSE) {
			fq_log(l, FQ_LOG_ERROR, "open_eventlog_obj() error.");
			goto return_FALSE;
		}
#else
		rc->evt_obj = NULL;
#endif

#ifdef _USE_EXTERNAL_ENV
		rc->ext_env_obj = NULL;
		if( open_external_env_obj(l, path, &rc->ext_env_obj)== FALSE) {
			fq_log(l, FQ_LOG_ERROR, "open_external_env_obj() error. To avoid this error, Make a FQ_external_env.conf in your queue directory.");
		}
#else
		rc->ext_env_obj = NULL;
#endif

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

		rc->stop_signal = 0;

		*obj = rc;

		en_eventlog(l, EVENT_FQ_OPEN, path, qname, TRUE, getpid());

		FQ_TRACE_EXIT(l);
		return(TRUE);
	}

return_FALSE:
	if( rc->d_obj ) {
		close_datafile_obj(l, &rc->d_obj, rc->h_obj->h->mapping_len);
	}
	if( rc->h_obj ) {
		close_headfile_obj(l, &rc->h_obj);
	}

	SAFE_FREE(rc->path);
	SAFE_FREE(rc->qname);
	SAFE_FREE(rc->eventlog_path);

	SAFE_FREE(rc->monID);
	SAFE_FREE(rc->hostname);

	rc->pid = -1;
	rc->tid = 0L;

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

	SAFE_FREE(rc);

	SAFE_FREE(*obj);

	en_eventlog(l, EVENT_FQ_OPEN, path, qname, FALSE, getpid());

	FQ_TRACE_EXIT(l);
	return(FALSE);
}

int SetOptions(fq_logger_t *l, fqueue_obj_t *obj, int stopwatch_on_off)
{
	obj->stopwatch_on_off = stopwatch_on_off;
	return(0);
}

#if 0
int 
CloseFileQueue(fq_logger_t *l, fqueue_obj_t **obj)
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
	// (*obj)->h_obj->en_flock->on_flock((*obj)->h_obj->en_flock);
	// (*obj)->h_obj->de_flock->on_flock((*obj)->h_obj->de_flock);
	fq_critical_section(l, obj, L_EN_ACTION, FQ_LOCK_ACTION);
	fq_critical_section(l, obj, L_DE_ACTION, FQ_LOCK_ACTION);


	(*obj)->stop_signal = 1;


	// (*obj)->h_obj->en_flock->on_funlock((*obj)->h_obj->en_flock);
	// (*obj)->h_obj->de_flock->on_funlock((*obj)->h_obj->de_flock);
	fq_critical_section(l, obj, L_EN_ACTION, FQ_UNLOCK_ACTION);
	fq_critical_section(l, obj, L_DE_ACTION, FQ_UNLOCK_ACTION);


	SAFE_FREE((*obj)->path);
	SAFE_FREE((*obj)->qname);
	SAFE_FREE((*obj)->eventlog_path);

	if( (*obj)->hb_obj ) {
		close_heartbeat_obj(l, &(*obj)->hb_obj );
	}

	fq_log(l, FQ_LOG_DEBUG, "close_datafile_obj() start");

	if( (*obj)->d_obj ) {
		rc = close_datafile_obj(l, &(*obj)->d_obj, (*obj)->h_obj->h->mapping_len );
		if( rc != TRUE ) {
			fq_log(l, FQ_LOG_ERROR, "close_datafile_obj() error.");
			goto return_FALSE;
		}
	}
	(*obj)->d_obj = NULL;
	fq_log(l, FQ_LOG_DEBUG, "close_datafile_obj() end");
	

	if( (*obj)->h_obj ) {
		rc = close_headfile_obj(l, &(*obj)->h_obj );
		if( rc != TRUE ) {
			fq_log(l, FQ_LOG_ERROR, "close_headfile_obj() error.");
			goto return_FALSE;
		}
	}
	(*obj)->h_obj = NULL;
	fq_log(l, FQ_LOG_DEBUG, "close_headfile_obj() end");

	if( (*obj)->b_obj ) {
		rc = close_backupfile_obj(l, &(*obj)->b_obj );
		if( rc != TRUE ) {
			fq_log(l, FQ_LOG_ERROR, "close_backupfile_obj() error.");
			goto return_FALSE;
		}
	}
	(*obj)->b_obj = NULL;
	fq_log(l, FQ_LOG_DEBUG, "close_backupfile_obj() end");

	if( (*obj)->uq_obj )  {
		rc = close_unixQ_obj(l, &(*obj)->uq_obj);
		if( rc == FALSE ) {
			fq_log(l, FQ_LOG_ERROR, "close_unixQ_obj() error.");
			goto return_FALSE;
		}
	}
	(*obj)->uq_obj = NULL;
	fq_log(l, FQ_LOG_DEBUG, "close_unixQ_obj() end.");

	if( (*obj)->evt_obj ) {
		rc = close_evenlog_obj(l, &(*obj)->evt_obj);
		if( rc == FALSE ) {
			fq_log(l, FQ_LOG_ERROR, "close_evenlog_obj() error.");
			goto return_FALSE;
		}
	}
	fq_log(l, FQ_LOG_DEBUG, "close_eventlog_obj() end.");

	SAFE_FREE(*obj);

	en_eventlog(l, EVENT_FQ_CLOSE, path, qname, TRUE, getpid());
	FQ_TRACE_EXIT(l);
	return(TRUE);

return_FALSE:

	en_eventlog(l, EVENT_FQ_CLOSE, path, qname, FALSE, getpid());
	FQ_TRACE_EXIT(l);
	return(FALSE);
}
#else
int 
CloseFileQueue(fq_logger_t *l, fqueue_obj_t **obj)
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
	// (*obj)->h_obj->en_flock->on_flock((*obj)->h_obj->en_flock);
	// (*obj)->h_obj->de_flock->on_flock((*obj)->h_obj->de_flock);
	fq_critical_section(l, (*obj), L_EN_ACTION, FQ_LOCK_ACTION);
	fq_critical_section(l, (*obj), L_DE_ACTION, FQ_LOCK_ACTION);


	(*obj)->stop_signal = 1;

	// (*obj)->h_obj->en_flock->on_funlock((*obj)->h_obj->en_flock);
	// (*obj)->h_obj->de_flock->on_funlock((*obj)->h_obj->de_flock);
	fq_critical_section(l, (*obj), L_EN_ACTION, FQ_UNLOCK_ACTION);
	fq_critical_section(l, (*obj), L_DE_ACTION, FQ_UNLOCK_ACTION);


	SAFE_FREE((*obj)->path);
	SAFE_FREE((*obj)->qname);
	SAFE_FREE((*obj)->eventlog_path);
	SAFE_FREE((*obj)->hostname);

	if( (*obj)->hb_obj ) {
		close_heartbeat_obj(l, &(*obj)->hb_obj );
	}

	fq_log(l, FQ_LOG_DEBUG, "close_datafile_obj() start");

	if( (*obj)->d_obj ) {
		rc = close_datafile_obj(l, &(*obj)->d_obj, (*obj)->h_obj->h->mapping_len );
		if( rc != TRUE ) {
			fq_log(l, FQ_LOG_ERROR, "close_datafile_obj() error.");
			goto return_FALSE;
		}
	}
	(*obj)->d_obj = NULL;
	fq_log(l, FQ_LOG_DEBUG, "close_datafile_obj() end");
	

	if( (*obj)->h_obj ) {
		rc = close_headfile_obj(l, &(*obj)->h_obj );
		if( rc != TRUE ) {
			fq_log(l, FQ_LOG_ERROR, "close_headfile_obj() error.");
			goto return_FALSE;
		}
	}
	(*obj)->h_obj = NULL;
	fq_log(l, FQ_LOG_DEBUG, "close_headfile_obj() end");

	if( (*obj)->b_obj ) {
		rc = close_backupfile_obj(l, &(*obj)->b_obj );
		if( rc != TRUE ) {
			fq_log(l, FQ_LOG_ERROR, "close_backupfile_obj() error.");
			goto return_FALSE;
		}
	}
	(*obj)->b_obj = NULL;
	fq_log(l, FQ_LOG_DEBUG, "close_backupfile_obj() end");

	if( (*obj)->uq_obj )  {
		rc = close_unixQ_obj(l, &(*obj)->uq_obj);
		if( rc == FALSE ) {
			fq_log(l, FQ_LOG_ERROR, "close_unixQ_obj() error.");
			goto return_FALSE;
		}
	}
	(*obj)->uq_obj = NULL;
	fq_log(l, FQ_LOG_DEBUG, "close_unixQ_obj() end.");

	if( (*obj)->evt_obj ) {
		rc = close_evenlog_obj(l, &(*obj)->evt_obj);
		if( rc == FALSE ) {
			fq_log(l, FQ_LOG_ERROR, "close_evenlog_obj() error.");
			goto return_FALSE;
		}
	}
	fq_log(l, FQ_LOG_DEBUG, "close_eventlog_obj() end.");


	if( (*obj)->ext_env_obj ) {
		close_external_env_obj( l, &(*obj)->ext_env_obj);
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

#endif

int HowManyAccumulated(fq_logger_t *l, char *path, char *qname)
{
	int diff;
	headfile_obj_t *h_obj=NULL;

	FQ_TRACE_ENTER(l);

	if( open_headfile_obj(l, path, qname, &h_obj) == FALSE ) {
		fq_log(l, FQ_LOG_ERROR, "open_headfile_obj() error.");
		return(-1);
	}

    diff = h_obj->h->en_sum - h_obj->h->de_sum;

	close_headfile_obj(l, &h_obj);

	FQ_TRACE_EXIT(l);
	return(diff);
}
int HowManyRemained(fq_logger_t *l, char *path, char *qname)
{
	int diff, remain;
	headfile_obj_t *h_obj=NULL;

	FQ_TRACE_ENTER(l);

	if( open_headfile_obj(l, path, qname, &h_obj) == FALSE ) {
		fq_log(l, FQ_LOG_ERROR, "open_headfile_obj() error.");
		return(-1);
	}

    diff = h_obj->h->en_sum - h_obj->h->de_sum;
	remain = h_obj->h->max_rec_cnt - diff;

	close_headfile_obj(l, &h_obj);

	FQ_TRACE_EXIT(l);
	return(remain);
}

int GetQueueInfo(fq_logger_t *l, char *path, char *qname, fqueue_info_t *qi)
{
	headfile_obj_t *h_obj=NULL;

	FQ_TRACE_ENTER(l);

	if( open_headfile_obj(l, path, qname, &h_obj) == FALSE ) {
		fq_log(l, FQ_LOG_ERROR, "open_headfile_obj() error.");
		goto return_FALSE;
	}

	qi->q_header_version = h_obj->h->q_header_version;
	qi->path = STRDUP(path);
	qi->qname = STRDUP(qname);
	qi->desc = STRDUP(h_obj->h->desc);
	qi->XA_ON_OFF = STRDUP((h_obj->h->XA_flag == XA_ON)?"ON":"OFF");
	qi->msglen = h_obj->h->msglen;
    qi->mapping_num = h_obj->h->mapping_num;
    qi->max_rec_cnt = h_obj->h->max_rec_cnt;
    qi->file_size = h_obj->h->file_size;
    qi->en_sum =  h_obj->h->en_sum;
    qi->de_sum =  h_obj->h->de_sum;

	/* auto recovery */
    qi->diff = h_obj->h->en_sum - h_obj->h->de_sum;
	if(qi->diff < 0) {
		qi->de_sum = qi->en_sum;
		qi->diff = 0;
	}

    qi->create_time = h_obj->h->create_time;
    qi->last_en_time = h_obj->h->last_en_time;
    qi->last_de_time = h_obj->h->last_de_time;

    qi->latest_en_time = h_obj->h->latest_en_time;
	qi->latest_en_time_date = h_obj->h->latest_en_time_date;
    qi->latest_de_time = h_obj->h->latest_de_time;
	qi->latest_de_time_date = h_obj->h->latest_de_time_date;
	qi->current_big_files = h_obj->h->current_big_files;
	qi->big_sum = h_obj->h->big_sum;
	qi->status = h_obj->h->status;
	
	close_headfile_obj(l, &h_obj);

	FQ_TRACE_EXIT(l);
	return(TRUE);

return_FALSE:
	FQ_TRACE_EXIT(l);
	return(FALSE);
}

float GetUsageFileQueue(fq_logger_t *l, char *path, char *qname)
{
	long diff;
	float usage;
	headfile_obj_t *h_obj=NULL;

	FQ_TRACE_ENTER(l);

	if( open_headfile_obj(l, path, qname, &h_obj) == FALSE ) {
		fq_log(l, FQ_LOG_ERROR, "open_headfile_obj() error.");
		return (-1.0);
	}

	diff = h_obj->h->en_sum - h_obj->h->de_sum;
	if( diff == 0 ) usage = 0.0;
    else {
        usage = ((float)diff*100.0)/(float)h_obj->h->max_rec_cnt;
    }
	close_headfile_obj(l, &h_obj);

	FQ_TRACE_EXIT(l);

	return(usage);
}

int FreeQueueInfo(fq_logger_t *l, fqueue_info_t *qi)
{
	FQ_TRACE_ENTER(l);

    SAFE_FREE(qi->path);
    SAFE_FREE(qi->qname);
    SAFE_FREE(qi->desc);
    SAFE_FREE(qi->XA_ON_OFF);

	FQ_TRACE_EXIT(l);

    return(TRUE);
}

int DisableFileQueue(fq_logger_t *l, char *path, char *qname)
{
	int rc;
	fqueue_obj_t *obj=NULL;

	rc =  OpenFileQueue(l, NULL,  NULL, NULL,  NULL, path, qname, &obj);
	if(rc != TRUE) {
		step_print("- OpenFileQueue() ", "NOK");
		goto return_FALSE;
	}
	step_print("- OpenFileQueue() ", "OK");

	rc = obj->on_disable(l, obj);
	if(rc != TRUE) {
		step_print("- Disable processing ", "NOK");
        goto return_FALSE;
    }
	step_print("- Disable processing ", "OK");

	CloseFileQueue(l, &obj);
	en_eventlog(l, EVENT_FQ_DISABLE, path, qname, TRUE, getpid());

	step_print("- File Queue closing ", "OK");
	FQ_TRACE_EXIT(l);
	return(TRUE);

return_FALSE:
	if(obj) {
		CloseFileQueue(l, &obj);
	}
	en_eventlog(l, EVENT_FQ_DISABLE, path, qname, FALSE, getpid());
	FQ_TRACE_EXIT(l);
	return(FALSE);
}
int EnableFileQueue(fq_logger_t *l, char *path, char *qname)
{
	int rc;
	fqueue_obj_t *obj=NULL;

	rc =  OpenFileQueue(l, NULL, NULL,  NULL, NULL, path, qname, &obj);
	if(rc != TRUE) {
		step_print("- OpenFileQueue() ", "NOK");
		goto return_FALSE;
	}
	step_print("- OpenFileQueue() ", "OK");

	rc = obj->on_enable(l, obj);
	if(rc != TRUE) {
		step_print("- Enable processing ", "NOK");
        goto return_FALSE;
    }
	step_print("- Enable processing ", "OK");

	CloseFileQueue(l, &obj);
	en_eventlog(l, EVENT_FQ_ENABLE, path, qname, TRUE, getpid());

	step_print("- File Queue closing ", "OK");
	FQ_TRACE_EXIT(l);
	return(TRUE);

return_FALSE:
	if(obj) {
		CloseFileQueue(l, &obj);
	}
	en_eventlog(l, EVENT_FQ_ENABLE, path, qname, FALSE, getpid());
	FQ_TRACE_EXIT(l);
	return(FALSE);
}

int ResetFileQueue(fq_logger_t *l, char *path, char *qname)
{
	int rc;
	fqueue_obj_t *obj=NULL;

	rc =  OpenFileQueue(l, NULL,NULL,  NULL, NULL, path, qname, &obj);
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

	CloseFileQueue(l, &obj);
	en_eventlog(l, EVENT_FQ_RESET, path, qname, TRUE, getpid());

	step_print("- Closing File Queue. ", "OK");
	FQ_TRACE_EXIT(l);
	return(TRUE);

return_FALSE:
	if(obj) {
		CloseFileQueue(l, &obj);
	}
	en_eventlog(l, EVENT_FQ_RESET, path, qname, FALSE, getpid());
	FQ_TRACE_EXIT(l);
	return(FALSE);
}

int FlushFileQueue(fq_logger_t *l, char *path, char *qname)
{
	int rc;
	fqueue_obj_t *obj=NULL;
	int buffer_size;

	rc =  OpenFileQueue(l, NULL,NULL,  NULL, NULL, path, qname, &obj);
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
		
	CloseFileQueue(l, &obj);
	FQ_TRACE_EXIT(l);

	step_print("- Closing ", "OK");
	return(TRUE);

return_FALSE:
	if(obj) {
		CloseFileQueue(l, &obj);
	}
	step_print("- Flush Processing ", "NOK");

	step_print("- Closing ", "OK");

	FQ_TRACE_EXIT(l);
	return(FALSE);
}

int InfoFileQueue(fq_logger_t *l, char *path, char *qname)
{
	int rc;
	fqueue_info_t qi;


	rc = GetQueueInfo(l, path, qname, &qi);
	if( rc != TRUE ) {
		step_print("- GetQueueInfo() Processing ", "NOK");
		goto end;
	}
	step_print("- GetQueueInfo() Processing ", "OK");

	printf("\t - q_header_version=[%d]\n", qi.q_header_version);
	printf("\t - path=[%s]\n", qi.path);
	printf("\t - qname=[%s]\n", qi.qname);
	printf("\t - desc=[%s]\n", qi.desc);
	printf("\t - msglen=[%d]\n", qi.msglen);
	printf("\t - mapping_num=[%d]\n", qi.mapping_num);
	printf("\t - max_rec_cnt=[%d]\n", qi.max_rec_cnt);
	printf("\t - file_size=[%ld]\n", qi.file_size);
	printf("\t - en_sum=[%ld]\n", qi.en_sum);
	printf("\t - de_sum=[%ld]\n", qi.de_sum);
	printf("\t - diff=[%ld]\n", qi.de_sum-qi.de_sum);
	printf("\t - current_big_files=[%d]\n", qi.current_big_files);
	printf("\t - latest_en_time=[%ld](micro)\n", qi.latest_en_time);
	printf("\t - latest_de_time=[%ld](micro)\n", qi.latest_de_time);
	 
	FreeQueueInfo(l, &qi);

	step_print("- FreeQueueInfo() Processing ", "OK");

	return(TRUE);
end:
	FreeQueueInfo(l, &qi);
	step_print("- FreeQueueInfo() Processing ", "OK");

	return(FALSE);

}

int ForceSkipFileQueue(fq_logger_t *l, char *path, char *qname)
{
	int rc;
	fqueue_obj_t *obj=NULL;

	rc =  OpenFileQueue(l, NULL,NULL,  NULL, NULL, path, qname, &obj);
	if(rc != TRUE) {
		step_print("- OpenFileQueue() ", "NOK");
		goto return_FALSE;
	}
	step_print("- OpenFileQueue() ", "OK");

	rc = obj->on_force_skip(l, obj);
	if(rc != TRUE) {
		step_print("- force skip processing", "NOK");
        goto return_FALSE;
    }
	step_print("- force skip processing", "OK");

	CloseFileQueue(l, &obj);

	step_print("- CloseFileQueue() ", "OK");
	FQ_TRACE_EXIT(l);
	return(TRUE);

return_FALSE:
	if(obj) {
		CloseFileQueue(l, &obj);
	}
	FQ_TRACE_EXIT(l);
	return(FALSE);
}

int DiagFileQueue(fq_logger_t *l, char *path, char *qname)
{
	int rc;
	fqueue_obj_t *obj=NULL;

	rc =  OpenFileQueue(l, NULL,NULL,   NULL, NULL, path, qname, &obj);
	if(rc != TRUE) {
		step_print("- Check  OpenFileQueue() ", "NOK");
		goto return_FALSE;
	}
	step_print("- Check  OpenFileQueue() ", "OK");

	rc = obj->on_diag(l, obj);
	if(rc != TRUE) {
		step_print("- Diagnostic Result", "NOK");
        goto return_FALSE;
    }
	step_print("- Diagnostic Result", "OK");

	CloseFileQueue(l, &obj);
	en_eventlog(l, EVENT_FQ_RESET, path, qname, TRUE, getpid());

	step_print("- File Queue Closing", "OK");

	FQ_TRACE_EXIT(l);
	return(TRUE);

return_FALSE:
	if(obj) {
		CloseFileQueue(l, &obj);
	}
	en_eventlog(l, EVENT_FQ_RESET, path, qname, FALSE, getpid());
	FQ_TRACE_EXIT(l);
	return(FALSE);
}

int ViewFileQueue(fq_logger_t *l, char *path, char *qname, char **ptr_buf, int *len, long *seq, bool *big_flag)
{
	int rc;
	fqueue_obj_t *obj=NULL;
	int buffer_size = (65536*10);
	long l_seq=0L;
	long run_time=0L;
	char *tmpbuf=NULL;

	bool BIG_flag = false;

	*big_flag = false;
	
	rc =  OpenFileQueue(l, NULL,NULL,   NULL, NULL, path, qname, &obj);
	if(rc != TRUE) {
		goto return_FALSE;
	}

	// buffer_size = obj->h_obj->h->msglen + 1;
	tmpbuf = calloc(buffer_size, sizeof(unsigned char));

	rc = obj->on_view(l, obj, tmpbuf, buffer_size, &l_seq, &run_time, &BIG_flag);
	if( rc == DQ_EMPTY_RETURN_VALUE ) {
		return(0);
	}
	else if( rc < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "on_view() error.");
		goto return_FALSE;
	}
	else {
		fq_log(l, FQ_LOG_DEBUG, "rc='%d', l_req='%ld', run_time='%ld'.", rc, l_seq, run_time);
		*ptr_buf = tmpbuf;
		*len = rc;
		*seq = l_seq;
		*big_flag = BIG_flag;
	}

	CloseFileQueue(l, &obj);

	FQ_TRACE_EXIT(l);
	return(rc);

return_FALSE:
	if(obj) {
		CloseFileQueue(l, &obj);
	}

	FQ_TRACE_EXIT(l);
	return(-1);
}

/*
** Func: on_view
** Description: This function sees the data to be read next in the queue.
** It does not lock for reading.
*/
#if 0 /* locking version */
static int 
on_view(fq_logger_t *l, fqueue_obj_t *obj, unsigned char *dst, int dstlen, long *seq, long *run_time)
{
	int ret=-1;
	unsigned char header[FQ_HEADER_SIZE+1];
	register off_t   record_no;
	register off_t   p_offset_from, p_offset_to;
	register off_t   mmap_offset;
	register off_t   add_offset;
	register long    nth_mapping;
	int     data_len=0;
	char	buf[256];

	FQ_TRACE_ENTER(l);


	// obj->h_obj->flock->on_flock(obj->h_obj->flock);

	fq_critical_section(l, obj, L_EN_ACTION, FQ_LOCK_ACTION);
	fq_critical_section(l, obj, L_DE_ACTION, FQ_LOCK_ACTION);


	if( obj->stop_signal == 1) {
		fq_log(l, FQ_LOG_ERROR, "Administrator sent stopping signal.");
		// obj->h_obj->flock->on_funlock(obj->h_obj->flock);
		fq_critical_section(l, obj, L_EN_ACTION, FQ_UNLOCK_ACTION);
		fq_critical_section(l, obj, L_DE_ACTION, FQ_UNLOCK_ACTION);

		FQ_TRACE_EXIT(l);
		return(MANAGER_STOP_RETURN_VALUE);
	} 

	if( obj->h_obj->h->status != QUEUE_ENABLE ) {
		fq_log(l, FQ_LOG_ERROR, "This queue is disabled by Admin.");
		// obj->h_obj->flock->on_funlock(obj->h_obj->flock);
		fq_critical_section(l, obj, L_EN_ACTION, FQ_UNLOCK_ACTION);
		fq_critical_section(l, obj, L_DE_ACTION, FQ_UNLOCK_ACTION);
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
    p_offset_to = p_offset_from + obj->h_obj->h->msglen;

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

	if( obj->d_obj->d == NULL) {
		if( (obj->d_obj->d = fq_mmapping(l, obj->d_obj->fd, obj->h_obj->h->mapping_len , mmap_offset)) == NULL ) {
			fq_log(l, FQ_LOG_ERROR, "first fq_mmapping(body) error.");
			goto unlock_return;
		}
		obj->d_obj->available_from_offset = mmap_offset;
		obj->d_obj->available_to_offset = mmap_offset + obj->h_obj->h->mapping_len;
	}
	else {
		if( (p_offset_from < obj->d_obj->available_from_offset) || 
			(p_offset_to > obj->d_obj->available_to_offset) ) {

			msync((void *)obj->d_obj->d, obj->h_obj->h->mapping_len, MS_SYNC|MS_INVALIDATE); /*  MS_INVALIDATE */
			if( fq_munmap(l, (void *)obj->d_obj->d, obj->h_obj->h->mapping_len) < 0 ) {
				fq_log(obj->l, FQ_LOG_ERROR, "fq_munmap() error. reason=[%s].", strerror(errno));
				obj->d_obj->d = NULL;
				goto unlock_return;
			}
			if( (obj->d_obj->d = fq_mmapping(l, obj->d_obj->fd, obj->h_obj->h->mapping_len, mmap_offset)) == NULL ) {
				fq_log(l, FQ_LOG_ERROR, "re-fq_mmapping(body) error.");
				goto unlock_return;
			}
			obj->d_obj->available_from_offset = mmap_offset;
			obj->d_obj->available_to_offset = mmap_offset + obj->h_obj->h->mapping_len;
		}
	}

	add_offset = p_offset_from - obj->d_obj->available_from_offset;

	memcpy( header, (void *)((off_t)obj->d_obj->d + add_offset), FQ_HEADER_SIZE);
	data_len = get_bodysize( header, FQ_HEADER_SIZE, 0);

	if( data_len <= 0 ) { /* check read message */
		fq_log(l, FQ_LOG_ERROR, "length of data is zero or minus. [%d]", data_len);
		ret = -9999;
		goto unlock_return;
	}

	if( data_len > dstlen-1) { /* overflow */
		fq_log(l, FQ_LOG_ERROR, "Your buffer size is less than size of data(%d)-overflow.", data_len);
		ret = -9999;
		goto unlock_return;
	}

	if( data_len > (obj->h_obj->h->msglen-FQ_HEADER_SIZE) ) { /* big message processing */
		char	big_Q_filename[256];
		char	*big_buf=NULL, *msg=NULL;
		int		file_size;
		long	row;

		msync((void *)obj->d_obj->d, obj->h_obj->h->mapping_len, MS_SYNC|MS_INVALIDATE); /*  MS_INVALIDATE */

		sprintf(big_Q_filename, "%s", (void *)((off_t)obj->d_obj->d + add_offset + FQ_HEADER_SIZE) );


		if( load_big_Q_file_2_buf(l, big_Q_filename , &big_buf, &file_size,  &msg) < 0) {
			fq_log(obj->l, FQ_LOG_ERROR, "fatal: load_file_buf(%s) error. errmsg=[%s]", big_Q_filename,  msg);

			*seq = obj->h_obj->h->de_sum;

			ret = DQ_FATAL_SKIP_RETURN_VALUE; /* for continue, You must skip this data. 2013/10/30 */

			goto unlock_return;
		}

		memcpy(dst, big_buf, file_size);
		SAFE_FREE(big_buf);
		SAFE_FREE(msg);

		unlink(big_Q_filename);
	}
	else { /* normal messages */
		memcpy( dst, (void *)((off_t)obj->d_obj->d + add_offset + FQ_HEADER_SIZE), data_len );
	}

	*seq = obj->h_obj->h->de_sum;

	ret = data_len;
	
	fq_log(l, FQ_LOG_DEBUG, "on_view() : positive return (%d).", ret);

unlock_return:
	// obj->h_obj->flock->on_funlock(obj->h_obj->flock);
	fq_critical_section(l, obj, L_DE_ACTION, FQ_UNLOCK_ACTION);
	fq_critical_section(l, obj, L_EN_ACTION, FQ_UNLOCK_ACTION);


	fq_log(l, FQ_LOG_DEBUG, "on_view() exit ret=[%d].", ret);


	FQ_TRACE_EXIT(l);
	return(ret);
}
#else /* no locking version */
/* This function has a bug. Don't use it until it is improved.*/
static int 
on_view(fq_logger_t *l, fqueue_obj_t *obj, unsigned char *dst, int dstlen, long *seq, long *run_time, bool *big_flag)
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

	FQ_TRACE_ENTER(l);

	*big_flag = false;

#ifdef _USE_PROCESS_CONTROL
	if( obj->stop_signal == 1) {
		fq_log(l, FQ_LOG_ERROR, "Administrator sent stopping signal.");
		FQ_TRACE_EXIT(l);
		return(MANAGER_STOP_RETURN_VALUE);
	} 
#endif
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

restart:

	// obj->h_obj->de_flock->on_flock(obj->h_obj->de_flock);
	fq_critical_section(l, obj, L_DE_ACTION, FQ_LOCK_ACTION);


	if( obj->stop_signal == 1) {
		fq_log(l, FQ_LOG_ERROR, "Administrator sent stopping signal.");
		// obj->h_obj->de_flock->on_funlock(obj->h_obj->de_flock);
		fq_critical_section(l, obj, L_DE_ACTION, FQ_UNLOCK_ACTION);


		FQ_TRACE_EXIT(l);
		return(MANAGER_STOP_RETURN_VALUE);
	} 

	if( obj->h_obj->h->status != QUEUE_ENABLE ) {
		fq_log(l, FQ_LOG_ERROR, "This queue is disabled by Admin.");
		// obj->h_obj->de_flock->on_funlock(obj->h_obj->de_flock);
		fq_critical_section(l, obj, L_DE_ACTION, FQ_UNLOCK_ACTION);

		FQ_TRACE_EXIT(l);
		return(MANAGER_STOP_RETURN_VALUE);
	} 

	if( dstlen < (obj->h_obj->h->msglen-FQ_HEADER_SIZE) ) {
		fq_log(obj->l, FQ_LOG_ERROR, "Your buffer size is short. available size is %d.",
			(obj->h_obj->h->msglen-FQ_HEADER_SIZE));
		goto unlock_return;
	}

	int diff;

	diff = obj->h_obj->h->en_sum - obj->h_obj->h->de_sum;
	if( diff == 0 ) {
		ret = DQ_EMPTY_RETURN_VALUE ;
		goto unlock_return;
	}
	if( diff < 0 ) {
		fq_log(obj->l, FQ_LOG_EMERG, "Fatal error. Automatic recovery.");
		obj->h_obj->h->de_sum = obj->h_obj->h->en_sum;
		ret = DQ_EMPTY_RETURN_VALUE ;

		save_Q_fatal_error(obj->path, obj->qname, "diff is minus value. Automatic recovery.", __FILE__, __LINE__);
		goto unlock_return;
	}
	
#if 0
	if( obj->h_obj->h->XA_ing_flag == 1 ) {
		time_t	current;
		current = time(0);

		if( (obj->h_obj->h->XA_lock_time + 60) < current ) {
			fq_log(obj->l, FQ_LOG_ERROR, "locking time is so long time.I ignore it.");
			obj->h_obj->h->XA_lock_time = time(0);
		}
		else {
			obj->h_obj->h->XA_ing_flag = 0; /* Add: 2016/01/16  */
			// obj->h_obj->de_flock->on_funlock(obj->h_obj->de_flock);
			fq_critical_section(l, obj, L_DE_ACTION, FQ_UNLOCK_ACTION);

			usleep(1000);
			goto restart;
		}
	}
#endif

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

	if( data_len > dstlen-1) { /* overflow */
		fq_log(l, FQ_LOG_ERROR, "Your buffer size is less than size of data(%d)-overflow. In this case, We return EMPTY to skip this data automatically", data_len);
		obj->h_obj->h->de_sum++; /* error data is skipped. */
		// ret = -9999;
		ret = DQ_EMPTY_RETURN_VALUE ;
		goto unlock_return;
	}

	char	big_Q_filename[256];
	if( data_len > (obj->h_obj->h->msglen-FQ_HEADER_SIZE) ) { /* big message processing */
		usleep(10000);
		sprintf(big_Q_filename, "%s", (char *)((off_t)obj->de_p + add_offset + FQ_HEADER_SIZE) );
		memcpy(dst, big_Q_filename, strlen(big_Q_filename));
		ret = strlen(big_Q_filename);
		*big_flag = true;
		/* We return a big path and filename in case of BIG data. */
	}
	else { /* normal messages */
		memcpy( dst, (void *)((off_t)obj->de_p + add_offset + FQ_HEADER_SIZE), data_len );
		ret = data_len;
		*big_flag = false;
	}

	*seq = obj->h_obj->h->de_sum;

	fq_log(l, FQ_LOG_DEBUG, "on_view() : positive return (%d).", ret);

unlock_return:

	// obj->h_obj->de_flock->on_funlock(obj->h_obj->de_flock);
	fq_critical_section(l, obj, L_DE_ACTION, FQ_UNLOCK_ACTION);


	fq_log(l, FQ_LOG_DEBUG, "on_view() exit ret=[%d].", ret);

	FQ_TRACE_EXIT(l);
	return(ret);
}

#endif

static int backup_for_XA( fq_logger_t *l, char *path, char *qname, unsigned char *data, int datalen)
{
	int fd=-1, n;
	char	filename[256];

	if(!data || !path || !qname || datalen < 1 ) {
		fq_log(l, FQ_LOG_ERROR, "illegal function request. Check parameters.");
		return(-3);
	}
		
	sprintf(filename, "%s/.%s_XA.bak", path, qname);
	
	/* We don't unlink for speed, so We open to not be O_EXCL. */
	/* if( (fd=open(filename, O_RDWR|O_CREAT|O_EXCL, 0666)) < 0 ) { */
	if( (fd=open(filename, O_RDWR|O_CREAT, 0666)) < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "file open error. reason=[%s] file=[%s]", strerror(errno), filename);
		return(-1);
	}
	if( (n=write(fd, data, datalen)) != datalen ) {
		fq_log(l, FQ_LOG_ERROR, "file write error. reason=[%s]", strerror(errno));
		SAFE_FD_CLOSE(fd);
		return(-2);
	}

	SAFE_FD_CLOSE(fd);
	fd = -1;

	return(n);
}

static int delete_for_XA( fq_logger_t *l, char *path, char *qname)
{
	char    filename[256];

	if(!path || !qname ) {
		fq_log(l, FQ_LOG_ERROR, "illegal function request. Check parameters.");
		return(-3);
	}
	sprintf(filename, "%s/.%s_XA.bak", path, qname);

	unlink(filename);
	
	return(0);
}

int hello_fq()
{
	printf("Hello. I'm fq library. version = '%s' \n", FQUEUE_LIB_VERSION );
	return(TRUE);	
}

int alloc_info_openes(fqueue_obj_t *obj, fqueue_info_t *qi)
{
	qi->path = STRDUP(obj->path);
	qi->qname = STRDUP(obj->qname);
	qi->desc = STRDUP(obj->h_obj->h->desc);
	qi->msglen = obj->h_obj->h->msglen;
	qi->mapping_num = obj->h_obj->h->mapping_num;
	qi->max_rec_cnt = obj->h_obj->h->max_rec_cnt;
	qi->file_size = obj->h_obj->h->file_size;
	qi->create_time = obj->h_obj->h->create_time;
	qi->en_sum = obj->h_obj->h->en_sum;
	qi->de_sum = obj->h_obj->h->de_sum;
	qi->diff = obj->h_obj->h->en_sum-obj->h_obj->h->de_sum;
	qi->last_en_time = obj->h_obj->h->last_en_time;
	qi->last_de_time = obj->h_obj->h->last_de_time;
	
	return(0);
}

int free_fqueue_info( fqueue_info_t *qi)
{
	SAFE_FREE(qi->path);
	SAFE_FREE(qi->qname);
	SAFE_FREE(qi->desc);

	return(0);
}
int 
OpenFileQueue_M(fq_logger_t *l, char *monID,  char *path, char *qname, fqueue_obj_t **obj)
{
	fqueue_obj_t *rc=NULL;

	FQ_TRACE_ENTER(l);


	if( !HASVALUE(path) || !HASVALUE(qname) )  {
        fq_log(l, FQ_LOG_ERROR, "illgal function call.");
		FQ_TRACE_EXIT(l);
		return(FALSE);
    }

	if( _checklicense( path ) < 0 ) {
		char *fq_home_path = getenv("FQ_DATA_HOME");

		if( _checklicense( fq_home_path ) < 0 ) {
			fq_log(l, FQ_LOG_ERROR, "You don't have a license in this system in directory[%s].", fq_home_path);
			FQ_TRACE_EXIT(l);
			return(FALSE);
		}
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

		if( monID ) {
			rc->monID = strdup(monID);
			if( open_monitor_obj(l, &rc->mon_obj) == FALSE) {
				fq_log(l, FQ_LOG_ERROR, "open_monitor_obj() error.");
				goto return_FALSE;
			}
		}
		else {
			rc->monID = NULL;
			rc->mon_obj=NULL;
		}


		rc->h_obj = NULL;
		if( open_headfile_obj(l, path, qname, &rc->h_obj) == FALSE ) {
			fq_log(l, FQ_LOG_ERROR, "open_headfile_obj() error.");
			goto return_FALSE;
		}

		rc->d_obj = NULL;
#if 0
		if( open_datafile_obj(l, path, qname, &rc->d_obj) == FALSE ) {
			fq_log(l, FQ_LOG_ERROR, "open_datafile_obj() error.");
			goto return_FALSE;
		}
#endif

		rc->uq_obj = NULL;
		if( open_unixQ_obj(l, FQ_UNIXQ_KEY_PATH,  FQ_UNIXQ_KEY_CHAR, &rc->uq_obj) == FALSE ) {
			fq_log(l, FQ_LOG_ERROR, "open_unixQ_obj() error.");
			goto return_FALSE;
		}

		rc->evt_obj=NULL;
		rc->ext_env_obj = NULL;

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

		rc->stop_signal = 0;

		*obj = rc;

		en_eventlog(l, EVENT_FQ_OPEN, path, qname, TRUE, getpid());

		FQ_TRACE_EXIT(l);
		return(TRUE);
	}

return_FALSE:
	if( rc->d_obj ) {
		close_datafile_obj(l, &rc->d_obj, rc->h_obj->h->mapping_len);
	}
	if( rc->h_obj ) {
		close_headfile_obj(l, &rc->h_obj);
	}

	SAFE_FREE(rc->path);
	SAFE_FREE(rc->qname);
	SAFE_FREE(rc->eventlog_path);

	SAFE_FREE(rc->monID);
	SAFE_FREE(rc->hostname);

	rc->pid = -1;
	rc->tid = 0L;

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

	SAFE_FREE(rc);

	SAFE_FREE(*obj);

	en_eventlog(l, EVENT_FQ_OPEN, path, qname, FALSE, getpid());

	FQ_TRACE_EXIT(l);
	return(FALSE);
}
int 
CloseFileQueue_M(fq_logger_t *l, fqueue_obj_t **obj)
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
	// (*obj)->h_obj->en_flock->on_flock((*obj)->h_obj->en_flock);
	// (*obj)->h_obj->de_flock->on_flock((*obj)->h_obj->de_flock);
	fq_critical_section(l, (*obj), L_DE_ACTION, FQ_LOCK_ACTION);
	fq_critical_section(l, (*obj), L_EN_ACTION, FQ_LOCK_ACTION);


	(*obj)->stop_signal = 1;


	// (*obj)->h_obj->en_flock->on_funlock((*obj)->h_obj->en_flock);
	// (*obj)->h_obj->de_flock->on_funlock((*obj)->h_obj->de_flock);
	fq_critical_section(l, (*obj), L_DE_ACTION, FQ_UNLOCK_ACTION);
	fq_critical_section(l, (*obj), L_EN_ACTION, FQ_UNLOCK_ACTION);


	SAFE_FREE((*obj)->path);
	SAFE_FREE((*obj)->qname);
	SAFE_FREE((*obj)->eventlog_path);

	if( (*obj)->hb_obj ) {
		close_heartbeat_obj(l, &(*obj)->hb_obj );
	}

	(*obj)->d_obj = NULL;

	if( (*obj)->h_obj ) {
		rc = close_headfile_obj(l, &(*obj)->h_obj );
		if( rc != TRUE ) {
			fq_log(l, FQ_LOG_ERROR, "close_headfile_obj() error.");
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

	SAFE_FREE(*obj);

	FQ_TRACE_EXIT(l);
	return(TRUE);
return_FALSE:

	FQ_TRACE_EXIT(l);
	return(FALSE);
}

static void save_Q_fatal_error(char *qpath, char *qname, char *msg, char *file, int src_line)
{
    char *fname = "/tmp/queue_fatal/fatal_error.log";
    FILE *lfp;

    lfp = fopen(fname, "a");
    if(lfp == NULL) return;

    fprintf(lfp, "Qpath=[%s], Qname=[%s], pid=[%d], msg=[%s], libsrc=[%s], line=[%d]\n",
        qpath, qname, getpid(),  msg, file, src_line);
    fflush(lfp);
    fclose(lfp);

    return;
}

int MoveFileQueue( fq_logger_t *l, char *from_path, char *from_qname, char *to_path, char *to_qname, int move_req_count)
{
	int rc;
	int moved_count = 0;
	fqueue_obj_t *from_obj=NULL, *to_obj=NULL;

	FQ_TRACE_ENTER(l);

	rc =  OpenFileQueue(l, NULL, NULL, NULL, NULL, from_path, from_qname, &from_obj);
	if( rc == FALSE ) {
		FQ_TRACE_EXIT(l);
		return(-1);
	}

	rc =  OpenFileQueue(l, NULL, NULL, NULL, NULL, to_path, to_qname, &to_obj);
	if( rc == FALSE ) {
		FQ_TRACE_EXIT(l);
		return(-1);
	}

	moved_count = MoveFileQueue_obj( l, from_obj, to_obj, move_req_count);

	if( moved_count < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "MoveFileQueue() error.");
	}

	CloseFileQueue(l, &from_obj);
	CloseFileQueue(l, &to_obj);
	
	FQ_TRACE_EXIT(l);

	return(moved_count);
}

int MoveFileQueue_obj(fq_logger_t *l, fqueue_obj_t *from_obj, fqueue_obj_t *to_obj, int move_req_count)
{
	int moved_count = 0;
	int buffer_size = 0;
	unsigned char *buf = NULL;
	int i=0;

	FQ_TRACE_ENTER(l);

	if( !from_obj || !to_obj ) {
		fq_log(l, FQ_LOG_ERROR, "fqueue_obj is null.");
		FQ_TRACE_EXIT(l);
        return(-1);
    }

	if( from_obj->h_obj->h->msglen > to_obj->h_obj->h->msglen) {
		fq_log(l, FQ_LOG_ERROR, "Length of destination queue is less than length of source queue.(%d > %d)",
				from_obj->h_obj->h->msglen, to_obj->h_obj->h->msglen); 

		FQ_TRACE_EXIT(l);
		return(-1);
	}

	buffer_size = from_obj->h_obj->h->msglen + 1;
	buf = malloc(buffer_size);
	if( !buf ) {
		fq_log(l, FQ_LOG_ERROR, "molloc() error.");
		FQ_TRACE_EXIT(l);
        return(-1);
    }
	for(i=0; i<move_req_count; i++) {
		long l_seq=0L;
		long run_time=0L;
		int rc;

		memset(buf, 0x00, sizeof(buffer_size));
		rc = from_obj->on_de(l, from_obj, buf, buffer_size, &l_seq, &run_time);

		if( rc == DQ_EMPTY_RETURN_VALUE ) {
			break;
		}
		else if( rc < 0 ) {
			if( rc == MANAGER_STOP_RETURN_VALUE ) {
				printf("Manager asked to stop a processing.\n");
				break;
			}
			goto stop;
		}
		else {
			int ret;

enQ_retry:
			ret =  to_obj->on_en(l, to_obj, EN_NORMAL_MODE, buf, buffer_size, rc, &l_seq, &run_time );
			if( rc == EQ_FULL_RETURN_VALUE )  {
				usleep(10000);
				goto enQ_retry;		
			}
			else if( rc < 0 ) { 
				if( rc == MANAGER_STOP_RETURN_VALUE ) {
					fq_log(l, FQ_LOG_DEBUG, "Manager asked to me stopping. current_moving count is [%d]\n", moved_count);
					goto stop;
				}
				goto stop;
			}
			else {
				moved_count++;
				continue;
			}
		}
	}

stop:
	SAFE_FREE(buf);
	FQ_TRACE_EXIT(l);

	return(moved_count);
}

int MoveFileQueue_XA( fq_logger_t *l, char *from_path, char *from_qname, char *to_path, char *to_qname, int move_req_count)
{
	int rc;
	int moved_count = 0;
	fqueue_obj_t *from_obj=NULL, *to_obj=NULL;

	FQ_TRACE_ENTER(l);

	rc =  OpenFileQueue(l, NULL, NULL, NULL, NULL, from_path, from_qname, &from_obj);
	if( rc == FALSE ) {
		FQ_TRACE_EXIT(l);
		return(-1);
	}

	rc =  OpenFileQueue(l, NULL, NULL, NULL, NULL, to_path, to_qname, &to_obj);
	if( rc == FALSE ) {
		FQ_TRACE_EXIT(l);
		return(-1);
	}

	moved_count = MoveFileQueue_XA_obj( l, from_obj, to_obj, move_req_count);

	if( moved_count < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "MoveFileQueue() error.");
	}

	CloseFileQueue(l, &from_obj);
	CloseFileQueue(l, &to_obj);
	
	FQ_TRACE_EXIT(l);

	return(moved_count);
}
int MoveFileQueue_XA_obj(fq_logger_t *l, fqueue_obj_t *from_obj, fqueue_obj_t *to_obj, int move_req_count)
{
	int moved_count = 0;
	int buffer_size = 0;
	unsigned char *buf = NULL;
	int i;

	FQ_TRACE_ENTER(l);

	if( !from_obj || !to_obj ) {
		fq_log(l, FQ_LOG_ERROR, "fqueue_obj is null.");
		FQ_TRACE_EXIT(l);
        return(-1);
    }

	if( from_obj->h_obj->h->msglen > to_obj->h_obj->h->msglen) {
		fq_log(l, FQ_LOG_ERROR, "Length of destination queue is less than length of source queue.(%d > %d)",
				from_obj->h_obj->h->msglen, to_obj->h_obj->h->msglen); 

		FQ_TRACE_EXIT(l);
		return(-1);
	}

	buffer_size = from_obj->h_obj->h->msglen + 1;
	buf = malloc(buffer_size);
	if( !buf ) {
		fq_log(l, FQ_LOG_ERROR, "molloc() error.");
		FQ_TRACE_EXIT(l);
        return(-1);
    }

	for(i=0; i<move_req_count; i++) {
		long from_l_seq=0L;
		long from_run_time=0L;
		int rc;
		char from_unlink_filename[256];

		memset(buf, 0x00, sizeof(buffer_size));
		memset(from_unlink_filename, 0x00, sizeof(from_unlink_filename));
		rc = from_obj->on_de_XA(l, from_obj, buf, buffer_size, &from_l_seq, &from_run_time, from_unlink_filename );

		if( rc == DQ_EMPTY_RETURN_VALUE ) {
			break;
		}
		else if( rc < 0 ) {
			if( rc == MANAGER_STOP_RETURN_VALUE ) {
				printf("Manager asked to stop a processing.\n");
				break;
			}
			goto stop;
		}
		else {
			int ret;
			long l_seq;
			long run_time;

enQ_retry:
			ret =  to_obj->on_en(l, to_obj, EN_NORMAL_MODE, buf, buffer_size, rc, &l_seq, &run_time );
			if( rc == EQ_FULL_RETURN_VALUE )  {
				usleep(10000);
				goto enQ_retry;		
			}
			else if( rc < 0 ) { 
				if( rc == MANAGER_STOP_RETURN_VALUE ) {
					fq_log(l, FQ_LOG_DEBUG, "Manager asked to me stopping. current_moving count is [%d]\n", moved_count);
					goto stop;
				}
				from_obj->on_cancel_XA(l, from_obj, from_l_seq, &run_time);
				goto stop;
			}
			else {
				from_obj->on_commit_XA(l, from_obj, from_l_seq, &run_time, from_unlink_filename);
				moved_count++;
				continue;
			}
		}
	}

stop:
	SAFE_FREE(buf);
	FQ_TRACE_EXIT(l);

	return(moved_count);
}

char *fqlibVersion()
{
	return(FQUEUE_LIB_VERSION);
}

/**********************************************************
** Wrapper for golang
**********************************************************/
int COpenLog(fq_logger_t **l, char *logpath,  char *logfile, char *log_level)
{
	int rc;
	char log_path_file[256];
	int i_log_level;

	sprintf(log_path_file, "%s/%s", logpath, logfile);
	i_log_level = get_log_level(log_level);	
	rc = fq_open_file_logger(l, log_path_file, i_log_level);
	return(rc);
}
int CCloseLog(fq_logger_t **l)
{
	int rc;

	rc = fq_close_file_logger(l);
	return(rc);
}
int COpenQ(fq_logger_t *l, fqueue_obj_t **obj,  char *path, char *qname)
{
	int rc;

	FQ_TRACE_ENTER(l);

	rc =  OpenFileQueue(l, NULL,NULL, NULL, NULL, path, qname, obj);
	if( rc != TRUE ) {
		fq_log(l, FQ_LOG_ERROR, "COpenQ() error.");
		FQ_TRACE_EXIT(l);
		return(-1);
	}
	fq_log(l, FQ_LOG_DEBUG, "OpenFileQueue('%s', '%s') success!", path, qname);

	FQ_TRACE_EXIT(l);
	return(0);
}

int CCloseQ( fq_logger_t *l, fqueue_obj_t *obj)
{
	int rc;

	FQ_TRACE_ENTER(l);
    rc = CloseFileQueue(l, &obj);
	FQ_TRACE_EXIT(l);
	return(rc);
}

int CenQ( fq_logger_t *l, fqueue_obj_t *obj, unsigned char *buf, int datasize)
{
	int rc;
	long l_seq, run_time;
	int databuflen = datasize + 1;

    FQ_TRACE_ENTER(l);

	rc =  obj->on_en(l, obj, EN_NORMAL_MODE, buf, databuflen, datasize, &l_seq, &run_time );
	if( rc < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "CenQ() error.");
	}

	fq_log(l, FQ_LOG_DEBUG, "on_en(len-'%d') success!", datasize);
	FQ_TRACE_EXIT(l);
	return(rc);
}

int CdeQ( fq_logger_t *l, fqueue_obj_t *obj, unsigned char **buf, long *seq)
{
	int rc;
	long l_seq, run_time;
	int buffer_size = 65536;
	unsigned char *p = NULL;

	FQ_TRACE_ENTER(l);
	
	p = calloc(buffer_size, sizeof(unsigned char));
	
	rc = obj->on_de(l, obj, p, buffer_size, &l_seq, &run_time);

	if( rc < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "CdeQ() error.");
	}

	fq_log(l, FQ_LOG_DEBUG, "CdeQ(): Native C debug: :[%d][%s]\n", rc, p);

	*buf = p;
	*seq = l_seq;
	
	FQ_TRACE_EXIT(l);
	return(rc);
}

int CdeQ_XA(fq_logger_t *l, fqueue_obj_t *obj, unsigned char **buf, long *seq, char **filename)
{
    int rc;
    int buffer_size = 65536;
    char unlink_filename[256];
    long l_seq, run_time;
    unsigned char *p = NULL;

    FQ_TRACE_ENTER(l);

    p = calloc(buffer_size, sizeof(unsigned char));

    memset(unlink_filename, 0x00, sizeof(unlink_filename));
    rc = obj->on_de_XA(l, obj, p, buffer_size, &l_seq, &run_time, unlink_filename);

    if( rc < 0 ) {
        fq_log(l, FQ_LOG_ERROR, "CdeQ_XA() error.");
    }

    fq_log(l, FQ_LOG_DEBUG, "CdeQ_XA(): Native C debug: :[%d][%s]\n", rc, p);

    *buf = p;
    *filename = strdup(unlink_filename);
    *seq =  l_seq;

    FQ_TRACE_EXIT(l);
    return(rc);
}

int CdeQ_XA_commit( fq_logger_t *l, fqueue_obj_t *obj, long l_seq, char *unlink_filename)
{
	long run_time;
	int rc;

	FQ_TRACE_ENTER(l);
	rc = obj->on_commit_XA(l, obj, l_seq, &run_time, unlink_filename);
	if( rc < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "CdeQ_XA_commit() error.");
	}
	FQ_TRACE_EXIT(l);
	return(rc);
}
int CQmsglen( fqueue_obj_t *obj )
{
	return(obj->h_obj->h->msglen);
}

int CdeQ_XA_cancel( fq_logger_t *l, fqueue_obj_t *obj, long l_seq)
{
	long run_time;
    int rc;

	FQ_TRACE_ENTER(l);

	rc = obj->on_cancel_XA(l, obj, l_seq, &run_time);
	if( rc < 0 ) {
        fq_log(l, FQ_LOG_ERROR, "CdeQ_XA_cancel() error.");
    }

    FQ_TRACE_EXIT(l);

    return(rc);
}


#include "fq_conf.h"
#include <stdbool.h>

bool COpenConf( char *filename, conf_obj_t **obj)
{

	int rc = open_conf_obj( filename, obj);
	if( rc == FALSE) {
		return false;
	}

	return true;
}

bool CCloseConf( conf_obj_t **obj)
{
	close_conf_obj( obj );
	return true;
}

#define FOUND_CONF		(1)
#define NOT_FOUND_CONF	(-9999)
bool CGetStrConf( conf_obj_t *obj, char *keyword, char **buf)
{
	int rc;
	char value[1024];

	memset(value, 0x00, sizeof(value));
	rc = obj->on_get_str(obj, keyword, value);	
	if( rc == FOUND_CONF ) {
		*buf = strdup(value);
		return true;
	}
	else return false;
}
/* End of Golang Wrapper */

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

	fq_container_free(&fq_c);
}

/*
** You must close filequeue after calling.
*/
int all_queue_scan_CB_n_times(int n_times, int sleep_time, bool(*your_func_name)(size_t value_sz, void *value) )
{
	linkedlist_t    *ll=NULL;
	fq_container_t *fq_c=NULL;
	fq_logger_t *l=NULL;

	int rc = load_fq_container(l, &fq_c);
	ll = linkedlist_new("fqueue_objects");
	bool tf;
    tf = OpenFileQueues_and_MakeLinkedlist(l, fq_c, ll);
    CHECK(tf == true);

	int i;
	for(i=0;i<n_times;i++) {
		bool result;
		result = linkedlist_callback(ll, your_func_name );
		if( result == false ) break;
		sleep(sleep_time);
	}
	if(ll) linkedlist_free(&ll);

	fq_container_free(&fq_c);

	return 0;
}

int 
ExportFileQueue(fq_logger_t *l, char *path, char *qname)
{
	int rc;
	safe_file_obj_t *obj=NULL;
	fqueue_obj_t *q_obj=NULL;
	int buffer_size = 65536;
	char    *ptr_buf=NULL;

	rc =  OpenFileQueue(l, NULL, NULL, NULL, NULL, path, qname, &q_obj);
	if( rc != TRUE ) {
		return(rc);
	}

	char export_filename[512];
	sprintf(export_filename, "/tmp/%s_export.dat", qname);

	fprintf(stdout, "Exported file: '%s'\n", export_filename);

	rc = open_file_safe(l, export_filename, true, &obj);
	if(rc != TRUE ) {
		printf("erorr: open_file_safe().\n");
		return(rc);
	}

	ptr_buf = calloc(buffer_size, sizeof(unsigned char));
	
	while(1) {
		long l_seq=0L, run_time;
		char unlink_filename[256];
		memset(unlink_filename, 0x00, sizeof(unlink_filename));
		int len;

		len = q_obj->on_de_XA(l, q_obj, ptr_buf, buffer_size, &l_seq, &run_time, unlink_filename);
		if( len == DQ_EMPTY_RETURN_VALUE ) {
			/* end */
			break;
		}
		else if( len < 0 ) {
			q_obj->on_cancel_XA(l, q_obj, l_seq, &run_time);
			fq_log(l, FQ_LOG_ERROR, "on_de_XA() error. rc=[%d].", len);
			break;
		}
		else {
			rc = obj->on_write_b( obj, ptr_buf, len);
			if( rc != TRUE ) {
				if(obj) close_file_safe(&obj);
				return(rc);
			}
			q_obj->on_commit_XA(l, q_obj, l_seq, &run_time, unlink_filename);
		}

	}

	if(obj) close_file_safe(&obj);
	if(q_obj) CloseFileQueue(l, &q_obj);
	return(TRUE);
}
int 
ImportFileQueue(fq_logger_t *l, char *path, char *qname)
{
	return(FALSE);
}

bool Make_all_fq_objects_linkedlist(fq_logger_t *l, linkedlist_t **all_q_ll )
{
	
	linkedlist_t *ll_qobj = NULL;
	FQ_TRACE_ENTER(l);

	/*
	** Make list all queue name
	*/
	linkedlist_t *ll = linkedlist_new("file queue linkedlist.");
	bool tf = MakeLinkedList_filequeues(l, ll);
	if( tf == false) {
		fq_log(l, FQ_LOG_ERROR, "MakeLinkedList_filequeues() failed .");
		FQ_TRACE_EXIT(l);
		return false;
	}

	ll_node_t *p;

	ll_qobj  = linkedlist_new("opened queues linkedlist.");
	
	int q_index =0;
	for(p=ll->head; p!=NULL; p=p->next) {
		fqueue_list_t *tmp=NULL;
		tmp = (fqueue_list_t *) p->value;

		fq_log(l, FQ_LOG_DEBUG, "path=[%s], name=[%s].", tmp->qpath, tmp->qname);

		all_queue_t *qtmp=NULL;
		qtmp = (all_queue_t *)calloc(1, sizeof(all_queue_t));
		qtmp->sn = q_index;

		char *p=NULL;
		int rc;
        if( (p= strstr(tmp->qname, "SHM_")) == NULL ) {
			rc =  OpenFileQueue(l, NULL, NULL, NULL, NULL, tmp->qpath, tmp->qname, &qtmp->obj);
			if( rc != TRUE ) {
				fq_log(l, FQ_LOG_ERROR, "OpenFileQueue('%s', '%s') failed.", tmp->qpath, tmp->qname);
				FQ_TRACE_EXIT(l);
				return(false);
			}
		} 
		else {
			qtmp->shmq_flag = 1;
			rc =  OpenShmQueue(l, tmp->qpath, tmp->qname, &qtmp->obj);
			if( rc != TRUE ) {
				fq_log(l, FQ_LOG_ERROR, "OpenShmQueue('%s', '%s') failed.", tmp->qpath, tmp->qname);
				FQ_TRACE_EXIT(l);
				return(false);
			}
		}

		char key[4];
		sprintf(key, "%03d", qtmp->sn);
		ll_node_t *ll_node=NULL;
		ll_node = linkedlist_put(ll_qobj, key, (void *)qtmp, sizeof(int)+sizeof(int)+sizeof(fqueue_obj_t));
		if( !ll_node ) {
			fq_log(l, FQ_LOG_ERROR, "linkedlist_put() failed.");
			FQ_TRACE_EXIT(l);
			return(false);
		}
		q_index++;

		usleep(1000);
	}

	*all_q_ll = ll_qobj;

	FQ_TRACE_EXIT(l);
	return true;
}

#include <sys/file.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

fqueue_obj_t *find_a_qobj( fq_logger_t *l, char *qpath, char *qname, linkedlist_t *ll )
{
	ll_node_t *p;
	fqueue_obj_t *find_obj = NULL;

	FQ_TRACE_ENTER(l);
	fq_log(l , FQ_LOG_DEBUG, "find: key: %s/%s.", qpath, qname);

	for(p=ll->head; p!=NULL; p=p->next) {
		all_queue_t *tmp;

		size_t   value_sz;
		tmp = (all_queue_t *) p->value;
		value_sz = p->value_sz;

		if(strcmp(qpath, tmp->obj->path) == 0 &&
		   strcmp(qname, tmp->obj->qname) == 0 ) {
			fq_log(l , FQ_LOG_DEBUG, "Selected fqueue object: index=[%d].", tmp->sn);
			FQ_TRACE_EXIT(l);
			return(tmp->obj);
		}
	}
	fq_log(l , FQ_LOG_ERROR, "Not found qobj:  %s/%s,", qpath, qname);
	FQ_TRACE_EXIT(l);
	return (NULL);
}
