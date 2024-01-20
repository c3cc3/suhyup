/*
** fq_sleep.c
*/
#include <stdio.h>
#include <sys/time.h>

#ifdef HAVE_SYS_SELECT_H
#  include <sys/select.h>
#endif

#ifdef HAVE_SYS_POLL_H
#  include <sys/poll.h>
#elif defined(HAVE_POLL_H)
#  include <poll.h>
#endif

#ifdef MSDOS
#  include <dos.h>
#endif

#include "fq_sleep.h"

void fq_msleep(long ms)
{
#if 0
  if(ms < 10 ) {
	fprintf(stderr, "fq_msleep() parameter 10ms is not available...\n");
  }
#endif

#if defined(MSDOS)
  delay(ms);
#elif defined(WIN32)
  Sleep(ms);
#elif defined(HAVE_POLL_FINE)
  poll((void *)0, 0, (int)ms);
#else
  struct timeval timeout;

  timeout.tv_sec = ms / 1000L;
  ms = ms % 1000L;
  timeout.tv_usec = ms * 1000L;
  select(0, NULL,  NULL, NULL, &timeout);
#endif
}
