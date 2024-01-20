/*
** fq_second_counter.c
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "fq_sec_counter.h"
#include "fq_common.h"

static int get_second_min_th(int *min_th)
{
	time_t tval;
	struct tm t;

	time(&tval);
	localtime_r(&tval, &t);
	*min_th = (t.tm_hour * 60) + t.tm_min;
	return(t.tm_sec);
}

static int on_second_count( second_counter_obj_t *obj)
{
	int i;
	char date[9], timebuf[7];
	int sum=0;


	obj->sec_th = get_second_min_th( &obj->min_th);

	obj->tot_cnt++;

	if( obj->tot_cnt == 1 ) {
		obj->start = time(0);
	}

	if( obj->cur_min_th == 0 ) {
		obj->cur_min_th = obj->min_th;
	}

	if( obj->cur_min_th != obj->min_th ) {
		get_time(date, timebuf);
		fprintf(obj->fp, "%s:%s\n", date, timebuf);
		for(i=0; i<60; i++) {
			fprintf(obj->fp, "\t[%d]-th [%d]\n", i, obj->sec_cnt[i]);
			sum = sum + obj->sec_cnt[i];
		}
		fprintf(obj->fp, "minute sum = [%d] ---------- total=[%d] of [%d] \n", sum, obj->tot_cnt, obj->test_cnt);
		fflush(obj->fp);
		sum = 0;
		memset(obj->sec_cnt, 0x00, sizeof(obj->sec_cnt));
	}

	obj->cur_min_th = obj->min_th;
	obj->sec_cnt[obj->sec_th]++;

	if( obj->tot_cnt == obj->test_cnt) {
		float TPS;

		obj->end = time(0);
		get_time(date, timebuf);
		fprintf(obj->fp, "%s:%s\n", date, timebuf);
		for(i=0; i<60; i++) {
			fprintf(obj->fp, "\t[%d]-th [%d]\n", i, obj->sec_cnt[i]);
			sum = sum + obj->sec_cnt[i];
		}
		fprintf(obj->fp, "minute sum = [%d] ---------- total=[%d] of [%d] \n", sum, obj->tot_cnt, obj->test_cnt);
		if( (obj->end - obj->start) == 0 ) {
			TPS = (float)obj->test_cnt;
		}
		else {
			TPS = (float)obj->test_cnt / (float)(obj->end - obj->start + 1) ;
		}

		fprintf(obj->fp, " TPS = [%f] , seconds=[%ld] \n", TPS, (obj->end-obj->start) );
		fprintf(obj->fp, "---------------- test end ------------------[%s:%s]\n", date, timebuf);
		fflush(obj->fp);
		sum = 0;
		return(TEST_STOP);
	}
	else {
		return(TEST_CONTINUE);
	}
}

int open_second_counter_obj( char *path, char *file, int test_count,  second_counter_obj_t ** obj)
{
	second_counter_obj_t *rc=NULL;

	if( !*path || !*file ) {
		printf("illegal request, Put path and file.\n");
		return(FALSE);
	}
	
	rc = (second_counter_obj_t *)calloc(1, sizeof(second_counter_obj_t));
	if( rc ) {
		rc->path = strdup(path);
		rc->file = strdup(file);

		rc->cur_min_th = 0;

		rc->test_cnt = test_count;
		rc->tot_cnt = 0;

		sprintf(rc->path_file, "%s/%s", rc->path, rc->file);
		rc->fp = fopen(rc->path_file, "a");

		memset(rc->sec_cnt, 0x00, sizeof(rc->sec_cnt));

		rc->on_second_count = on_second_count;

		*obj = rc;
		return(TRUE);
	}
	return(FALSE);
}

void close_second_counter_obj( second_counter_obj_t **obj)
{
	if((*obj)->path) free((*obj)->path);
	if((*obj)->file) free((*obj)->file);

	if((*obj)->fp ) fclose((*obj)->fp);

	free( (*obj) );
	return;
}

