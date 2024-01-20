/*
** delimiter_list.c
*/

# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <unistd.h>
# include <pthread.h>

#include "fq_defs.h"
#include "fq_delimiter_list.h"
#include "fq_logger.h"
#include "fq_common.h"

fq_logger_t *l=NULL;

int main()
{
	delimiter_list_obj_t	*obj=NULL;
	delimiter_list_t *this_entry=NULL;
	int rc;
	int t_no;
	
	rc = fq_open_file_logger(&l, "/tmp/delimiter_list.log", FQ_LOG_TRACE_LEVEL);
	CHECK(rc == TRUE);

	rc = open_delimiter_list(l, &obj, "AAA|BBB|CCC|DDD|EEE", '|');
	CHECK( rc == TRUE );

	printf("count = [%d]\n", obj->count);
	// obj->on_print(obj);

	this_entry = obj->head;

	for( t_no=0;  (this_entry->next && this_entry->value); t_no++ ) {
		printf("this_entry->value=[%s]\n", this_entry->value);
        this_entry = this_entry->next;
    }

	close_delimiter_list(l, &obj);

	fq_close_file_logger(&l);

	return(0);
}
