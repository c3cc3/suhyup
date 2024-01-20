/* vi: set sw=4 ts=4: */
/*
 * fq_external_env.c
 * 설명: 이 프로그램은 외부 환경값을 라이브러리 객체에서 참조하기 위한 객체 소스이다.
 *       open_external_env_obj() 함수를 통하여 객체를 생성하고,
 *       inner 함수인 on_get_extenv() 함수를 통하여 환경값을 가져와서
 *       fq 라이브러리 소스내에서 이 값을 참조할 수 있도록 하기 위함이다.
 * 사용예제: fqueue.c 의 on_de() 함수 첫부분에 이 함수를 사용하는 예제가 코멘트로 막혀있다.
 * 제작일: 2013/02/05
 */
#include <stdio.h>
#include <string.h> /* strerror() */
#include <time.h>
#include <errno.h> /* for EBUSY */
#include <unistd.h> /* for sleep */
#include <pthread.h>
#include <sys/types.h>

#include "config.h"
#include "fqueue.h"
#include "fq_common.h"
#include "fq_external_env.h"

#define FQ_EXTERNAL_EVN_C_VERSION "1.0.0"

static int on_get_extenv( fq_logger_t *l, external_env_obj_t *obj, char *key, char *value);

/*
 * return
 * 		TRUE: 1
 * 		FALSE: 0
 */

static int load_envfile_2_buf(fq_logger_t *l, char *fname, char**ptr_dst, int *flen);

#define EXTERNAL_FNAME "FQ_external_env.conf"
int 
open_external_env_obj( fq_logger_t *l, char *path, external_env_obj_t **obj)
{
	external_env_obj_t *rc=NULL;

	FQ_TRACE_ENTER(l);

	if( !HASVALUE(path) )  {
		fq_log(l, FQ_LOG_ERROR, "illgal function call.");
		FQ_TRACE_EXIT(l);
		return(FALSE);
	}

	rc  = (external_env_obj_t *)malloc(sizeof(external_env_obj_t));
	if(rc) {
		int 	rtn;
		char	path_file[256];

		rc->path = strdup(path);
		rc->env_fname = strdup(EXTERNAL_FNAME);
		rc->l = l;
		rc->contents = NULL;
		rc->on_get_extenv = on_get_extenv;

		sprintf(path_file, "%s/%s", rc->path, rc->env_fname);

		if( is_file( path_file ) ) {
			rtn = load_envfile_2_buf(l, path_file, &rc->contents, &rc->contents_length);
			if(rtn < 0) {
				fq_log(l, FQ_LOG_ERROR, "load_envfile_2_buf(%s) error.", path_file);
				goto return_FALSE;

			}
			else {
				*obj = rc;
				fq_log(l, FQ_LOG_DEBUG, "external environment file(%s) loading OK.", path_file);
				FQ_TRACE_EXIT(l);
				return(TRUE);
			}
		}
		else {
			int fd;
			if( (fd=open(path_file, O_RDWR|O_CREAT, 0666)) < 0 ) {
				fq_log(l, FQ_LOG_ERROR, "open(O_CREAT) error.");
				goto return_FALSE;
			}
			close(fd);
			FQ_TRACE_EXIT(l);
			return(TRUE);
		}
	}
return_FALSE:

	SAFE_FREE(rc->path);
	SAFE_FREE(rc->env_fname);
	SAFE_FREE(rc->contents);
	SAFE_FREE(*obj);
	
	return(FALSE);
}

int free_external_env_obj( fq_logger_t *l, external_env_obj_t **obj)
{
	FQ_TRACE_ENTER(l);

	SAFE_FREE((*obj)->contents);

	FQ_TRACE_EXIT(l);

	return(TRUE);
}

int close_external_env_obj( fq_logger_t *l, external_env_obj_t **obj)
{
	FQ_TRACE_ENTER(l);

	SAFE_FREE( (*obj)->path );
	SAFE_FREE( (*obj)->env_fname );
	SAFE_FREE( (*obj)->contents );

	SAFE_FREE( (*obj) );

	FQ_TRACE_EXIT(l);
	return(TRUE);
}

static int load_envfile_2_buf(fq_logger_t *l, char *fname, char**ptr_dst, int *flen)
{
	int fdin, rc=-1;
	struct stat statbuf;
	off_t len=0;
	char	*p=NULL;
	int	n;

	if(( fdin = open(fname ,O_RDONLY)) < 0 ) { /* option : O_RDWR */
		fq_log(l, FQ_LOG_ERROR, "external environment file open error.");
		goto L0;
	}

	if(( fstat(fdin, &statbuf)) < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "external environment file fstat error.");
		goto L1;
	}

	len = statbuf.st_size;
	*flen = len;

	p = malloc(len+1);
	if(!p) {
		fq_log(l, FQ_LOG_ERROR, "memory allocation error. fatal..");
		goto L1;
	}

	if(( n = read( fdin, p, len)) != len ) {
		fq_log(l, FQ_LOG_ERROR, "external environment file read error.");
		goto L1;
    }

	*ptr_dst = p;

	rc = 0;
L1:
	SAFE_FD_CLOSE(fdin);
L0:
	/*
	 We don't unlink for high speed. 
	 unlink(filename);
	*/

	return(rc);
}

static int on_get_extenv( fq_logger_t *l, external_env_obj_t *obj, char *key, char *value)
{
	int rc=-1;

	FQ_TRACE_ENTER(obj->l);

	rc =  get_item_XML(obj->contents, key, value, 128);
	if(rc < 0) { /* not found */
		goto return_FALSE;
	}

	FQ_TRACE_EXIT(obj->l);
	return(TRUE);

return_FALSE:

	FQ_TRACE_EXIT(obj->l);
	return(FALSE);
}
