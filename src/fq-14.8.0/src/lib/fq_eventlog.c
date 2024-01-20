/*
 * fq_eventlog.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fq_common.h"
#include "fq_logger.h"
#include "fq_eventlog.h"
#include "fq_unixQ.h"
#include "fqueue.h"

#define FQ_EVENTLOG_C_VERSION "1.0.0"

int on_eventlog(fq_logger_t *l, eventlog_obj_t *obj, fq_event_t event_id, ...);
void on_en_eventlog(fq_logger_t *l, unixQ_obj_t *uq_obj, fq_event_t event_id, ...);
int on_update(fq_logger_t *l, mmap_eventlog_obj_t *obj, char *msg);

const char* event_code[] = {
	"CR", 	/* 0 : create */
	"OP",	/* 1 : open */
	"EN",	/* 2 : en  */
	"DA",   /* 3 : de automatic */
	"DN",	/* 4 : de normal */
	"DX",	/* 5 : de_XA */
	"CM", 	/* 6 : commit  */
	"CL",	/* 7 : close  */ 
	"UL",	/* 8 : unlink */ 
	"RS",   /* 9 : reset */
	"EA",   /* 10: enable */
	"DS", 	/* 11: Disable */
	"QO",   /* 12: set QOS */
	"EX",   /* 13: extend */
};

/*******************************************************
 ** eventlog file version 
 */
int open_eventlog_obj( fq_logger_t *l, char *path, eventlog_obj_t **obj)
{
	eventlog_obj_t	*rc=NULL;

	FQ_TRACE_ENTER(l);

	if( !HASVALUE(path) )  {
		fq_log(l, FQ_LOG_ERROR, "illgal function call.");
		FQ_TRACE_EXIT(l);
		return(FALSE);
	}

	rc = (eventlog_obj_t *)calloc( 1, sizeof(eventlog_obj_t));
	if(rc) {
		char datestr[9], timestr[7];
		char fn[256];

		rc->path = strdup(path);
		rc->opened_time = time(0);

		get_time(datestr, timestr);
		snprintf(fn, sizeof(fn), "%s/FQ_%s.eventlog", path, datestr);
		rc->fn = strdup(fn);

		rc->on_eventlog = on_eventlog; /* put function pointer */
		rc->on_en_eventlog = on_en_eventlog;

		rc->fp = fopen(fn, "a");
		if(!rc->fp) {
			fq_log(l, FQ_LOG_ERROR, "failed to open eventlog file. [%s]", fn);
			goto return_FALSE;
		}

		*obj = rc;
		FQ_TRACE_EXIT(l);
		return(TRUE);
	}
return_FALSE:
	SAFE_FREE(rc->path);
	SAFE_FREE(rc->fn);
	if(rc->fp) fclose(rc->fp);
	rc->fp = NULL;
	SAFE_FREE(rc);
	SAFE_FREE(*obj);

	FQ_TRACE_EXIT(l);
	return(FALSE);
}

int close_evenlog_obj(fq_logger_t *l, eventlog_obj_t **obj)
{
	FQ_TRACE_ENTER(l);

	SAFE_FREE( (*obj)->path );
	SAFE_FREE( (*obj)->fn );
	if((*obj)->fp) fclose((*obj)->fp);
	(*obj)->fp = NULL;
	SAFE_FREE( (*obj) );

	FQ_TRACE_EXIT(l);

	return(TRUE);
}

