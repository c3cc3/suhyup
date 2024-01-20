/*
** fq_mem.c
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fq_mem.h"

static void mprint(mem_obj_t *obj)
{
	int i;

	for(i=0; i<obj->size; i++) {
		// if( obj->p[i] == 0 ) break;
		printf("%c", obj->p[i]);
	}
	printf("\n");

	return;
}

static void mgets(mem_obj_t *obj, char *dst)
{
	int i;

	for(i=0; i<obj->size; i++) {
		if( obj->p[i] == 0 ) break;
		dst[i] = obj->p[i];
	}

	dst[i] = 0x00;

	return;
}

static int mback(mem_obj_t *obj, int offset)
{
	obj->c -= offset;
	return(offset);
}
		
static int mseek(mem_obj_t *obj, int offset)
{
	obj->c = offset;
	return(offset);
}

static int mcopy(mem_obj_t *obj, char *s, int s_len)
{
	int i;

	for(i=0; i<s_len; i++) {
		obj->mputc(obj, s[i]);
	}

	return(0);
}

static int mputc(mem_obj_t *obj, char c)
{
	obj->p[obj->c] = c;
	obj->c++;
	return(0);
}

static char mgetc(mem_obj_t *obj)
{
	char c;

	c = obj->p[obj->c];
	obj->c++;
	return(c);
}

static int meof( mem_obj_t *obj)
{
	if( obj->c >= obj->size )
		return(1);
	else
		return(0);
}

int open_mem_obj( fq_logger_t *l, mem_obj_t **obj, int size)
{
	mem_obj_t *rc=NULL;

	FQ_TRACE_ENTER(l);

	rc = (mem_obj_t *)calloc(1, sizeof(mem_obj_t));

	if( rc ) {
		rc->size = size;
		rc->p = calloc(size, sizeof(char));
		rc->c = 0;

		rc->mgetc = mgetc;
		rc->mputc = mputc;
		rc->meof = meof;
		rc->mcopy = mcopy;
		rc->mseek = mseek;
		rc->mprint = mprint;
		rc->mback = mback;
		rc->mgets = mgets;

		*obj = rc;
		FQ_TRACE_EXIT(l);
		return(0);
	}

	if(rc) free(rc);

	FQ_TRACE_EXIT(l);
	return(-1);
}

int close_mem_obj( fq_logger_t *l, mem_obj_t **obj )
{
	FQ_TRACE_ENTER(l);

	if( (*obj)->p ) free((*obj)->p);
	if( (*obj) ) free((*obj));

	FQ_TRACE_EXIT(l);
	return(0);
}

#if 0
gssi_proc( MEM *fp, gssi_t *gssi )
{
	while ( !meof(fp) ) {
		dst[0] = '\0';
		if ( (c1 = getc(fp)) == EOF )
			break;
		else if (c1 == '^') {
			if ( (c2 = getc(fp)) == EOF ) {
				n += gssi_putc(gssi, c1);
				break;
			}
			else if ( c2 == '@' ) {
				tagid = gssi_gettag(fp, tag);
				(void)gssi_getarg(gssi, fp, arg);

				switch ( gssi_tagtype(tagid) ) {
					case TAGTYPE_VARIABLE:
						rc = gssi_variable(gssi, tagid, arg, dst);
						if ( rc < 0 )
							gssi->tagerrors++;
						n += gssi_puts(gssi, dst);
						break;
					case TAGTYPE_TRANSLATE: 
						rc = gssi_translate(gssi, tagid, arg, dst);
						if ( rc < 0 )
							gssi->tagerrors++;
						n += gssi_puts(gssi, dst);
						break;
					case TAGTYPE_CONDITION:
						rc = gssi_condition(gssi, tagid, arg, &gssi->skip);
						if ( rc < 0 )
							gssi->tagerrors++;
						break;
					case TAGTYPE_TABLE:
						rc = gssi_table(gssi, tagid, arg);
						if ( rc < 0 )
							gssi->tagerrors++;
						else
							n += rc;
						break;
					case TAGTYPE_SELECT:
						rc = gssi_select(gssi, tagid, arg);
						if ( rc < 0 )
							gssi->tagerrors++;
						else
							n += rc;
						break;
					case TAGTYPE_INCLUDE:
						gssi_included = gssi_copy(gssi, arg);
						if ( !gssi_included ) {
							gssi->tagerrors++;
							break;
						}
						rc = gssi_proc(gssi_included, 0);
						if ( rc < 0 )
							gssi->tagerrors++;
						else
							n += rc;
						gssi_free(&gssi_included);
						break;
					case TAGTYPE_PIS:
						rc = insert_pismap(_pismap, gssi);
						if ( rc < 0 )
							gssi->tagerrors++;
						else
							n += rc;
						break;
					default:
						gssi->tagerrors++;
				}
			}
			else {
				n += gssi_putc(gssi, c1);
				n += gssi_putc(gssi, c2);
			}
		}
		else {
			n += gssi_putc(gssi, c1);
		}
	}
#endif
