/*
** fq_dir_CB_test.c
*/


#include <stdio.h>
#include "fq_dir.h"
#include "fq_logger.h"


int my_CB_func(fq_logger_t *l, char *pathfile)
{
	printf("%s\n", pathfile);
	return(0);
}

int main()
{
	recursive_dir_CB( NULL, ".", my_CB_func);
	return(0);
}
