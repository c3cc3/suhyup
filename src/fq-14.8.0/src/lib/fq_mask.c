/***********************************************
* fq_mask.c
*
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>

#include "fq_mask.h"
#include "fq_common.h"

#define FQ_MASK_C_VERSION "1.0.0"

static int on_set_mask( mask_obj_t *);
static int on_get_mask( mask_obj_t *);
static void on_print_mask( mask_obj_t *);

int open_mask_obj( char *name, mask_obj_t **obj)
{
	mask_obj_t *rc;

	assert(name);

	rc = (mask_obj_t *)calloc(1, sizeof(mask_obj_t));

	if(rc) {
		rc->name = strdup(name);
		rc->mask = 0L;
		rc->on_set_mask = on_set_mask;
		rc->on_get_mask = on_get_mask;
		rc->on_print_mask = on_print_mask;
		rc->yesno = NULL;

		*obj = rc;
		return(TRUE);
	}

	SAFE_FREE( (*obj) );

	return(FALSE);
}

int close_mask_obj(mask_obj_t **obj)
{
	SAFE_FREE( (*obj)->name );
	SAFE_FREE( (*obj)->yesno );
	SAFE_FREE( (*obj) );

	return(TRUE);
}

static int on_set_mask( mask_obj_t *obj)
{
	if ( !obj->yesno )
		return(FALSE);

	if( obj->bit < 0 || obj->bit > 32 )
		return(FALSE);

	if ( strcmp(obj->yesno, "yes") == 0 ) {
		obj->mask = (obj->mask | (1L << obj->bit));
		return(TRUE);
	}
	else if ( strcmp(obj->yesno, "no") == 0 ) {
		obj->mask = (obj->mask & ~(1L << obj->bit));
		return(TRUE);
	}
	else
		return(FALSE);
}

/******************************************************************
**
*/
static int on_get_mask( mask_obj_t *obj)
{
	if( obj->bit < 0 || obj->bit > 32 )
		return(FALSE);

	return ( (obj->mask & (1L << obj->bit)) > 0L ? 1 : 0 );
}

static void on_print_mask( mask_obj_t *obj )
{
	int j;
	int mask;

	if(!obj) return;
	if(!obj->name) return;

	printf("[%s] masking result : ", obj->name );
	for(j=0; j<32; j++) {
		obj->bit = j;
		mask = obj->on_get_mask( obj );
		printf("%d", mask);
	}
	printf("\n");

	return;
}


/*
 * This is a sample code using mask object.
 */
#if 0
int is_online=0;

void TRACE(int trace_flag, mask_obj_t *obj,  const char *fmt, ...)
{
    char buf[1024];
    va_list ap;

    if ( is_online && !(trace_flag == TRACE_EVENT) )
        return;

	obj->bit = trace_flag;
    if ( obj && !obj->on_get_mask(obj) )
        return;

    assert(fmt);

    va_start(ap, fmt);
        vsprintf(buf, fmt, ap);
    va_end(ap);

    if ( strlen(buf) > 4096 ) {
        buf[4092] = buf[4093] = buf[4094] = '.';
        buf[4095] = '\n'; buf[4096] = '\0';
    }
    fprintf(stdout, "%s", buf);
    fflush(stdout);
}

int main(int ac, char **av)
{
	mask_obj_t *obj;
	int rc;
	int	flag;

	rc = open_mask_obj( "config_trace_flag", &obj );
	if( rc != TRUE ) {
		printf("open_mask_obj() error.\n");
		return(rc);
	}

	obj->bit = TRACE_SOCK;
	obj->yesno = strdup("yes");
	obj->on_set_mask(obj);

	obj->bit = TRACE_SHM;
	obj->yesno = strdup("yes");
	obj->on_set_mask(obj);

	obj->bit = TRACE_SOCK;
	flag = obj->on_get_mask(obj);
	printf("bit TRACE_SOCK is [%d]\n", flag);

	obj->on_print_mask(obj);

	TRACE(TRACE_SOCK, obj,  "%s", "socket trace.\n");
	TRACE(TRACE_SESS,  obj, "%s", "session trace.\n");
	TRACE(TRACE_SHM,  obj, "%s", "shared memory trace.\n");
	TRACE(TRACE_EVENT,  obj, "%s", "event trace.\n");

	close_mask_obj(&obj);
}
#endif
