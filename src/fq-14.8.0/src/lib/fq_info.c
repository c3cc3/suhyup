/* vi: set sw=4 ts=4: */
/******************************************************************
 ** fq_info.c
 */
#include "config.h"
#include "fq_file_list.h"
#include "fq_manage.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>

#if defined (_OS_LINUX)
#ifdef HAVE_FEATURES_H
#include <features.h>
#endif
#endif

#include <pthread.h>
#include <setjmp.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "fq_info.h"
#include "fq_info_param.h"
#include "fq_common.h"
#include "fq_cache.h"
#include "fq_logger.h"

#define FQ_INFO_C_VERSION "1.0.1"

char* info_keywords[NUM_FQ_INFO_ITEM] = {
		"QNAME",			/* 0 */
        "QPATH",			/* 1 */
        "MSGLEN",			/* 2 */
		"MMAPPING_NUM",		/* 3 */
		"MULTI_NUM",		/* 4 */
		"DESC",				/* 5 */
		"XA_MODE_ON_OFF",	/* 6 */
		"WAIT_MODE_ON_OFF", /* 7 */
		"MASTER_USE_FLAG", /* 8 */
		"MASTER_HOSTNAME" /* 9 */
};

/******************************************************************
**
*/
fq_info_t* new_fq_info(const char* name)
{
        int i;
		fq_info_t  *t=NULL;

        t = (fq_info_t *)malloc(sizeof(fq_info_t));
        if ( !t ) {
                errf("Unable to allocate fq_info_t\n");
                oserrf("malloc fail in new_fq_info\n");
        }

        t->name = (char *)strdup(name);

        for (i=0; i<NUM_FQ_INFO_ITEM; i++) {
                t->item[i].str_value = NULL;
                t->item[i].int_value = 0;
        }
        t->mtime = (time_t)0;
        t->options = fqlist_new('C', "Options");

        pthread_mutex_init(&t->lock, NULL);

        return(t);
}

void free_fq_info(fq_info_t **ptrt)
{
        int i;

		SAFE_FREE((*ptrt)->name );

        for (i=0; i<NUM_FQ_INFO_ITEM; i++) {
                SAFE_FREE((*ptrt)->item[i].str_value);
                (*ptrt)->item[i].int_value = 0;
        }
        fqlist_free(&(*ptrt)->options);
        pthread_mutex_destroy(&(*ptrt)->lock);

		SAFE_FREE(*ptrt);
}

void clear_fq_info_unlocked(fq_info_t *t)
{
        int i;

        for (i=0; i<NUM_FQ_INFO_ITEM; i++) {
				SAFE_FREE(t->item[i].str_value);
                t->item[i].int_value = 0;
        }
        fqlist_clear(t->options);
}

void clear_fq_info(fq_info_t *t)
{
        pthread_mutex_lock(&t->lock);
        clear_fq_info_unlocked(t);
        pthread_mutex_unlock(&t->lock);
}

char* get_info_keyword(int id)
{
        if ( id >=0 && id < NUM_FQ_INFO_ITEM )
                return (info_keywords[id]);
        return(NULL);
}

int get_infoindex(char* keyword)
{
        int i;
        for (i=0; i < NUM_FQ_INFO_ITEM; i++)
                if ( STRCCMP(info_keywords[i], keyword) == 0 )
                        return(i);
        return(-1);
}

void lock_fq_info(fq_info_t *t)
{
        pthread_mutex_lock(&t->lock);
}

void unlock_fq_info(fq_info_t *t)
{
        pthread_mutex_unlock(&t->lock);
}

void set_fq_infoitem_unlocked(fq_info_t *t, int id, char* value)
{
        if ( id >=0 && id < NUM_FQ_INFO_ITEM ) {
                SAFE_FREE(t->item[id].str_value);
                t->item[id].str_value = (char *)strdup(value);
                t->item[id].int_value = atoi(value);
        }
}

/*-----------------------------------------------------------------------
**  fq_infos : Get a reserved config parameter with string type
**     Argument : int id;    - configuration id
**                char* dst; - destination string pointer
**     Return   : string pointer to the value associated with the keyword.
*/
char* 
fq_infos(fq_info_t *t, int id, char* dst)
{
        *dst = '\0';
        if ( id < 0 || id >= NUM_FQ_INFO_ITEM )
                return(NULL);

        lock_fq_info(t);

        if ( t->item[id].str_value ) {
                strcpy(dst, t->item[id].str_value);
                unlock_fq_info(t);
                return(dst);
        }
        else {
                dst = '\0';
                unlock_fq_info(t);
                return(NULL);
        }
}

