/* fq_utl.c */

#define FQ_UTL_C_VERSION "1.0.2"

/* 1.0.2 :2013/09/02  get_seq() -> fq_get_seq() */
/* 1.0.3 :2013/11/09  MakeTestData upgrade */

#include "config.h"
#include "fq_common.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/stat.h>

int MakeTestData(char *fn, int cnt, int msglen)
{
	int i;
	FILE *fp=NULL;
	char *buf=NULL;
	char randbuf[17];
	char	szLen[8];
	long	l_seq;
	char	szSeq[32];

	fp = fopen(fn, "w");
	if(!fp) {
		printf("fopen('%s') error.\n", fn);
		return(-1);
	}

	buf = malloc(msglen+1);

	for(i=0; i<cnt; i++) {
		memset(buf, 0x00, msglen+1);
		memset(buf, 'D', msglen);

		sprintf(szLen, "%06d|", msglen); 
		memcpy(buf, szLen, 7);

		l_seq = fq_get_seq();
		sprintf(szSeq, "%012ld|",l_seq);
		memcpy(buf+7, szSeq, 13);

		get_seed_ratio_rand_str(randbuf, sizeof(randbuf), "0123456789ABCDEF");
		memcpy(&buf[20], randbuf, 16);
		buf[36] = '|';
		buf[msglen-2] = 'Z';
		buf[msglen-1] = 'Z';
		buf[msglen] = 0x00;

		fprintf(fp, "%s\n", buf);
		fflush(fp);
	}

	if(buf) free(buf);

	fclose(fp);
	return(0);
}

int CountFileLine( char *fn, int *LineLength)
{
	int count=0;
    FILE *fp=NULL;
	char	*buf=NULL;
	int	fd;
	int	line;
	struct stat statbuf;
	unsigned char *p=NULL;

    fp = fopen(fn, "rt");
    if(!fp) {
        printf("fopen('%s') error.\n", fn);
        return(-1);
    }

	buf = malloc( 65536+1);
	memset(buf,0x00, 65536+1);

	p = fgets(buf, 65536, fp);
	CHECK( p );

	line = strlen(buf)-1; /* 개행문자를 뺀값 */
	*LineLength = line;
	fclose(fp);

	fd = open(fn ,O_RDONLY);
	fstat(fd, &statbuf);

	count = statbuf.st_size/(line+1); /* 레코트수 계산시에는 개행문자 포함 */

	if(buf) free(buf);
	close(fd);

    return(count);
}

#include <stdio.h>
#include <sys/statvfs.h>

bool checkFileSystemSpace( fq_logger_t *l, const char *path, unsigned long long requiredSpace) 
{
    struct statvfs vfs;

    FQ_TRACE_ENTER(l);

    if (statvfs(path, &vfs) == 0) {
        unsigned long long availableSpace = vfs.f_frsize * vfs.f_bavail;

        if (requiredSpace > availableSpace) {
            printf("Error: Not enough space on the file system.\n");
            fq_log(l, FQ_LOG_ERROR, "Not enough space on the file system.");
            FQ_TRACE_EXIT(l);
            return false;
        } else {
            fq_log(l, FQ_LOG_DEBUG, "Available space: %llu bytes.", availableSpace);
            printf("Available space: %llu bytes\n", availableSpace);
            FQ_TRACE_EXIT(l);
            return true; 
        }
    } else {
        perror("Error checking file system space");
        FQ_TRACE_EXIT(l);
        return false;
    }
}

#if 0
int main() {
    const char *filesystemPath = "/";
    unsigned long long requiredSpace = 1024 * 1024 * 100; // �: 100MB

    if (checkFileSystemSpace(filesystemPath, requiredSpace)) {
        printf("Proceed with the program...\n");
    } else {
        printf("Exiting the program due to insufficient space...\n");
    }

    return 0;
}
#endif
