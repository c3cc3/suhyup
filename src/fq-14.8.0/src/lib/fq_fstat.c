/*
 * fq_fstat.c
 */
#define FQ_FSTAT_C_VERSION "1.0.0"

#include <stdio.h>
#include <pthread.h>
#include <dirent.h>

#include "fq_common.h"
#include "fq_logger.h"

static int
get_mode_typeOfFile(mode_t mode)
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

#ifdef DT_DIR
static int my_select(const struct dirent *s_dirent)
{ /* filter */
	int g_my_select = DT_DIR;
	int g_my_select_mask = 0;

	if(s_dirent != ((struct dirent *)0)) {
		if((s_dirent->d_type & g_my_select) == g_my_select_mask)
			return(1);
	}
	return(0);
}

extern int errno;

int fq_scandir(const char *s_path, int sub_dir_name_include_flag)
{
	int s_check, s_index;
	struct dirent **s_dirlist;

	if( sub_dir_name_include_flag ) {
		s_check = scandir(s_path, (struct dirent ***)(&s_dirlist), 0, alphasort);
		printf("included sub-directoies.\n");
	}
	else {
		s_check = scandir(s_path, (struct dirent ***)(&s_dirlist), my_select, alphasort);
		printf("is not included sub-directoies.\n");
	}

	if(s_check >= 0) {
		(void)fprintf(stdout, "scandir result=%d\n", s_check);
		for(s_index = 0;s_index < s_check;s_index++) {
			struct stat fbuf;
			char fullname[512];

			sprintf(fullname, "%s/%s", s_path, (char *)s_dirlist[s_index]->d_name);

			if( stat(fullname, &fbuf) < 0 ) {
				fprintf(stderr, "stat() error. resson=[%s]\n", strerror(errno));
				continue;
			}

			(void)fprintf(stdout, "[%-30.30s] (%12ld)bytes. mode=[%d] Type=[%s] \n", 
					(char *)s_dirlist[s_index]->d_name, fbuf.st_size, get_mode_typeOfFile(fbuf.st_mode), get_str_typeOfFile(fbuf.st_mode) );

			free((void *)s_dirlist[s_index]);
		}
		
		if(s_dirlist != ((void *)0)) {
			free((void *)s_dirlist);
		}
		(void)fprintf(stdout, "\x1b[1;35mTOTAL result\x1b[0m=%d\n", s_check);
	}
	return(s_check);
}
#endif

#if 0
int main(int ac, char **av)
{
	int n;

	while ( 1 )  {
		char	*path=NULL;
		char    noti_msg[40], syscmd[128];

		path = getenv(ENV_FQ_DATA_HOME);

		if( (n=fq_scandir(path, 1)) > 0) {
			fprintf(stdout, "파일 [%d]건 발견!\n",  n);
			sprintf(noti_msg, "국민은행 거래 미확인 파일 [%d]건 발견!\n",  n);
		}
		else {
			fprintf(stdout, "국민은행 정상.\n");
		}
		usleep(100000);
	}

	return(0);
}
#endif
