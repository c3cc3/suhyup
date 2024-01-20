/*
** fq_hashobj.c
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include "fq_logger.h"
#include "fq_hashobj.h"
#include "fq_flock.h"
#include "fq_common.h"
#include "hash_info.h"
#include "hash_info_param.h"

static int on_put( fq_logger_t *l, hashmap_obj_t *obj, char *key, void* value, int value_size);
static int on_insert( fq_logger_t *l, hashmap_obj_t *obj, char *key, void *value, int value_size);
static int on_get( fq_logger_t *l, hashmap_obj_t *obj, char *key, void** value);
static int on_get_and_del( fq_logger_t *l, hashmap_obj_t *obj, char *key, void* value);
static int on_remove( fq_logger_t *l, hashmap_obj_t *obj, char *key);
static int on_clean_table( fq_logger_t *l, hashmap_obj_t *obj);
static int on_get_by_index( fq_logger_t *l, hashmap_obj_t *obj, int index, char *key, void* value);
static int open_hash_data_obj( fq_logger_t *l, char *path, char *name, hash_head_obj_t *h_obj, hash_data_obj_t **obj);
static int open_hash_head_obj( fq_logger_t *l, char *path, char *hashname, hash_head_obj_t **obj);
static int close_hash_head_obj(fq_logger_t *l,  hash_head_obj_t **obj);
static int close_hash_data_obj(fq_logger_t *l,  hash_data_obj_t **obj, int mapping_size);

static unsigned long crc32_tab[] = {
      0x00000000L, 0x77073096L, 0xee0e612cL, 0x990951baL, 0x076dc419L,
      0x706af48fL, 0xe963a535L, 0x9e6495a3L, 0x0edb8832L, 0x79dcb8a4L,
      0xe0d5e91eL, 0x97d2d988L, 0x09b64c2bL, 0x7eb17cbdL, 0xe7b82d07L,
      0x90bf1d91L, 0x1db71064L, 0x6ab020f2L, 0xf3b97148L, 0x84be41deL,
      0x1adad47dL, 0x6ddde4ebL, 0xf4d4b551L, 0x83d385c7L, 0x136c9856L,
      0x646ba8c0L, 0xfd62f97aL, 0x8a65c9ecL, 0x14015c4fL, 0x63066cd9L,
      0xfa0f3d63L, 0x8d080df5L, 0x3b6e20c8L, 0x4c69105eL, 0xd56041e4L,
      0xa2677172L, 0x3c03e4d1L, 0x4b04d447L, 0xd20d85fdL, 0xa50ab56bL,
      0x35b5a8faL, 0x42b2986cL, 0xdbbbc9d6L, 0xacbcf940L, 0x32d86ce3L,
      0x45df5c75L, 0xdcd60dcfL, 0xabd13d59L, 0x26d930acL, 0x51de003aL,
      0xc8d75180L, 0xbfd06116L, 0x21b4f4b5L, 0x56b3c423L, 0xcfba9599L,
      0xb8bda50fL, 0x2802b89eL, 0x5f058808L, 0xc60cd9b2L, 0xb10be924L,
      0x2f6f7c87L, 0x58684c11L, 0xc1611dabL, 0xb6662d3dL, 0x76dc4190L,
      0x01db7106L, 0x98d220bcL, 0xefd5102aL, 0x71b18589L, 0x06b6b51fL,
      0x9fbfe4a5L, 0xe8b8d433L, 0x7807c9a2L, 0x0f00f934L, 0x9609a88eL,
      0xe10e9818L, 0x7f6a0dbbL, 0x086d3d2dL, 0x91646c97L, 0xe6635c01L,
      0x6b6b51f4L, 0x1c6c6162L, 0x856530d8L, 0xf262004eL, 0x6c0695edL,
      0x1b01a57bL, 0x8208f4c1L, 0xf50fc457L, 0x65b0d9c6L, 0x12b7e950L,
      0x8bbeb8eaL, 0xfcb9887cL, 0x62dd1ddfL, 0x15da2d49L, 0x8cd37cf3L,
      0xfbd44c65L, 0x4db26158L, 0x3ab551ceL, 0xa3bc0074L, 0xd4bb30e2L,
      0x4adfa541L, 0x3dd895d7L, 0xa4d1c46dL, 0xd3d6f4fbL, 0x4369e96aL,
      0x346ed9fcL, 0xad678846L, 0xda60b8d0L, 0x44042d73L, 0x33031de5L,
      0xaa0a4c5fL, 0xdd0d7cc9L, 0x5005713cL, 0x270241aaL, 0xbe0b1010L,
      0xc90c2086L, 0x5768b525L, 0x206f85b3L, 0xb966d409L, 0xce61e49fL,
      0x5edef90eL, 0x29d9c998L, 0xb0d09822L, 0xc7d7a8b4L, 0x59b33d17L,
      0x2eb40d81L, 0xb7bd5c3bL, 0xc0ba6cadL, 0xedb88320L, 0x9abfb3b6L,
      0x03b6e20cL, 0x74b1d29aL, 0xead54739L, 0x9dd277afL, 0x04db2615L,
      0x73dc1683L, 0xe3630b12L, 0x94643b84L, 0x0d6d6a3eL, 0x7a6a5aa8L,
      0xe40ecf0bL, 0x9309ff9dL, 0x0a00ae27L, 0x7d079eb1L, 0xf00f9344L,
      0x8708a3d2L, 0x1e01f268L, 0x6906c2feL, 0xf762575dL, 0x806567cbL,
      0x196c3671L, 0x6e6b06e7L, 0xfed41b76L, 0x89d32be0L, 0x10da7a5aL,
      0x67dd4accL, 0xf9b9df6fL, 0x8ebeeff9L, 0x17b7be43L, 0x60b08ed5L,
      0xd6d6a3e8L, 0xa1d1937eL, 0x38d8c2c4L, 0x4fdff252L, 0xd1bb67f1L,
      0xa6bc5767L, 0x3fb506ddL, 0x48b2364bL, 0xd80d2bdaL, 0xaf0a1b4cL,
      0x36034af6L, 0x41047a60L, 0xdf60efc3L, 0xa867df55L, 0x316e8eefL,
      0x4669be79L, 0xcb61b38cL, 0xbc66831aL, 0x256fd2a0L, 0x5268e236L,
      0xcc0c7795L, 0xbb0b4703L, 0x220216b9L, 0x5505262fL, 0xc5ba3bbeL,
      0xb2bd0b28L, 0x2bb45a92L, 0x5cb36a04L, 0xc2d7ffa7L, 0xb5d0cf31L,
      0x2cd99e8bL, 0x5bdeae1dL, 0x9b64c2b0L, 0xec63f226L, 0x756aa39cL,
      0x026d930aL, 0x9c0906a9L, 0xeb0e363fL, 0x72076785L, 0x05005713L,
      0x95bf4a82L, 0xe2b87a14L, 0x7bb12baeL, 0x0cb61b38L, 0x92d28e9bL,
      0xe5d5be0dL, 0x7cdcefb7L, 0x0bdbdf21L, 0x86d3d2d4L, 0xf1d4e242L,
      0x68ddb3f8L, 0x1fda836eL, 0x81be16cdL, 0xf6b9265bL, 0x6fb077e1L,
      0x18b74777L, 0x88085ae6L, 0xff0f6a70L, 0x66063bcaL, 0x11010b5cL,
      0x8f659effL, 0xf862ae69L, 0x616bffd3L, 0x166ccf45L, 0xa00ae278L,
      0xd70dd2eeL, 0x4e048354L, 0x3903b3c2L, 0xa7672661L, 0xd06016f7L,
      0x4969474dL, 0x3e6e77dbL, 0xaed16a4aL, 0xd9d65adcL, 0x40df0b66L,
      0x37d83bf0L, 0xa9bcae53L, 0xdebb9ec5L, 0x47b2cf7fL, 0x30b5ffe9L,
      0xbdbdf21cL, 0xcabac28aL, 0x53b39330L, 0x24b4a3a6L, 0xbad03605L,
      0xcdd70693L, 0x54de5729L, 0x23d967bfL, 0xb3667a2eL, 0xc4614ab8L,
      0x5d681b02L, 0x2a6f2b94L, 0xb40bbe37L, 0xc30c8ea1L, 0x5a05df1bL,
      0x2d02ef8dL
};

/* Return a 32-bit CRC of the contents of the buffer. */
static unsigned long crc32(const unsigned char *s, unsigned int len)
{
  unsigned int i;
  unsigned long crc32val;
  
  crc32val = 0;
  for (i = 0;  i < len;  i ++)
    {
      crc32val =
	crc32_tab[(crc32val ^ s[i]) & 0xff] ^
	  (crc32val >> 8);
    }
  return crc32val;
}

