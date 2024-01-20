/*
** fq_syslog.c
*/
#include <stdio.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <syslog.h>

#include "fq_syslog.h"
/*
** syslog usage:
**	$ su - root
** 	1. vi /etc/syslog.conf
**	2. add local0.info /var/log/$(gwlib}.log
**	3. touch /var/log/${gwlib}.log
**	4. chmod 755 /var/log/${gwlib}.log
** 	5. chown sg /var/log/${gwlib}.log
**	6. chgrp other /var/log/${gwlib}.log
**	7. ps -ef|grep syslogd
**	8. kill -HUP pid
*/

/*
** default filename: /var/log/messages
*/
void init_strace( char *pname )
{
	if(pname[0] == 0x00 ) openlog("none", LOG_PID, LOG_LOCAL0);
	else openlog(pname, LOG_PID, LOG_LOCAL0);

	return;
}

void strace(int iLevel, char *fmt, ...)
{
	va_list ap;
	char buf[8192];

    va_start(ap, fmt);
    vsprintf(buf, fmt, ap);
	switch(iLevel) {
		case MSG_LVL_INFO:	
			syslog(LOG_INFO|LOG_LOCAL0, "%s", buf);
			break;
		case MSG_LVL_ERROR:	
			syslog(LOG_ERR|LOG_LOCAL0, "%s", buf);
			break;
		case MSG_LVL_WARN:	
			syslog(LOG_WARNING|LOG_LOCAL0, "%s", buf);
			break;
		case MSG_LVL_ETC:	
			syslog(LOG_NOTICE|LOG_LOCAL0, "%s", buf);
			break;
	}
    va_end(ap);

	return;
}
void end_strace()
{
	closelog();
}