int on_eventlog(fq_logger_t *l, eventlog_obj_t *obj, fq_event_t event_id, ...)
{
#if 1 /* 2013/10/02: Bug was found.(memory leak) */
	char datestr[9], timestr[7];
	va_list ap;
	char	opened_time[32];
	char	current_time[32];

	if(!obj->fp)
		goto return_FALSE;

	char *p=NULL;
	sprintf( opened_time, "%s", (p=asc_time(obj->opened_time)));
	if(p) free(p);
	sprintf( current_time, "%s", (p=asc_time(time(0))));
	if(p) free(p);

	get_time(datestr, timestr);

	if( strncmp(opened_time, current_time, 10) != 0 )  {
		char fn[256];

		fclose(obj->fp);
		obj->fp = NULL;
		snprintf(fn, sizeof(fn), "%s/FQ_%s.eventlog", obj->path, datestr);
		obj->fp = fopen(fn, "a");

		if(!obj->fp) { 
			fq_log(l, FQ_LOG_ERROR, "failed to open eventlog file. [%s]", fn);
			goto return_FALSE;
		}
		obj->opened_time = time(0);
	}

	va_start(ap, event_id);
	fprintf(obj->fp, "%s|%s|%s|", event_code[event_id], datestr, timestr);

	switch ( event_id ) {
		case EVENT_FQ_CREATE:
		case EVENT_FQ_OPEN:
		case EVENT_FQ_CLOSE:
		case EVENT_FQ_UNLINK:
		case EVENT_FQ_RESET:
		case EVENT_FQ_ENABLE:
		case EVENT_FQ_DISABLE:
		case EVENT_FQ_EXTEND:
			/* path, qname, result,pid */
			vfprintf(obj->fp, "%s|%s|%d|%d|\n", ap); 
			break;
		case EVENT_FQ_EN:
		case EVENT_FQ_DE_AUTO:
		case EVENT_FQ_DE:
		case EVENT_FQ_DE_XA:
		case EVENT_FQ_COMMIT:
		case EVENT_FQ_CANCEL:
			/* path, qname, result, pid, seq, delay_time */
			vfprintf(obj->fp, "%s|%s|%d|%d|%lu|%lu|\n", ap); 
			break;
		default:
			vfprintf(obj->fp, "%s|\n", ap);
			break;
	}
	va_end(ap);

	fflush(obj->fp);
	return(TRUE);

return_FALSE:
	fprintf(stderr, "on_eventlog() failed.\n");
	return(FALSE);
	return(TRUE);

#endif
}

int eventlog(fq_logger_t *l, fq_event_t event_id, ...)
{
	char fn[256];
	char datestr[40], timestr[40];
	FILE* fp=NULL;
	va_list ap;


	/*
	   pthread_mutex_lock(&log_lock);
	   */

	get_time(datestr, timestr);
	snprintf(fn, sizeof(fn), "%s/FQ_%s.eventlog", EVENT_LOG_PATH, datestr);

	fp = fopen(fn, "a");
	if(!fp) {
		fq_log(l, FQ_LOG_ERROR, "failed to open eventlog file. [%s]", fn);
		goto return_FALSE;
	}

	va_start(ap, event_id);
	fprintf(fp, "%s|%s|%s|", event_code[event_id], datestr, timestr);

	switch ( event_id ) {
		case EVENT_FQ_CREATE:
		case EVENT_FQ_OPEN:
		case EVENT_FQ_CLOSE:
		case EVENT_FQ_UNLINK:
		case EVENT_FQ_RESET:
		case EVENT_FQ_ENABLE:
		case EVENT_FQ_DISABLE:
		case EVENT_FQ_EXTEND:
			/* path, qname, result,pid */
			vfprintf(fp, "%s|%s|%d|%d|\n", ap); 
			break;
		case EVENT_FQ_EN:
		case EVENT_FQ_DE_AUTO:
		case EVENT_FQ_DE:
		case EVENT_FQ_DE_XA:
		case EVENT_FQ_COMMIT:
		case EVENT_FQ_CANCEL:
			/* path, qname, result, pid, seq, delay_time */
			vfprintf(fp, "%s|%s|%d|%d|%lu|%lu|\n", ap); 
			break;
		default:
			vfprintf(fp, "%s|\n", ap);
			break;
	}
	if(fp) fclose(fp);
	va_end(ap);
	/*
	   pthread_mutex_unlock(&log_lock);
	   */

	return(TRUE);

return_FALSE:
	if(fp) fclose(fp);
	/*
	   pthread_mutex_unlock(&log_lock);
	   */
	return(FALSE);
}

/*************************************************************************************
 ** UnixQ version eventlog
 */
