/*
** fq_posixlib.c
*/
#define FQ_POSIXLIB_C_VERSION "1.0.0"

#include "config.h"

#ifdef HAVE_BASENAME
#include <libgen.h>

char *getLastElement(char *path)
{
	return(basename(path));
}
#endif

#ifdef HAVE_DIFFTIME
#include <time.h>

long DiffTime(time_t t1, time_t t2)
{
	return(difftime(t1, t2));
}
#endif
