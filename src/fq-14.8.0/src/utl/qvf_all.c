/*
** qvf_all.c
*/
#include <stdio.h>
#include <unistd.h>
#include "fqueue.h"
#include "fq_logger.h"
#include "fq_common.h"
#include "fq_config.h"
#include "fq_param.h"
#include "fq_manage.h"
#include "fq_info.h"
#include "fq_info_param.h"
#include "fq_monitor.h"
#include "fq_tokenizer.h"

#include "fq_file_list.h"

#define QVF_ALL_C_VERSION "1.0.0"

typedef struct _env_param {
		fq_logger_t	*l;
		char	*fq_home_path;
		int	period_time;
} env_param_t;


void *QU_thread(void *arg)
{
	file_list_obj_t *obj=NULL;
	FILELIST *this_entry=NULL;
	int  dir_no=0;
	int  rc;

	env_param_t *env_param = (env_param_t *)arg;
	char fq_directories_list_filename[128];

	fq_log(env_param->l, FQ_LOG_INFO, "%s", "QU_thread started.");
	
	sprintf(fq_directories_list_filename, "%s/%s", env_param->fq_home_path, FQ_DIRECTORIES_LIST );

	rc = open_file_list(env_param->l, &obj, fq_directories_list_filename);
    CHECK( rc == TRUE );

	fq_log(env_param->l, FQ_LOG_INFO, "fq_directories_list_filename=[%s] open success.",
		fq_directories_list_filename);

	while(1) {
		this_entry = obj->head;
		for( dir_no=0;  (this_entry->next && this_entry->value); dir_no++ ) 
		{
			char	filename[128];
			FILE	*fp = NULL;
			char	qname[80];
			int		columns=0;
			char	line[0124];
			char	buf[1024];

			sprintf(filename, "%s/%s", this_entry->value, FQ_LIST_FILE_NAME);
			fp = fopen(filename, "r");
			if(fp == NULL) {
				fq_log(env_param->l, FQ_LOG_ERROR, "fopen(%s) error. reason=[%s]", filename, strerror(errno));
				break;
			}		

			fq_log(env_param->l, FQ_LOG_INFO, "FQ list filename:[%s].", filename);

			while( fgets( qname, 80, fp) ) {
				char    info_filename[256];
				fq_info_t   *_fq_info=NULL;
				char    info_qpath[128];
				char    info_qname[128];
				fqueue_info_t qi;

				str_lrtrim(qname);
				if( qname[0] == '#' ) continue;

				sprintf(info_filename, "%s/%s.Q.info", this_entry->value, qname);
				if( !can_access_file(env_param->l, info_filename)) {
					fq_log(env_param->l, FQ_LOG_ERROR, "Can't access to [%s].", info_filename);
					break;
				}

				_fq_info = new_fq_info(info_filename);
				if(!_fq_info) {
					fq_log(env_param->l, FQ_LOG_ERROR, "new_fq_info() error.");
					break;
				}
				fq_log(env_param->l, FQ_LOG_INFO, "new_fq_info('%s') success", info_filename);

				if( load_info_param(env_param->l, _fq_info, info_filename) < 0 ) {
					fq_log(env_param->l, FQ_LOG_ERROR, "load_param() error.");
					if(_fq_info) free_fq_info(&_fq_info);
					break;
				}

				fq_log(env_param->l, FQ_LOG_INFO, "load_info_param('%s') success", info_filename);
				
				get_fq_info(_fq_info, "QPATH", info_qpath);
				get_fq_info(_fq_info, "QNAME", info_qname);

				if( GetQueueInfo(env_param->l, info_qpath, info_qname, &qi) == FALSE ) {
					fq_log(env_param->l, FQ_LOG_ERROR, "GetQueueInfo(%s, %s) Error.", info_qpath, info_qname);
					break;
				}
				fq_log(env_param->l, FQ_LOG_INFO, "GetQueueInfo('%s', '%s') success.", info_qpath, info_qname);

				memset(buf, 0x00, sizeof(buf));

				sprintf( buf, "%s/%s: XA:%s, %d, %d, %ld, %ld, %ld", info_qpath, info_qname, qi.XA_ON_OFF, qi.msglen, qi.max_rec_cnt, qi.en_sum, qi.de_sum, qi.diff);
				printf("%s\n", buf);

				FreeQueueInfo(NULL, &qi);

				if( _fq_info ) {
					free_fq_info( &_fq_info );
					_fq_info = NULL;
				}

			}

			columns = strlen(buf);
			memset( line, 0x00, sizeof(line));
			memset( line, '-', columns);
			printf("%s\n", line);

			if(fp) fclose(fp);
		
			sleep(env_param->period_time);

			this_entry = this_entry->next;

		} /* for end */

	} /* while end */

	if( obj ) close_file_list(env_param->l, &obj);

	fq_log(env_param->l, FQ_LOG_ERROR, "%s.", "QU_thread stoped.");

	pthread_exit(0);
}

int main(int ac, char **av)
{
	env_param_t env_params;
	pthread_t  QU_thread_id;
	void    *rtn=NULL;
	int rc;
	char *logname = "/tmp/qvf_all.log";
	fq_logger_t	*l=NULL;

	rc = fq_open_file_logger(&l, logname, FQ_LOG_INFO_LEVEL);
    CHECK( rc > 0 );

	fprintf(stdout, "logging file: [%s].\n", logname);

	env_params.l = l;

	env_params.fq_home_path = getenv("FQ_HOME");
	if( env_params.fq_home_path == NULL ) {
		fprintf(stderr, "Set FQ_HOME value to your .bashrc or .profile or .bash_profile.\n");
		exit(0);
	}
	
	env_params.period_time = 3;

	pthread_create(&QU_thread_id, NULL, &QU_thread, &env_params);
	pthread_join( QU_thread_id, &rtn);

	exit(0);
}
