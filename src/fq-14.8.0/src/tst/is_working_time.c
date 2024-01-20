#include "fq_common.h"
#include <stdbool.h>
#include <time.h>

int main(int ac, char **av)
{
	fq_logger_t *l=NULL;
	int log_level=2; /* info */
	char *logname = "/tmp/is_working_time.log";
	int ret;

	if( ac != 3 ) {
		// printf("Usage: $ %s [limit_start_hour] [during_hour] <enter>\n", av[0]);
		printf("Usage: $ %s [from_hour] [to_hour] <enter>\n", av[0]);
		exit(0);
	}

	ret = fq_open_file_logger(&l, logname, log_level);
	CHECK( ret > 0 );

	bool using_limit_working_time_flag = 1;

	char *from_hour = av[1];
	char *to_hour = av[2];
	

	if( using_limit_working_time_flag == 1 ) {
		while(1) {
			bool rc;
			rc = is_working_time(l, from_hour, to_hour); /* 금일 08:00:00 ~ 22:59:59 발송정지 */
			if( rc == true ) {
				printf("It is working time.\n");
				fq_log(l, FQ_LOG_INFO, "It is working time.");
			}
			else {
				printf("It is not working time.\n");
				fq_log(l, FQ_LOG_INFO, "It is not working time.");
			}
			sleep(1);
		}
	}
	if(l) {
        fq_close_file_logger(&l);
    }

	return(0);
}