char* get_fq_info(fq_info_t *t, char* keyword, char* dst)
{
        char* ptr = NULL;
        int i;

        *dst = '\0';

        lock_fq_info(t);

        i = get_infoindex(keyword);

        if (i == -1) {
                ptr = fqlist_getval_unlocked(t->options, keyword);
                if ( ptr )
                        strcpy(dst, ptr);
                unlock_fq_info(t);
                return(dst);
        }
        else {
                unlock_fq_info(t);
                return(fq_infos(t, i, dst));
        }
}

/*
** return values
**	success : read integer value
**  failure : -1
*/
int get_fq_info_i(fq_info_t *t, char* keyword)
{
        char* ptr = NULL;
        int i;
		char buf[128];
		int rc=-1;

        lock_fq_info(t);

        i = get_infoindex(keyword);

        if (i == -1) {
                ptr = fqlist_getval_unlocked(t->options, keyword);
                if ( ptr ) {
						rc = atoi(ptr);
				}
                unlock_fq_info(t);
                return(-1);
        }
        else {
                unlock_fq_info(t);
                fq_infos(t, i, buf);
				rc = atoi(buf);
        }
}

/*-----------------------------------------------------------------------
**  fq_infoi : Get a reserved config parameter with integer type
**     Argument : int id;   - configuration id
**     Return   : integer value associated with the keyword.
**                -1 if no keyword found
*/
int fq_infoi(fq_info_t *t, int id)
{
        char value[1024];

        if (fq_infos(t, id, value))
                return (atoi(value));
        return (-1);
}


void fq_info_setdefault_unlocked(fq_info_t *t)
{
        set_fq_infoitem_unlocked(t, MSGLEN, (char *)D_MSGLEN);
}

void fq_info_setdefault(fq_info_t* t)
{
        lock_fq_info(t);
        fq_info_setdefault_unlocked(t);
        unlock_fq_info(t);
}

/*
 * This function checks mandetory items(SVR_HOME_DIR, SVR_LOG_DIR, SVR_PROG_NAME)
 */
int check_fq_info_unlocked(fq_info_t *t)
{
		int id;

		/*
		** MASTER_USE_FLAG ,
		** MASTER_HOSTNAME are optional 
		*/
		if( !HASVALUE(t->item[id=QNAME].str_value) ||
			!HASVALUE(t->item[id=QPATH].str_value) ||
			!HASVALUE(t->item[id=MSGLEN].str_value) ||
			!HASVALUE(t->item[id=MMAPPING_NUM].str_value) ||
			!HASVALUE(t->item[id=MULTI_NUM].str_value) ||
			!HASVALUE(t->item[id=DESC].str_value) ||
			!HASVALUE(t->item[id=XA_MODE_ON_OFF].str_value) ||
			!HASVALUE(t->item[id=WAIT_MODE_ON_OFF].str_value) )  {
				 fprintf(stderr, "%s not defined or invalid in %s\n",
					info_keywords[id], t->name);
				fprintf(stderr, "Stop.\n");
                return(-1);
        }
        return (0);
}

/*-----------------------------------------------------------------------
**  read_fq_info : Read configuration file
**     Argument : char *filename;   - configuration file name
**    Operation : It reads the specified configuration file which contains
**       'keyword = value' type assignments for each line. The settings
**       are stored in a pairwise linked list called 'config'.
*/
int read_fq_info(fq_logger_t *l, fq_info_t *t, const char* filename)
{
        FILE *fp=NULL;
        struct stat stbuf;
        char buf[1024], keyword[128], value[1024];
        int id, line=1;

		FQ_TRACE_ENTER(l);

        if ( !t || !filename ) {
			fq_log(l, FQ_LOG_ERROR, "fq_info_t or filename is not exist.");
			FQ_TRACE_EXIT(l);
            return (-1);
		}

        if ( stat(filename, &stbuf) != 0 ) {
            fprintf(stderr, "Can't access file '%s', %s\n", filename, strerror(errno));
			FQ_TRACE_EXIT(l);
            return(-1);
        }

        lock_fq_info(t);
        if ( stbuf.st_mtime == t->mtime ) {
            unlock_fq_info(t);
			FQ_TRACE_EXIT(l);
            return(1);
        }
        t->mtime = stbuf.st_mtime;
        fp = fopen(filename, "r");
        if ( fp ) {
                clear_fq_info_unlocked(t);
                fq_info_setdefault_unlocked(t);

                while ( !feof(fp) ) {
                        line += skip_comment(fp);

                        if ( get_feol(fp, buf) == -1 )
                                break;
                        if ( !buf[0] || (strlen(buf) < 2)) {
                                line++;
                                continue;
                        }
                        if ( get_assign(buf, keyword, value) != 0 )
                                errf("Invalid configuration setting at line %d at %s\n",
                                        line, filename);
                        else {
                                id = get_infoindex(&keyword[0]);
                                if ( id != -1 )
                                        set_fq_infoitem_unlocked(t, id, value);
                        }
                        line++;
                }
        }
        else {
                errf("Can't open info file %s, %s\n", filename, strerror(errno));
                unlock_fq_info(t);
                return(-1);
        }

		SAFE_FP_CLOSE(fp);
        check_fq_info_unlocked(t);

        unlock_fq_info(t);

        /* printf("Loading %s ok\n", filename); */

		FQ_TRACE_EXIT(l);
        return(0);
}