void en_eventlog(fq_logger_t *l, fq_event_t event_id, ...)
{
#ifdef _USE_EVENTLOG
	unixQ_obj_t *uq_obj=NULL;
	char datestr[40], timestr[40];
	va_list ap;
	long	eventlogID = UNIXQ_EVENTLOG_ID; /* UNIXQ_EVENTLOG_ID : 999999 */
	long    returnID;
	char	buf[256];
	char	buf1[128];
	char	buf2[128];
	int 	rc;

	FQ_TRACE_EXIT(l);

	rc = open_unixQ_obj( l, FQ_UNIXQ_KEY_PATH, FQ_UNIXQ_KEY_CHAR, &uq_obj);
	if( rc == FALSE) {
		fq_log(l, FQ_LOG_ERROR, "open_unixQ_obj() error.");
		goto return_VOID;
	}

	memset(buf, 0x00, sizeof(buf));
	memset(buf1, 0x00, sizeof(buf1));
	memset(buf2, 0x00, sizeof(buf2));

	va_start(ap, event_id);
	get_time(datestr, timestr);
	sprintf(buf1, "%s|%s|%s|", event_code[event_id], datestr, timestr);

	switch ( event_id ) {
		case EVENT_FQ_CREATE:
		case EVENT_FQ_OPEN:
		case EVENT_FQ_CLOSE:
		case EVENT_FQ_UNLINK:
		case EVENT_FQ_RESET:
		case EVENT_FQ_ENABLE:
		case EVENT_FQ_DISABLE:
		case EVENT_FQ_EXTEND:
			/* path, qname, result,pid */
			vsprintf(buf2, "%s|%s|%d|%d|", ap); 
			break;
		case EVENT_FQ_EN:
		case EVENT_FQ_DE_AUTO:
		case EVENT_FQ_DE:
		case EVENT_FQ_DE_XA:
		case EVENT_FQ_COMMIT:
		case EVENT_FQ_CANCEL:
			/* path, qname, result, pid, seq, delay_time */
			vsprintf(buf2, "%s|%s|%d|%d|%lu|%lu|", ap); 
			break;
		default:
			vsprintf(buf2, "%s|", ap);
			break;
	}
	va_end(ap);

	sprintf(buf, "%s%s", buf1, buf2);

	returnID = (long)getpid();
	rc = uq_obj->on_en(uq_obj, buf, strlen(buf), eventlogID, returnID,  IPC_NOWAIT);
	if( rc == UQ_FULL_RETURN_VALUE) {
		fq_log(l, FQ_LOG_ERROR, "busy(full).");
		goto return_VOID;
	}
	else if( rc < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "on_en(eventlog queue) erro.");
		goto return_VOID;
	}
	else {
		/* success */
	}

	va_end(ap);

return_VOID:

	close_unixQ_obj(l, &uq_obj);

	FQ_TRACE_EXIT(l);
#endif
	return;
}

/*
 * 이함수는 uq_obj이 만들어 질때 (오픈할때) 생성된 uq_obj에 이벤트로그를 전송하는 프로그램이다.
 */
void on_en_eventlog(fq_logger_t *l, unixQ_obj_t *uq_obj, fq_event_t event_id, ...)
{
#ifdef _USE_EVENTLOG
	char datestr[40], timestr[40];
	va_list ap;
	long	eventlogID = UNIXQ_EVENTLOG_ID; /* UNIXQ_EVENTLOG_ID : 999999 */
	long    returnID;
	char	buf[256];
	char	buf1[128];
	char	buf2[128];
	int 	rc;

	FQ_TRACE_EXIT(l);

	memset(buf, 0x00, sizeof(buf));
	memset(buf1, 0x00, sizeof(buf1));
	memset(buf2, 0x00, sizeof(buf2));

	va_start(ap, event_id);
	get_time(datestr, timestr);
	sprintf(buf1, "%s|%s|%s|", event_code[event_id], datestr, timestr);

	switch ( event_id ) {
		case EVENT_FQ_CREATE:
		case EVENT_FQ_OPEN:
		case EVENT_FQ_CLOSE:
		case EVENT_FQ_UNLINK:
		case EVENT_FQ_ENABLE:
		case EVENT_FQ_DISABLE:
		case EVENT_FQ_SETQOS:
		case EVENT_FQ_EXTEND:
			/* path, qname, result,pid */
			vsprintf(buf2, "%s|%s|%d|%d|", ap); 
			break;
		case EVENT_FQ_EN:
		case EVENT_FQ_DE_AUTO:
		case EVENT_FQ_DE:
		case EVENT_FQ_DE_XA:
		case EVENT_FQ_COMMIT:
		case EVENT_FQ_CANCEL:
			/* path, qname, result, pid, seq, delay_time */
			vsprintf(buf2, "%s|%s|%d|%d|%lu|%lu|", ap); 
			break;
		default:
			vsprintf(buf2, "%s|", ap);
			break;
	}
	va_end(ap);

	sprintf(buf, "%s%s", buf1, buf2);

	returnID = (long)getpid();
	rc = uq_obj->on_en(uq_obj, buf, strlen(buf), eventlogID, returnID,  IPC_NOWAIT);
	if( rc == UQ_FULL_RETURN_VALUE) {
		/*
		   fq_log(l, FQ_LOG_ERROR, "busy(full).");
		   */
		goto return_VOID;
	}
	else if( rc < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "on_en(eventlog queue) erro.");
		goto return_VOID;
	}
	else {
		/* success */
	}

	va_end(ap);

