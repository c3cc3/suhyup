/* 
** fq_common.c
*/
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include "fq_dir.h"
#include "fq_common.h"
/*
** input string format: yyyymmdd HHMMSS
**						0123456789ABCDE
*/
#if 0
static time_t bin_time( char *src )
{
	struct tm when;
	time_t	bin_time;
	/* char *weekday[]={ "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", NULL}; */


	char yyyy[5], mm[3], dd[3], HH[3], MM[3], SSSS[3];
    long i_yyyy, i_mm, i_dd, i_HH, i_MM, i_SS;


    memcpy(yyyy, &src[0], 4); i_yyyy = atoi(yyyy);
    memcpy(mm, &src[4], 2); i_mm = atoi(mm);
    memcpy(dd, &src[6], 2); i_dd = atoi(dd);

    memcpy(HH, &src[9], 2); i_HH = atoi(HH);
    memcpy(MM, &src[11], 2);i_MM = atoi(MM);
    memcpy(SSSS, &src[13], 2);i_SS = atoi(SSSS);
	
	memset(&when, 0, sizeof(when));

	when.tm_year = i_yyyy-1900;
	when.tm_mon = i_mm-1;
	when.tm_mday = i_dd;

	when.tm_hour = i_HH;
	when.tm_min = i_MM;
	when.tm_sec = i_SS;

	bin_time =  mktime(&when);

	/* printf ("That day is a %s.\n", weekday[when.tm_wday]); */

	return(bin_time);

}
#endif

static int get_mode_typeOfFile(mode_t mode)
{
    return (mode & S_IFMT);
}

static char *
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

    return("unknown file type.");
}

static int get_int_typeOfFile(mode_t mode)
{
    switch (mode & S_IFMT) {
                case S_IFREG:
                        return(1);
                case S_IFDIR:
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

static char *asc_time_1( time_t tval )
{
    char    tmp[128];
    char    *ptr;
    struct tm t;

    localtime_r(&tval, &t);

    sprintf(tmp, "%04d%02d%02d %02d%02d%02d" ,
            t.tm_year+1900, t.tm_mon+1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);

    ptr = strdup(tmp);
    return(ptr);
}

/*
 * last_time : seconds
 */
int auto_delete(const char *s_path, int sure_flag, time_t last_time)
{
    int deleted_cnt=0;
	DIR *dir;
	struct dirent *dirent;

	if ((dir = opendir( s_path )) == NULL) {
		fprintf(stderr, "opendir('%s') failed.\n", s_path);
		return(-1);
	}
	while ((dirent = readdir(dir)) != NULL) {
		char *create_date=NULL;
		time_t  current;
		time_t  diff;
		struct stat fbuf;
		char fullname[512];

		if (strcmp (dirent->d_name,   "." ) == 0 || strcmp (dirent->d_name,   "..") == 0) continue;
		sprintf(fullname, "%s/%s", s_path, dirent->d_name);

		if( stat(fullname, &fbuf) < 0 ) {
			fprintf(stderr, "stat() error. resson=[%s]\n", strerror(errno));
			continue;
		}

		if( get_int_typeOfFile(fbuf.st_mode) != 1 ) {
				continue;
		}

		time(&current);
		diff = current - fbuf.st_ctime;

		if( diff > last_time ) {
				char    command[512];

				create_date = asc_time_1(fbuf.st_ctime);
				(void)fprintf(stdout, "[%-30.30s] (%12ld)bytes. mode=[%d] Type=[%s] create-[%s] \n",
								dirent->d_name, 
								fbuf.st_size,  
								get_mode_typeOfFile(fbuf.st_mode), 
								get_str_typeOfFile(fbuf.st_mode), create_date );

				sprintf(command, "rm %s/%s", s_path, dirent->d_name);
				deleted_cnt++;
	
				if( sure_flag ) {
					int rc;
					rc = system(command);
					CHECK(rc >= 0 );
				}
					
				if(create_date) free(create_date);
		}
		else {
			fprintf(stdout, "[%s]->skipped. Create time is [%ld] seconds ago.\n", dirent->d_name, diff);
		}
    }

	(void)fprintf(stdout, "\x1b[1;35mTOTAL result: deleted (%d). \x1b[0m\n", deleted_cnt);

	closedir(dir);
    return(0);
}
/*
 * last_time : seconds
 * recursive scanning a directory and When it meet a file, call callback function.
 */
// typedef int (*userCBtype)(fq_logger_t *l, char *);

int recursive_dir_CB(fq_logger_t *l, const char *s_path, userCBtype userFP )
{
	DIR *dir;
	struct dirent *dirent;
	int	ftype;

	if ((dir = opendir( s_path )) == NULL) {
		fprintf(stderr, "opendir('%s') failed.\n", s_path);
		return(-1);
	}
	while ((dirent = readdir(dir)) != NULL) {
		struct stat fbuf;
		char fullname[512];

		if (strcmp (dirent->d_name,   "." ) == 0 || strcmp (dirent->d_name,   "..") == 0) continue;
		sprintf(fullname, "%s/%s", s_path, dirent->d_name);

		if( stat(fullname, &fbuf) < 0 ) {
			fprintf(stderr, "stat() error. resson=[%s]\n", strerror(errno));
			continue;
		}

		if( (ftype = get_int_typeOfFile(fbuf.st_mode)) == 1 ) { /* file */
			userFP(l, fullname);
		}
		else if( ftype == 2) { /* directory */
			if( recursive_dir_CB(l, fullname, userFP) < 0 ) {
				break;
			}
			else {
				continue;
			}
		}
    }

	closedir(dir);
    return(0);
}
