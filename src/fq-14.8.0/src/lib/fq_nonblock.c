/*
** fq_nonblock.c
*/
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#if (defined(HAVE_IOCTL_FIONBIO) && defined(NETWARE))
#include <sys/filio.h>
#endif
#ifdef __VMS
#include <in.h>
#include <inet.h>
#endif

#include "fq_nonblock.h"
/*
 * curlx_nonblock() set the given socket to either blocking or non-blocking
 * mode based on the 'nonblock' boolean argument. This function is highly
 * portable.
 */
int fq_nonblock(int sockfd,    /* operate on this */
                   int nonblock   /* TRUE or FALSE */)
{
#if defined(USE_BLOCKING_SOCKETS)

  return 0; /* returns success */

#elif defined(HAVE_FCNTL_O_NONBLOCK)

  /* most recent unix versions */
  int flags;
  flags = sfcntl(sockfd, F_GETFL, 0);
  if(nonblock)
    return sfcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
  else
    return sfcntl(sockfd, F_SETFL, flags & (~O_NONBLOCK));

#elif defined(HAVE_IOCTL_FIONBIO)

  /* older unix versions */
  int flags = nonblock ? 1 : 0;
  return ioctl(sockfd, FIONBIO, &flags);

#elif defined(HAVE_IOCTLSOCKET_FIONBIO)

  /* Windows */
  unsigned long flags = nonblock ? 1UL : 0UL;
  return ioctlsocket(sockfd, FIONBIO, &flags);

#elif defined(HAVE_IOCTLSOCKET_CAMEL_FIONBIO)

  /* Amiga */
  long flags = nonblock ? 1L : 0L;
  return IoctlSocket(sockfd, FIONBIO, flags);

#elif defined(HAVE_SETSOCKOPT_SO_NONBLOCK)

  /* BeOS */
  long b = nonblock ? 1L : 0L;
  return setsockopt(sockfd, SOL_SOCKET, SO_NONBLOCK, &b, sizeof(b));

#else
	return(-1);
#endif
}
