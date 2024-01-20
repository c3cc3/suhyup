/******************************************************************
 ** fq_config.c
 */

#define FQ_CONFIG_C_VERSION "1.0.1"

/*
** 1.0.1: delete build setting in load_config()
**
*/

#include "config.h"

#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#ifdef HAVE_CTYPE_H
#include <ctype.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#if defined (_OS_LINUX)
#ifdef HAVE_FEATURES_H
#include <features.h>
#endif
#endif

#ifdef HAVE_PTHREAD_H
#include <pthread.h>
#endif

#ifdef HAVE_SETJMP_H
#include <setjmp.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "fq_param.h"
#include "fq_common.h"
#include "fq_cache.h"
#include "fq_logger.h"

char* cfg_keywords[NUM_CONFIG_ITEM] = {
		/* Server Define Part - These items are mandatories. */
		"SVR_PROG_NAME",			/* 0 */
        "SVR_HOME_DIR",				/* 1 */
        "SVR_LOG_DIR",				/* 2 */
		/* Queue define Part */
		"EN_QUEUE_PATH",			/* 3 */
		"EN_QUEUE_NAME",			/* 4 */
		"DE_QUEUE_PATH",			/* 5 */
		"DE_QUEUE_NAME",			/* 6 */
		"QUEUE_MSG_LEN",			/* 7 */
		"QUEUE_RECORDS",			/* 8 */
		"QUEUE_WAIT_MODE", 			/* 9 */
		/* TCP common Part */
		"TCP_HEADER_TYPE",			/* 10 */
		"TCP_HEADER_LEN",			/* 11 */
		/* Listen Server part */
		"TCP_LISTEN_IP",			/* 12 */
		"TCP_LISTEN_PORT",			/* 13 */
		"TCP_LISTEN_TIMEOUT",		/* 14 */
		"TCP_WAIT_TIMEOUT",			/* 15 */

		/* Connecting Client Part */
		"TCP_CONNECT_IP",			/* 16 */
		"TCP_CONNECT_PORT",			/* 17 */
		"TCP_CONNECT_TIMEOUT",		/* 17 */
		"TCP_CONNECT_RETRY_COUNT",  /* 19 */

		/* TCI Formating Part */
		"FORMAT_FILE_PATH",			/* 20 */
		"FORMAT_FILE_NAME"			/* 21 */
};

char* trace_keywords[NUM_TRACE_FLAG] = {
        "TRACE_ERROR",
        "TRACE_DEBUG",
        "TRACE_QUEUE",
        "TRACE_SOCKET",
        "TRACE_INFO",
        "TRACE_TRACE",
};

/******************************************************************
** set_mask - set trace mask
*/
long set_mask(long mask, int bit, char* yesno)
{

        if ( !yesno )
                return(mask);
        if ( STRCCMP(yesno, "yes") == 0 )
                return(mask | (1L << bit));
        else if ( STRCCMP(yesno, "no") == 0 )
                return(mask & ~(1L << bit));
        else
                return(mask);
}

/******************************************************************
**
*/
int get_mask(long mask, int bit)
{
        return ( (mask & (1L << bit)) > 0L ? 1 : 0 );
}

/******************************************************************
**
*/
config_t* new_config(const char* name)
{
        int i;
		config_t  *t=NULL;

        t = (config_t *)malloc(sizeof(config_t));
        if ( !t ) {
                errf("Unable to allocate config_t\n");
                oserrf("malloc fail in new_config\n");
        }

        t->name = (char *)strdup(name);

        for (i=0; i<NUM_CONFIG_ITEM; i++) {
                t->item[i].str_value = NULL;
                t->item[i].int_value = 0;
        }
        t->mtime = (time_t)0;
        t->options = fqlist_new('C', "Options");
        t->traceflag = 0L;

        pthread_mutex_init(&t->lock, NULL);

        return(t);
}

void free_config(config_t **ptrt)
{
        int i;

		SAFE_FREE((*ptrt)->name );

        for (i=0; i<NUM_CONFIG_ITEM; i++) {
                SAFE_FREE((*ptrt)->item[i].str_value);
                (*ptrt)->item[i].int_value = 0;
        }
        fqlist_free(&(*ptrt)->options);
        pthread_mutex_destroy(&(*ptrt)->lock);

		SAFE_FREE(*ptrt);
}