/*
 * Hashing function for a string
 */
static unsigned int hashmap_hash_int(int table_size, char* keystring){

    unsigned long key = crc32((unsigned char*)(keystring), strlen(keystring));

	/* Robert Jenkins' 32 bit Mix Function */
	key += (key << 12);
	key ^= (key >> 22);
	key += (key << 4);
	key ^= (key >> 9);
	key += (key << 10);
	key ^= (key >> 2);
	key += (key << 7);
	key ^= (key >> 12);

	/* Knuth's Multiplicative Method */
	key = (key >> 3) * 2654435761UL;

	return key % table_size;
}

/*
 * Return the integer of the location in data
 * to store the point to the item, or MAP_FULL.
 */
static int hashmap_hash( hashmap_obj_t *obj, char* key, int *new_flag){
	int curr;
	int i;
	int record_size;
	long long offset;
	unsigned char *r=NULL;
	char *d_key=NULL;

	FQ_TRACE_ENTER(obj->l);

	/* If full, return immediately */
	if(obj->h->h->curr_elements >= (obj->h->h->table_length/2)) {
		fq_log(obj->l, FQ_LOG_ERROR, "hashmap is full. Please increase hash table length.");
		FQ_TRACE_EXIT(obj->l);
		return MAP_FULL;
	}

	record_size = obj->h->h->max_key_length + obj->h->h->max_data_length;

	/* Find the best index */
	curr = hashmap_hash_int(obj->h->h->table_length, key);

	fq_log(obj->l, FQ_LOG_DEBUG, "hashmap_hash_int() result. curr=[%d]\n", curr);
	/* Linear probing */
	for(i = 0; i< MAX_CHAIN_LENGTH; i++){
		int key_len;

		offset = curr * record_size;
		fq_log(obj->l, FQ_LOG_DEBUG, "offset=[%lld]\n", offset);

		/* read from file key and data */
		r = calloc(record_size, sizeof(unsigned char ));
		memcpy( r, obj->d->d+offset, record_size);

		d_key = calloc(obj->h->h->max_key_length+1, sizeof(char));
		memcpy(d_key, r, obj->h->h->max_key_length);

		if(d_key[0] == 0x00) {
			*new_flag = 1; /* inserted */
			fq_log(obj->l, FQ_LOG_DEBUG, "chain[%d]: new_key: [%d]-th is empty record.", i, curr);

			SAFE_FREE(d_key);
			SAFE_FREE(r);
			FQ_TRACE_EXIT(obj->l);
			return curr;
		}

		key_len = strlen(key);
		if(strncmp(d_key, key, key_len)==0) {
			*new_flag=0; /* replaced */
			fq_log(obj->l, FQ_LOG_DEBUG, "chain[%d]: key collision: already key exist: [%d]-th is same record.", i, curr);
			SAFE_FREE(d_key);
			SAFE_FREE(r);
			FQ_TRACE_EXIT(obj->l);
			return curr;
		}

		SAFE_FREE(d_key);
		SAFE_FREE(r);

		curr = (curr + 1) % obj->h->h->table_length;
	}

	SAFE_FREE(d_key);
	SAFE_FREE(r);

	FQ_TRACE_EXIT(obj->l);
	return MAP_FULL;
}
static int hashmap_hash_no_collision( hashmap_obj_t *obj, char* key, int *new_flag){
	int curr;
	int i;
	int record_size;
	long long offset;
	unsigned char *r=NULL;
	char *d_key=NULL;

	FQ_TRACE_ENTER(obj->l);

	/* If full, return immediately */
	if(obj->h->h->curr_elements >= (obj->h->h->table_length/2)) {
		fq_log(obj->l, FQ_LOG_ERROR, "hashmap is full. Please increase hash table length.");
		FQ_TRACE_EXIT(obj->l);
		return MAP_FULL;
	}

	record_size = obj->h->h->max_key_length + obj->h->h->max_data_length;

	/* Find the best index */
	curr = hashmap_hash_int(obj->h->h->table_length, key);

	fq_log(obj->l, FQ_LOG_DEBUG, "hashmap_hash_int() result. curr=[%d]\n", curr);
	/* Linear probing */
	for(i = 0; i< MAX_CHAIN_LENGTH; i++){
		int key_len;

		offset = curr * record_size;
		fq_log(obj->l, FQ_LOG_DEBUG, "offset=[%lld]\n", offset);

		/* read from file key and data */
		r = calloc(record_size, sizeof(unsigned char ));
		memcpy( r, obj->d->d+offset, record_size);

		d_key = calloc(obj->h->h->max_key_length+1, sizeof(char));
		memcpy(d_key, r, obj->h->h->max_key_length);

		if(d_key[0] == 0x00) {
			*new_flag = 1;
			fq_log(obj->l, FQ_LOG_DEBUG, "chain[%d]: new_key: [%d]-th is empty record.", i, curr);

			SAFE_FREE(d_key);
			SAFE_FREE(r);
			FQ_TRACE_EXIT(obj->l);
			return curr;
		}

		key_len = strlen(key);
		if(strncmp(d_key, key, key_len)==0) {
			fq_log(obj->l, FQ_LOG_DEBUG, "chain[%d]: already key exist: [%d]-th is same record.", i, curr);
			SAFE_FREE(d_key);
			SAFE_FREE(r);
			FQ_TRACE_EXIT(obj->l);
			return MAP_FULL; /* don't replace */
		}

		SAFE_FREE(d_key);
		SAFE_FREE(r);

		curr = (curr + 1) % obj->h->h->table_length;
	}

	SAFE_FREE(d_key);
	SAFE_FREE(r);

	FQ_TRACE_EXIT(obj->l);
	return MAP_FULL;
}

