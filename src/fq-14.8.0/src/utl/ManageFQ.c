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
	char *logname = "/tmp/ManageFQ.log";
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
		printf("Usage: [전체큐를 대상으로 수행]: $ %s %s [create|unlink|disable|enable|reset|flush|info|diag|extend]\n", av[0], data_home);
		printf("Usage: [개별큐를 대상으로 수행]: $ %s %s TST [create|unlink|disable|enable|reset|flush|info|skip|diag|extend]\n", av[0], data_home);
		printf("\t\t - create : 큐생성. \n");
		printf("\t\t - unlink : 큐삭제. \n");
		printf("\t\t - disable: 큐 사용중지. \n");
		printf("\t\t - enable : 큐 사용재개. \n");
		printf("\t\t - reset  : 큐 정보초기화. \n");
		printf("\t\t - flush  : 큐 정보 꺼내어 버리기. \n");
		printf("\t\t - info  : 큐 정보 보기. \n");
		printf("\t\t - skip   : 강제 큐 빼냄.(deQ에 문제발생시 사용, 개별큐를 대상으로만 가능) \n");
		printf("\t\t - diag   : 큐 진단. \n\n");
		printf("\t\t - extend   : 큐 확장. \n\n");
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
			fq_cmd = FQ_CREATE;
		}
		else if( strcmp( cmd, "unlink") == 0 ) {	
			fq_cmd = FQ_UNLINK;
		}
		else if( strcmp( cmd, "disable") == 0 ) {	
			fq_cmd = FQ_DISABLE;
		}
		else if( strcmp( cmd, "enable") == 0 ) {	
			fq_cmd = FQ_ENABLE;
		}
		else if( strcmp( cmd, "reset") == 0 ) {	
			fq_cmd = FQ_RESET;
		}
		else if( strcmp( cmd, "flush") == 0 ) {	
			fq_cmd = FQ_FLUSH;
		}
		else if( strcmp( cmd, "info") == 0 ) {	
			fq_cmd = FQ_INFO;
		}
		/* 
		** skip은 전체큐를 대상으로 사용할 수 없는 옵션이기 때문에..
		else if( strcmp( cmd, "skip") == 0 ) {	
			fq_cmd = FQ_FORCE_SKIP;
		}
		*/
		else if( strcmp( cmd, "diag") == 0 ) {	
			fq_cmd = FQ_DIAG;
		}
		else if( strcmp( cmd, "extend") == 0 ) {	
			fq_cmd = FQ_EXTEND;
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
			fq_cmd = FQ_CREATE;
		}
		else if( strcmp( cmd, "unlink") == 0 ) {	
			fq_cmd = FQ_UNLINK;
		}
		else if( strcmp( cmd, "disable") == 0 ) {	
			fq_cmd = FQ_DISABLE;
		}
		else if( strcmp( cmd, "enable") == 0 ) {	
			fq_cmd = FQ_ENABLE;
		}
		else if( strcmp( cmd, "reset") == 0 ) {	
			fq_cmd = FQ_RESET;
		}
		else if( strcmp( cmd, "flush") == 0 ) {	
			fq_cmd = FQ_FLUSH;
		}
		else if( strcmp( cmd, "info") == 0 ) {	
			fq_cmd = FQ_INFO;
		}
		else if( strcmp( cmd, "skip") == 0 ) {	
			fq_cmd = FQ_FORCE_SKIP;
		}
		else if( strcmp( cmd, "diag") == 0 ) {	
			fq_cmd = FQ_DIAG;
		}
		else if( strcmp( cmd, "extend") == 0 ) {	
			fq_cmd = FQ_EXTEND;
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