void clear_config_unlocked(config_t *t)
{
        int i;

        for (i=0; i<NUM_CONFIG_ITEM; i++) {
				SAFE_FREE(t->item[i].str_value);
                t->item[i].int_value = 0;
        }
        fqlist_clear(t->options);
        t->traceflag = 0L;
}

void clear_config(config_t *t)
{
        pthread_mutex_lock(&t->lock);
        clear_config_unlocked(t);
        pthread_mutex_unlock(&t->lock);
}

char* get_cfgkeyword(int id)
{
        if ( id >=0 && id < NUM_CONFIG_ITEM )
                return (cfg_keywords[id]);
        return(NULL);
}

int get_cfgindex(char* keyword)
{
        int i;
        for (i=0; i < NUM_CONFIG_ITEM; i++)
                if ( STRCCMP(cfg_keywords[i], keyword) == 0 )
                        return(i);
        return(-1);
}

char* get_tracekeyword(int id)
{
        if ( id >=0 && id < NUM_TRACE_FLAG )
                return (trace_keywords[id]);
        return(NULL);
}

int get_traceindex(char* keyword)
{
        int i;
        for (i=0; i < NUM_TRACE_FLAG; i++)
                if ( STRCCMP(trace_keywords[i], keyword) == 0 )
                        return(i);
        return(-1);
}

void lock_config(config_t *t)
{
        pthread_mutex_lock(&t->lock);
}

void unlock_config(config_t *t)
{
        pthread_mutex_unlock(&t->lock);
}

void set_configitem_unlocked(config_t *t, int id, char* value)
{
        if ( id >=0 && id < NUM_CONFIG_ITEM ) {
                SAFE_FREE(t->item[id].str_value);
                t->item[id].str_value = (char *)strdup(value);
                t->item[id].int_value = atoi(value);
        }
}

/*-----------------------------------------------------------------------
**  configs : Get a reserved config parameter with string type
**     Argument : int id;   -  configuration id 
**                char* dst; - destination string pointer 
**     Return   : string pointer to the value associated with the keyword.
*/
char* 
configs(config_t *t, int id, char* dst)
{

        *dst = '\0';
        if ( id < 0 || id >= NUM_CONFIG_ITEM )
                return(NULL);

        lock_config(t);

        if ( t->item[id].str_value ) {
                strcpy(dst, t->item[id].str_value);
                unlock_config(t);
                return(dst);
        }
        else {
                dst = '\0';
                unlock_config(t);
                return(NULL);
        }
}

char* get_config(config_t *t, char* keyword, char* dst)
{
        char* ptr = NULL;
        int i;

        *dst = '\0';
        lock_config(t);
        i = get_cfgindex(keyword);

        if (i == -1) {
                ptr = fqlist_getval_unlocked(t->options, keyword);
                if ( ptr ) {
                    strcpy(dst, ptr);
					unlock_config(t);
					return(dst);
				}
				else {
					unlock_config(t);
					return NULL;
				}
        }
        else {
                unlock_config(t);
                return(configs(t, i, dst));
        }
}

/*-----------------------------------------------------------------------
**  configi : Get a reserved config parameter with integer type
**     Argument : int id;   - configuration id
**     Return   : integer value associated with the keyword.
**                -1 if no keyword found
*/
int configi(config_t *t, int id)
{
        char value[1024];

        if (configs(t, id, value))
                return (atoi(value));
        return (-1);
}


void config_setdefault_unlocked(config_t *t)
{
#if 0
        set_configitem_unlocked(t, TCP_LISTEN_TIMEOUT, (char *)D_TCP_LISTEN_TIMEOUT);
        set_configitem_unlocked(t, TCP_WAIT_TIMEOUT, (char *)D_TCP_WAIT_TIMEOUT);
        set_configitem_unlocked(t, TCP_CONNECT_TIMEOUT, (char *)D_TCP_CONNECT_TIMEOUT);
#endif
}