static int
unlink_headfile(fq_logger_t *l, char *path, char *name)
{
	char filename[256];

	FQ_TRACE_ENTER(l);
	sprintf(filename, "%s/%s.%s", path, name, HASHMAP_HEADER_EXTENSION_NAME);

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

static int
unlink_datafile(fq_logger_t *l, char *path, char *name)
{
	char filename[256];

	FQ_TRACE_ENTER(l);

	sprintf(filename, "%s/%s.%s", path, name, HASHMAP_DATA_EXTENSION_NAME);

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

int UnlinkHashMapFiles( fq_logger_t *l, char *path, char *name)
{
	char pathfile[128];
	int rc;
	char flock_pathfile[256];

	FQ_TRACE_ENTER(l);

	unlink_headfile(l, path, name);
	step_print("- Delete hash header file ", "OK");

	unlink_datafile(l, path, name);
	step_print("- Delete hash data file ", "OK");

	sprintf(pathfile, "%s/.%s.flock", path, name);
	unlink_flock(l, pathfile);
	step_print("- Delete flock file ", "OK");
	
	FQ_TRACE_EXIT(l);
    return(TRUE);
}

int OpenHashMapFiles(fq_logger_t *l, char *path, char *name, hashmap_obj_t **obj)
{
	hashmap_obj_t *rc=NULL;

	rc = (hashmap_obj_t *)calloc(1,  sizeof(hashmap_obj_t));
	if(rc) {
		int ret;
		rc->path = strdup(path);
		rc->name = strdup(name);

		rc->h = NULL;
		
		ret = open_hash_head_obj(l, path, name, &rc->h);
		if( ret != TRUE ) {
			fq_log(l, FQ_LOG_ERROR, "open_hash_head_obj('%s', '%s') error.", path, name);
			goto return_FALSE;
		}
	
		rc->d = NULL;

		ret = open_hash_data_obj(l, path, name, rc->h, &rc->d);
		if( ret != TRUE ) {
			fq_log(l, FQ_LOG_ERROR, "open_hash_data_obj('%s', '%s') error.", path, name);
			goto return_FALSE;
		}

		ret = open_flock_obj( l, path, name, ETC_FLOCK, &rc->flock);
		if( ret != TRUE ) {
			fq_log(l, FQ_LOG_ERROR, "open_flock_obj('%s', '%s') error.", path, name);
			goto return_FALSE;
        }

		rc->on_put = on_put;
		rc->on_insert = on_insert;
        rc->on_get = on_get;
        rc->on_get_and_del = on_get_and_del;
        rc->on_remove = on_remove;
        rc->on_clean_table = on_clean_table;
		rc->on_get_by_index = on_get_by_index;
	
		rc->l = l;

		*obj = rc;

		FQ_TRACE_EXIT(l);
		return(TRUE);
	}

return_FALSE:
    if( rc->d ) {
		 int mapping_size;
		mapping_size = (rc->h->h->max_key_length + rc->h->h->max_data_length) * rc->h->h->table_length;
        close_hash_data_obj(l, &rc->d, mapping_size);
    }

	if( rc->h ) {
        close_hash_head_obj(l, &rc->h);
    }

	SAFE_FREE(rc->path);
	SAFE_FREE(rc->name);

	rc->on_put = NULL;
	rc->on_insert = NULL;
    rc->on_get = NULL;
    rc->on_get_and_del = NULL;
    rc->on_remove = NULL;
    rc->on_clean_table = NULL;
    rc->on_get_by_index = NULL;
		
	SAFE_FREE(rc);

	FQ_TRACE_EXIT(l);
	return(FALSE);
}

int CloseHashMapFiles(fq_logger_t *l, hashmap_obj_t **obj)
{
	int rc;

	FQ_TRACE_ENTER(l);

	SAFE_FREE((*obj)->path);
	SAFE_FREE((*obj)->name);

	int mapping_size;
	mapping_size = ((*obj)->h->h->max_key_length + (*obj)->h->h->max_data_length) * (*obj)->h->h->table_length;
	// printf("mapping_size = [%d]\n", mapping_size);

	if( (*obj)->d ) {
		rc = close_hash_data_obj(l, &(*obj)->d, mapping_size );
		if( rc != TRUE ) {
			fq_log(l, FQ_LOG_ERROR, "close_hash_data_obj() error.");
			goto return_FALSE;
		}
	}
	(*obj)->d = NULL;

	if( (*obj)->h ) {
		rc = close_hash_head_obj(l, &(*obj)->h );
		if( rc != TRUE ) {
			fq_log(l, FQ_LOG_ERROR, "close_hash_head_obj() error.");
			goto return_FALSE;
		}
	}
	(*obj)->h = NULL;

	if((*obj)->flock ) {
		close_flock_obj(l, &(*obj)->flock);
	}
	(*obj)->flock = NULL;

	SAFE_FREE(*obj);

	FQ_TRACE_EXIT(l);
	return(TRUE);

return_FALSE:
	FQ_TRACE_EXIT(l);
	return(FALSE);
}


static int 
create_dummy_file(fq_logger_t *l, const char *path, const char *fname, size_t size)
{
	int		fd;
	int		rc = -1;
	char	filename[256];
	mode_t	mode=S_IRUSR| S_IWUSR| S_IRGRP| S_IWGRP| S_IROTH| S_IWOTH;

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
		fq_log(l, FQ_LOG_ERROR, "open(%s) error. [%s]. Check your filename or If permission error, Change mode of dir to 777.", filename, strerror(errno));
		goto L0;
	}
	fq_log(l, FQ_LOG_INFO, "dummy file create open OK.[%s]", filename); 

	if( ftruncate(fd, size) < 0 ) {
		fq_log(l,FQ_LOG_ERROR,"ftruncate() error. [%s] size=[%ld].", strerror(errno), size);
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
static int 
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
		fq_log(l,FQ_LOG_ERROR,"ftruncate() error. [%s] size=[%ld].", strerror(errno), size);
		goto L0;
	}
	fq_log(l, FQ_LOG_INFO, "dummy file re-ftruncate OK.[%s]", filename); 

	rc = fd;
L0:
	FQ_TRACE_EXIT(l);
	return(rc);
}

static void *
file_mmapping(fq_logger_t *l, int fd, size_t space_size, off_t from_offset)
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
file_munmap(fq_logger_t *l, void *addr, size_t length)
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
			char *hashname, 
			char *desc,
			int  table_length,
			int	 max_key_length,
			int	 max_data_length)
{
	hashmap_header_t  *h=NULL;
	int 	rtn=-1;
	char	filename[1024];
	char	fname[256];
	int		fd=0;
	
	FQ_TRACE_ENTER(l);

	sprintf(fname, "%s.%s", hashname, HASHMAP_HEADER_EXTENSION_NAME);
	sprintf(filename, "%s/%s", path, fname);

	if( is_file(filename) ) { /* checks whether the file exists. */
		fq_log(l, FQ_LOG_ERROR, "Already the file[%s] exists.", filename);
		goto L0;
	}

	if( (fd=create_dummy_file(l, path, fname, sizeof(hashmap_header_t)))  < 0) {
		fq_log(l, FQ_LOG_ERROR, "create_dummy_file() error. path=[%s] name=[%s]", path, fname);
		goto L0;
	}

	if( (h = (hashmap_header_t *)file_mmapping(l, fd, sizeof(hashmap_header_t), (off_t)0)) == NULL) {
        fq_log(l, FQ_LOG_ERROR, "file_mmapping() error.");
        goto L0;
    }

	// sprintf(h->path, "%s", path);
    // sprintf(h->hashname, "%s", hashname);
    // sprintf(h->desc, "%s", desc);
    // sprintf(h->pathfile, "%s", filename);

	strcpy(h->path, path);
    strcpy(h->hashname, hashname);
    strcpy(h->desc,  desc);
    strcpy(h->pathfile, filename);

    sprintf(h->data_pathfile, "%s/%s.%s", path, hashname, HASHMAP_DATA_EXTENSION_NAME);

	h->table_length = table_length;
	h->curr_elements = 0;
	h->max_key_length = max_key_length;
	h->max_data_length = max_data_length;
	h->data_file_size =  (h->max_key_length + h->max_data_length) * h->table_length;

	chmod( filename , 0666);

	rtn = 0;
L0:
	if(fd > 0) close(fd);

	if((fd > 0) && (rtn != 0) ) {
		if( unlink(filename) < 0 ) {
			fq_log(l, FQ_LOG_ERROR, "unlink([%s]) error.", filename);
		}
	}

	if(h)  file_munmap(l, (void *)h, sizeof(hashmap_header_t));
	
	FQ_TRACE_EXIT(l);

	return(rtn);
}
static int 
on_print( fq_logger_t *l, hash_head_obj_t *h_obj)
{
	FQ_TRACE_ENTER(l);

	printf("\t ----< header file infomation >-----\n");
	fprintf(stdout, "\t\t- hashname =[%s]\n", h_obj->h->hashname);
	fprintf(stdout, "\t\t- desc =[%s]\n", h_obj->h->desc);
	fprintf(stdout, "\t\t- path =[%s]\n", h_obj->h->path);
	fprintf(stdout, "\t\t- pathfile =[%s]\n", h_obj->h->pathfile);
	fprintf(stdout, "\t\t- data_pathfile =[%s]\n", h_obj->h->data_pathfile);

	fprintf(stdout, "\t\t- table_length = [%d]\n", h_obj->h->table_length);
	fprintf(stdout, "\t\t- curr_elements = [%d]\n", h_obj->h->curr_elements);
	fprintf(stdout, "\t\t- max_key_length = [%d]\n", h_obj->h->max_key_length);
	fprintf(stdout, "\t\t- max_data_length = [%d]\n", h_obj->h->max_data_length);
	fprintf(stdout, "\t\t- data_file_size = [%d]\n", h_obj->h->data_file_size);

	printf("\t ----< data file status >-----\n");
    printf("\t\t - Last Access:        %s", ctime(&h_obj->st.st_atime));
    printf("\t\t - Last Modification:  %s", ctime(&h_obj->st.st_mtime));
    printf("\t\t - Last I-Node Change: %s", ctime(&h_obj->st.st_ctime));
	
	FQ_TRACE_EXIT(l);
	return(TRUE);
}

