/*
** utl/qvf.c
*/

#include "config.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <locale.h>
#include "fqueue.h"
#include "fq_cache.h"
#include "fq_common.h"
#include "fq_logger.h"
#include "fq_monitor.h"

#define QVF_C_VERSION "2.0.2"

/* golbal variables */
char    *g_path=NULL;
char    *g_qname=NULL;
char    *g_logname = NULL;
char    *g_loglevel = NULL;
char 	*g_hangul_set = NULL;
int		g_hangul_flag = 0;

int     i_loglevel;
fq_logger_t *g_l=NULL;
fqueue_obj_t *g_obj=NULL;
pthread_mutex_t g_mutex;
int     g_break_flag = 0;
int     g_max_rows;
int 	g_FQ_server_count=0;

long before_en = 0;
long before_de = 0;

void print_version(char *p)
{
    printf("\nversion: %s.\n\n", QVF_C_VERSION);
    return;
}

void print_help(char *p)
{
	char *data_home=NULL;
	char *pwd=NULL;

	data_home = getenv("FQ_DATA_HOME");
	pwd = getenv("PWD");

	printf("\n\nUsage  : $ %s [-V] [-h] [-P path] [-Q qname] [-l logname] [-o loglevel] [-H hangul] <enter>\n", p);
	printf(" -V: version \n");
	printf(" -h: help \n");
	printf(" -P: path \n");
	printf(" -Q: qname \n");
	printf(" -l: log file name \n");
	printf(" -o: log level ( trace|debug|info|error|emerg )\n");
	printf(" -H: Hangul flag \n");

	printf("example: $ %s -P %s -Q TST -l %s/%s.log -o error -H off <enter>\n", p, data_home?data_home:"/data", pwd, p);
	return;
}

int get_process_count( char *program_name )
{
	FILE	*fp;
	char	pid[80];
	int count=0;
	char	cmd[256];

	sprintf(cmd, "ps auxww | grep '%s' | grep -v grep | gawk -F\" \" '{ printf $2 \"\\n\" }'",  program_name);

	fp = popen(cmd, "r");

	if( !fp ) {
		fprintf(stderr, "popen() error.\n");
        return(-1);
    }

	while(fgets(pid, 80, fp) != NULL) {
		count++;
	}

	if(fp != NULL ) {
		pclose(fp);
        fp = NULL;
    }

	return(count);
}

