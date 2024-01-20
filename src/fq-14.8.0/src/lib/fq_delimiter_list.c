/*
 * fq_delimiter_list.c
 */
#define FQ_DELIMITER_LIST_C_VERSION "1.0.0"

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <strings.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h> /* for chmod() */
#include <stdbool.h>

#include "fq_common.h"
#include "fq_logger.h"
#include "fq_delimiter_list.h"

static int on_print( delimiter_list_obj_t *obj )
{
  delimiter_list_t *this_entry=NULL;

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
static void assign( delimiter_list_t *entry, char *buf)
{
  entry->value     = strdup(buf);
}

static void
entry_free(delimiter_list_t *t)
{
    SAFE_FREE(t->value);
    SAFE_FREE(t);
}

int close_delimiter_list( fq_logger_t *l,  delimiter_list_obj_t **obj )
{
  	delimiter_list_t *p, *q;

	FQ_TRACE_ENTER(l);
  	p = (*obj)->head;

  	while (p) {
		q = p;
		p = p->next;
		entry_free(q);
  	}
	(*obj)->head = NULL;

    SAFE_FREE((*obj)->entry_ptr);

    SAFE_FREE( (*obj));

	FQ_TRACE_EXIT(l);
	return TRUE;
}

int 
open_delimiter_list( fq_logger_t *l, delimiter_list_obj_t **obj, char *src, char delimiter) 
{
	delimiter_list_obj_t *rc=NULL;
	char ch;

	FQ_TRACE_ENTER(l);

	if ( !src ) {
		FQ_TRACE_EXIT(l);
		return(FALSE);
	}

	rc = (delimiter_list_obj_t *)calloc(1, sizeof(delimiter_list_obj_t));

	rc->entry_ptr = (delimiter_list_t *)malloc(sizeof(delimiter_list_t));
	rc->entry_ptr->next = NULL;
	rc->head = rc->entry_ptr;
	rc->count = 0;
	if(rc) {
		int len;
		char *p;
		char tmpbuf[MAX_SRC_SIZE];

		len = 0;
		rc->entry_ptr = rc->head;
		p  = src;

		memset(tmpbuf, 0x00, sizeof(tmpbuf));

		while ( 1 ) {
			ch = *p;

			if( ch == 0x00 ) { /* last item */ 
				tmpbuf[len] = 0x00;
				fq_log(l, FQ_LOG_DEBUG, "tmpbuf=[%s]\n", tmpbuf);
				assign(rc->entry_ptr, tmpbuf);
				rc->count++;

				if (rc->entry_ptr->next == NULL) {
					rc->entry_ptr->next = (delimiter_list_t *)malloc(sizeof(delimiter_list_t));
					rc->entry_ptr->next->next = NULL;
				}
				rc->entry_ptr = rc->entry_ptr->next;
				break;
			}	
			if( ch == delimiter ) {
				tmpbuf[len] = 0x00;
				fq_log(l, FQ_LOG_DEBUG, "tmpbuf=[%s]\n", tmpbuf);
				assign(rc->entry_ptr, tmpbuf);
				len = 0;
				memset(tmpbuf, 0x00, sizeof(tmpbuf));
				rc->count++;

				if (rc->entry_ptr->next == NULL) {
					rc->entry_ptr->next = (delimiter_list_t *)malloc(sizeof(delimiter_list_t));
					rc->entry_ptr->next->next = NULL;
				}
				rc->entry_ptr = rc->entry_ptr->next;
				p++;
				
			}
			else {
				tmpbuf[len++] = ch;
				p++;
			}

		}

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

	SAFE_FREE(rc);
	SAFE_FREE(*obj);
	
	FQ_TRACE_EXIT(l);
	return(FALSE);
}

bool ListCallback( delimiter_list_obj_t *obj, ListCB usrFP )
{
  delimiter_list_t *this_entry=NULL;

  FQ_TRACE_ENTER(obj->l);
  this_entry = obj->head;

  while (this_entry->next && this_entry->value ) {
	bool usr_rc;
	usr_rc = usrFP( this_entry->value );
	if( usr_rc == false ) return false;
	// printf("this_entry->value=[%s]\n", this_entry->value);
	this_entry = this_entry->next;
  }

  FQ_TRACE_EXIT(obj->l);
  return true;
}
