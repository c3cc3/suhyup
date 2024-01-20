/*
    Copyright (C) HWPORT.COM
    All rights reserved.
    Author: JAEHYUK CHO <mailto:minzkn@minzkn.com>
*/

#if 0
#if !defined(_ISOC99_SOURCE)
# define _ISOC99_SOURCE (1L)
#endif

#if !defined(_GNU_SOURCE)
# define _GNU_SOURCE (1L)
#endif
#endif

#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include "fq_logger.h"

int fq_launcher(fq_logger_t *l)
{
    pid_t s_pid;

    for(;;) {
        s_pid = fork();
        if(s_pid == ((pid_t)(-1))) {
            return(-1);
        }
        else if(s_pid == ((pid_t)0)) {
            /* ok. immortal process start */
            
            /* need to default signal handler ! */
#if defined(SIGBUS)            
            (void)signal(SIGBUS, SIG_DFL);
#endif
#if defined(SIGSTKFLT)
            (void)signal(SIGSTKFLT, SIG_DFL);
#endif            
            (void)signal(SIGILL, SIG_DFL);
            (void)signal(SIGFPE, SIG_DFL);
            (void)signal(SIGSEGV, SIG_DFL);
			
            break;
        }
        else {
            pid_t s_waitpid_check;
            int s_status = 0;
            int s_options = 0;
            int s_signum = (-1);
          
#if 1L /* DEBUG - SIGINT */ 
            (void)signal(SIGINT, SIG_IGN);
#endif
 
			if(l) 
				fq_log(l, FQ_LOG_EMERG, "Start monitoring by fq_launcher ! (pid=%ld).", (long)s_pid);
			else
				(void)fprintf(stdout, "Start monitoring by fq_launcher ! (pid=%ld)\n", (long)s_pid);
            
#if defined(WUNTRACED)
            s_options |= WUNTRACED;
#endif
#if defined(WCONTINUED)
            s_options |= WCONTINUED;
#endif

            do {
                s_waitpid_check = waitpid(s_pid, (int *)(&s_status), s_options);
                if(s_waitpid_check == ((pid_t)(-1))) {
                    /* what happen ? */
             
					if(l) 
						fq_log(l, FQ_LOG_ERROR, "Waitpid failed by fq_launcher ! (pid=%ld).", (long)s_pid);
					else
						(void)fprintf(stdout, "Waitpid failed by fq_launcher ! (pid=%ld).\n", (long)s_pid);

                    exit(EXIT_SUCCESS);
                } 

                if(WIFEXITED(s_status) != 0) {
                    /* normal exit */
                    
					if(l) 
						fq_log(l, FQ_LOG_EMERG, "Stop monitoring by fq_launcher ! (pid=%long)\n", (long)s_pid);
					else
						(void)fprintf(stdout, "Stop monitoring by fq_launcher ! (pid=%long)\n", (long)s_pid);

                    exit(EXIT_SUCCESS);
                }
                else if(WIFSIGNALED(s_status) != 0) {
                    s_signum = WTERMSIG(s_status);
                    if(
#if defined(SIGBUS)
                        (s_signum != SIGBUS) &&
#endif
#if defined(SIGSTKFLT)
                        (s_signum != SIGSTKFLT) &&
#endif
                        (s_signum != SIGILL) &&
                        (s_signum != SIGFPE) &&
                        (s_signum != SIGSEGV) &&
                        (s_signum != SIGPIPE)) {
                        /* normal exit */

						if(l) 
							fq_log(l, FQ_LOG_ERROR,  "Stop monitoring by fq_launcher ! (pid=%long, signum=%d)\n", (long)s_pid, s_signum);
						else
							(void)fprintf(stdout, "Stop monitoring by fq_launcher ! (pid=%long, signum=%d)\n", (long)s_pid, s_signum);

                        exit(EXIT_SUCCESS);
                    }
                }
            }while((WIFEXITED(s_status) == 0) && (WIFSIGNALED(s_status) == 0));

			if(l) 
				fq_log(l, FQ_LOG_ERROR,  "Restarting by fq_launcher ! (pid=%ld, signum=%d)\n", (long)s_pid, s_signum);
			else
            (void)fprintf(stdout, "Restarting by fq_launcher ! (pid=%ld, signum=%d)\n", (long)s_pid, s_signum);

            sleep(3);

            /* retry launch loop */
        }
    }

    return(0);
}