int main(int ac, char **av)
{
	int rc;
	int		ch;
	char *oldLocale = setlocale(LC_NUMERIC, NULL);

	printf("Compiled on %s %s version=[%s]\n", __TIME__, __DATE__, QVF_C_VERSION);

	while(( ch = getopt(ac, av, "Vvh:P:Q:l:o:H:")) != -1) {
		switch(ch) {
			case 'h':
				print_help(av[0]);
				return(0);
			case 'V':
			case 'v':
				print_version(av[0]);
				return(0);
			case 'P':
				g_path = strdup(optarg);
				break;
			case 'Q':
				g_qname = strdup(optarg);
				break;
			case 'l':
				g_logname = strdup(optarg);
				break;
			case 'o':
				g_loglevel = strdup(optarg);
				break;
			case 'H':
				g_hangul_set = strdup(optarg);
				break;
			default:
				print_help(av[0]);
				return(0);
		}
	}

	/* Checking mandatory parameters */
	if( !g_path || !g_qname || !g_logname || !g_loglevel ) {
		print_help(av[0]);
		return(0);
	}

	i_loglevel = get_log_level(g_loglevel);

	rc = fq_open_file_logger(&g_l, g_logname, i_loglevel);
    if( rc <= 0 ) {
        printf("log file[%s] open error\n", g_logname);
        _exit(0);
    }

	/* _M is opening for monitoring */
    if( OpenFileQueue_M(g_l, NULL, g_path, g_qname, &g_obj) == FALSE ) {
        printf("Queue File [%s][%s] open error.\n", g_path, g_qname);
        _exit(0);
    }

	printf("Queue open OK.\n");

	g_FQ_server_count = get_process_count("FQ_server -f");

	while(1) {

		fprintf(stdout,  "%-40.40s:%s\n", 	"The path of queue file", 	g_obj->path);
		fprintf(stdout,  "%-40.40s:%s\n", 	"The queue Name",	g_obj->qname);
		fprintf(stdout,  "%-40.40s:%'ld (bytes)\n", 	"File size",	(long int)g_obj->h_obj->h->file_size);
		fprintf(stdout,  "%-40.40s:%'d \n", 	"The created queue spaces", g_obj->h_obj->h->max_rec_cnt);
		fprintf(stdout,  "%-40.40s:%'ld (%ld TPS)\n",	"The number of incoming msg(en-queue)",  g_obj->h_obj->h->en_sum, (before_en>0)?(g_obj->h_obj->h->en_sum-before_en): before_en);
		before_en = g_obj->h_obj->h->en_sum;
		fprintf(stdout,  "%-40.40s:%'ld (%ld TPS)\n", "The number of outcoming msg(de-queue)", g_obj->h_obj->h->de_sum, (before_de>0)?(g_obj->h_obj->h->de_sum-before_de): before_de);
		// printc(3,  "%-40.40s:%'ld (%ld TPS)\n", "The number of outcoming msg(de-queue)", g_obj->h_obj->h->de_sum, (before_de>0)?(g_obj->h_obj->h->de_sum-before_de): before_de);
		before_de = g_obj->h_obj->h->de_sum;

		fprintf(stdout,  "%-40.40s:%'ld ( The remaining space =[%'ld] )\n", "A gap between en and de", 
			(g_obj->h_obj->h->en_sum-g_obj->h_obj->h->de_sum), (g_obj->h_obj->h->max_rec_cnt-(g_obj->h_obj->h->en_sum-g_obj->h_obj->h->de_sum)) );

		fprintf(stdout,  "%-40.40s:%'d\n", 	"Untreated big messages's count", g_obj->h_obj->h->current_big_files);
		fprintf(stdout,  "%-40.40s:%'ld (bytes)\n", 	"The buffer size for a message(no-big)", g_obj->h_obj->h->msglen);
		char *p=NULL;
		fprintf(stdout,  "%-40.40s:%s\n", 	"last en queue time", (p=asc_time(g_obj->h_obj->h->last_en_time)));
		if(p) free(p);
		fprintf(stdout,  "%-40.40s:%s\n", 	"last de queue time", (p=asc_time(g_obj->h_obj->h->last_de_time)));
		if(p) free(p);
		fprintf(stdout,  "%-40.40s:%s\n", 	"latest en queue time (micro sec)", (p=asc_time(g_obj->h_obj->h->latest_en_time_date)));
		if(p) free(p);
		fprintf(stdout,  "%-40.40s:%s\n", 	"latest de queue time (micro sec)", (p=asc_time(g_obj->h_obj->h->latest_de_time_date)));
		if(p) free(p);
		fprintf(stdout,  "%-40.40s:%ld\n", 	"mapping length(area size) bytes", (long int)g_obj->h_obj->h->mapping_len);
		fprintf(stdout,  "%-40.40s:%ld-%ld\n", 	"en: from_offset-to_offset ", g_obj->h_obj->h->en_available_from_offset, g_obj->h_obj->h->en_available_to_offset);
		fprintf(stdout,  "%-40.40s:%ld-%ld\n", 	"de: from_offset-to_offset ", g_obj->h_obj->h->de_available_from_offset, g_obj->h_obj->h->de_available_to_offset);
		fprintf(stdout,  "%-40.40s:%d\n", 	"Current BIG files ", g_obj->h_obj->h->current_big_files);
		fprintf(stdout,  "%-40.40s:%d\n", 	"Master-Assign-Flag", g_obj->h_obj->h->Master_assign_flag );
		fprintf(stdout,  "%-40.40s:%s\n", 	"Master-Assign-Host", (g_obj->h_obj->h->MasterHostName[0]!=0x00)?g_obj->h_obj->h->MasterHostName:"not assign" );
		sleep(1);
		rc = system("clear");
		CHECK(rc != -1);
	}

    rc = CloseFileQueue_M(g_l, &g_obj);
	CHECK(rc==TRUE);
	
    fq_close_file_logger(&g_l);

	setlocale(LC_NUMERIC, oldLocale);

	fprintf(stdout, "Bye...\n");

	return(rc);
}
