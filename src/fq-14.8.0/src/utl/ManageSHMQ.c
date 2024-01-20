/* vi: set sw=4 ts=4: */
/*
 * ManageFQ.c
 * modified: 2014/03/27: add FQ_EXTEND.
 */
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>

#include "fq_manage.h"
#include "fq_common.h"

int main(int ac, char **av)  
{  
	int rc;
	fq_logger_t *l=NULL;
	char *path=NULL;
	char *qname=NULL;
	char *cmd=NULL;
	char *logname = "/tmp/ManageSHMQ.log";
	char *data_home = NULL;

	fq_cmd_t fq_cmd;


	data_home = getenv("FQ_DATA_HOME");
	if( !data_home ) {
		data_home = getenv("HOME");
		if( !data_home ) {
			data_home = strdup("/data");
		}
	}

	printf("Compiled on %s %s\n", __TIME__, __DATE__);
	printf("logging path: [%s]. 에러발생시 로그파일 참조.\n", logname);

	if( ac != 3 && ac != 4 ) {
		printf("\x1b[1;32m Usage \x1b[0m: $ %s [path] [cmd]\n", av[0]);
		printf("Usage: $ %s [path] [qname] [cmd]\n", av[0]);
		printf("Usage: [전체큐를 대상으로 수행]: $ %s %s [create|unlink]\n", av[0], data_home);
		printf("Usage: [개별큐를 대상으로 수행]: $ %s %s TST [create|unlink]\n", av[0], data_home);
		printf("\t\t - create : 큐생성. \n");
		printf("\t\t - unlink : 큐삭제. \n");
		printf("\x1b[1;35m Notice(주의)!! \x1b[0m, 본 유틸리티 사용을 위한 사전 준비사항.\n");
		printf("\t 1) 지정한 디렉토리에 쓰기/읽기 권한이 있어야 함.\n");
		printf("\t 2) 지정한 디렉토리에 라이센스 파일이 존재 하여야 함.\n");
		printf("\t 3) 지정한 디렉토리에 ListFQ.info(전체큐 목록) 파일이 작성되어 있어야 함.\n");
		printf("\t 4) ListFQ.info 파일에 생성하고자 하는 큐의 이름이 등록되어야 함.\n");
		printf("\t 5) 지정한 디렉토리에 각각 큐별 정보파일 (큐이름.Q.info)이 존재 하여야 함.\n");
		return(0);
	}

	rc = fq_open_file_logger(&l, logname, FQ_LOG_TRACE_LEVEL);
	CHECK( rc > 0 );

	if( ac == 3 ) { /* all test */
		path = av[1];
		cmd = av[2];

		if( strcmp( cmd, "create") == 0 ) {
			fq_cmd = SHMQ_CREATE;
		}
		else if( strcmp( cmd, "unlink") == 0 ) {	
			fq_cmd = SHMQ_UNLINK;
		}
		else {
			printf("un-supported cmd.\n");
			return(0);
		}

		rc = fq_manage_all(l, path, fq_cmd);
		CHECK(rc==TRUE);
	}
	else if( ac == 4 ) { /* one by one test */
		path = av[1];
		qname = av[2];
		cmd = av[3];

		if( strcmp( cmd, "create") == 0 ) {
			fq_cmd = SHMQ_CREATE;
		}
		else if( strcmp( cmd, "unlink") == 0 ) {	
			fq_cmd = SHMQ_UNLINK;
		}
		else {
			printf("un-supported cmd.\n");
			return(0);
		}
		rc = fq_manage_one(l, path, qname, fq_cmd);

		CHECK(rc==TRUE);
	}

	fq_close_file_logger(&l);

	printf("Success!! OK.\n");
	return(rc);
}