static int 
open_hash_head_obj( fq_logger_t *l, char *path, char *hashname, hash_head_obj_t **obj)
{
	hash_head_obj_t *rc=NULL;

	FQ_TRACE_ENTER(l);

	rc = (hash_head_obj_t *)calloc(1, sizeof(hash_head_obj_t));

	if(rc) {
		char filename[256];

		sprintf(filename, "%s/%s.%s", path, hashname, HASHMAP_HEADER_EXTENSION_NAME);
		rc->fullname = strdup(filename);

		if( !can_access_file(l, rc->fullname)) {
			fq_log(l, FQ_LOG_ERROR, "hash headfile[%s] can not access.", rc->fullname);
			goto return_FALSE;
		}

		rc->fd = 0;
		if( (rc->fd=open(rc->fullname, O_RDWR|O_SYNC, 0666)) < 0 ) {
			fq_log(l, FQ_LOG_ERROR, "hash headfile[%s] can not open.", rc->fullname);
			goto return_FALSE;
		}

		if(( fstat(rc->fd, &rc->st)) < 0 ) {
			fq_log(l, FQ_LOG_ERROR, "fstat() error. reason=[%s]", strerror(errno));
			goto return_FALSE;
		}

		rc->h = NULL;
		if( (rc->h = (hashmap_header_t *)file_mmapping(l, rc->fd, sizeof(hashmap_header_t), (off_t)0)) == NULL ) {
			fq_log(l, FQ_LOG_ERROR, "header file file_mmapping() error.");
			goto return_FALSE;
		}
#if  0
		rc->flock = NULL;
		if( open_flock_obj(l, path, name, ETC_FLOCK, &rc->flock) != TRUE ) {
			fq_log(l, FQ_LOG_ERROR, "open_flock_obj() error.");
			goto return_FALSE;
		}
#endif
		rc->l = l;
		rc->on_print = on_print;

		*obj = rc;

		FQ_TRACE_EXIT(l);

		return(TRUE);
	}

return_FALSE:
	if(rc->fd) close(rc->fd);
	if(rc->h)  {
		file_munmap(l, (void *)rc->h, sizeof(hashmap_header_t));
		rc->h = NULL;
	}
	if( rc ) free(rc);

	*obj = NULL;
	FQ_TRACE_EXIT(l);
	return(FALSE);
}
static int 
close_hash_head_obj(fq_logger_t *l,  hash_head_obj_t **obj)
{
	FQ_TRACE_ENTER(l);

	SAFE_FREE((*obj)->fullname);
		
	if( (*obj)->fd > 0 ) {
		close( (*obj)->fd );
		(*obj)->fd = -1;
	}

	if( (*obj)->h ) {
		file_munmap(l, (void *)(*obj)->h, sizeof(hashmap_header_t));
		(*obj)->h = NULL;
	}

	SAFE_FREE(*obj);

	FQ_TRACE_EXIT(l);
	return(TRUE);
}

static int
open_hash_data_obj( fq_logger_t *l, char *path, char *name, hash_head_obj_t *h_obj, hash_data_obj_t **obj)
{
	hash_data_obj_t *rc=NULL;
	
	FQ_TRACE_ENTER(l);

	rc = (hash_data_obj_t *)calloc( 1, sizeof(hash_data_obj_t));

	if( rc ) {
		char filename[256];

		sprintf(filename, "%s/%s.%s", path, name, HASHMAP_DATA_EXTENSION_NAME);
		rc->fullname = strdup(filename);

		if( !can_access_file(l, rc->fullname)) {
            fq_log(l, FQ_LOG_ERROR, "Hash data file[%s] can not access.", rc->fullname);
            goto return_FALSE;
        }
        rc->fd = 0;
        if( (rc->fd=open(rc->fullname, O_RDWR|O_SYNC, 0666)) < 0 ) {
            fq_log(l, FQ_LOG_ERROR, "Queue headfile[%s] can not open.", rc->fullname);
            goto return_FALSE;
        }

        if(( fstat(rc->fd, &rc->st)) < 0 ) {
            fq_log(l, FQ_LOG_ERROR, "fstat() error. reason=[%s]", strerror(errno));
            goto return_FALSE;
        }
		

		rc->d = NULL;

		// printf("mapping size -> %s: %d\n", __FUNCTION__,  h_obj->h->data_file_size);

		if( (rc->d = (void *)file_mmapping(l, rc->fd, h_obj->h->data_file_size, (off_t)0)) == NULL ) {
			fq_log(l, FQ_LOG_ERROR, "data file file_mmapping() error.");
			goto return_FALSE;
		}

		rc->l = l;

        *obj = rc;
        FQ_TRACE_EXIT(l);
        return(TRUE);
    }

return_FALSE:
    if(rc->fd) close(rc->fd);

    if(rc ) free(rc);

    FQ_TRACE_EXIT(l);
    return(FALSE);
}

static int 
close_hash_data_obj(fq_logger_t *l,  hash_data_obj_t **obj, int mapping_size)
{
	FQ_TRACE_ENTER(l);

	FREE((*obj)->fullname);

	if( (*obj)->fd > 0 ) {
		close( (*obj)->fd );
		(*obj)->fd = -1;
	}

	// printf("%s: %d\n", __FUNCTION__, mapping_size);

	if( (*obj)->d ) {
		file_munmap(l, (void *)(*obj)->d, mapping_size);
		(*obj)->d = NULL;
	}


	FREE(*obj);

	FQ_TRACE_EXIT(l);
	return(TRUE);
}

static int
create_datafile(fq_logger_t *l, hash_head_obj_t *h_obj)
{
	int fd;
	char	filename[256], name[256];

	FQ_TRACE_ENTER(l);

	sprintf(filename, "%s/%s.%s", h_obj->h->path, h_obj->h->hashname, HASHMAP_DATA_EXTENSION_NAME);
	sprintf(name, "%s.%s", h_obj->h->hashname, HASHMAP_DATA_EXTENSION_NAME);

	if( is_file(filename) ) {
		fq_log(l, FQ_LOG_ERROR, "Already the file[%s] exists.", filename);
		goto L0;
	}
	
	if( (fd=create_dummy_file(l, h_obj->h->path, name, h_obj->h->data_file_size )) < 0) { 
		fq_log(l, FQ_LOG_ERROR, "create_dummy_file() error. path=[%s] file=[%s]", h_obj->h->path, name);
		goto L0;
	}

	FQ_TRACE_EXIT(l);
    return(TRUE);
	
L0:
	FQ_TRACE_EXIT(l);
	return(FALSE);
}

