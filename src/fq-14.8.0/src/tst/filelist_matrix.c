/*
** filelist_matrix.c
*/

# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <unistd.h>
# include <pthread.h>

#include "fq_defs.h"
#include "fq_file_list.h"
#include "fq_logger.h"
#include "fq_common.h"

int main()
{
	fq_logger_t *l=NULL;
	file_list_obj_t	*agent_co_list=NULL;
	file_list_obj_t	*mb_list=NULL;
	FILELIST *this_entry_agent_co=NULL;
	FILELIST *this_entry_mb=NULL;
	int count;
	int rc;
	
	rc = fq_open_file_logger(&l, "/tmp/filelist_matrix.log", FQ_LOG_TRACE_LEVEL);
	CHECK(rc == TRUE);

	rc = open_file_list(l, &agent_co_list, "./agent_co.list");
	CHECK( rc == TRUE );
	// printf("count = [%d]\n", agent_co_list->count);
	// agent_co_list->on_print(agent_co_list);

	rc = open_file_list(l, &mb_list, "./mb.list");
	CHECK( rc == TRUE );
	// printf("count = [%d]\n", mb_list->count);
	// mb_list->on_print(mb_list);

	this_entry_agent_co = agent_co_list->head;

	count = 0;
	while( this_entry_agent_co->next && this_entry_agent_co->value ) {
		this_entry_mb = mb_list->head;
		while( this_entry_mb->next && this_entry_mb->value ) {
			printf("[%d]: [%s]-[%s]\n", count, this_entry_agent_co->value, this_entry_mb->value);
			this_entry_mb = this_entry_mb->next;
			count++;
		}
        this_entry_agent_co = this_entry_agent_co->next;
    }

	close_file_list(l, &agent_co_list);
	close_file_list(l, &mb_list);

	fq_close_file_logger(&l);

	return(0);
}
