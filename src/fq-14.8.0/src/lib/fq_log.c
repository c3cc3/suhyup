/*
** fq_log.c
*/
#define FQ_LOG_C_VERSION "1.0.0"

#define FQ_LOG_LOCKING

#include "config.h"

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#include <sys/file.h> /* for flock() */

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "fq_defs.h"
#include "fq_logger.h"
#include "fq_types.h"
#include "fq_common.h"

#define FQ_TIME_CONV_MILLI    "%Q"
#define FQ_TIME_CONV_MICRO    "%q"
#define FQ_TIME_PATTERN_MILLI "000"
#define FQ_TIME_PATTERN_MICRO "000000"
#define FQ_TIME_FORMAT_NONE   "[%a %b %d %H:%M:%S %Y] "
#define FQ_TIME_FORMAT_MILLI  "[%a %b %d %H:%M:%S." FQ_TIME_CONV_MILLI " %Y] "
#define FQ_TIME_FORMAT_MICRO  "[%a %b %d %H:%M:%S." FQ_TIME_CONV_MICRO " %Y] "
#define FQ_TIME_SUBSEC_NONE   (0)
#define FQ_TIME_SUBSEC_MILLI  (1)
#define FQ_TIME_SUBSEC_MICRO  (2)

#define HUGE_BUFFER_SIZE (8*1024)

#define fq_gettid()    0

static const char *fq_level_verbs[] = {
    "[" FQ_LOG_TRACE_VERB "] ",
    "[" FQ_LOG_DEBUG_VERB "] ",
    "[" FQ_LOG_INFO_VERB "] ",
    "[" FQ_LOG_WARN_VERB "] ",
    "[" FQ_LOG_ERROR_VERB "] ",
    "[" FQ_LOG_EMERG_VERB "] ",
    NULL
};

/* Sleep for 100ms */
void fq_sleep(int ms)
{
#ifdef OS2
    DosSleep(ms);
#elif defined(BEOS)
    snooze(ms * 1000);
#elif defined(NETWARE)
    delay(ms);
#elif defined(WIN32)
    Sleep(ms);
#else
    struct timeval tv;
    tv.tv_usec = (ms % 1000) * 1000;
    tv.tv_sec = ms / 1000;
    select(0, NULL, NULL, NULL, &tv);
#endif
}

/* Replace the first occurence of a sub second time format character
 * by a series of zero digits with the right precision.
 * We format our timestamp with strftime, but this can not handle
 * sub second timestamps.
 * So we first patch the milliseconds or microseconds literally into
 * the format string, and then pass it on the strftime.
 * In order to do that efficiently, we prepare a format string, that
 * already has placeholder digits for the sub second time stamp
 * and we save the position and time precision of this placeholder.
 */
