/*
** fq_proc_manage.c
*/

#include "fq_defs.h"
#include "fq_common.h"
#include "fq_logger.h"
#include "fq_proc_manage.h"

static int get_pid_from_file( fq_logger_t *l, char *filename )
{
	int fdin;
	struct stat statbuf;
    off_t len=0;
    char    *p=NULL;
    int n;
	int rc;

	if(( fdin = open(filename ,O_RDONLY)) < 0 ) { 
		fq_log(l, FQ_LOG_ERROR, "[%s] file open error.", filename);
		return(-1);
	}

	if(( fstat(fdin, &statbuf)) < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "[%s] fstat error.", filename);
		close(fdin);
		return(-2);
	}

	len = statbuf.st_size;

	p = calloc(len+1, sizeof(char));
	if(!p) {
		fq_log(l, FQ_LOG_ERROR, "[%s] memory allocation error.", filename);
		close(fdin);
		return(-3);
	}

	if(( n = read( fdin, p, len)) != len ) {
		fq_log(l, FQ_LOG_ERROR, "[%s] : read() error.", filename);
		close(fdin);
		return(-4);
	}

	rc = atoi(p);
	
	SAFE_FREE(p);

	close(fdin);

	return(rc); // success
}

static int update_pidfile(fq_logger_t *l, int pid, char *filename)
{
	int fd;
	int     o_flag=O_CREAT|O_EXCL|O_RDWR|O_SYNC;
	mode_t  mode=S_IRUSR| S_IWUSR| S_IRGRP| S_IWGRP| S_IROTH| S_IWOTH;
	char szPid[10];

	unlink(filename);

	if( (fd = open(filename, o_flag, mode ))<0) {
		fq_log(l, FQ_LOG_ERROR, "[%s] open() error.", filename);
		return(-1);
	}

	memset(szPid, 0x00, sizeof(szPid));
	sprintf(szPid, "%d", pid);
	if( write(fd, szPid, strlen(szPid)) <= 0 ) {
		fq_log(l, FQ_LOG_ERROR, "[%s] write() error.", filename);
		return(-2);
	}

	return(0);
}

static int create_pidfile(fq_logger_t *l, int pid, char *filename)
{
	int fd;
	int     o_flag=O_CREAT|O_EXCL|O_RDWR|O_SYNC;
	mode_t  mode=S_IRUSR| S_IWUSR| S_IRGRP| S_IWGRP| S_IROTH| S_IWOTH;
	char szPid[10];

	unlink(filename);

	if( (fd = open(filename, o_flag, mode ))<0) {
		fq_log(l, FQ_LOG_ERROR, "[%s] open() error.", filename);
		return(-1);
	}

	memset(szPid, 0x00, sizeof(szPid));
	sprintf(szPid, "%d", pid);
	if( write(fd, szPid, strlen(szPid)) <= 0 ) {
		fq_log(l, FQ_LOG_ERROR, "[%s] write() error.", filename);
		return(-2);
	}

	return(0);
}


/*
** found: return(1);
** not found: return(0);
** error: return(<0);
*/
#define MAXLINE 512
static int  get_process_name( fq_logger_t *l, int pid, char  *pname) 
{
	char    CMD[128];
	char	buf[1024];
	FILE    *fp=NULL;

	sprintf(CMD, "ps -ef| grep '%d' | grep -v grep | grep -v tail | grep -v vi", pid);
	if( (fp=popen(CMD, "r")) == NULL) {
		fq_log(l, FQ_LOG_ERROR, "popen() error.");
		return(-1);
	}

	while(fgets(buf, MAXLINE, fp) != NULL) {
		sscanf(buf, "%*s %*s %*s %*s %*s %*s %*s %s %*s", pname);
		pclose(fp);
		return(1); /* found */ 
	}

	if(fp) pclose(fp);
	return(0); /* not found */
}

