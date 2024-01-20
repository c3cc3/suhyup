/*
**	fqpm.c
	FileQueue Process Manager
**
    Copyright (C) Clang Co.
    All rights reserved.
    Author: gwisang.choi <mailto:gwisang.cho@gmail.com>
	Tel: 082-10-7202-1516
*/
#if !defined(_ISOC99_SOURCE)
# define _ISOC99_SOURCE (1L)
#endif

#if !defined(_GNU_SOURCE)
# define _GNU_SOURCE (1L)
#endif

#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include "fq_common.h"
#include "fq_conf.h"
#include "fq_launcher.h"
#include "fq_manage.h"

static void make_alarm_msg(fq_logger_t *l, char *err_kind, fq_ps_info_t *p, char **alarm_msg, int *alarm_msg_len);
static int enQ(fq_logger_t *l, unsigned char *data, int data_len, fqueue_obj_t *obj);
static void mysignal_handler(int s_signum);

int main(int ac, char **av);

static int g_is_break = 0;

static void mysignal_handler(int s_signum)
{
    (void)fprintf(stderr, "CTRL+C\n");

    g_is_break = 1;

    (void)signal(s_signum, &mysignal_handler);
}

int main(int ac, char **av)
{
	int rc;
    int s_ticks;
	char	*path=NULL;
	char	*conf_file=NULL;
	conf_obj_t	*c=NULL;
	fq_logger_t *l=NULL;
	fqpm_eventlog_obj_t *evt_log_obj = NULL;
	fqueue_obj_t *alarm_q_obj = NULL;

	if( ac == 2 ) {
		conf_file = av[1];
	}
	else {
		path = getenv("FQ_DATA_HOME");
		char conf_filename[256];
		sprintf(conf_filename, "%s/fqpm.conf", path);
		conf_file = strdup(conf_filename);
		printf("We use '%s' config file.\n", conf_filename);
	}
	/* config */
	rc = open_conf_obj( conf_file, &c);
	CHECK(rc == TRUE);

	
	/* logging */
	char logpath[128];
	char logname[128];
	char loglevel[16];
	char logpathfile[256];
	char alarm_q_path[128];
	char alarm_q_name[32];
	c->on_get_str(c, "LOG_PATH", logpath);
	c->on_get_str(c, "LOG_NAME", logname);
	c->on_get_str(c, "LOG_LEVEL", loglevel);
	c->on_get_str(c, "ALARM_Q_PATH", alarm_q_path);
	c->on_get_str(c, "ALARM_Q_NAME", alarm_q_name);

	sprintf(logpathfile, "%s/%s", logpath, logname);
	rc = fq_open_file_logger(&l, logpathfile, get_log_level(loglevel));
	CHECK( rc > 0 );

	rc =  OpenFileQueue(l, NULL, NULL, NULL, NULL, alarm_q_path, alarm_q_name, &alarm_q_obj);
	CHECK( rc == TRUE );

	bool tf;
	tf =  open_fqpm_eventlog_obj( l, path, &evt_log_obj);
	CHECK(tf == true); 

    if(fq_launcher(l) == (-1)) {
        fq_log(l, FQ_LOG_ERROR, "hwport_launcher failed !.");
		exit(0);
    }

    (void)signal(SIGINT, &mysignal_handler); /* CTRL+C */

	/*
	** To do your jobs in here
	*/
	while(1) {
		char *path=NULL;
		pslist_t *pslist=NULL;
		pslist = pslist_new();	
		
		bool	tf;
		tf = make_ps_list( l, path, pslist); 
		if(tf == false);

		// ToDo
		// Full scans nodes of a list.

		ps_t *p=NULL;
		fq_log(l, FQ_LOG_DEBUG, "pslist contains %d elements.", pslist->length);
		fq_log(l, FQ_LOG_DEBUG, "Dead process list:");
		
		for( p=pslist->head; p != NULL; p=p->next) {

			if( is_live_process(p->psinfo->pid) ) {
				pid_t pid = p->psinfo->pid;
				if( !is_block_process(l, path, pid, 70) ) {
					/* normal process */
				 	continue;
				}
				else {
					/* block */
					fq_log(l, FQ_LOG_EMERG,"\t- %d is blocked, I sent kill signal to %d.", pid, pid);
					kill(pid, 9);
					evt_log_obj->on_eventlog(l, evt_log_obj, "block->kill", p->psinfo->run_name, p->psinfo->arg_0);

					/* enQ( FQPM.ALARM ) */
					char *alarm_msg=NULL;
					int	 alarm_msg_len = 0;
					make_alarm_msg(l, "BLOCK", p->psinfo, &alarm_msg, &alarm_msg_len);
					fq_log(l, FQ_LOG_DEBUG, "alarm_msg=[%s]", alarm_msg);
					enQ(l, alarm_msg, alarm_msg_len, alarm_q_obj);
					if(alarm_msg) free(alarm_msg);

					sleep(1);
				}
			}

			/* Process is dead. */
			/* enQ( FQPM.ALARM ) */
			char *alarm_msg=NULL;
			int	 alarm_msg_len = 0;
			make_alarm_msg(l, "DEAD", p->psinfo, &alarm_msg, &alarm_msg_len);
			enQ(l, alarm_msg, alarm_msg_len, alarm_q_obj);
			if(alarm_msg) free(alarm_msg);

			fq_log(l, FQ_LOG_EMERG,"\t- '%d' is dead, run_name='%s'.", 
				p->psinfo->pid, p->psinfo->run_name );

			evt_log_obj->on_eventlog(l, evt_log_obj, "dead->run", p->psinfo->run_name, p->psinfo->arg_0);

			delete_me_on_fq_framework( l, path, p->psinfo->pid);
		
			/* Start up a process */
			char run_cmd[512];
			sprintf(run_cmd, "%s %s &", p->psinfo->run_name, p->psinfo->arg_0);
			fq_log(l, FQ_LOG_EMERG, "Auto restart -> '%s'.", run_cmd);
			system(run_cmd);

			// make a full string of process status and Send to queue(CIRCULAR_MODE) for WebGate.
		}

		if(pslist) pslist_free(&pslist);

    	sleep(5);
	}

#if 0
    for(s_ticks = 0;(s_ticks < 60) && (g_is_break == 0);s_ticks++) {
        (void)fprintf(stdout, "I'm working. alive (ticks=%d)\n",  s_ticks);

        if(s_ticks == 10) {
			printf("I meet a segment fault.\n");
            *((unsigned long *)0) = 1234; /* make a segment fault ! */
        }

        sleep(1);
    }
#endif

	if(alarm_q_obj) CloseFileQueue(l, &alarm_q_obj);
	if(evt_log_obj) close_fqpm_eventlog_obj(l, &evt_log_obj);
	if(l) fq_close_file_logger(&l);
	if(c) close_conf_obj(&c);

    return(EXIT_SUCCESS);
}