void fq_set_time_fmt(fq_logger_t *l, const char *fq_log_fmt)
{
    if (l) {
        char *s;

        if (!fq_log_fmt) {
#ifndef NO_GETTIMEOFDAY
            fq_log_fmt = FQ_TIME_FORMAT_MILLI;
#else
            fq_log_fmt = FQ_TIME_FORMAT_NONE;
#endif
        }
        l->log_fmt_type = FQ_TIME_SUBSEC_NONE;
        l->log_fmt_offset = 0;
        l->log_fmt_size = 0;
        l->log_fmt = fq_log_fmt;

/* Look for the first occurence of FQ_TIME_CONV_MILLI */
        if ((s = strstr(fq_log_fmt, FQ_TIME_CONV_MILLI))) {
            size_t offset = s - fq_log_fmt;
            size_t len = strlen(FQ_TIME_PATTERN_MILLI);

/* If we don't have enough space in our fixed-length char array,
 * we simply stick to the default format, ignoring FQ_TIME_CONV_MILLI.
 * Otherwise we replace the first occurence of FQ_TIME_CONV_MILLI by FQ_TIME_PATTERN_MILLI.
 */
            if (offset + len < FQ_TIME_MAX_SIZE) {
                l->log_fmt_type = FQ_TIME_SUBSEC_MILLI;
                l->log_fmt_offset = offset;
                strncpy(l->log_fmt_subsec, fq_log_fmt, offset);
                strncpy(l->log_fmt_subsec + offset, FQ_TIME_PATTERN_MILLI, len);
                strncpy(l->log_fmt_subsec + offset + len,
                        s + strlen(FQ_TIME_CONV_MILLI),
                        FQ_TIME_MAX_SIZE - offset - len - 1);
/* Now we put a stop mark into the string to make it's length at most FQ_TIME_MAX_SIZE-1
 * plus terminating '\0'.
 */
                l->log_fmt_subsec[FQ_TIME_MAX_SIZE-1] = '\0';
                l->log_fmt_size = strlen(l->log_fmt_subsec);
            }
/* Look for the first occurence of FQ_TIME_CONV_MICRO */
        }
        else if ((s = strstr(fq_log_fmt, FQ_TIME_CONV_MICRO))) {
            size_t offset = s - fq_log_fmt;
            size_t len = strlen(FQ_TIME_PATTERN_MICRO);

/* If we don't have enough space in our fixed-length char array,
 * we simply stick to the default format, ignoring FQ_TIME_CONV_MICRO.
 * Otherwise we replace the first occurence of FQ_TIME_CONV_MICRO by FQ_TIME_PATTERN_MICRO.
 */
            if (offset + len < FQ_TIME_MAX_SIZE) {
                l->log_fmt_type = FQ_TIME_SUBSEC_MICRO;
                l->log_fmt_offset = offset;
                strncpy(l->log_fmt_subsec, fq_log_fmt, offset);
                strncpy(l->log_fmt_subsec + offset, FQ_TIME_PATTERN_MICRO, len);
                strncpy(l->log_fmt_subsec + offset + len,
                        s + strlen(FQ_TIME_CONV_MICRO),
                        FQ_TIME_MAX_SIZE - offset - len - 1);
/* Now we put a stop mark into the string to make it's length at most FQ_TIME_MAX_SIZE-1
 * plus terminating '\0'.
 */
                l->log_fmt_subsec[FQ_TIME_MAX_SIZE-1] = '\0';
                l->log_fmt_size = strlen(l->log_fmt_subsec);
            }
        }
        fq_log(l, FQ_LOG_DEBUG, "Pre-processed log time stamp format is '%s'",
               l->log_fmt_type == FQ_TIME_SUBSEC_NONE ? l->log_fmt : l->log_fmt_subsec);
    }
}

static int set_time_str(char *str, int len, fq_logger_t *l)
{
    time_t t;
    struct tm *tms;
#ifdef _MT_CODE_PTHREAD
    struct tm res;
#endif
    int done;
/* We want to use a fixed maximum size buffer here.
 * If we would dynamically adjust it to the real format
 * string length, we could support longer format strings,
 * but we would have to allocate and free for each log line.
 */
    char log_fmt[FQ_TIME_MAX_SIZE];

    if (!l || !l->log_fmt) {
        return 0;
    }

    log_fmt[0] = '\0';

#ifndef NO_GETTIMEOFDAY
    if ( l->log_fmt_type != FQ_TIME_SUBSEC_NONE ) {
        struct timeval tv;
        int rc = 0;

#ifdef WIN32
        gettimeofday(&tv, NULL);
#else
        rc = gettimeofday(&tv, NULL);
#endif
        if (rc == 0) {
/* We need this subsec buffer, because we convert
 * the integer with sprintf(), but we don't
 * want to write the terminating '\0' into our
 * final log format string.
 */
            char subsec[7];
            t = tv.tv_sec;
            strncpy(log_fmt, l->log_fmt_subsec, l->log_fmt_size + 1);
            if (l->log_fmt_type == FQ_TIME_SUBSEC_MILLI) {
                sprintf(subsec, "%03d", (int)(tv.tv_usec/1000));
                strncpy(log_fmt + l->log_fmt_offset, subsec, 3);
            }
            else if (l->log_fmt_type == FQ_TIME_SUBSEC_MICRO) {
                sprintf(subsec, "%06d", (int)(tv.tv_usec));
                strncpy(log_fmt + l->log_fmt_offset, subsec, 6);
            }
        }
        else {
            t = time(NULL);
        }
    }
    else {
        t = time(NULL);
    }
#else
    t = time(NULL);
#endif
#ifdef _MT_CODE_PTHREAD
    tms = localtime_r(&t, &res);
#else
    tms = localtime(&t);
#endif
    if (log_fmt[0])
        done = (int)strftime(str, len, log_fmt, tms);
    else
        done = (int)strftime(str, len, l->log_fmt, tms);
    return done;

}