int regist_pid(fq_logger_t *l, int pid, char *my_process_name,  char *filename)
{
	char pname[128];

	if( is_file(filename) ) {
		int file_pid;

		file_pid = get_pid_from_file( l, filename );
		if( file_pid > 0) {
			if( is_alive_pid_in_Linux(file_pid) ) {
				int rc;

				rc = get_process_name( l, file_pid, pname );
				fq_log(l, FQ_LOG_DEBUG, "pid[%d] , pname=[%s]", file_pid, VALUE(pname) ); 

				if( HASVALUE(pname) && strstr(pname, my_process_name) ) {
					fq_log(l, FQ_LOG_ERROR, "Already Process[%d] is running.", file_pid);
					return(-1);
				}
				else {
					int rc;

					fq_log(l, FQ_LOG_ERROR, "Other process[%s] is using pid[%d].", VALUE(pname), file_pid);

					rc = update_pidfile( l, pid, filename);
					if( rc < 0 ) {
						fq_log(l, FQ_LOG_ERROR, "Update pid error.[%s]", filename);
						return(-2);
					}
					else {
						fq_log(l, FQ_LOG_DEBUG, "Update pid(%d) success to '%s'", pid, filename);
						return(0); // success
					}
				}
			}
			else {
				int rc;
				rc = update_pidfile( l, pid, filename);
				if( rc < 0 ) {
					fq_log(l, FQ_LOG_ERROR, "Update pid error.[%s]", filename);
					return(-2);
				}
				else {
					fq_log(l, FQ_LOG_DEBUG, "Update pid(%d) success to '%s'", pid, filename);
					return(0); // success
				}
			}
		}
		else {
			int rc;
			rc = create_pidfile( l, pid, filename );
			if( rc < 0 ) {
				fq_log(l, FQ_LOG_ERROR, "Create pid error.[%s]", filename);
				return(-3);
			}
			else {
				fq_log(l, FQ_LOG_DEBUG, "Success to create pidfile[%s] pid=[%d]", filename, pid);
			}
		}
	}
	else {
		int rc;
		rc = create_pidfile( l, pid, filename );
		if( rc < 0 ) {
			fq_log(l, FQ_LOG_ERROR, "Create pid error.[%s]", filename);
			return(-3);
		}
		else {
			fq_log(l, FQ_LOG_DEBUG, "Success to create pidfile[%s] pid=[%d]", filename, pid);
		}
	}

	return(0); // success
}

int pid_file_process_stop ( fq_logger_t *l, char *my_process_name,  char *pid_file )
{
    int rc, file_pid;
    char pname[128];

    file_pid = get_pid_from_file( l, pid_file );
    if( file_pid > 0) {
        if( is_alive_pid_in_Linux(file_pid) ) {
            int rc;

            rc = get_process_name( l, file_pid, pname );
            fq_log(l, FQ_LOG_DEBUG, "pid[%d] , pname=[%s]", file_pid, VALUE(pname) );

            if( HASVALUE(pname) && strstr(pname, my_process_name) ) {

                fq_log(l, FQ_LOG_DEBUG, "I'll kill Process[%d].", file_pid);
                kill( file_pid, SIGKILL );
                return(0);
            }
            else {
                fq_log(l, FQ_LOG_DEBUG, "There is no process[%s].", my_process_name);
                return(-1);
            }
        }
    }

    fq_log(l, FQ_LOG_DEBUG, "There is no process[%s].", my_process_name);

    return(-1);
}

bool_t is_alive_process(fq_logger_t *l, char *my_process_name,  char *filename, pid_t *pid)
{
    char pname[128];

    if( is_file(filename) ) {
        int file_pid;

        file_pid = get_pid_from_file( l, filename );
        if( file_pid > 0) {
            if( is_alive_pid_in_Linux(file_pid) ) {
                int rc;

                rc = get_process_name( l, file_pid, pname );
                fq_log(l, FQ_LOG_DEBUG, "pid[%d] , pname=[%s]", file_pid, VALUE(pname) );

                if( HASVALUE(pname) && strstr(pname, my_process_name) ) {
                    fq_log(l, FQ_LOG_DEBUG, "Process[%d] is running.", file_pid);
					*pid = file_pid;
                    return(TRUE);
                }
                else {
                    return(FALSE);
                }
            }
            else {
                return(FALSE);
            }
        }
        else {
            return(FALSE);
        }
    }
    else {
        fq_log(l, FQ_LOG_ERROR, "pidfile[%s] not found.", filename);
    }
    return(FALSE);
}