int CreateHashMapFiles( fq_logger_t *l, char *path, char *name)
{
	hash_info_t	*_hash_info = NULL;
	char info_filename[256];
	hash_head_obj_t *h=NULL;
	char    info_path[256];
    char    info_hashname[256];
    char    info_table_length[16];
    char    info_max_key_length[16];
    char    info_max_data_length[16];
    char    info_desc[256];
	int i_table_length;
	int i_max_key_length;
	int i_max_data_length;
	int rc;

	FQ_TRACE_ENTER(l);

	sprintf(info_filename, "%s/%s.%s", path, name, HASHMAP_INFO_EXTENSION);
	if( !can_access_file(l, info_filename)) {
		fq_log(l, FQ_LOG_ERROR, "cannot find info_file[%s]", info_filename);
		step_print("- hash info file loading.", "NOK");
		goto return_FALSE;
	}

	_hash_info = new_hash_info(info_filename);
	if( !_hash_info ) {
		fq_log(l, FQ_LOG_ERROR, "new_hash_info() error.");
		step_print("- hash info file loading.", "NOK");
		goto return_FALSE;
    }

	if( load_hash_info_param(_hash_info, info_filename) < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "load_hash_info_param('%s') error.", info_filename);
		step_print("- hash info file loading.", "NOK");
		goto return_FALSE;
    }

	step_print("- hash info file loading.", "OK");

	get_hash_info(_hash_info, "PATH", info_path);
	get_hash_info(_hash_info, "HASHNAME", info_hashname);
	get_hash_info(_hash_info, "DESC", info_desc);
	get_hash_info(_hash_info, "TABLE_LENGTH", info_table_length);
	get_hash_info(_hash_info, "MAX_KEY_LENGTH", info_max_key_length);
	get_hash_info(_hash_info, "MAX_DATA_LENGTH", info_max_data_length);

	i_table_length = atoi(info_table_length);
	i_max_key_length = atoi(info_max_key_length);
	i_max_data_length = atoi(info_max_data_length);

	rc = create_headerfile(l, 
		info_path, 
		info_hashname, 
		info_desc, 
		i_table_length, 
		i_max_key_length, 
		i_max_data_length);

	if( rc < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "create_headerfile('%s', '%s') error.", 
			info_path, info_hashname);
		step_print("- create hash header file.", "NOK");
		return(-1);
	}
	step_print("- create hash header file.", "OK");

	rc=open_hash_head_obj(l, info_path, info_hashname, &h);
	if( rc != TRUE ) {
		fq_log(l, FQ_LOG_ERROR, "open_hash_head_obj('%s', '%s') error.",
			info_path, info_hashname);
		step_print("- open hash header file.", "NOK");
		return(-1);
	}

	step_print("- open hash header file.", "OK");

	// h->on_print(l, h);
	
	rc = create_datafile(l, h);
	if( rc != TRUE ) {
		fq_log(l, FQ_LOG_ERROR, "create_datafile() error.");
		step_print("- create hash data file.", "NOK");
		return(-1);
	}

	step_print("- create hash data file.", "OK");

	if( h ) close_hash_head_obj(l, &h);
	
	if(_hash_info) free_hash_info(&_hash_info);

	step_print("- Finalize", "OK");

	FQ_TRACE_EXIT(l);
	return(TRUE);

return_FALSE:
	if( h ) close_hash_head_obj(l, &h);
	if(_hash_info) free_hash_info(&_hash_info);

	FQ_TRACE_EXIT(l);
	return(FALSE);
}

int CreateHashMapFiles_noinfo( fq_logger_t *l, 
		char *path, char *name, char *desc, int table_length,
		int max_key_length, int max_data_length)

{
	hash_head_obj_t *h=NULL;
	int rc;

	FQ_TRACE_ENTER(l);

	rc = create_headerfile(l, 
		path, 
		name, 
		desc, 
		table_length, 
		max_key_length, 
		max_data_length);

	if( rc < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "create_headerfile('%s', '%s') error.", 
			path, name);
		return(-1);
	}

	rc=open_hash_head_obj(l, path, name, &h);
	if( rc != TRUE ) {
		fq_log(l, FQ_LOG_ERROR, "open_hash_head_obj('%s', '%s') error.",
			path, name);
		return(-1);
	}

	// h->on_print(l, h);
	
	rc = create_datafile(l, h);
	if( rc != TRUE ) {
		fq_log(l, FQ_LOG_ERROR, "create_datafile() error.");
		return(-1);
	}

	if( h ) close_hash_head_obj(l, &h);
	
	FQ_TRACE_EXIT(l);
	return(TRUE);

return_FALSE:
	if( h ) close_hash_head_obj(l, &h);

	FQ_TRACE_EXIT(l);
	return(FALSE);
}

int hashmap_extend(hashmap_obj_t *obj)
{
	char new_name[128];
	hashmap_obj_t *new_obj=NULL;
	int i;
	int record_size;
	long long offset;
	int rc;

	FQ_TRACE_ENTER(obj->l);

	sprintf(new_name, "%s.new", obj->name);
	rc = CreateHashMapFiles_noinfo( obj->l, obj->path, new_name, "rehash file", obj->h->h->table_length*2, obj->h->h->max_key_length, obj->h->h->max_data_length);
	if( rc < 0 ) {
		fq_log(obj->l, FQ_LOG_ERROR, "CreateHashMapFiles('%s/%s') failed.", obj->path, new_name);
		FQ_TRACE_EXIT(obj->l);
		return MAP_OMEM;
	}

	rc = OpenHashMapFiles(obj->l, obj->path, new_name, &new_obj);
	if( rc != TRUE) {
		fq_log(obj->l, FQ_LOG_ERROR, "OpenHashMapFiles('%s/%s') failed.", obj->path, new_name);
		FQ_TRACE_EXIT(obj->l);
        return MAP_OMEM;
    }

	record_size = obj->h->h->max_key_length + obj->h->h->max_data_length;

	for( i=0; i<obj->h->h->table_length; i++) {
		char *key=NULL;
		void *value=NULL;

		offset = i * record_size;

		key = calloc(obj->h->h->max_key_length, sizeof(char));
		memcpy(key, obj->d->d+offset, obj->h->h->max_key_length);

		value = calloc(obj->h->h->max_data_length, sizeof(unsigned char));
		memcpy(value, obj->d->d+offset+obj->h->h->max_key_length, obj->h->h->max_data_length);

		if( key[0] != 0x00 ) {
			rc = new_obj->on_put(obj->l, new_obj, key, value, obj->h->h->max_data_length);
			if( rc != MAP_OK ) {
				fq_log(obj->l, FQ_LOG_ERROR, "new_obj->on_put() error.");
				CloseHashMapFiles(obj->l, &new_obj);
				SAFE_FREE(key);
				SAFE_FREE(value);
				FQ_TRACE_EXIT(obj->l);
				return MAP_OMEM;
			}
		}

		SAFE_FREE(key);
		SAFE_FREE(value);
	}
	new_obj->h->h->curr_elements = obj->h->h->curr_elements;
	
	FQ_TRACE_EXIT(obj->l);
	return MAP_OK;
}

static int
on_put( fq_logger_t *l, hashmap_obj_t *obj, char *key, void *value, int value_size)
{
	int index;
	long long offset;
	int new_flag = 0;

	FQ_TRACE_ENTER(l);

	obj->flock->on_flock(obj->flock);

	index = hashmap_hash(obj, key, &new_flag);

#if 0
	while(index == MAP_FULL){
		if (hashmap_rehash(obj) == MAP_OMEM) {
			return MAP_OMEM;
		}
		index = hashmap_hash(obj, key, &new_flag);
	}
#else
	if( index == MAP_FULL ) {
		fq_log(l, FQ_LOG_DEBUG, "hashmap_hash() error.(FULL/Key Colision) key=[%s].", key);
		obj->flock->on_funlock(obj->flock);
		FQ_TRACE_EXIT(l);
		return(MAP_FULL);
	}
#endif

	// printf("index=[%d] key=[%s]\n", index, key);

	offset = index * (obj->h->h->max_key_length + obj->h->h->max_data_length);
	fq_log(l, FQ_LOG_DEBUG, "index=[%d], offset=[%lld]\n", index, offset);

	/* Copy key */
	memset(obj->d->d+offset, 0x00, obj->h->h->max_key_length);
	memcpy(obj->d->d+offset, key, strlen(key));
	
	/* Copy value */
	memset(obj->d->d+offset+obj->h->h->max_key_length, 0x00, obj->h->h->max_data_length);
	memcpy(obj->d->d+offset+obj->h->h->max_key_length, value, value_size);

	if( new_flag == 1 )
		obj->h->h->curr_elements++;

	obj->flock->on_funlock(obj->flock);

	FQ_TRACE_EXIT(l);
	return(MAP_OK);
}