static int enQ(fq_logger_t *l, unsigned char *data, int data_len, fqueue_obj_t *obj)
{
	int rc;
	long l_seq=0L;
    long run_time=0L;

	while(1) {
		rc =  obj->on_en(l, obj, EN_NORMAL_MODE, data, data_len+1, data_len, &l_seq, &run_time );
		if( rc == EQ_FULL_RETURN_VALUE )  {
			fq_log(l, FQ_LOG_EMERG, "Queue('%s', '%s') is full.\n", obj->path, obj->qname);
			usleep(100000);
			continue;
		}
		else if( rc < 0 ) { 
			fq_log(l, FQ_LOG_ERROR, "Queue('%s', '%s'): on_en() error.\n", obj->path, obj->qname);
			return(rc);
		}
		else {
			if( FQ_IS_DEBUG_LEVEL(l) ) {
				fq_log(l, FQ_LOG_DEBUG, "data=[%s] data_len :[%d], rc=[%d]. seq=[%ld].", 
					data, data_len, rc, l_seq );
			}
			break;
		}
	}
	return(rc);
}
void make_alarm_msg(fq_logger_t *l, char *err_kind, fq_ps_info_t *p, char **alarm_msg, int *alarm_msg_len)
{
	char enQ_msg[256];
	sprintf(enQ_msg, "%s|%s|%s|%s", err_kind, p->exe_name, p->arg_0, p->arg_1);
	*alarm_msg = strdup(enQ_msg);
	*alarm_msg_len = strlen(*alarm_msg);
	return;
}

/* vim: set expandtab: */
/* End of source */