/*-----------------------------------------------------------------------
**  print_fq_info : Print the current configuration settings to the log file
*/
void print_fq_info(fq_info_t *t, FILE* fp)
{
        int i;
        lock_fq_info(t);
        fprintf(fp, "fq_info '%s':\n", VALUE(t->name));
        fflush(fp);
        for (i=0; i<NUM_FQ_INFO_ITEM; i++)
                fprintf(fp, "\t%s='%s' %d\n", info_keywords[i],
                        VALUE(t->item[i].str_value),
                        t->item[i].int_value);

        fqlist_print(t->options, fp);
        fflush(fp);
        unlock_fq_info(t);
}

int _fq_info_isyes(fq_info_t *_fq_info, int item)
{
        char buf[1024];
        char* ptr;

        ptr = fq_infos(_fq_info, item, buf);
        if ( !ptr )
                return(0);
        if ( STRCCMP(buf, "yes") == 0 )
                return(1);
        return(0);
}
/*
** Create a new info file automatically when it is not exist. 
*/
bool generate_fq_info_file( fq_logger_t *l, char *qpath, char *qname, fq_info_t *t)
{
	int i;
	FILE *wfp = NULL;
	char info_filename[256];

	FQ_TRACE_ENTER(l);
	
	CHECK(t);

	sprintf(info_filename, "%s/%s.Q.info", qpath, qname);

	/* Check Already existing */
	if( is_file(info_filename) == TRUE ) {
		fq_log(l, FQ_LOG_ERROR, "Already [%s] is exist.", info_filename);
		return false;
	}

	if( (wfp = fopen( info_filename, "w") ) == NULL) { 
		fq_log(l, FQ_LOG_ERROR, "fopen() error. reason=[%s].", strerror(errno));
		return false;
	}

	for (i=0; i<NUM_FQ_INFO_ITEM; i++) {
		switch (i) {
			case 0:
				t->item[i].str_value = strdup(qname);
				t->item[i].int_value = -1;
				break;
			case 1:
				t->item[i].str_value = strdup(qpath);
				t->item[i].int_value = -1;
				break;
			case 2:
				t->item[i].int_value = 4096;
				break;
			case 3:
				t->item[i].int_value = 10;
				break;
			case 4:
				t->item[i].int_value = 100;
				break;
			case 5:
				t->item[i].str_value = strdup("temporary");
				t->item[i].int_value = -1;
				break;
			case 6:
				t->item[i].str_value = strdup("ON");
				t->item[i].int_value = -1;
				break;
			case 7:
				t->item[i].str_value = strdup("OFF");
				t->item[i].int_value = -1;
				break;
		}
		if( t->item[i].int_value < 0 ) {
			fprintf(wfp, "%s = \"%s\"\n", info_keywords[i], VALUE(t->item[i].str_value) );
		}
		else {
			fprintf(wfp, "%s = \"%d\"\n", info_keywords[i], t->item[i].int_value);
		}
	}
	fflush(wfp);
	fclose(wfp);


	FQ_TRACE_EXIT(l);

	return true;
}

bool check_qname_in_listfile(fq_logger_t *l, char *qpath, char *qname)
{
	int rc;
	file_list_obj_t *obj=NULL;
	FILELIST *this_entry=NULL;
	char list_path_filename[256];

	FQ_TRACE_ENTER(l);
	
	sprintf(list_path_filename, "%s/%s", qpath, FQ_LIST_FILE_NAME);

	rc = open_file_list(l, &obj, list_path_filename);
	if( rc == FALSE ) {
		fq_log(l, FQ_LOG_ERROR, "open_file_list('%s') error.", list_path_filename);
		FQ_TRACE_EXIT(l);
		return false;
	}

	this_entry = obj->head;
	int i;
	for(i=0; (this_entry->next && this_entry->value); i++) {
		if( strcmp( qname, this_entry->value) == 0 ) {
			close_file_list(l, &obj);

			FQ_TRACE_EXIT(l);
			return false;
		}
		this_entry = this_entry->next;
	}

	close_file_list(l, &obj);

	FQ_TRACE_EXIT(l);
	return true;
}