static int
on_insert( fq_logger_t *l, hashmap_obj_t *obj, char *key, void *value, int value_size)
{
	int index;
	long long offset;
	int new_flag = 0;

	FQ_TRACE_ENTER(l);

	obj->flock->on_flock(obj->flock);

	index = hashmap_hash_no_collision(obj, key, &new_flag);

#if 0
	while(index == MAP_FULL){
		if (hashmap_rehash(obj) == MAP_OMEM) {
			return MAP_OMEM;
		}
		index = hashmap_hash(obj, key, &new_flag);
	}
#else
	if( index == MAP_FULL ) {
		fq_log(l, FQ_LOG_ERROR, "hashmap_hash() error.(FULL/Key Colision) key=[%s].", key);
		obj->flock->on_funlock(obj->flock);
		FQ_TRACE_EXIT(l);
		return(MAP_FULL);
	}
#endif

	// printf("index=[%d] key=[%s]\n", index, key);

	offset = index * (obj->h->h->max_key_length + obj->h->h->max_data_length);

	fq_log(l, FQ_LOG_DEBUG, "index=[%d], offset=[%lld]\n", index, offset);

	/* Copy key */
	memset(obj->d->d+offset, 0x00, obj->h->h->max_key_length);
	memcpy(obj->d->d+offset, key, strlen(key));
	/* Copy value */
	memset(obj->d->d+offset+obj->h->h->max_key_length, 0x00, obj->h->h->max_data_length);
	memcpy(obj->d->d+offset+obj->h->h->max_key_length, value, value_size);

	msync(obj->d->d+offset+obj->h->h->max_key_length, value_size, MS_SYNC|MS_INVALIDATE); /*  MS_INVALIDATE */

	if( new_flag == 1 )
		obj->h->h->curr_elements++;

	obj->flock->on_funlock(obj->flock);

	FQ_TRACE_EXIT(l);
	return(MAP_OK);
}

int
on_get( fq_logger_t *l, hashmap_obj_t *obj, char *key, void **value)
{
	int curr;
	int i;
	int record_size;
	long long offset;
	unsigned char *r=NULL;
	char *d_key=NULL;

	FQ_TRACE_ENTER(l);

	record_size = obj->h->h->max_key_length + obj->h->h->max_data_length;
	curr = hashmap_hash_int(obj->h->h->table_length, key);

	fq_log(obj->l, FQ_LOG_DEBUG, "index = [%d]", curr);

	for(i = 0; i<MAX_CHAIN_LENGTH; i++){
		int key_len;

		offset = curr * record_size;
		fq_log(obj->l, FQ_LOG_DEBUG, "offset=[%lld]\n", offset);

		r = calloc(record_size, sizeof(unsigned char ));
		memcpy( r, obj->d->d+offset, record_size);

		d_key = calloc(obj->h->h->max_key_length+1, sizeof(char));
		memcpy(d_key, r, obj->h->h->max_key_length);

		fq_log(obj->l, FQ_LOG_DEBUG, "d_key=[%s]n", d_key);

		if(d_key[0] == 0x00) {
			SAFE_FREE(r);
			SAFE_FREE(d_key);
			continue;
		}

		key_len = strlen(key);
		if(strncmp(d_key, key, key_len)==0) {
			fq_log(obj->l, FQ_LOG_DEBUG, "in [%d]-th recored, [%d]-th chain found key.", curr, i);

			memcpy(*value, obj->d->d+offset+obj->h->h->max_key_length, obj->h->h->max_data_length);

			if(r) free(r);
			if(d_key) free(d_key);
			FQ_TRACE_EXIT(obj->l);

			return(MAP_OK);
		}
		else {
			SAFE_FREE(r);
			SAFE_FREE(d_key);
		}
		curr = (curr + 1) % obj->h->h->table_length;
	}

	if(r) free(r);
	if(d_key) free(d_key);

	FQ_TRACE_EXIT(l);
	return(MAP_MISSING);
}

int
on_get_and_del( fq_logger_t *l, hashmap_obj_t *obj, char *key, void *value)
{
	int curr;
	int i;
	int record_size;
	long long offset;
	unsigned char *r=NULL;
	char *d_key=NULL;

	FQ_TRACE_ENTER(l);

	record_size = obj->h->h->max_key_length + obj->h->h->max_data_length;
	curr = hashmap_hash_int(obj->h->h->table_length, key);

	fq_log(obj->l, FQ_LOG_DEBUG, "index = [%d]", curr);

	for(i = 0; i<MAX_CHAIN_LENGTH; i++){
		int key_len;

		offset = curr * record_size;
		fq_log(obj->l, FQ_LOG_DEBUG, "offset=[%lld]\n", offset);

		r = calloc(record_size, sizeof(unsigned char ));
		memcpy( r, obj->d->d+offset, record_size);

		d_key = calloc(obj->h->h->max_key_length+1, sizeof(char));
		memcpy(d_key, r, obj->h->h->max_key_length);

		fq_log(obj->l, FQ_LOG_DEBUG, "d_key=[%s]n", d_key);

		if(d_key[0] == 0x00) {
			SAFE_FREE(r);
			SAFE_FREE(d_key);
			continue;
		}

		key_len = strlen(key);
		if(strncmp(d_key, key, key_len)==0) {
			fq_log(obj->l, FQ_LOG_DEBUG, "in [%d]-th recored, [%d]-th chain found key.", curr, i);
			/* copy a value */
			memcpy(value, obj->d->d+offset+obj->h->h->max_key_length, obj->h->h->max_data_length);

			/* delete a key */
			memset(obj->d->d+offset, 0x00, obj->h->h->max_key_length);
			obj->h->h->curr_elements--;

			if(r) free(r);
			if(d_key) free(d_key);
			FQ_TRACE_EXIT(obj->l);

			return(MAP_OK);
		}
		curr = (curr + 1) % obj->h->h->table_length;
	}

	if(r) free(r);
	if(d_key) free(d_key);

	FQ_TRACE_EXIT(l);
	return(MAP_MISSING);
}
int
on_get_by_index( fq_logger_t *l, hashmap_obj_t *obj, int index, char *key, void *value)
{
	int record_size;
	long long offset;
	unsigned char *r=NULL;

	FQ_TRACE_ENTER(l);

	record_size = obj->h->h->max_key_length + obj->h->h->max_data_length;

	fq_log(obj->l, FQ_LOG_DEBUG, "index = [%d]", index);

	offset = index * record_size;
	fq_log(obj->l, FQ_LOG_DEBUG, "offset=[%lld]\n", offset);

	r = calloc(record_size, sizeof(unsigned char ));
	memcpy( r, obj->d->d+offset, record_size);

	if(r[0] != 0x00) {
		memcpy(key, obj->d->d+offset, obj->h->h->max_key_length);
		memcpy(value, obj->d->d+offset+obj->h->h->max_key_length, obj->h->h->max_data_length);

		SAFE_FREE(r);
		FQ_TRACE_EXIT(obj->l);
		return(MAP_OK);
	}

	FQ_TRACE_EXIT(l);
	return(MAP_MISSING);
}

int
on_remove( fq_logger_t *l, hashmap_obj_t *obj, char *key)
{
	int curr;
	int i;
	int record_size;
	long long offset;
	unsigned char *r=NULL;
	char *d_key=NULL;

	FQ_TRACE_ENTER(l);

	obj->flock->on_flock(obj->flock);

	record_size = obj->h->h->max_key_length + obj->h->h->max_data_length;
	curr = hashmap_hash_int(obj->h->h->table_length, key);

	for(i = 0; i<MAX_CHAIN_LENGTH; i++){
		int key_len;

		offset = curr * record_size;
		fq_log(obj->l, FQ_LOG_DEBUG, "offset=[%lld]\n", offset);

		r = calloc(record_size, sizeof(unsigned char ));
		memcpy( r, obj->d->d+offset, record_size);

		d_key = calloc(obj->h->h->max_key_length+1, sizeof(char));
		memcpy(d_key, r, obj->h->h->max_key_length);

		if(d_key[0] == 0x00) {
			SAFE_FREE(r);
			SAFE_FREE(d_key);
			continue;
		}
		key_len = strlen(key);
		if(strncmp(d_key,key, key_len)==0) {
			fq_log(obj->l, FQ_LOG_DEBUG, "in [%d]-th recored, [%d]-th chain found key.", curr, i);

			memset(obj->d->d+offset, 0x00, record_size);

			obj->h->h->curr_elements--;

			if(d_key) free(d_key);

			obj->flock->on_funlock(obj->flock);

			FQ_TRACE_EXIT(obj->l);

			return(MAP_OK);
		}
		curr = (curr + 1) % obj->h->h->table_length;
	}

	if(d_key) free(d_key);

	obj->flock->on_funlock(obj->flock);

	FQ_TRACE_EXIT(l);
	return(MAP_MISSING);
}
int
on_clean_table( fq_logger_t *l, hashmap_obj_t *obj)
{
	int i;
	int record_size;
	long long offset;

	FQ_TRACE_ENTER(l);

	obj->flock->on_flock(obj->flock);

	record_size = obj->h->h->max_key_length + obj->h->h->max_data_length;
	for( i=0; i<obj->h->h->table_length; i++) {
		offset = i * record_size;
		memset(obj->d->d+offset, 0x00, record_size);
	}
	obj->h->h->curr_elements = 0;

	obj->flock->on_funlock(obj->flock);
	FQ_TRACE_EXIT(l);
	return(MAP_OK);
}

