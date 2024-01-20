
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <unistd.h>
# include <pthread.h>

#include "fq_file_table.h"
#include "fq_logger.h"
#include "fq_common.h"

int main()
{
	fq_logger_t *l=NULL;
	file_table_obj_t	*obj=NULL;
	int rc;
	CRON *this_entry=NULL;

	rc = fq_open_file_logger(&l, "./file_table.log", get_log_level("trace"));
	CHECK(rc == TRUE);

	rc = open_file_table(l, &obj, "./crontab");

	CHECK( rc == TRUE );
	printf("open_file_table() success!\n");

	obj->on_print(obj);

	this_entry = obj->head;
	while (this_entry->next && this_entry->mn && this_entry->cmd) {
		printf("%s %s %s %s %s %s\n",  this_entry->mn,
										this_entry->hr,
										this_entry->day,
										this_entry->mon,
										this_entry->wkd,
										this_entry->cmd);
		this_entry = this_entry->next;
	}

	close_file_table(l, &obj);

	fq_close_file_logger(&l);

	return(0);
}