void config_setdefault(config_t* t)
{
        lock_config(t);
        config_setdefault_unlocked(t);
        unlock_config(t);
}

/*
 * This function checks mandetory items(SVR_HOME_DIR, SVR_LOG_DIR, SVR_PROG_NAME)
 */
int check_config_unlocked(config_t *t)
{
#if 0
		int id;
		if( !HASVALUE(t->item[id=SVR_PROG_NAME].str_value) ||
			!HASVALUE(t->item[id=SVR_HOME_DIR].str_value) ||
			!HASVALUE(t->item[id=SVR_LOG_DIR].str_value) ) {
				 fprintf(stderr, "%s not defined or invalid in %s\n",
					cfg_keywords[id], t->name);
				fprintf(stderr, "Stop.\n");
                exit(-1);
        }
#endif
        return (0);
}

/*-----------------------------------------------------------------------
**  read_config : Read configuration file
**     Argument : char *filename;   - configuration file name 
**    Operation : It reads the specified configuration file which contains
**       'keyword = value' type assignments for each line. The settings
**       are stored in a pairwise linked list called 'config'.
*/
int read_config(config_t *t, const char* filename)
{
        FILE *fp=NULL;
        struct stat stbuf;
        char buf[1024], keyword[128], value[1024];
#if 0
        char *home;
#endif
        int id, line=1;

        if ( !t || !filename )
                return (-1);

        if ( stat(filename, &stbuf) != 0 ) {
                fprintf(stderr, "Can't access file '%s', %s\n", filename, strerror(errno));
                return(-1);
        }

        lock_config(t);
        if ( stbuf.st_mtime == t->mtime ) {
                unlock_config(t);
                return(1);
        }
        t->mtime = stbuf.st_mtime;
        fp = fopen(filename, "r");
        if ( fp ) {
                clear_config_unlocked(t);
                config_setdefault_unlocked(t);

                while ( !feof(fp) ) {
                        line += skip_comment(fp);

                        if ( get_feol(fp, buf) == -1 )
                                break;
                        if ( !buf[0] || (strlen(buf) < 2) ) {
                                line++;
                                continue;
                        }
                        if ( get_assign(buf, keyword, value) != 0 )
                                errf("Invalid configuration setting at line %d at %s\n",
                                        line, filename);
                        else {
                                id = get_cfgindex(&keyword[0]);
                                if ( id != -1 )
                                        set_configitem_unlocked(t, id, value);
                                else {
                                        id = get_traceindex(&keyword[0]);
                                        if ( id != -1 )
                                                t->traceflag = set_mask(t->traceflag, id, value);
                                        else
                                                fqlist_add(t->options, keyword, value, 0);
                                }
                        }
                        line++;
                }
        }
        else {
                errf("Can't open config %s, %s\n", filename, strerror(errno));
                unlock_config(t);
                return(-1);
        }

		SAFE_FP_CLOSE(fp);
        check_config_unlocked(t);

#if 0
        /*
        ** build path
        */
        home = t->item[SVR_HOME_DIR].str_value;

        sprintf(buf, "%s/%s", home, t->item[SVR_LOG_DIR].str_value);
        set_configitem_unlocked(t, SVR_LOG_DIR, buf);

        sprintf(buf, "%s/%s", home, t->item[FORMAT_FILE_PATH].str_value);
        set_configitem_unlocked(t, FORMAT_FILE_PATH, buf);
#endif
        unlock_config(t);

        // printf("Loading %s ok\n", filename);

        return(0);
}

