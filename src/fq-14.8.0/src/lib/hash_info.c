/* vi: set sw=4 ts=4: */
/******************************************************************
 ** hash_info.c
 */
#include "config.h"

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

#include "hash_info.h"
#include "hash_info_param.h"
#include "fq_common.h"
#include "fq_cache.h"
#include "fq_logger.h"

#define HASH_INFO_C_VERSION "1.0.0"

char* hash_info_keywords[NUM_HASH_INFO_ITEM] = {
		"HASHNAME",			/* 0 */
        "PATH",			/* 1 */
        "DESC",			/* 2 */
		"TABLE_LENGTH",		/* 3 */
		"MAX_KEY_LENGTH",	/* 4 */
		"MAX_DATA_LENGTH",	/* 5 */
};

/******************************************************************
**
*/
hash_info_t* new_hash_info(const char* name)
{
        int i;
		hash_info_t  *t=NULL;

        t = (hash_info_t *)malloc(sizeof(hash_info_t));
        if ( !t ) {
                errf("Unable to allocate hash_info_t\n");
                oserrf("malloc fail in new_hash_info\n");
        }

        t->name = (char *)strdup(name);

        for (i=0; i<NUM_HASH_INFO_ITEM; i++) {
                t->item[i].str_value = NULL;
                t->item[i].int_value = 0;
        }
        t->mtime = (time_t)0;
        t->options = fqlist_new('C', "Options");

        pthread_mutex_init(&t->lock, NULL);

        return(t);
}

void free_hash_info(hash_info_t **ptrt)
{
        int i;

		SAFE_FREE((*ptrt)->name );

        for (i=0; i<NUM_HASH_INFO_ITEM; i++) {
                SAFE_FREE((*ptrt)->item[i].str_value);
                (*ptrt)->item[i].int_value = 0;
        }
        fqlist_free(&(*ptrt)->options);
        pthread_mutex_destroy(&(*ptrt)->lock);

		SAFE_FREE(*ptrt);
}

void clear_hash_info_unlocked(hash_info_t *t)
{
        int i;

        for (i=0; i<NUM_HASH_INFO_ITEM; i++) {
				SAFE_FREE(t->item[i].str_value);
                t->item[i].int_value = 0;
        }
        fqlist_clear(t->options);
}

void clear_hash_info(hash_info_t *t)
{
        pthread_mutex_lock(&t->lock);
        clear_hash_info_unlocked(t);
        pthread_mutex_unlock(&t->lock);
}

char* get_hash_info_keyword(int id)
{
        if ( id >=0 && id < NUM_HASH_INFO_ITEM )
                return (hash_info_keywords[id]);
        return(NULL);
}

int get_hash_infoindex(char* keyword)
{
        int i;
        for (i=0; i < NUM_HASH_INFO_ITEM; i++)
                if ( STRCCMP(hash_info_keywords[i], keyword) == 0 )
                        return(i);
        return(-1);
}

void lock_hash_info(hash_info_t *t)
{
        pthread_mutex_lock(&t->lock);
}

void unlock_hash_info(hash_info_t *t)
{
        pthread_mutex_unlock(&t->lock);
}

void set_hash_infoitem_unlocked(hash_info_t *t, int id, char* value)
{
        if ( id >=0 && id < NUM_HASH_INFO_ITEM ) {
                SAFE_FREE(t->item[id].str_value);
                t->item[id].str_value = (char *)strdup(value);
                t->item[id].int_value = atoi(value);
        }
}

/*-----------------------------------------------------------------------
**  hash_infos : Get a reserved config parameter with string type
**     Argument : int id;    - configuration id
**                char* dst; - destination string pointer
**     Return   : string pointer to the value associated with the keyword.
*/
char* 
hash_infos(hash_info_t *t, int id, char* dst)
{
        *dst = '\0';
        if ( id < 0 || id >= NUM_HASH_INFO_ITEM )
                return(NULL);

        lock_hash_info(t);

        if ( t->item[id].str_value ) {
                strcpy(dst, t->item[id].str_value);
                unlock_hash_info(t);
                return(dst);
        }
        else {
                dst = '\0';
                unlock_hash_info(t);
                return(NULL);
        }
}

char* get_hash_info(hash_info_t *t, char* keyword, char* dst)
{
        char* ptr = NULL;
        int i;

        *dst = '\0';
        lock_hash_info(t);
        i = get_hash_infoindex(keyword);

        if (i == -1) {
                ptr = fqlist_getval_unlocked(t->options, keyword);
                if ( ptr )
                        strcpy(dst, ptr);
                unlock_hash_info(t);
                return(dst);
        }
        else {
                unlock_hash_info(t);
                return(hash_infos(t, i, dst));
        }
}