return_VOID:


	FQ_TRACE_EXIT(l);
#endif
	return;
}

#if 0
#define MMAP_EVENTLOG_DIR_NAME	"/tmp"
#define MMAP_EVENTLOG_FILE_NAME	"FQ_STATISTIC.DB"
#endif

static int get_code_by_index(int idx, char *code, char *desc)
{
	switch(idx) {
		case 0:
			sprintf(code, "%s", "CR");
			sprintf(desc, "%s", "create");
			break;
		case 1:
			sprintf(code, "%s", "OP");
			sprintf(desc, "%s", "open");
			break;
		case 2:
			sprintf(code, "%s", "EN");
			sprintf(desc, "%s", "enQ");
			break;
		case 3:
			sprintf(code, "%s", "DA");
			sprintf(desc, "%s", "enQ-auto");
			break;
		case 4:
			sprintf(code, "%s", "DN");
			sprintf(desc, "%s", "enQ-normal");
			break;
		case 5:
			sprintf(code, "%s", "DX");
			sprintf(desc, "%s", "enQ-XA");
			break;
		case 6:
			sprintf(code, "%s", "CM");
			sprintf(desc, "%s", "XA-commit");
			break;
		case 7:
			sprintf(code, "%s", "CL");
			sprintf(desc, "%s", "close");
			break;
		case 8:
			sprintf(code, "%s", "UL");
			sprintf(desc, "%s", "unlink");
			break;
		case 9:
			sprintf(code, "%s", "RS");
			sprintf(desc, "%s", "reset");
			break;
		case 10:
			sprintf(code, "%s", "EA");
			sprintf(desc, "%s", "enable");
			break;
		case 11:
			sprintf(code, "%s", "DS");
			sprintf(desc, "%s", "enable");
			break;
		case 12:
			sprintf(code, "%s", "QO");
			sprintf(desc, "%s", "set QOS");
			break;
		case 13:
			sprintf(code, "%s", "EX");
			sprintf(desc, "%s", "extend Queue");
			break;
		default:
			sprintf(code, "%s", "XX");
			sprintf(desc, "%s", "undefined Code");
			break;
	}
	return(TRUE);
}

	int 