static int FQ_METHOD log_to_file(fq_logger_t *l, int level, int used, char *what)
{
    if (l &&
        (l->level <= level || level == FQ_LOG_REQUEST_LEVEL) &&
        l->logger_private && what) {
        fq_file_logger_t *p = l->logger_private;
        if (p->logfile) {
            what[used++] = '\n';
            what[used] = '\0';
#if defined(FQ_LOG_LOCKING)
#if defined(WIN32) && defined(_MSC_VER)
            LockFile((HANDLE)_get_osfhandle(_fileno(p->logfile)),
                     0, 0, 1, 0);
#else
			(void)flock(fileno(p->logfile), LOCK_SH);
#endif
#endif
            fputs(what, p->logfile);
            /* [V] Flush the dam' thing! */
            fflush(p->logfile);
#if defined(FQ_LOG_LOCKING)
#if defined(WIN32) && defined(_MSC_VER)
            UnlockFile((HANDLE)_get_osfhandle(_fileno(p->logfile)),
                       0, 0, 1, 0);
#else
			(void)flock(fileno(p->logfile), LOCK_UN);
#endif
#endif
        }
        return FQ_TRUE;
    }
    return FQ_FALSE;
}

int get_log_level(const char *level)
{
    if (0 == strcasecmp(level, FQ_LOG_TRACE_VERB)) {
        return FQ_LOG_TRACE_LEVEL;
    }

    if (0 == strcasecmp(level, FQ_LOG_DEBUG_VERB)) {
        return FQ_LOG_DEBUG_LEVEL;
    }

    if (0 == strcasecmp(level, FQ_LOG_INFO_VERB)) {
        return FQ_LOG_INFO_LEVEL;
    }

    if (0 == strcasecmp(level, FQ_LOG_WARN_VERB)) {
        return FQ_LOG_WARNING_LEVEL;
    }

    if (0 == strcasecmp(level, FQ_LOG_ERROR_VERB)) {
        return FQ_LOG_ERROR_LEVEL;
    }

    if (0 == strcasecmp(level, FQ_LOG_EMERG_VERB)) {
        return FQ_LOG_EMERG_LEVEL;
    }
    return FQ_LOG_DEF_LEVEL;
}

char *get_log_level_str( int level, char *dst )
{
	switch(level) {
		case 0:
			sprintf(dst, "%s", "trace");
			break;
		case 1:
			sprintf(dst, "%s", "debug");
			break;
		case 2:
			sprintf(dst, "%s", "info");
			break;
		case 3:
			sprintf(dst, "%s", "warn");
			break;
		case 4:
			sprintf(dst, "%s", "error");
			break;
		case 5:
			sprintf(dst, "%s", "emerg");
			break;
		default:
			return(NULL);
	}
	return(dst);
}

int fq_open_file_logger(fq_logger_t **l, const char *file, int level)
{
    if (l && file) {

        fq_logger_t *rc = (fq_logger_t *)calloc(1, sizeof(fq_logger_t));
        fq_file_logger_t *p = (fq_file_logger_t *) malloc(sizeof(fq_file_logger_t));
        if (rc && p) {
            rc->log = log_to_file;
            rc->level = level;
            rc->logger_private = p;
#if defined(AS400) && !defined(AS400_UTF8)
            p->logfile = fopen(file, "a+, o_ccsid=0");
#elif defined(WIN32) && defined(_MSC_VER)
            p->logfile = fopen(file, "a+c");
#else
            p->logfile = fopen(file, "a+");
#endif
            if (p->logfile) {
                *l = rc;
                fq_set_time_fmt(rc, NULL);
				chmod(file, 0666);
                return FQ_TRUE;
            }
        }
        if (rc) {
            free(rc);
			rc = NULL;
        }
        if (p) {
            free(p);
			p = NULL;
        }

        *l = NULL;
    }
    return FQ_FALSE;
}

