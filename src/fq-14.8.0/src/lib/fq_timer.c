/*
 * fq_timer.c
 */
#define FQ_TIMER_C_VERSION "1.0.0"

#include "config.h"
#include "fq_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef HAVE_TIME_H
#include <time.h>
#endif

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#include "fq_timer.h"

struct timeval tp1,tp2;

void start_timer()
{
	gettimeofday(&tp1,NULL);
}

double end_timer()
{
	gettimeofday(&tp2,NULL);
	return (tp2.tv_sec + (tp2.tv_usec*1.0e-6)) - 
		(tp1.tv_sec + (tp1.tv_usec*1.0e-6));
}

static void Start( StopWatch_t *obj);
static void Work( StopWatch_t *obj, int work_nsec);
static void Stop( StopWatch_t *obj);
static double GetElapsed(StopWatch_t *obj);
static void PrintElapsed(StopWatch_t *obj);

int open_StopWatch( StopWatch_t **obj)
{
	StopWatch_t	*rc=NULL;

	rc = (StopWatch_t *)calloc(1, sizeof(StopWatch_t));
	if( rc ) {
		rc->started = 0;
		rc->stopped = 0;

		rc->Start = Start;
		rc->Work = Work;
		rc->Stop = Stop;
		rc->GetElapsed = GetElapsed;
		rc->PrintElapsed = PrintElapsed;

		*obj = rc;
		return(TRUE);
	}

	SAFE_FREE( (*obj) );
	return(FALSE);
}
int close_StopWatch( StopWatch_t **obj)
{
	SAFE_FREE( (*obj) );
	return(TRUE);
}

static void Start( StopWatch_t *obj)
{
	clock_gettime(CLOCK, &obj->requestStart);
	return;
}
static void Work( StopWatch_t *obj, int work_nanosec)
{
	obj->req.tv_nsec = work_nanosec;
	obj->req.tv_sec = 0;
	clock_nanosleep(CLOCK, 0, &obj->req, NULL);

	return;
}
static void Stop( StopWatch_t *obj)
{
	clock_gettime(CLOCK, &obj->requestEnd);
	return;
}

static double GetElapsed(StopWatch_t *obj)
{
	obj->elapsed = ( obj->requestEnd.tv_sec - obj->requestStart.tv_sec ) / 1e-6
			+ ( obj->requestEnd.tv_nsec - obj->requestStart.tv_nsec ) / 1e3;
	return(obj->elapsed);
}

static void PrintElapsed(StopWatch_t *obj)
{
	double sec;

	obj->elapsed = ( obj->requestEnd.tv_sec - obj->requestStart.tv_sec ) / 1e-6
			+ ( obj->requestEnd.tv_nsec - obj->requestStart.tv_nsec ) / 1e3;


	sec  = microsec2sec(obj->elapsed);

	fprintf(stdout, "elapsed time: [%.3lf] us(micro sec).%02d:%02d \n", obj->elapsed,  (int)((int)sec/60), (int)((int)sec%60));
}
