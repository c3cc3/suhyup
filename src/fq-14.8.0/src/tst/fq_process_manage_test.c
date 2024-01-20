#include <stdio.h>
#include <stdbool.h>
#include <sys/types.h>
#include <unistd.h>
#include <libgen.h>

#include "fq_common.h"
#include "fq_cache.h"
#include "fq_logger.h"
#include "fq_manage.h"
#include "parson.h"

#if 0
#include "fq_conf.h"
struct fq_ps_info {
	char *run_name;
	char *exe_name;
	char *cwd;
	char *user_cmd;
	char *arg_0;
	char *arg_1;
	long uptime_bin;
	char *uptime_ascii;
};
typedef struct fq_ps_info fq_ps_info_t;
#endif

int main()
{
	bool	tf;

	char *path = ".";
	// char *path = NULL; /* If the path value is 0, we use the value defined in the environment variable(FQ_DATA_PATH). */
	fq_logger_t *l=NULL;
	char *logname="./my.log";

	int rc;
	rc = fq_open_file_logger(&l, logname, FQ_LOG_ERROR_LEVEL);
	CHECK(rc > 0);

	tf = regist_me_on_fq_framework( l, path, getpid(), false, 60 );
	CHECK( tf==true);

#if 0
	bool all_flag=true;
	check_fq_framework_process(l, path, getpid(), all_flag);

	tf = delete_me_on_fq_framework( l, path, getpid());
	
	fq_ps_info_t *ps_info=NULL;
	ps_info = (fq_ps_info_t *)calloc(1, sizeof(fq_ps_info_t));

	get_ps_info( l, path, getpid(), ps_info, true);

	printf("exe_name = '%s'\n", ps_info->exe_name);
	printf("uptime_bin = '%ld'\n", ps_info->uptime_bin);
	printf("uptime_ascii = '%s'\n", ps_info->uptime_ascii);

	if(ps_info) free(ps_info);
#endif

	while(1) {
		pslist_t *pslist=NULL;
		pslist = pslist_new();	
		
		tf = make_ps_list( l, path, pslist); 
		CHECK(tf == true);

		// ToDo
		// Full scans nodes of a list.
		ps_t *p=NULL;
		fprintf(stdout, "pslist contains %d elements\n", pslist->length);
		
		for( p=pslist->head; p != NULL; p=p->next) {
			fprintf(stdout, "\t- pid='%d', run_name='%s', uptime_accii='%s', isLive='%d' mtime='%ld'\n", 
				p->psinfo->pid, p->psinfo->run_name, p->psinfo->uptime_ascii, p->psinfo->isLive, p->psinfo->mtime);
		}

		JSON_Value *rootValue;
		JSON_Object *rootObject;

		rootValue = json_value_init_object();             // JSON_Value 생성 및 초기화
		rootObject = json_value_get_object(rootValue);    // JSON_Value에서 JSON_Object를 얻음

		json_object_set_value(rootObject, "ProcessIDs", json_value_init_array());
		JSON_Array *jary_pids = json_object_get_array(rootObject, "ProcessIDs");

		char *buf=NULL;
		bool pretty_flag = true;
		int	list_count=0;
		conv_from_pslist_to_json(l, pslist, rootValue, rootObject, jary_pids, &buf, &list_count, pretty_flag);
		printf("%s\n", buf);
		if(buf) free(buf);

		/* get an array of ProcessIDs  from jroot */
		int i;
		JSON_Array *array = json_object_get_array(rootObject, "ProcessIDs");
		for (i = 0; i < json_array_get_count(array); i++)  {
			double d_pid;
			int i_pid;

			d_pid = json_array_get_number(array, i);
			i_pid = d_pid;
			printf("pid=%d\n", i_pid);
			char key[256];

			sprintf(key, "%d.%s", i_pid, "Run_name");
			printf("%s: %s\n", key, json_object_dotget_string(rootObject, key));

			sprintf(key, "%d.%s", i_pid, "Mtime");
			printf("%s: %f\n", key, json_object_dotget_number(rootObject, key));

			bool tf;
			tf = is_live_process(i_pid);
			printf("live is %s\n", tf?"true":"false");

			tf = is_block_process( l, path,  i_pid, 3);
			printf("is block? %s\n", tf?"true":"false");

		}
		tf = touch_me_on_fq_framework( l, path, getpid() );
		if( tf == false ) break;
	
		json_value_free(rootValue);

		if(pslist) pslist_free(&pslist);
		usleep(1000000);
	} // end while

	return 0;
}
