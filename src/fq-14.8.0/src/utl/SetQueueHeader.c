/*
 * $(HOME)/src/utl/SetQueueHeader.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include "fqueue.h"

int SetHeaderValue( char *path, char *qname);

int main(int ac, char **av)  
{  
	char *path="/home/pi/data";
	char *qname="TST";

	SetHeaderValue( path, qname);

	return(0);
}

int SetHeaderValue( char *path, char *qname)
{
	fqueue_obj_t *obj=NULL;

	OpenFileQueue(NULL, NULL, NULL, NULL, NULL, path, qname, &obj);

	obj->h_obj->h->de_sum = obj->h_obj->h->en_sum + 1;

	CloseFileQueue(NULL, &obj);
	return(0);
}
