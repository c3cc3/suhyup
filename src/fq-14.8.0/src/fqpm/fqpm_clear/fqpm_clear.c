/*
** fpqm_clear.c
*/
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <locale.h>
#include "fqueue.h"
#include "fq_cache.h"
#include "fq_common.h"
#include "fq_logger.h"
#include "fq_manage.h"

void all_clear_fqpm_pids( char *path );
void one_clear_fqpm_pid( char *path, pid_t target_pid );

void print_help(char *progname)
{
	printf("Usage: $ %s -t [pid]\n", progname);
	printf("Usage: $ %s -p [path] -t [pid]\n", progname);
	return;
}

int main(int ac, char **av)
{
	int rc;
	int		ch;
	char *path=NULL;
	pid_t	target_pid=-1;


	if( ac < 2 ) {
		print_help(av[0]);
		return(0);
	}

	printf("Compiled on %s %s.\n", __TIME__, __DATE__);

	while(( ch = getopt(ac, av, "hH:p:t:")) != -1) {
		switch(ch) {
			case 'h':
			case 'H':
				print_help(av[0]);
				return(0);
			case 'p': /* clear */
				path = strdup(optarg);
				break;
			case 't': /* target pid */
				target_pid = (pid_t) atoi(optarg);
				break;
			default:
				print_help(av[0]);
				return(0);
		}
	}

	/* Checking mandatory parameters */
	if( !path ) {
		char    *fq_home_path=NULL;

		fq_home_path = getenv("FQ_DATA_HOME");
		if( !fq_home_path ) {
			fprintf(stderr, "There is no FQ_DATA_HOME in env of server.\n");
			return(-1);
		}
		char pids_path[256];
		sprintf(pids_path, "%s/FQPM", fq_home_path);
		path = strdup(pids_path);
	}

	if( target_pid > 0 ) {
		one_clear_fqpm_pid(path, target_pid);
	}
	else {
		all_clear_fqpm_pids(path);
	}

	return 0;
}
void one_clear_fqpm_pid( char *path, pid_t target_pid )
{
	pslist_t *pslist;
	char answer[2];

	pslist = pslist_new();

	/* This makes a linkedlist with $FQ_DATA_HOME/FQMP/* */
	bool tf;
	tf = make_ps_list(0x00, path, pslist);
	if( tf == false) return;

	ps_t *p=NULL;
	fprintf(stdout, "pslist contains %d elements\n", pslist->length);
	
	for( p=pslist->head; p != NULL; p=p->next) {
		if( is_live_process(p->psinfo->pid) ) {
			printf("Process: %s.\n", p->psinfo->run_name);
			getprompt_str("Will you stop it(y/n):", answer, 1);
reget:
			if( answer[0] == 'n')  {
				printf("pid=[%d] is alive. -> skip\n", p->psinfo->pid);
				continue;
			}
			else if( answer[0] == 'y')  {
				int rc;
				char cmd[256];
				sprintf(cmd, "kill -9 %d", p->psinfo->pid);
				system(cmd);
				// kill( p->psinfo->pid, 0); // Don't work.
			}
			else {
				goto reget;
			}
		}
		fprintf(stdout, "\t clear -> - pid='%d', run_name='%s'\n", 
			p->psinfo->pid, p->psinfo->run_name );

		if( p->psinfo->pid == target_pid ) {
			delete_me_on_fq_framework( 0x00, path, p->psinfo->pid);
		}
	}
	if(pslist) pslist_free(&pslist);

	return;
}

void all_clear_fqpm_pids( char *path )
{
	pslist_t *pslist;
	char answer[2];

	pslist = pslist_new();


	/* This makes a linkedlist with $FQ_DATA_HOME/FQMP/* */
	bool tf;
	tf = make_ps_list(0x00, path, pslist);
	if( tf == false) return;

	ps_t *p=NULL;
	fprintf(stdout, "pslist contains %d elements\n", pslist->length);
	
	for( p=pslist->head; p != NULL; p=p->next) {
		if( is_live_process(p->psinfo->pid) ) {
reget:
			printf("Process: %s.\n", p->psinfo->run_name);
			getprompt_str("Will you stop it(y/n):", answer, 1);
			if( answer[0] == 'n')  {
				printf("pid=[%d] is alive. -> skip\n", p->psinfo->pid);
				continue;
			}
			else if( answer[0] == 'y')  {
				int rc;
				char cmd[256];
				sprintf(cmd, "kill -9 %d", p->psinfo->pid);
				system(cmd);
				// kill( p->psinfo->pid, 0); // Don't work.
			}
			else {
				goto reget;
			}
		}

		fprintf(stdout, "\t clear -> - pid='%d', run_name='%s'\n", 
			p->psinfo->pid, p->psinfo->run_name );

		delete_me_on_fq_framework( 0x00, path, p->psinfo->pid);
	}
	if(pslist) pslist_free(&pslist);

	return;
}
