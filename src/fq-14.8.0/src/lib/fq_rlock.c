/*
 * fq_rlock.c
 */


#define FQ_RLOCK_C_VERSION "1.0.0"

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <strings.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h> /* for chmod() */

#include "fq_common.h"

int 
create_lock_table(char *path, char *lock_table_name, int max_records) 
{
	int fd;
	void *p;
	char	lock_pathfile[128];

	sprintf(lock_pathfile, "%s/.%s.lock", path, lock_table_name);

	if( !is_file(lock_pathfile) ) {
		if( (fd = open(lock_pathfile, O_CREAT|O_EXCL|O_RDWR, 0666)) < 0){
			fprintf(stderr, "[%s] open(CREATE) error. reason=[%s]\n", lock_pathfile, strerror(errno));
			return(-1);
		}
		chmod(lock_pathfile, 0666);

		p = malloc(max_records);
		memset(p, 0x00, max_records);

		if( write(fd, p, max_records) < 0 ) {
			fprintf(stderr, "[%s] file write error. reason=[%s]\n", lock_pathfile, strerror(errno));
			return(-1);
		}
		close(fd);
	}
	return(0);
}

int 
unlink_lock_table(char *lock_pathfile) 
{
	if (!is_file(lock_pathfile) ) {
		fprintf(stderr, "[%s] does not exist.\n", lock_pathfile);
		return(-1);
	}

	unlink(lock_pathfile);
	return(0);
}

int 
open_lock_table(char *lock_pathfile)
{
	int fd;

	if( (fd=open(lock_pathfile, O_RDWR)) < 0 ) {
		fprintf(stderr, "lock file open error.[%s] reason=[%s]\n", lock_pathfile, strerror(errno));
		return(-1);
	}
	return(fd);
}

int 
close_lock_table(int fd)
{
	if(fd > 0 ) {
		close(fd);
	}
	return(0);
}

int 
r_lock_table( int fd,  int index )
{
 	int status;

	if( fd <= 0 ) {
		fprintf(stderr, "fd error. fd=[%d]\n", fd);
		return(-1);
	}
	if( lseek(fd, index, SEEK_SET) < 0 ) {
		fprintf(stderr, "lseek( %d ) error. reason=[%s]\n", index, strerror(errno));
		return(-1);
	}

	status = lockf(fd, F_LOCK, 1);
	if(status < 0 ) {
		fprintf(stderr, "lockf() failed. reason=[%s]\n", strerror(errno));
		return(-1);
	}
	return(0);
}

int 
r_unlock_table( int fd, int rec_index )
{
	if( fd <= 0 ) {
		fprintf(stderr, "fd error. fd=[%d]\n", fd);
		return(-1);
	}
	if( lseek(fd, rec_index, SEEK_SET) < 0 ) {
		fprintf(stderr, "lseek( %d ) error. reason=[%s]\n", rec_index, strerror(errno));
		return(-1);
	}
	return(lockf(fd, F_ULOCK, 1));
} 

/*
** Record locking function.
** return value: > 0 : offset
                 < 0 : error
*/
int r_lock( int fd,  size_t rec_length, int index )
{
 	int status;
	size_t offset;

	if( fd <= 0 ) {
		fprintf(stderr, "fd error. fd=[%d]\n", fd);
        return(-1);
    }
	offset = index * rec_length;

	if( lseek(fd, offset, SEEK_SET) < 0 ) {
		fprintf(stderr, "lseek( %ld ) error. reason=[%s]\n", (long int)offset, strerror(errno));
		return(-1);
	}

	status = lockf(fd, F_LOCK, rec_length);
	if(status < 0 ) {
		fprintf(stderr, "lockf() failed. reason=[%s]\n", strerror(errno));
		return(-1);
	}
	return(0);
}

int r_unlock( int fd, size_t rec_length, int rec_index )
{
	size_t offset;

	if( fd <= 0 ) {
		fprintf(stderr, "fd error. fd=[%d]\n", fd);
        return(-1);
    }

	offset = rec_index * rec_length;

	if( lseek(fd, offset, SEEK_SET) < 0 ) {
		fprintf(stderr, "lseek( %ld ) error. reason=[%s]\n", (long int)offset, strerror(errno));
		return(-1);
	}

	return(lockf(fd, F_ULOCK, rec_length));
} 