int fq_attach_file_logger(fq_logger_t **l, int fd, int level)
{
    if (l && fd >= 0) {

        fq_logger_t *rc = (fq_logger_t *)malloc(sizeof(fq_logger_t));
        fq_file_logger_t *p = (fq_file_logger_t *) malloc(sizeof(fq_file_logger_t));
        if (rc && p) {
            rc->log = log_to_file;
            rc->level = level;
            rc->logger_private = p;
#if defined(AS400) && !defined(AS400_UTF8)
            p->logfile = fdopen(fd, "a+, o_ccsid=0");
#elif defined(WIN32) && defined(_MSC_VER)
            p->logfile = fdopen(fd, "a+c");
#else
            p->logfile = fdopen(fd, "a+");
#endif
            if (p->logfile) {
                *l = rc;
                fq_set_time_fmt(rc, NULL);
                return FQ_TRUE;
            }
        }
        if (rc) {
            free(rc);
			rc = NULL;
        }
        if (p) {
            free(p);
			p = NULL;
        }

        *l = NULL;
    }
    return FQ_FALSE;
}

int fq_close_file_logger(fq_logger_t **l)
{
    if (l && *l) {
        fq_file_logger_t *p = (*l)->logger_private;
        if (p) {
            fflush(p->logfile);
            fclose(p->logfile);
			p->logfile = (FILE *)-1;
            free(p);
			p = NULL;
        }
        free(*l);
        *l = NULL;

        return FQ_TRUE;
    }
    return FQ_FALSE;
}

int 
fq_log(fq_logger_t *l,
           const char *file, int line, const char *funcname, int level,
           const char *fmt, ...)
{
    int rc = 0;
    /*
     * Need to reserve space for terminating zero byte
     * and platform specific line endings added during the call
     * to the output routing.
     */
    static int usable_size = HUGE_BUFFER_SIZE - 3;

    if (!l || !file || !fmt) {
        return -1;
    }

    if ((l->level <= level) || (level == FQ_LOG_REQUEST_LEVEL)) {
#ifdef NETWARE
        /* On NetWare, this can get called on a thread that has a limited stack so */
        /* we will allocate and free the temporary buffer in this function         */
        char *buf;
#else
        char buf[HUGE_BUFFER_SIZE];
#endif
        char *f = (char *)(file + strlen(file) - 1);
        va_list args;
        int used = 0;

        while (f != file && '\\' != *f && '/' != *f) {
            f--;
        }
        if (f != file) {
            f++;
        }

#ifdef NETWARE
        buf = (char *)malloc(HUGE_BUFFER_SIZE);
        if (NULL == buf)
            return -1;
#endif
        used = set_time_str(buf, usable_size, l);

        if (line) { /* line==0 only used for request log item */
            /* Log [pid:threadid] for all levels except REQUEST. */
            /* This information helps to correlate lines from different logs. */
            /* Performance is no issue, because with production log levels */
            /* we only call it often, if we have a lot of errors */
#ifdef OS_SOLARIS
            rc = snprintf(buf + used, usable_size - used,
                          "[%" FQ_PID_T_FMT ":%" FQ_PTHREAD_T_FMT "] ", (int)getpid(), (long unsigned int) fq_gettid());
#else
            rc = snprintf(buf + used, usable_size - used,
                          "[%" FQ_PID_T_FMT ":%" FQ_PTHREAD_T_FMT "] ", getpid(), (long unsigned int) fq_gettid());

#endif
            used += rc;
            if (rc < 0 ) {
                return 0;
            }

            rc = (int)strlen(fq_level_verbs[level]);
            if (usable_size - used >= rc) {
                strncpy(buf + used, fq_level_verbs[level], rc);
                used += rc;
            }
            else {
                return 0;           /* [V] not sure what to return... */
            }

            if (funcname) {
                rc = (int)strlen(funcname);
                if (usable_size - used >= rc + 2) {
                    strncpy(buf + used, funcname, rc);
                    used += rc;
                    strncpy(buf + used, "::", 2);
                    used += 2;
                }
                else {
                    return 0;           /* [V] not sure what to return... */
                }
            }

            rc = (int)strlen(f);
            if (usable_size - used >= rc) {
                strncpy(buf + used, f, rc);
                used += rc;
            }
            else {
                return 0;           /* [V] not sure what to return... */
            }

			
#ifdef HAVE_SNPRINTF_H
            rc = snprintf(buf + used, usable_size - used,
                          " (%d): ", line);
#else
			sprintf(buf + used, " (%05d): ", line);
			rc = 10;
#endif

            used += rc;
            if (rc < 0 || usable_size - used < 0) {
                return 0;           /* [V] not sure what to return... */
            }
        }

        va_start(args, fmt);
        rc = vsnprintf(buf + used, usable_size - used, fmt, args);
        va_end(args);
        if ( rc <= usable_size - used ) {
            used += rc;
        } else {
            used = usable_size;
        }

        l->log(l, level, used, buf);

#ifdef NETWARE
        free(buf);
		buf = NULL;
#endif
    }

    return rc;
}