int CleanHashMap( fq_logger_t *l, char *path, char *hashname)
{
	int rc;
	hashmap_obj_t *obj=NULL;

	FQ_TRACE_ENTER(l);

	rc = OpenHashMapFiles(l, path, hashname, &obj);
	if( rc < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "OpenHashMapFiles('%s', '%s') error.",
			path, hashname);
		step_print("- Open hash header file ", "NOK");
		return(FALSE);
	}
	step_print("- Open hash header file.", "OK");

	rc = obj->on_clean_table(l, obj);
	if( rc != MAP_OK ) {
		fq_log(l, FQ_LOG_ERROR, "on_clean_table() error.");
		CloseHashMapFiles(l, &obj);
		step_print("- Clean hash table file", "NOK");
		FQ_TRACE_EXIT(l);
		return(FALSE);
	}
	step_print("- Clean hash table file.", "OK");

	rc = CloseHashMapFiles(l, &obj);

	step_print("- Finalize.", "OK");
	FQ_TRACE_EXIT(l);
	return(TRUE);
}

int ExtendHashMap( fq_logger_t *l, char *path, char *hashname)
{
	int rc;
	hashmap_obj_t *obj=NULL;
	char    mv_header[256];
    char    mv_data[256];

	FQ_TRACE_ENTER(l);
	
	rc = OpenHashMapFiles(l, path, hashname, &obj);
	if( rc < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "OpenHashMapFiles('%s', '%s') error.",
			path, hashname);
		return(FALSE);
	}
	step_print("- Open hashmap file.", "OK");

	rc = hashmap_extend(obj);
	if( rc != MAP_OK) {
		fq_log(l, FQ_LOG_ERROR, "hashmap_extend() error.");
		CloseHashMapFiles(l, &obj);
		FQ_TRACE_EXIT(l);
		return(FALSE);
	}
	step_print("- hashmap_expend function processing.", "OK");

	rc = CloseHashMapFiles(l, &obj);

	sprintf(mv_header, "mv %s/%s.new.%s %s/%s.%s", 
		path, hashname, HASHMAP_HEADER_EXTENSION_NAME, path, hashname, HASHMAP_HEADER_EXTENSION_NAME);

    rc = system(mv_header);
	if( rc != -1);
	step_print("- moving header file.", "OK");

    sprintf(mv_data, "mv %s/%s.new.%s %s/%s.%s", 
		path, hashname, HASHMAP_DATA_EXTENSION_NAME, path, hashname, HASHMAP_DATA_EXTENSION_NAME);
    rc = system(mv_data);
	if( rc != -1);
	step_print("- moving data file.", "OK");

	step_print("- Finalize.", "OK");
	FQ_TRACE_EXIT(l);

	return(TRUE);
}

int ViewHashMap( fq_logger_t *l, char *path, char *hashname)
{
	int rc;
	hashmap_obj_t *obj=NULL;

	FQ_TRACE_ENTER(l);
	
	rc = OpenHashMapFiles(l, path, hashname, &obj);
	if( rc < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "OpenHashMapFiles('%s', '%s') error.",
			path, hashname);
		return(FALSE);
	}
	step_print("- Open hashmap file.", "OK");

	obj->h->on_print(l, obj->h);

	step_print("- hashmap on_print() function processing.", "OK");

	rc = CloseHashMapFiles(l, &obj);

	step_print("- Finalize.", "OK");
	FQ_TRACE_EXIT(l);

	return(TRUE);
}
int ScanHashMap( fq_logger_t *l, char *path, char *hashname)
{
	int rc;
	hashmap_obj_t *obj=NULL;

	FQ_TRACE_ENTER(l);
	
	rc = OpenHashMapFiles(l, path, hashname, &obj);
	if( rc < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "OpenHashMapFiles('%s', '%s') error.",
			path, hashname);
		return(FALSE);
	}
	step_print("- Open hashmap file.", "OK");

	rc = Scan_Hash( l, obj);
	if( rc == FALSE ) {
		fq_log(l, FQ_LOG_ERROR,  "Scan_Hash() failed." );
		CloseHashMapFiles(l, &obj);
		FQ_TRACE_EXIT(l);
		return(FALSE);
	}

	step_print("- hashmap on_print() function processing.", "OK");

	rc = CloseHashMapFiles(l, &obj);

	step_print("- Finalize.", "OK");
	FQ_TRACE_EXIT(l);

	return(TRUE);
}

int PutHash( fq_logger_t *l, hashmap_obj_t *hash_obj, char *key, char *value)
{
	int rc;
	
	FQ_TRACE_ENTER(l);
	
	fq_log(l, FQ_LOG_DEBUG, "key=[%s] value=[%s]", key, value);

	rc = hash_obj->on_put(l, hash_obj, key, value, strlen(value));
	if( rc == MAP_FULL ) {
		fq_log(l, FQ_LOG_ERROR, "'%s', '%s': MAP_FULL.", hash_obj->h->h->path, hash_obj->h->h->hashname);
		return(FALSE);
	}
	else {
		fq_log(l, FQ_LOG_DEBUG, "'%s', '%s': Put OK. key=[%s]", hash_obj->h->h->path, hash_obj->h->h->hashname, key);
	}
	FQ_TRACE_EXIT(l);

	return(TRUE);
}
int InsertHash( fq_logger_t *l, hashmap_obj_t *hash_obj, char *key, char *value)
{
	int rc;
	
	FQ_TRACE_ENTER(l);
	
	fq_log(l, FQ_LOG_INFO, "key=[%s] value=[%s]", key, value);

	rc = hash_obj->on_insert(l, hash_obj, key, value, strlen(value));
	if( rc == MAP_FULL ) {
		fq_log(l, FQ_LOG_ERROR, "'%s', '%s': MAP_FULL.", hash_obj->h->h->path, hash_obj->h->h->hashname);
		return(FALSE);
	}
	else {
		fq_log(l, FQ_LOG_DEBUG, "'%s', '%s': Put OK. key=[%s]", hash_obj->h->h->path, hash_obj->h->h->hashname, key);
	}
	FQ_TRACE_EXIT(l);

	return(TRUE);
}

/* Get but do not delete(remain) */
int GetHash( fq_logger_t *l, hashmap_obj_t *hash_obj, char *key, char **value)
{
	int rc;
	void *out=NULL;
	
	FQ_TRACE_ENTER(l);

	out = calloc(hash_obj->h->h->max_data_length+1, sizeof(unsigned char));
	rc = hash_obj->on_get(l, hash_obj, key, &out);
	if( rc == MAP_MISSING ) {
		fq_log(l, FQ_LOG_INFO, "Not found: '%s', '%s': key(%s) missing in Hashmap.", hash_obj->h->h->path, hash_obj->h->h->hashname, key);
		SAFE_FREE(out);
		return(FALSE);
	}
	else {
		fq_log(l, FQ_LOG_DEBUG, "'%s', '%s': Get OK. key=[%s] value=[%s]", hash_obj->h->h->path, hash_obj->h->h->hashname, key, out);
		//memcpy(*value, out, hash_obj->h->h->max_data_length);
		*value = strdup(out);
	}
	SAFE_FREE(out);
	FQ_TRACE_EXIT(l);

	return(TRUE);
}
int Sure_GetHash( fq_logger_t *l, hashmap_obj_t *hash_obj, char *key, char **value)
{
	int rc;
	char *out=NULL;
	
	FQ_TRACE_ENTER(l);

reget:
	rc = GetHash(l, hash_obj, key, &out);
	if( rc == TRUE ) {
		if( out[0] == 0x00 ) {
			SAFE_FREE(out);
			usleep(100000);
			goto reget;
		}
		else {
			*value = strdup(out);
		}
	}
	else {
		return FALSE;
	}

	SAFE_FREE(out);
	return TRUE;
}

