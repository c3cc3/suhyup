/*
 * fq_file_list.c
 */
#define FQ_FILE_LIST_C_VERSION "1.0.0"

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <strings.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h> /* for chmod() */

#include "fq_common.h"
#include "fq_logger.h"
#include "fq_file_list.h"

static int on_print( file_list_obj_t *obj )
{
  FILELIST *this_entry=NULL;

  FQ_TRACE_ENTER(obj->l);
  this_entry = obj->head;

  while (this_entry->next && this_entry->value ) {
	printf("this_entry->value=[%s]\n", this_entry->value);
	this_entry = this_entry->next;
  }

  FQ_TRACE_EXIT(obj->l);
  return 0;
}

/* Assign the field values to the current crontab entry in core. */
static void assign(entry, line)
FILELIST *entry;
char *line;
{
  entry->value     = strdup(line);
}

static void
entry_free(FILELIST *t)
{
	SAFE_FREE(t->value);
    SAFE_FREE(t);
}

int close_file_list( fq_logger_t *l,  file_list_obj_t **obj )
{
  	FILELIST *p, *q;

	FQ_TRACE_ENTER(l);
  	p = (*obj)->head;

  	while (p) {
		q = p;
		p = p->next;
		entry_free(q);
  	}
	(*obj)->head = NULL;

    SAFE_FREE( (*obj));

	FQ_TRACE_EXIT(l);
	return TRUE;
}

int 
open_file_list( fq_logger_t *l, file_list_obj_t **obj, char *list_path_filename) 
{
	struct stat file_st; /* file status */
	file_list_obj_t *rc=NULL;
	FILE *fp=NULL;

	FQ_TRACE_ENTER(l);

	if (stat(list_path_filename, &file_st)) {
		FQ_TRACE_EXIT(l);
		return(FALSE);
	}

	rc = (file_list_obj_t *)calloc(1, sizeof(file_list_obj_t));

	rc->entry_ptr = (FILELIST *)malloc(sizeof(FILELIST));
	rc->entry_ptr->next = NULL;
	rc->head = rc->entry_ptr;
	rc->count = 0;
	if(rc) {
		char line[MAX_LINE_SIZE];

		if ((fp = fopen(list_path_filename, "r")) == NULL) {
			goto return_FALSE;
		}

		rc->prv_time = file_st.st_mtime;
		
		rc->entry_ptr = rc->head;

		while (fgets(line, MAX_LINE_SIZE, fp) != NULL) {
			int len;

			if (line[0] == '#' || line[0] == '\n') continue;

			len = strlen(line);
			line[len-1] = 0x00; /* remove NewLine Charactor */
			assign(rc->entry_ptr, line);
			rc->count++;

			if (rc->entry_ptr->next == NULL) {
				rc->entry_ptr->next = (FILELIST *)malloc(sizeof(FILELIST));
				rc->entry_ptr->next->next = NULL;
			}

			rc->entry_ptr = rc->entry_ptr->next;
		}
		
		(void) fclose(fp);
		while (rc->entry_ptr) {
			rc->entry_ptr->value = NULL;
			rc->entry_ptr = rc->entry_ptr->next;
		}
		
		rc->on_print = on_print;
		*obj = rc;

		FQ_TRACE_EXIT(l);
		return(TRUE);
	}

return_FALSE:
	if(fp) { 
		fclose(fp);
		fp = NULL;
	}

	SAFE_FREE(rc);
	SAFE_FREE(*obj);
	
	FQ_TRACE_EXIT(l);
	return(FALSE);
}
int append_file_list(fq_logger_t *l, file_list_obj_t *obj, char *new_filename)
{
	FILELIST *this_entry = NULL;
	FILE *fp=NULL;
	char line[MAX_LINE_SIZE];

	FQ_TRACE_ENTER(l);
	this_entry = obj->head;

	while( this_entry->next && this_entry->value) {
		this_entry = this_entry->next;
	}

	obj->entry_ptr = this_entry;

	if(( fp = fopen(new_filename, "r")) == NULL) {
		fq_log(l, FQ_LOG_ERROR, "[%s] fopen error.", new_filename);
		goto return_FALSE;
	}

	while( fgets( line, MAX_LINE_SIZE, fp) != NULL) {
		int len;
	
		if( line[0] =='#' || line[0] == '\n') continue;

		str_lrtrim(line);
		len = strlen(line);
		line[len-1] = 0x00; /* remove new line char */
		assign( obj->entry_ptr, line);
		obj->count++;

		if( obj->entry_ptr->next == NULL ) { /* last node */
			obj->entry_ptr->next = (FILELIST *)malloc(sizeof(FILELIST)); /* allocate new node */
			obj->entry_ptr->next->next = NULL;
		}

		fq_log(l, FQ_LOG_DEBUG, "A new line is appended.[%s].", line);

		obj->entry_ptr = obj->entry_ptr->next;
	}
	(void) fclose(fp);

	while(obj->entry_ptr) {
		obj->entry_ptr->value = NULL;
		obj->entry_ptr = obj->entry_ptr->next;
	}

	FQ_TRACE_EXIT(l);
	return(TRUE);

return_FALSE:
	if( fp ) fclose(fp);
	FQ_TRACE_EXIT(l);
	return(FALSE);
}