bool check_qname_in_shm_listfile(fq_logger_t *l, char *qpath, char *qname)
{
	int rc;
	file_list_obj_t *obj=NULL;
	FILELIST *this_entry=NULL;
	char list_path_filename[256];

	FQ_TRACE_ENTER(l);
	
	sprintf(list_path_filename, "%s/%s", qpath, SHMQ_LIST_FILE_NAME);

	rc = open_file_list(l, &obj, list_path_filename);
	if( rc == FALSE ) {
		fq_log(l, FQ_LOG_ERROR, "open_file_list('%s') error.", list_path_filename);
		FQ_TRACE_EXIT(l);
		return false;
	}

	this_entry = obj->head;
	int i;
	for(i=0; (this_entry->next && this_entry->value); i++) {
		if( strcmp( qname, this_entry->value) == 0 ) {
			close_file_list(l, &obj);

			FQ_TRACE_EXIT(l);
			return false;
		}
		this_entry = this_entry->next;
	}

	close_file_list(l, &obj);

	FQ_TRACE_EXIT(l);
	return true;
}

bool append_new_qname_to_ListInfo( fq_logger_t *l, char *qpath, char *qname)
{
	FILE *afp=NULL;
	char path_filename[256];

	FQ_TRACE_ENTER(l);

	sprintf(path_filename, "%s/ListFQ.info", qpath);

	int line;
	line = find_a_line_from_file(l, path_filename, qname);
	if( line > 0 ) {
		fq_log(l, FQ_LOG_INFO, "'%s' is  already exist in '%s'.", qname, path_filename);
		FQ_TRACE_EXIT(l);
		return true;
	}

	afp = fopen(path_filename, "a");
	if( afp == NULL) {
		fq_log(l, FQ_LOG_ERROR, "fopen('%s') error.\n", path_filename);
		FQ_TRACE_EXIT(l);
		return(false);
	}

	fprintf(afp, "%s\n", qname);
	fflush(afp);
	fclose(afp);

	FQ_TRACE_EXIT(l);
	return true;
}

bool append_new_qname_to_shm_ListInfo( fq_logger_t *l, char *qpath, char *qname)
{
	FILE *afp=NULL;
	char path_filename[256];

	FQ_TRACE_ENTER(l);

	sprintf(path_filename, "%s/ListSHMQ.info", qpath);

	int line;
	line = find_a_line_from_file(l, path_filename, qname);
	if( line > 0 ) {
		fq_log(l, FQ_LOG_INFO, "'%s' is  already exist in '%s'.", qname, path_filename);
		FQ_TRACE_EXIT(l);
		return true;
	}

	afp = fopen(path_filename, "a");
	if( afp == NULL) {
		fq_log(l, FQ_LOG_ERROR, "fopen('%s') error.\n", path_filename);
		FQ_TRACE_EXIT(l);
		return(false);
	}
	fprintf(afp, "%s\n", qname);
	fflush(afp);
	fclose(afp);

	FQ_TRACE_EXIT(l);
	return true;
}

bool delete_qname_to_ListInfo( fq_logger_t *l, int kind, char *qpath, char *qname)
{
	char path_filename[256];

	FQ_TRACE_ENTER(l);

	if( kind == 0 ) { // file_queue 
		sprintf(path_filename, "%s/%s", qpath, FQ_LIST_FILE_NAME);
	}
	else if(kind == 1) {
		sprintf(path_filename, "%s/%s", qpath, SHMQ_LIST_FILE_NAME);
	}

	bool tf;

	tf =  check_qname_in_listfile(l, qpath, qname);
	if( tf == false ) {
		fq_log(l, FQ_LOG_DEBUG, "There is no [%s] in [%s]. We will skip.", qname, path_filename);
		return true;

	}

	int delete_line;
	delete_line  = find_a_line_from_file( l, path_filename, qname);

	int rc;
	rc  = delete_a_line_from_file( l, path_filename, delete_line);
	if( rc != 0 ) {
		fq_log(l, FQ_LOG_ERROR, "delete_a_line_from_file('%s', line='%d') failed.", path_filename, delete_line);
		return false;
	}

	fq_log(l, FQ_LOG_INFO, "[%s] is deleted in '%s'(line=%d).\n", qname, path_filename, delete_line);

	FQ_TRACE_EXIT(l);
	return true;
}
