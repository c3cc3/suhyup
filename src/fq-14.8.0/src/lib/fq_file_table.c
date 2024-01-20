/*
 * fq_file_table.c
 */
#define FQ_FILE_TABLE_C_VERSION "1.0.0"

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
#include "fq_file_table.h"

static int on_print( file_table_obj_t *obj )
{
  CRON *this_entry=NULL;

  FQ_TRACE_ENTER(obj->l);
  this_entry = obj->head;

  while (this_entry->next && this_entry->mn && this_entry->cmd) {
	printf("this_entry->cmd=[%s]\n", this_entry->cmd);
	this_entry = this_entry->next;
  }

  FQ_TRACE_EXIT(obj->l);
  return 0;
}

/* Assign the field values to the current crontab entry in core. */
static void assign(entry, line)
CRON *entry;
char *line;
{
  entry->mn     = strdup(strtok(line, SEPARATOR));
  entry->hr     = strdup(strtok( (char *)NULL, SEPARATOR));
  entry->day    = strdup(strtok( (char *)NULL, SEPARATOR));
  entry->mon    = strdup(strtok( (char *)NULL, SEPARATOR));
  entry->wkd    = strdup(strtok( (char *)NULL, SEPARATOR));
  // entry->cmd    = strdup(strchr(entry->wkd, '\0') + 1);
  entry->cmd    = strdup(strtok( (char *)NULL, SEPARATOR));
}

static void
entry_free(CRON *t)
{
	SAFE_FREE(t->mn);
	SAFE_FREE(t->hr);
	SAFE_FREE(t->day);
	SAFE_FREE(t->mon);
	SAFE_FREE(t->wkd);
	SAFE_FREE(t->cmd);
    SAFE_FREE(t);
}

int close_file_table( fq_logger_t *l,  file_table_obj_t **obj )
{
  	CRON *p, *q;

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
open_file_table( fq_logger_t *l, file_table_obj_t **obj, char *table_path_filename) 
{
	struct stat file_st; /* file status */
	file_table_obj_t *rc=NULL;
	FILE *fp=NULL;

	FQ_TRACE_ENTER(l);

	if (stat(table_path_filename, &file_st)) {
		FQ_TRACE_EXIT(l);
		return(FALSE);
	}

	rc = (file_table_obj_t *)calloc(1, sizeof(file_table_obj_t));

	rc->entry_ptr = (CRON *)malloc(sizeof(CRON));
	rc->entry_ptr->next = NULL;
	rc->head = rc->entry_ptr;
	if(rc) {
		// char line[MAX_LINE_SIZE];

		if ((fp = fopen(table_path_filename, "r")) == NULL) {
			goto return_FALSE;
		}

		rc->prv_time = file_st.st_mtime;
		
		rc->entry_ptr = rc->head;

		while (fgets(rc->line_buf, MAX_LINE_SIZE, fp) != NULL) {
			int len;

			if (rc->line_buf[0] == '#' || rc->line_buf[0] == '\n') continue;

			len = strlen(rc->line_buf);
			rc->line_buf[len-1] = 0x00; /* remove NewLine Charactor */

			assign(rc->entry_ptr, rc->line_buf);

			if (rc->entry_ptr->next == NULL) {
				rc->entry_ptr->next = (CRON *)malloc(sizeof(CRON));
				rc->entry_ptr->next->next = NULL;
			}

			rc->entry_ptr = rc->entry_ptr->next;
		}
		
		(void) fclose(fp);
		while (rc->entry_ptr) {
			rc->entry_ptr->mn = NULL;
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
