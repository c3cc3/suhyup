/*
** fq_trace.c
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "fq_common.h"
#include "fq_mask.h"
#include "fq_trace.h"

static void on_trace(trace_obj_t *obj, int trace_flag, const char *fmt, ...);

static int get_traceindex(char* keyword)
{
    int i;
    for (i=0; i < NUM_TRACE_FLAG; i++)
        if ( STRCCMP(fq_trace_keywords[i], keyword) == 0 )
            return(i);
    return(-1);
}

int open_trace_obj( char *path, char *filename, char *trace_flags, int online_flag, int stdout_flag, int print_max_length,  trace_obj_t **obj)
{
	trace_obj_t *rc=NULL;

	if( !HASVALUE(path) || !HASVALUE(filename) ) {
		fprintf(stderr, "illgal function call.");
		return(FALSE);
	}

	rc = (trace_obj_t *)calloc(1, sizeof(trace_obj_t));
	if( rc ) {
		int 	i, ret;
		char	tmp[256];
		char	*p = trace_flags, *q, buf[80];

		rc->path = strdup(path);
		rc->filename = strdup(filename);
		sprintf(tmp, "%s/%s", path, filename);
		rc->pathfile = strdup(tmp);
		if( trace_flags )
			rc->trace_flags = strdup(trace_flags);
		rc->online_flag = online_flag;
		rc->stdout_flag = stdout_flag;

		if( print_max_length >= MAX_LOG_LINE_SIZE ) {
			fprintf( stderr, "print_max_length(%d) is too long. MAX_LOG_LINE_SIZE=[%d]\n", print_max_length,  MAX_LOG_LINE_SIZE);
			goto return_FALSE;
        }

		rc->print_max_length = print_max_length;

		rc->fp = fopen(rc->pathfile, "a");
		if( rc->fp == NULL ) {
			fprintf( stderr, "Can't open '%s', reason='%s'.\n", rc->pathfile, strerror(errno));
			goto return_FALSE;
		}

		ret = open_mask_obj( "trace_flag", &rc->mask_obj );
		if( ret != TRUE) {
			fprintf(stderr, "open_mask_obj() error.\n");
			goto return_FALSE;
		}

		/* 
		** masking
		** trace_string : 
		*/
		for(i=0; i<NUM_TRACE_FLAG; i++ ) {
			int id;

			if( !p ) break;

			if( (q=strchr(p, '|')) ) {
				strncpy(buf, p, q-p);
				buf[q-p ] = '\0';
				p = q+1;

			}
			else {
				strcpy(buf, p);
				p = NULL;
			}

			id = get_traceindex(buf);

			rc->mask_obj->bit = id;
			rc->mask_obj->yesno = strdup("yes");
			rc->mask_obj->on_set_mask(rc->mask_obj);
		}
		// rc->mask_obj->on_print_mask(rc->mask_obj);

		rc->on_trace = on_trace;

		*obj = rc;
		return(TRUE);
	}
return_FALSE:
	SAFE_FREE( rc->path );
	SAFE_FREE( rc->filename );
	SAFE_FREE( rc->pathfile );

	if(rc->fp) fclose(rc->fp);

	SAFE_FREE(*obj);

	*obj = NULL;

	return(FALSE);
}

int close_trace_obj( trace_obj_t **obj)
{

	SAFE_FREE( (*obj)->path );
    SAFE_FREE( (*obj)->filename );
    SAFE_FREE( (*obj)->pathfile );

    if((*obj)->fp) fclose((*obj)->fp);

	SAFE_FREE(*obj);

	return(TRUE);
}

static void on_trace(trace_obj_t *obj, int trace_flag, const char *fmt, ...)
{
    // char *buf=NULL;
	char	buf[MAX_LOG_LINE_SIZE];
    va_list ap;
	char cur_date[20], cur_time[20];
	char print_fmt[40];

	get_time(cur_date, cur_time);

    if ( obj->online_flag )
        return;

	// buf = calloc(obj->print_max_length+1, sizeof(char));

	obj->mask_obj->bit = trace_flag;
    if ( obj->mask_obj && !obj->mask_obj->on_get_mask(obj->mask_obj) )
        return;

    assert(fmt);

    va_start(ap, fmt);
        vsprintf(buf, fmt, ap);
    va_end(ap);

	memset(print_fmt, 0x00, sizeof(print_fmt));
	if ( strlen(buf) > obj->print_max_length ) {
		sprintf(print_fmt, "%%s:%%s[%%s] %%-%d.%ds...\n", obj->print_max_length, obj->print_max_length);
	}
	else {
		sprintf(print_fmt, "%%s:%%s[%%s] %%s");
	}

	if( obj->stdout_flag ) {
		fprintf(stdout, print_fmt, cur_date, cur_time, fq_trace_keywords[trace_flag], buf);
		fflush(stdout);
	}
	else {
		fprintf(obj->fp, print_fmt, cur_date, cur_time, fq_trace_keywords[trace_flag], buf);
		fflush(obj->fp);
	}

	// if(buf) free(buf);
}