open_mmap_eventlog(fq_logger_t *l, char *path, mmap_eventlog_obj_t **obj)
{
	mmap_eventlog_obj_t *rc=NULL;
	char	filename[256];

	FQ_TRACE_ENTER(l);

	sprintf(filename, "%s/%s", path, MMAP_EVENTLOG_FILE_NAME);

	if( !is_file(filename) ) { /* checks whether the file exists.*/
		int	i;
		int fd;
		mmap_eventlog_t *e=NULL;

		if( (fd=create_dummy_file(l, path, MMAP_EVENTLOG_FILE_NAME, sizeof(mmap_eventlog_t)))  < 0) {
			fq_log(l, FQ_LOG_ERROR, "create_dummy_file() error. path=[%s] name=[%s]", path, MMAP_EVENTLOG_FILE_NAME);
			goto return_FALSE;
		}
		/* initialize */
		if( (e = (mmap_eventlog_t *)fq_mmapping(l, fd, sizeof(mmap_eventlog_t), (off_t)0)) == NULL ) {
			fq_log(l, FQ_LOG_ERROR, "mmap_eventlog file fq_mmapping() error.");
			goto return_FALSE;
		}

		for(i=0; i<KINDS_OF_EVENTLOG; i++) {
			memset(e->rows[i].evt_code, 0x00, sizeof(e->rows[i].evt_code));
			memset(e->rows[i].desc, 0x00, sizeof(e->rows[i].desc));
			get_code_by_index(i, e->rows[i].evt_code, e->rows[i].desc);

			e->rows[i].count = 0;
			e->rows[i].varidation = 0;
		}
		e->nums_event = KINDS_OF_EVENTLOG;
		fq_munmap(l, (void *)e, sizeof(mmap_eventlog_t));

		SAFE_FD_CLOSE(fd);
		chmod( filename , 0666);
	}

	rc = (mmap_eventlog_obj_t *)calloc( 1, sizeof(mmap_eventlog_obj_t));
	if(rc) {
		rc->name = strdup(filename);
		if( !can_access_file(l, rc->name)) {
			fq_log(l, FQ_LOG_ERROR, "[%s] can not access.", rc->name);
			goto return_FALSE;
		}

		rc->fd = 0;
		if( (rc->fd=open(rc->name, O_RDWR|O_SYNC, 0666)) < 0 ) {
			fq_log(l, FQ_LOG_ERROR, "[%s] can not open.", rc->name);
			goto return_FALSE;
		}

		if(( fstat(rc->fd, &rc->st)) < 0 ) {
			fq_log(l, FQ_LOG_ERROR, "fstat() error. reason=[%s]", strerror(errno));
			goto return_FALSE;
		}

		rc->e = NULL;
		rc->mapping_len = sizeof(mmap_eventlog_t);

		if( (rc->e = (mmap_eventlog_t *)fq_mmapping(l, rc->fd, rc->mapping_len, (off_t)0)) == NULL ) {
			fq_log(l, FQ_LOG_ERROR, "mmap_eventlog file fq_mmapping() error.");
			goto return_FALSE;
		}

		rc->l = l;
		rc->on_update = on_update;
		/*
		   rc->on_retrieve = on_retrieve;
		   rc->on_reset = on_reset;
		   */

		*obj = rc;
		FQ_TRACE_EXIT(l);
		return(TRUE);
	}

return_FALSE:
	SAFE_FD_CLOSE(rc->fd);
	if(rc->e)  {
		fq_munmap(l, (void *)rc->e, rc->mapping_len);
		rc->e = NULL;
	}

	*obj = NULL;
	FQ_TRACE_EXIT(l);
	return(FALSE);
}

static int get_index_by_event_code(char *event_code)
{
	int code_idx;

	if(strncmp(event_code, "CR", 2)==0) code_idx = 0;
	else if(strncmp(event_code, "OP", 2)==0) code_idx = 1;
	else if(strncmp(event_code, "EN", 2)==0) code_idx = 2;
	else if(strncmp(event_code, "DA", 2)==0) code_idx = 3;
	else if(strncmp(event_code, "DN", 2)==0) code_idx = 4;
	else if(strncmp(event_code, "DX", 2)==0) code_idx = 5;
	else if(strncmp(event_code, "DM", 2)==0) code_idx = 6;
	else if(strncmp(event_code, "CL", 2)==0) code_idx = 7;
	else if(strncmp(event_code, "UL", 2)==0) code_idx = 8;
	else if(strncmp(event_code, "RS", 2)==0) code_idx = 9;
	else if(strncmp(event_code, "EA", 2)==0) code_idx = 10;
	else if(strncmp(event_code, "DS", 2)==0) code_idx = 11;
	else if(strncmp(event_code, "QO", 2)==0) code_idx = 12;
	else if(strncmp(event_code, "EX", 2)==0) code_idx = 13;
	else {
		code_idx = -1;
	}
	return(code_idx);
}

int on_update(fq_logger_t *l, mmap_eventlog_obj_t *obj, char *msg)
{
	int idx=0;
	char	event_code[3];

	FQ_TRACE_ENTER(l);

	memcpy(event_code, msg, 2);
	event_code[2] = 0x00;

	idx = get_index_by_event_code(event_code);

	obj->e->rows[idx].count++;
	obj->e->rows[idx].varidation = 100;

	FQ_TRACE_EXIT(l);
	return(TRUE);
}