/*-----------------------------------------------------------------------
**  hash_infoi : Get a reserved config parameter with integer type
**     Argument : int id;   - configuration id
**     Return   : integer value associated with the keyword.
**                -1 if no keyword found
*/
int hash_infoi(hash_info_t *t, int id)
{
        char value[1024];

        if (hash_infos(t, id, value))
                return (atoi(value));
        return (-1);
}

void hash_info_setdefault_unlocked(hash_info_t *t)
{
        set_hash_infoitem_unlocked(t, MAX_KEY_LENGTH, (char *)D_MAX_KEY_LENGTH);
}

void hash_info_setdefault(hash_info_t* t)
{
        lock_hash_info(t);
        hash_info_setdefault_unlocked(t);
        unlock_hash_info(t);
}

/*
 * This function checks mandetory items(SVR_HOME_DIR, SVR_LOG_DIR, SVR_PROG_NAME)
 */
int check_hash_info_unlocked(hash_info_t *t)
{
		int id;
		if( !HASVALUE(t->item[id=HASHNAME].str_value) ||
			!HASVALUE(t->item[id=PATH].str_value) ||
			!HASVALUE(t->item[id=DESC].str_value) ||
			!HASVALUE(t->item[id=TABLE_LENGTH].str_value) ||
			!HASVALUE(t->item[id=MAX_KEY_LENGTH].str_value) ||
			!HASVALUE(t->item[id=MAX_DATA_LENGTH].str_value))  {
				 fprintf(stderr, "%s not defined or invalid in %s\n",
					hash_info_keywords[id], t->name);
				fprintf(stderr, "Stop.\n");
                exit(-1);
        }
        return (0);
}

/*-----------------------------------------------------------------------
**  read_hash_info : Read configuration file
**     Argument : char *filename;   - configuration file name
**    Operation : It reads the specified configuration file which contains
**       'keyword = value' type assignments for each line. The settings
**       are stored in a pairwise linked list called 'config'.
*/
int read_hash_info(hash_info_t *t, const char* filename)
{
        FILE *fp=NULL;
        struct stat stbuf;
        char buf[1024], keyword[128], value[1024];
        int id, line=1;

        if ( !t || !filename )
                return (-1);

        if ( stat(filename, &stbuf) != 0 ) {
                fprintf(stderr, "Can't access file '%s', %s\n", filename, strerror(errno));
                return(-1);
        }

        lock_hash_info(t);
        if ( stbuf.st_mtime == t->mtime ) {
                unlock_hash_info(t);
                return(1);
        }
        t->mtime = stbuf.st_mtime;
        fp = fopen(filename, "r");
        if ( fp ) {
                clear_hash_info_unlocked(t);
                hash_info_setdefault_unlocked(t);

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
								/* debug
								printf("keyword=[%s], value=[%s]\n",
								keyword, value);
								*/

                                id = get_hash_infoindex(&keyword[0]);
                                if ( id != -1 ) {
                                        set_hash_infoitem_unlocked(t, id, value);
								}
								else {
									errf("get_hash_infoindex(%s) error\n", keyword);
								}
				
                        }
                        line++;
                }
        }
        else {
                errf("Can't open info file %s, %s\n", filename, strerror(errno));
                unlock_hash_info(t);
                return(-1);
        }

		SAFE_FP_CLOSE(fp);
        check_hash_info_unlocked(t);

        unlock_hash_info(t);

        /* printf("Loading %s ok\n", filename); */

        return(0);
}

/*-----------------------------------------------------------------------
**  print_hash_info : Print the current configuration settings to the log file
*/
void print_hash_info(hash_info_t *t, FILE* fp)
{
        int i;
        lock_hash_info(t);
        fprintf(fp, "hash_info '%s':\n", VALUE(t->name));
        fflush(fp);
        for (i=0; i<NUM_HASH_INFO_ITEM; i++)
                fprintf(fp, "\t%s='%s' %d\n", hash_info_keywords[i],
                        VALUE(t->item[i].str_value),
                        t->item[i].int_value);

        fqlist_print(t->options, fp);
        fflush(fp);
        unlock_hash_info(t);
}

int _hash_info_isyes(hash_info_t *_hash_info, int item)
{
        char buf[1024];
        char* ptr;

        ptr = hash_infos(_hash_info, item, buf);
        if ( !ptr )
                return(0);
        if ( STRCCMP(buf, "yes") == 0 )
                return(1);
        return(0);
}
