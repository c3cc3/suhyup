/* mv_file_b_to_fq_multi.c 
**  Source License : CLANG co.
**  You can't copy a part of this source to your program without permition of CLANG Co.
**  This program move contents of a file which written with binary type to a file queue.
**
**  Meaning of b is that binary flag is true.
**
*/

/*
** Check list
** 1. memory leak -> passed
** 2. LARGEFILE = 3.2 Gbytes -> passed
** 3. Restart -> passed
** 4. config file test -> passed
** 5. enQ test
*/

#include "config.h"
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include <time.h>
#include "fq_defs.h"
#include "fq_logger.h"
#include "fq_types.h"
#include "fq_common.h"
#include "fq_config.h"
#include "fq_param.h"
#include "fq_safe_read_multi.h"
#include "fqueue.h"

#define MAX_MSG_LEN 65535

extern int errno;
char g_Progname[256];


void print_help(char *p)
{
	printf("\n\n Usage: $ %s [-f config filename] <enter>\n", p);
	return;
}

int main(int ac, char **av)
{
	int rc;
	char	buffer[256];
	char *data_filename=NULL; 
	char *logname = NULL;
	char *loglevel=NULL;
	int i_loglevel=4;
	char *config_filename=NULL;
	char *linebuf=NULL;
	fq_logger_t *l=NULL;
	int ch;
	config_t *t=NULL;
	char *qpath=NULL;
	char *qname=NULL;
	fqueue_obj_t *q_obj=NULL;
	safe_read_file_multi_obj_t *obj=NULL;
	char *starting_usage = NULL;
	float f_starting_usage;
	char *stdout_print=NULL;
	char *file_delete_option_str = NULL;
	int	 file_delete_option = 0;
	char *file_delete_hour_min = NULL;
	char del_hour_str[3], del_min_str[3];
	char *unique_process_id = NULL;
	
	getProgName(av[0], g_Progname);

	if(ac < 3 ) {
		print_help(g_Progname);
		return(0);
	}

	while(( ch = getopt(ac, av, "f:")) != -1) {
		switch(ch) {
			case 'f':
				config_filename = strdup(optarg);
				break;
			default:
				print_help(av[0]);
				return(0);
		}
	}

	if(!HASVALUE(config_filename) ) { 
		print_help(g_Progname);
		return(0);
	}

	/* Loading server environment */

	t = new_config(config_filename);

	if( load_param(t, config_filename) < 0 ) {
		fprintf(stderr, "load_param() error.\n");
		return(EXIT_FAILURE);
	}

	get_config(t, "DATA_FILENAME", buffer);
	data_filename = strdup(buffer);

	get_config(t, "LOG_NAME", buffer);
	logname = strdup(buffer);

	get_config(t, "LOG_LEVEL", buffer);
	loglevel = strdup(buffer);

	get_config(t, "QPATH", buffer);
	qpath = strdup(buffer);

	get_config(t, "QNAME", buffer);
	qname = strdup(buffer);

	get_config(t, "ENQ_STARTING_USAGE", buffer);
	starting_usage = strdup(buffer);
	f_starting_usage = atof(starting_usage);
	if( f_starting_usage > 99.9 || f_starting_usage < 0.0 ) {
		step_print("Checking ENQ_STARTING_USAGE value", "NOT");
		fprintf(stdout, "Check your ENQ_STARTING_USAGE value in config.\n");
		return(0);
	}
	step_print("Checking ENQ_STARTING_USAGE", "OK");

	get_config(t, "STDOUT_PRINT", buffer);
	stdout_print = strdup(buffer);
	if( (STRCCMP(stdout_print, "yes") != 0) && (STRCCMP(stdout_print, "no") != 0) ) {
		step_print("Checking STDOUT_PRINT value", "NOK");
		fprintf(stdout, "Check your STDOUT_PRINT value in config. (valid is yes or no)\n");
		return(0);
	}
	step_print("Checking STDOUT_PRINT", buffer);

	get_config(t, "FILE_DELETE_OPTION", buffer);
	if( !HASVALUE(buffer) ) {
		step_print("Checking FILE_DELETE_OPTION", "NOK");
		fprintf(stdout, "Check your FILE_DELETE_OPTION value in config. (valid is yes or no)\n");
		return(0);
	}
	step_print("Checking FILE_DELETE_OPTION", "OK");

	get_config(t, "UNIQUE_PROCESS_ID", buffer);
	if( !HASVALUE(buffer) ) {
		step_print("Checking UNIQUE_PROCESS_ID", "NOK");
		fprintf(stdout, "Check your UNIQUE_PROCESS_ID value in config.\n");
		return(0);
	}
	step_print("Checking UNIQUE_PROCESS_ID", "OK");

	file_delete_option_str = strdup(buffer);
	if( (STRCCMP(file_delete_option_str, "yes") == 0)) {
		step_print("File delete option", "YES");
		file_delete_option = TRUE;
	}
	else {
		step_print("File delete option", "NO");
		file_delete_option = FALSE;

		get_config(t, "FILE_DELETE_HOUR_MIN", buffer);
		if( !HASVALUE( buffer ) ) {
			step_print("Checking FILE_DELETE_HOUR_MIN", "NOK");
			fprintf(stdout, "Check your FILE_DELETE_HOUR_MIN value in config. (valid is mmhh(2330))\n");
			return(0);
		}
		step_print("Checking FILE_DELETE_HOUR_MIN", "OK");
		file_delete_hour_min = strdup(buffer);

		memset(del_hour_str, 0x00, sizeof(del_hour_str));
		memcpy(del_hour_str, file_delete_hour_min, 2);
		memset(del_min_str, 0x00, sizeof(del_min_str));
		memcpy(del_min_str, &file_delete_hour_min[2], 2);
	}

	/* Checking mandatory parameters */
	if( !data_filename|| !logname || !loglevel || !qpath || !qname || !starting_usage) {
		step_print("Checking values in your config", "NOT");
		printf("Check items(DATA_FILENAME/LOG_NAME/LOG_LEVEL/QPATH/QNAME/ENQ_STARTING_USAGE) in config file.\n");
		return(0);
	}


	step_print("Checking values of yours config.", "OK");

	i_loglevel = get_log_level(loglevel);
	rc = fq_open_file_logger(&l, logname, i_loglevel);
	if( rc <= 0 ) {
		step_print("Opening logfile.", "NOK");
		fprintf(stderr, "Please check log filename and directory of your config.\n");
		return(0);
	}
	step_print("Opening logfile.", "OK");

	if( strcmp(stdout_print,"yes")==0) {
		fq_file_logger_t *p = l->logger_private;
		fclose(p->logfile);
		p->logfile = stdout;
	}
	step_print("Checking value of STDOUT_PRINT.", "OK");

	rc =  OpenFileQueue(l, NULL, NULL, NULL, NULL, qpath, qname, &q_obj);
	if(rc != TRUE) {
		step_print("Opening File Queue.", "NOK");
		fq_log(l, FQ_LOG_ERROR, "OpenFileQueue('%s', '%s') error.\n Please Check your file queue path and name.", qpath, qname);
		return(0);
	}
	step_print("file queue open.", "OK");

	rc = open_file_4_read_safe_multi(l, data_filename, unique_process_id, true, &obj); /* binary flag is true */
	if(rc != TRUE) {
		step_print("Opening SafeFile Object.", "NOK");
		fq_log(l, FQ_LOG_ERROR, "open_file_4_read_safe_multi('%s', '%s') error.", data_filename, unique_process_id);
		return(0);
	}
	step_print("Opening SafeFile(binary) Object.", "OK");

	fq_log(l, FQ_LOG_EMERG, "Server started.(compiled:[%s][%s]", __DATE__, __TIME__);

	step_print("Server start.", "OK");
	while(1) { 
		long l_seq, run_time;
		int msglen;
		off_t offset;
		float q_current_usage =  q_obj->on_get_usage(l, q_obj);
		float progress_percent = 0.0;

		if( q_current_usage > f_starting_usage) {
			fq_log(l, FQ_LOG_INFO, "Current usage(%f) is bigger than setting value at config.(%f)\n",
				 q_current_usage, f_starting_usage);
			sleep(1);
			continue;
		}

		rc = obj->on_read_b(obj, &linebuf, &msglen); /* binary flag is true */
		switch(rc) {
			case FILE_NOT_EXIST:
				fq_log(l, FQ_LOG_INFO, "FILE_NOT_EXIST\n");
				sleep(5);
				break;
			case FILE_READ_SUCCESS:
				offset = obj->on_get_offset(obj);
				progress_percent = obj->on_progress(obj);

				fq_log(l, FQ_LOG_INFO, "read OK. message_len=[%zu], offset=[%ld], remain=[%5.2f]%%.", 
						msglen, obj->on_get_offset(obj), (100.0-progress_percent));
				/* ToDo your job with linebuf in here.
				*/
				while(1) {
					rc =  q_obj->on_en(l, q_obj, EN_NORMAL_MODE, linebuf, msglen+1, msglen, &l_seq, &run_time );
					if( rc == EQ_FULL_RETURN_VALUE )  {
						fq_log(l, FQ_LOG_INFO, "Queue is full. I will retry.");
						usleep(1000);
						continue;
					}
					else if( rc < 0 ) { 
						fq_log(l, FQ_LOG_ERROR, "fatal: enQ failure. We'll stop processing. rc=[%d].", rc);
						if( rc == MANAGER_STOP_RETURN_VALUE ) {
							fq_log(l, FQ_LOG_ERROR, "Manager asked to stop a processing.");
						}
						goto stop;
					}
					else {
						fq_log(l, FQ_LOG_INFO, "enQ success rc=[%d].", rc);
						break;
					}
				}

				break;
			case FILE_READ_FATAL:
				fq_log(l, FQ_LOG_ERROR, "fatat: FILE_READ_FATAL. Good bye.\n");
				return(-1);
			case FILE_NO_CHANGE:
				fq_log(l, FQ_LOG_INFO, "FILE_NO_CHANGE.(current offset=[%ld]\n", obj->offset);
				sleep(1);
				break;
			case FILE_READ_RETRY:
				fq_log(l, FQ_LOG_INFO, "FILE_READ_RETRY.\n");
				sleep(1);
				break;
			default:
				fq_log(l, FQ_LOG_ERROR, "Undefined return.\n");
				return(-1);
		}
		SAFE_FREE(linebuf);
	}

stop:
	fq_log(l, FQ_LOG_EMERG, "Procss stop normally.\n");

	if(obj) close_file_4_read_safe_multi(l, &obj);
	if(t) free_config(&t);
	fq_close_file_logger(&l);

	return(0);
}

