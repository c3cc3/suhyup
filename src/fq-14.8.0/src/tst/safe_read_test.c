/* How to read a log file safely */
/* If the process stops and runs, it reads contents from last offset */

#include "config.h"
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include <stdbool.h>
#include "fq_defs.h"
#include "fq_logger.h"
#include "fq_types.h"
#include "fq_common.h"
#include "fq_safe_read.h"

int main(int ac, char **av)
{
	int rc;
	char *data_filename=NULL;
	char *linebuf=NULL;
	long	processing_count = 0L;
	safe_read_file_obj_t *obj=NULL;
	fq_logger_t *l=NULL;

	if( ac != 2 ) {
		printf("Usage: $ %s [data filename] <enter>\n", av[1]);
		exit(0);
	}
	data_filename = av[1];

	rc = fq_open_file_logger(&l, "/tmp/read_log.log", get_log_level("debug"));
	CHECK(rc==TRUE);

	rc = open_file_4_read_safe(l, data_filename, false, &obj);
	CHECK(rc == TRUE);

	while(1) {
		rc = obj->on_read(obj, &linebuf);
		switch(rc) {
			case FILE_NOT_EXIST:
				printf("FILE_NOT_EXIST\n");
				sleep(5);
				break;
			case FILE_READ_SUCCESS:
				printf("linebuf len=[%zu]\n", strlen(linebuf));

				/* ToDo your job with linebuf in here.
				**
				**
				**
				*/

				break;
			case FILE_READ_FATAL:
				printf("FILE_READ_FATAL. Good bye.\n");
				return(-1);
			case FILE_NO_CHANGE:
				printf("FILE_NO_CHANGE.\n");
				sleep(1);
				break;
			case FILE_READ_RETRY:
				printf("FILE_READ_RETRY.\n");
				sleep(1);
				break;
			default:
				printf("Undefined return.\n");
				return(-1);
		}
		SAFE_FREE(linebuf);
	}
	return(0);
}