#if 0
#include "fq_logger.h"

int main()
{
	fq_logger_t *l;

	fq_open_file_logger(&l, "aaa", FQ_LOG_DEBUG_LEVEL);

	fq_log(l, FQ_LOG_DEBUG, "HI, logger. My level is %s.", "DEBUG");
	fq_log(l, FQ_LOG_INFO, "HI, logger. My level is %s.", "INFO");
	fq_log(l, FQ_LOG_EMERG, "HI, logger. My level is %s.", "EMERG");

	fq_close_file_logger(&l);

	return(0);
}
#endif

#include <pthread.h>
#include <stdarg.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif
/*
** syslog usage:
**	$ su - root
** 	1. vi /etc/syslog.conf
**	2. add local0.info /var/log/gwlib.log
**	3. touch /var/log/gwlib.log
**	4. chmod 755 /var/log/gwlib.log
** 	5. chown sg /var/log/gwlib.log
**	6. chgrp other /var/log/gwlib.log
**	7. ps -ef|grep syslogd
**	8. kill -HUP pid
*/
#ifdef HAVE_SYSLOG_H
void fq_syslog(char *fmt, ...)
{
   va_list ap;
   char buf[1024];

	openlog("fqlib:", LOG_PID, LOG_LOCAL0);

    va_start(ap, fmt);
    vsprintf(buf, fmt, ap);
    va_end(ap);

	syslog(LOG_INFO|LOG_LOCAL0, "%s", buf);

	closelog();
	return;
}
#endif

pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;

void fq_eventlog(char *fmt, ...) 
{
    char filename[128];
    char datestr[40], timestr[40];
    va_list ap;
    FILE *fp;
    char buf[1024];

    pthread_mutex_lock(&log_lock);

    get_time(datestr, timestr);

    sprintf(filename, "/tmp/fq_event_%s.log", datestr);

    if ( (fp = fopen(filename, "a")) == NULL ) {
        fprintf(stderr, "Unable to open event log file %s, %s--->fatal.\n", filename, strerror(errno));
        pthread_mutex_unlock(&log_lock);
        return;
    }

    va_start(ap, fmt);
    vsprintf(buf, fmt, ap);
    fprintf(fp, "%s:%s %s\n", datestr, timestr, buf);

    fflush(fp);
    fclose(fp);
    va_end(ap);

    pthread_mutex_unlock(&log_lock);
}
/* This function makes a temporary logname at /tmp 
**	- success: 
** 		returns: file descriptor > 0
**	             logname
**		- failed: < 0
*/
int gen_tmp_logname( fq_logger_t *l, char *Prefix, char *logname, size_t bufsize)
{
	int filedes;
	char buf[256];

	FQ_TRACE_ENTER(l);
	memset(logname, 0x00, bufsize);

	sprintf(buf, "/tmp/%s-XXXXXX", Prefix);
	filedes  = mkstemp(buf);
	if( filedes < 0 ) {
		fq_log(l, FQ_LOG_ERROR , "mkstemp(%s) error.", buf);
		FQ_TRACE_EXIT(l);
		return(filedes);
	}
	if( strlen(buf)+4 < bufsize ) {
		sprintf( logname, "%s.log", buf);
	} else {
		fq_log(l, FQ_LOG_ERROR, "buffer is too small.");
		FQ_TRACE_EXIT(l);
		return(-2);
	}

	fq_log(l, FQ_LOG_DEBUG, "logname=[%s]", logname);

	FQ_TRACE_EXIT(l);
	return(filedes);
}
/*
** It rotates logfile. Before use it, close opened logger_t
** and re-open logfile.
*/
void backup_log( char *log_pathfile, char *log_open_date)
{
	char new_log_pathfile[128];

	sprintf(new_log_pathfile, "%s_%s", log_pathfile, log_open_date);
	rename(log_pathfile, new_log_pathfile);

	return;
}
