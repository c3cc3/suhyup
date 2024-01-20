#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <stdio.h>
#include <setjmp.h>
#include "fqueue.h"
#include "fq_common.h"
#include "fq_logger.h"
#include "fq_manage.h"
#include "fq_file_list.h"
 
typedef void (*deCB)();

timer_t timerID_1;
int count_1=0;
jmp_buf jmpbuf;
int errno;

bool unlink_lockfile( fq_logger_t *l, char *path, char *qname);

// 타이머 주기에 따라 호출될 타이머
void timer_todo_1()
{
	count_1++;
    printf("1-[%d]-th timer\n", count_1);
	if( count_1 > 0) {
		timer_delete(timerID_1);
		printf("timer was deleted.\n"); 
		longjmp(jmpbuf,2);
	}
}
 
int createTimer( timer_t *timerID, int sec, int msec, deCB userFP )  
{  
    struct sigevent         te;  
    struct itimerspec       its;  
    struct sigaction        sa;  
    int                     sigNo = SIGRTMIN;  
   
    /* Set up signal handler. */  
    sa.sa_flags = SA_SIGINFO;  
    sa.sa_sigaction = userFP;     // 타이머 호출시 호출할 함수 
    sigemptyset(&sa.sa_mask);  
  
    if (sigaction(sigNo, &sa, NULL) == -1)  
    {  
        printf("sigaction error\n");
        return -1;  
    }  
   
    /* Set and enable alarm */  
    te.sigev_notify = SIGEV_SIGNAL;  
    te.sigev_signo = sigNo;  
    te.sigev_value.sival_ptr = timerID;  
    timer_create(CLOCK_REALTIME, &te, timerID);  
   
    its.it_interval.tv_sec = sec;
    its.it_interval.tv_nsec = msec * 1000000;  
    its.it_value.tv_sec = sec;
    
    its.it_value.tv_nsec = msec * 1000000;
    timer_settime(*timerID, 0, &its, NULL);  
   
    return 0;  
}
 
int main(int ac, char **av)
{
	if( ac !=2 ) {
		printf("Usage: $ %s [timout_sec] <enter>\n", av[0]);
		exit(0);
	}
    
	fq_logger_t *l=NULL;
	int timeout_sec = atoi(av[1]);

	int rc;
	rc = fq_open_file_logger(&l, "/tmp/diag_fqueue.log", FQ_LOG_DEBUG_LEVEL);
    CHECK(rc == TRUE);

	char *fq_home_path = getenv("FQ_DATA_HOME");
	char listinfo_file[255];
	sprintf(listinfo_file, "%s/%s", fq_home_path, FQ_LIST_INFO_FILE);
	fq_log(l, FQ_LOG_DEBUG, "FQ_LIST_INFO_FILE='%s'" , listinfo_file);

	file_list_obj_t *qname_list = NULL;
	rc = open_file_list(l, &qname_list, listinfo_file);
	CHECK( rc == TRUE );

	printf("count = [%d]\n", qname_list->count);

	FILELIST *this_entry =NULL;
	this_entry = qname_list->head;

	while( this_entry->next && this_entry->value ) {
		// 타이머를 만든다
		// 매개변수 1 : 타이머 변수
		// 매개변수 2 : second
		// 매개변수 3 : ms
		char *qname = this_entry->value;
		createTimer(&timerID_1, timeout_sec, 0, timer_todo_1);

		int ret=0;
		ret = setjmp(jmpbuf);
		while(1)
		{
			if(ret == 2) break; /* timer makes ret to 2. */
			/* job block */
			// break;
			int rc;
			rc = DiagFileQueue(0, fq_home_path, qname);
			printf("rc = [%d]\n", rc);
			break;
		}
		if(ret == 0 ) {
			timer_delete(timerID_1);
			fq_log(l, FQ_LOG_INFO, "DiagFileQueue('%s', '%s') OK.", fq_home_path, qname);
		}
		else {
			unlink_lockfile(l, fq_home_path, qname);
			fq_log(l, FQ_LOG_ERROR, "DiagFileQueue('%s', '%s') failed.\n", fq_home_path, qname);
			fq_log(l, FQ_LOG_ERROR, "exit in while loop. ret=[%d] Good bye.\n", ret);
			/* unlink en_flock, de_flock */
			/* and restart related processes */
			
			/* We can't continue */
		}
	
		this_entry = this_entry->next;
	}
	printf("good bye..\n");

	close_file_list(l, &qname_list);

	fq_close_file_logger(&l);
	return 0;
    
}

bool unlink_lockfile( fq_logger_t *l, char *path, char *qname)
{
	char en_lock_pathfile[255];
	char de_lock_pathfile[255];
	int rc;

	sprintf(en_lock_pathfile, "%s/.%s.en_flock", path, qname);
	fq_log(l, FQ_LOG_EMERG, "en_lock_pathfile = %s.", en_lock_pathfile);

	rc = unlink(en_lock_pathfile);
	if( rc != 0 ) {
		fq_log(l, FQ_LOG_ERROR, "unlink('%s) failed. reason='%s'", 
				en_lock_pathfile, strerror(errno));
	}

	sprintf(de_lock_pathfile, "%s/.%s.de_flock", path, qname);
	fq_log(l, FQ_LOG_EMERG, "de_lock_pathfile = %s.", de_lock_pathfile);

	rc = unlink(de_lock_pathfile);
	if( rc != 0 ) {
		fq_log(l, FQ_LOG_ERROR, "unlink('%s) failed. reason='%s'", 
				en_lock_pathfile, strerror(errno));
	
	}
	return true;
}