int read_multi_config(config_t *t, const char* filename, const char *svrname)
{
        FILE *fp;
        struct stat stbuf;
        char buf[1024], keyword[128], value[1024];
        char *home;
        int id, line=1;
		int	svr_start_flag = 0;
		int other_start_flag = 0;

        if ( !t || !filename )
                return (-1);

        if ( stat(filename, &stbuf) != 0 ) {
                fprintf(stderr, "Can't access file '%s', %s\n", filename, strerror(errno));
                return(-1);
        }

        lock_config(t);
        if ( stbuf.st_mtime == t->mtime ) {
                unlock_config(t);
                return(1);
        }
        t->mtime = stbuf.st_mtime;
        fp = fopen(filename, "r");
        if ( fp ) {
                clear_config_unlocked(t);
                config_setdefault_unlocked(t);

                while ( !feof(fp) ) {
                        line += skip_comment(fp);

                        if ( get_feol(fp, buf) == -1 )
                                break;

                        if ( !buf[0] || (strlen(buf)<2) ) {
                                line++;
                                continue;
                        }
						/* common part */
						if( strncmp(buf, "[COMMON]", 8) == 0 ) {
							/* fprintf(stdout, "COMMON found. line skip\n"); */
							line++;
							continue;
						}

						if ( buf[0] == '[' ) {
							/* fprintf(stdout, "[ founded.\n"); */
							if( (strncmp(&buf[1], svrname, strlen(svrname)) ==0)  && (buf[strlen(svrname)+1] == ']') ) {
								/* fprintf(stdout, "Server part start.\n"); */
								other_start_flag = 0;
								svr_start_flag = 1;
								line++;
								continue;
							}
							else {
								/* fprintf(stdout, "other part start.\n"); */
								if( svr_start_flag == 1) {
									/* fprintf(stdout, "server part read end. -->break.\n"); */
									line++;
									break;
								}
								else {
									/* fprintf(stdout, "other server part read continue. skip\n"); */
									line++;
									other_start_flag = 1;
									continue;
								}
							}
						}
						if(other_start_flag == 1 ) {
							/* fprintf(stdout, "other part skip.\n"); */
							continue;
						}

                        if ( get_assign(buf, keyword, value) != 0 ) {
                                errf("Invalid configuration setting at line %d at %s\n", line, filename);
						}
                        else {
                                id = get_cfgindex(&keyword[0]);
                                if ( id != -1 )
                                        set_configitem_unlocked(t, id, value);
                                else {
                                        id = get_traceindex(&keyword[0]);
                                        if ( id != -1 )
                                                t->traceflag = set_mask(t->traceflag, id, value);
                                        else
                                                fqlist_add(t->options, keyword, value, 0);
                                }
                        }
                        line++;
                }
        }
        else {
                errf("Can't open config %s, %s\n", filename, strerror(errno));
                unlock_config(t);
                return(-1);
        }

		SAFE_FP_CLOSE(fp);
        check_config_unlocked(t);


        /*
        ** build path
        */
        home = t->item[SVR_HOME_DIR].str_value;

        sprintf(buf, "%s/%s", home, t->item[SVR_LOG_DIR].str_value);
        set_configitem_unlocked(t, SVR_LOG_DIR, buf);

        unlock_config(t);

        // printf("Loading %s ok\n", filename);

        return(0);
}

/*-----------------------------------------------------------------------
**  print_config : Print the current configuration settings to the log file
*/
void print_config(config_t *t, FILE* fp)
{
        int i;
        lock_config(t);
        fprintf(fp, "Config '%s':\n", VALUE(t->name));
        fflush(fp);
        for (i=0; i<NUM_CONFIG_ITEM; i++)
                fprintf(fp, "\t%s='%s' %d\n", cfg_keywords[i],
                        VALUE(t->item[i].str_value),
                        t->item[i].int_value);

        fprintf(fp, "Trace flags:\n");
        for (i=0; i<NUM_TRACE_FLAG;i++)
                if ( get_mask(t->traceflag, i) == 1 )
                        fprintf(fp, "\t%s=`%s'\n", trace_keywords[i], "Yes");
                else
                        fprintf(fp, "\t%s=`%s'\n", trace_keywords[i], "No");
        fqlist_print(t->options, fp);
        fflush(fp);
        unlock_config(t);
}

int _config_isyes(config_t *_config, int item)
{
        char buf[1024];
        char* ptr;

        ptr = configs(_config, item, buf);
        if ( !ptr )
                return(0);
        if ( STRCCMP(buf, "yes") == 0 )
                return(1);
        return(0);
}
int test_log(fq_logger_t *l)
{
	fq_log(l, FQ_LOG_ERROR, "test");
	return(0);
}
