#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>

#include "fq_mask.h"
#include "fq_common.h"

typedef enum { TRACE_EVENT=0, TRACE_SESS, TRACE_SOCK, TRACE_SEMA, TRACE_SHM, TRACE_CONF, TRACE_CONV, TRACE_DEBUG } loglevel_t; 

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
	step_print("open_mask_obj()", "OK");

	obj->bit = TRACE_SOCK;
	obj->yesno = strdup("yes");
	obj->on_set_mask(obj);
	step_print("on_set_mask():SOCK", "OK");

	obj->bit = TRACE_SHM;
	obj->yesno = strdup("yes");
	obj->on_set_mask(obj);
	step_print("on_set_mask():SHM", "OK");

	obj->bit = TRACE_SOCK;
	flag = obj->on_get_mask(obj);
	printf("bit TRACE_SOCK is [%d]\n", flag);
	step_print("on_get_mask()", "OK");

	obj->on_print_mask(obj);

#if 1
	TRACE(TRACE_SOCK, obj,  "%s", "socket trace.\n");
	TRACE(TRACE_SESS,  obj, "%s", "session trace.\n");
	TRACE(TRACE_SHM,  obj, "%s", "shared memory trace.\n");
	TRACE(TRACE_EVENT,  obj, "%s", "event trace.\n");
#endif
	step_print("masking exercise TRACE()", "OK");

	close_mask_obj(&obj);
	step_print("close_mask_obj()", "OK");
}