/* Get and Delete */
int GetDelHash( fq_logger_t *l, hashmap_obj_t *hash_obj, char *key, char **value)
{
	int rc;
	void *out=NULL;
	
	FQ_TRACE_ENTER(l);

	out = calloc(hash_obj->h->h->max_data_length+1, sizeof(unsigned char));
	rc = hash_obj->on_get_and_del(l, hash_obj, key, out);
	if( rc == MAP_MISSING ) {
		fq_log(l, FQ_LOG_ERROR, "'%s', '%s': key(%s) missing in Hashmap.", hash_obj->h->h->path, hash_obj->h->h->hashname, key);
		SAFE_FREE(out);
		return(FALSE);
	}
	else {
		fq_log(l, FQ_LOG_DEBUG, "'%s', '%s': Get OK. key=[%s] value=[%s]", hash_obj->h->h->path, hash_obj->h->h->hashname, key, out);
		*value = strdup(out);
	}
	SAFE_FREE(out);
	FQ_TRACE_EXIT(l);

	return(TRUE);
}

int DeleteHash( fq_logger_t *l, hashmap_obj_t *hash_obj, char *key)
{
	int rc;
	void *out=NULL;
	
	FQ_TRACE_ENTER(l);

	rc = hash_obj->on_remove(l, hash_obj, key);
	if( rc == MAP_MISSING ) {
		fq_log(l, FQ_LOG_ERROR, "'%s', '%s': key(%s) missing in Hashmap.", hash_obj->h->h->path, hash_obj->h->h->hashname, key);
		return(FALSE);
	}
	else {
		fq_log(l, FQ_LOG_DEBUG, "'%s', '%s': Delete OK. key=[%s]", hash_obj->h->h->path, hash_obj->h->h->hashname, key);
	}
	FQ_TRACE_EXIT(l);

	return(TRUE);
}

/* 
** 이 함수는 코드맵 파일을 HashMap에 로딩한다.
** PutHash() 함수를 사용하여 이미 들어가 있는경우 update로 동작한다.
*/
int LoadingHashMap(fq_logger_t *l, hashmap_obj_t *hash_obj,  char *filename)
{
	int rc;
	codemap_t *p=NULL;
	fqlist_t *t;
	tlist_t *k;
	node_t *n;

	FQ_TRACE_ENTER(l);

	p = new_codemap(l, "config");
	if( (rc=read_codemap(l, p, filename)) < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "read_codemap('%s') error.", filename);
		rc = -1;
		goto end;
	}
	
    fq_log(l, FQ_LOG_DEBUG, "Codemap '%s':", p->name);
    if ( p->map ) {
		 	fq_log(l, FQ_LOG_DEBUG, " list '%s' type '%c' contains %d elements.",
		 	p->map->list->key, p->map->list->type, p->map->list->length);

			for( n = p->map->list->head; n != NULL; n = n->next) {
				int rc;

				fq_log(l, FQ_LOG_DEBUG, "  %s = '%s\' %d.", n->key, n->value, n->flag);
				rc = PutHash(l, hash_obj, n->key, n->value);
				// rc = InsertHash(l, hash_obj, n->key, n->value);
				// if(rc == TRUE) fq_log(l, FQ_LOG_DEBUG, "new insert success.");
				// else fq_log(l, FQ_LOG_DEBUG, "Already exist key='%s'.", n->key);
			}
    }
	rc = p->map->list->length;

	free_codemap(l, &p);
end:
	FQ_TRACE_EXIT(l);
	return(rc);
}

int Export_Hash( fq_logger_t *l, hashmap_obj_t *hash_obj, char *export_filename)
{
	int rc;
	char *key=NULL;
	void *out=NULL;
	int		index;
	int 	export_count=0;
	FILE *fp=NULL;
	
	FQ_TRACE_ENTER(l);

	fp = fopen(export_filename, "w");
    if( fp == NULL ) {
        fprintf(stderr, "fopen('%s') error.\n", export_filename);
        FQ_TRACE_EXIT(l);
        return FALSE;
    }

	for(index=0; index<hash_obj->h->h->table_length; index++) {
		unsigned char *p;

		key = calloc(hash_obj->h->h->max_key_length+1, sizeof(char));
		out = calloc(hash_obj->h->h->max_data_length+1, sizeof(unsigned char));

		rc = hash_obj->on_get_by_index(l, hash_obj, index, key, out);
		if( rc == MAP_MISSING ) {
			SAFE_FREE(key);
			SAFE_FREE(out);
			continue;
		}
		else {
			printf("index=[%d] key=[%s]\n", index, key);
			p = (unsigned char *)out;
			fprintf(fp, "%s|%s\n", key, p);
			fflush(fp);
			export_count++;
		}
	}
	fprintf(stdout, "Total exported words : %d \n", export_count);
	FQ_TRACE_EXIT(l);

	if(fp) fclose(fp);

	return TRUE;
}

int Scan_Hash( fq_logger_t *l, hashmap_obj_t *hash_obj)
{
	int rc;
	char *key=NULL;
	void *out=NULL;
	int		index;
	int 	export_count=0;
	
	FQ_TRACE_ENTER(l);

	for(index=0; index<hash_obj->h->h->table_length; index++) {
		unsigned char *p;

		key = calloc(hash_obj->h->h->max_key_length+1, sizeof(char));
		out = calloc(hash_obj->h->h->max_data_length+1, sizeof(unsigned char));

		rc = hash_obj->on_get_by_index(l, hash_obj, index, key, out);
		if( rc == MAP_MISSING ) {
			SAFE_FREE(key);
			SAFE_FREE(out);
			continue;
		}
		else {
			printf("index=[%d] key=[%s]\n", index, key);
			p = (unsigned char *)out;
			fprintf(stdout, "%s|%s\n", key, p);
			export_count++;
		}
	}
	fprintf(stdout, "Total scanned records : %d \n", export_count);
	FQ_TRACE_EXIT(l);

	return TRUE;
}

int Import_Hash( fq_logger_t *l, char *pathfile, hashmap_obj_t *hash_obj)
{
	FILE *fp=NULL;
	char keyb[64+1], datab[512+1];
	char buf[1024];
	int 	dup_cnt=0, import_cnt=0;
	int rc;

	FQ_TRACE_ENTER(l);

	fp = fopen(pathfile, "r");
	if( fp == NULL ) {
		fprintf(stderr, "fopen('%s') error.\n", pathfile);
		FQ_TRACE_EXIT(l);
		return FALSE;
	}

	while(fgets( buf, 1024, fp)) {
		int keylen, datalen;
		char	*p=NULL;

		/* This removes a newline character */
		buf[strlen(buf)-1] = 0x00;

		memset(keyb, 0x00,sizeof(keyb));
		memset(datab, 0x00,sizeof(datab));

		/* parsing */
		p = strstr(buf, "|");
		if(p == NULL ) continue;

		keylen = strlen(buf) - strlen(p);
		datalen = strlen(buf) - keylen;
		// printf("keylen = [%d] , datalen = [%d]\n", keylen, datalen);

		memcpy(keyb, buf, keylen);
		memcpy(datab, p+1, datalen);

		rc = hash_obj->on_put(l, hash_obj, keyb, datab, sizeof(datab));
		if( rc != MAP_OK ) {
			fq_log(l, FQ_LOG_ERROR, "on_put('%s') error.", keyb);
			return FALSE;
		}
		else {
			// printf("key (%s) insert.\n", keyb);
			import_cnt++;
		}
	}
	printf("Total inserted keys=[%d]\n", import_cnt);

	fclose(fp);

	FQ_TRACE_EXIT(l);
	return TRUE;
}
