#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fq_conf.h"
#include "fq_common.h"

int main(int ac, char **av)
{
	int rc=-1;
	conf_obj_t *obj=NULL;
	char	buf[128];
	char	c;
	int		i;
	float	f;
	long	l;

	if(ac != 2) {
		printf("Usage: $ %s [config_file] <enter>\n", av[0]);
		return(-1);
	}

	rc = open_conf_obj( "./tst.cfg", &obj);
	if( rc == FALSE) {
		fprintf(stderr, "open_conf_obj() error.\n");
		return(-2);
	}
	step_print("open_conf_obj()", "OK");

	obj->on_get_str(obj, "SVR_NAME", buf);
	printf("SVR_NAME=[%s]\n", buf);
	step_print("obj->on_get_str()", "OK");

	c = obj->on_get_char(obj, "KEY_CHAR");
	printf("KEY_CHAR=[%c]\n", c);
	step_print("obj->on_get_char()", "OK");

	i = obj->on_get_int(obj, "MAX_SESSIONS");
	printf("MAX_SESSIONS=[%d]\n", i);
	step_print("obj->on_get_int()", "OK");

	f = obj->on_get_float(obj, "AVERAGE");
	printf("AVERAGE=[%f]\n", f);
	step_print("obj->on_get_float()", "OK");

	l = obj->on_get_long(obj, "TOTAL");
	printf("TOTAL=[%ld]\n", l);
	step_print("obj->on_get_long()", "OK");

	obj->on_print_conf(obj);
	step_print("obj->on_print_conf()", "OK");

	close_conf_obj(&obj);
	step_print("close_conf_obj()", "OK");

	return(0);

}
