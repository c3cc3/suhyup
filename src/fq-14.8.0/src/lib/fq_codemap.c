/*
** fq_codemap.c
*/
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "fq_codemap.h"
#include "fq_common.h"

static void lock_codemap(codemap_t *m)
{
        pthread_mutex_lock(&m->lock);
}

static void unlock_codemap(codemap_t *m)
{
        pthread_mutex_unlock(&m->lock);
}

#if 0
static void free_codemap_unlocked(codemap_t **ptrm)
{
        if ( !(*ptrm) )
                return;

        SAFE_FREE((*ptrm)->name);
        fqlist_free(&(*ptrm)->map);
        SAFE_FREE( *ptrm );
        *ptrm = NULL;
}
#endif

static void clear_codemap_unlocked(codemap_t *m)
{
        if ( m->map )
                fqlist_clear(m->map);
}

codemap_t *new_codemap(fq_logger_t *l, char* name)
{
        codemap_t *m;

		FQ_TRACE_ENTER(l);

        m = (codemap_t *)calloc(1, sizeof(codemap_t));

        m->name= strdup(name);
        m->map = fqlist_new('C', "");
        m->mtime  = (time_t)0;
        pthread_mutex_init(&m->lock, NULL);

		FQ_TRACE_EXIT(l);
        return (m);
}

void free_codemap(fq_logger_t *l, codemap_t **ptrm)
{
		FQ_TRACE_ENTER(l);

        if ( !(*ptrm) ) {
				FQ_TRACE_EXIT(l);
                return;
		}

        pthread_mutex_lock(&(*ptrm)->lock);
        SAFE_FREE((*ptrm)->name);
        fqlist_free(&(*ptrm)->map);
        pthread_mutex_unlock(&(*ptrm)->lock);
        pthread_mutex_destroy(&(*ptrm)->lock);
        SAFE_FREE( *ptrm );
        *ptrm = NULL;

		FQ_TRACE_EXIT(l);
		return;
}


void clear_codemap( fq_logger_t *l, codemap_t *m)
{
		
		FQ_TRACE_ENTER(l);

        pthread_mutex_lock(&m->lock);
        if ( m->map )
                fqlist_clear(m->map);
        pthread_mutex_unlock(&m->lock);

		FQ_TRACE_EXIT(l);
}


/*
** return values:
** 	success : 0
**	fail	: < 0
**	skipped : 1
** 
** sample of writing codemap file.
	key 	value1|value2|value3
*/
int read_codemap(fq_logger_t *l, codemap_t *code_map, char* filename)
{
        FILE *fp;
        struct stat stbuf;
        char key[256], value[1024], flag[80];
        int j,k,line=1;

		FQ_TRACE_ENTER(l);

        fq_log(l, FQ_LOG_DEBUG, "Loading codemap %s.", filename);

        if ( !filename || !code_map ) {
                fq_log(l, FQ_LOG_ERROR, "codemap %s not found.", filename);
				FQ_TRACE_EXIT(l);
                return (-1);
        }

        if ( stat(filename, &stbuf) != 0 ) {
                fq_log(l, FQ_LOG_ERROR, "codemap %s access fail. reason=[%s].", strerror(errno),filename);
				FQ_TRACE_EXIT(l);
                return(-1);
        }

        lock_codemap(code_map);

		/* If it is same, don't load */
        if ( stbuf.st_mtime == code_map->mtime ) {
                unlock_codemap(code_map);
				FQ_TRACE_EXIT(l);
                return(1);
        }

        code_map->mtime = stbuf.st_mtime;

        fp = fopen(filename, "r");

		if ( fp ) {
                clear_codemap_unlocked(code_map);

                line += skip_comment(fp);

                while ( !feof(fp) ) {
                        j = get_token(fp, key, &k);

                        if ( j == EOF )
                                break;
                        line += j;
                        if ( k > 0 ) {
								fq_log(l, FQ_LOG_ERROR, "invalid format.");
                                goto on_error;
                        }

                        j = get_token(fp, value, &k);
                        if ( j != 0 ) {
								if ( j == EOF ) break;
								else {
									fq_log(l, FQ_LOG_ERROR, "invalid format.");
									goto on_error;
								}
                        }

                        if ( k == 0 && !feof(fp) ) {            /* option specified */
                                j = get_token(fp, flag, &k);
								if ( j == EOF ) break;

                                if ( (j != 0 || k < 1) && !feof(fp) ) {
										fq_log(l, FQ_LOG_ERROR, "invalid format.");
										break;
                                }
                        } else {
                                flag[0] = '0';
                        }
                        fqlist_add(code_map->map, key, value, (int)flag[0]);
                        line += k;
                }
        }
        else {
                fq_log(l, FQ_LOG_ERROR, "Can't open codemap %s, %s.", filename, strerror(errno));
                unlock_codemap(code_map);
                return(-1);
        }

		fclose(fp);
        unlock_codemap(code_map);

        fq_log(l, FQ_LOG_DEBUG, "Loading %s ok, %d items.", filename, code_map->map->list->length);

		FQ_TRACE_EXIT(l);
        return(0);

on_error:
        fclose(fp);
        clear_codemap_unlocked(code_map);
        fq_log(l, FQ_LOG_DEBUG, "Invalid data in codemap '%s' file_name='%s'(line %d).", code_map->name, filename, line);
        unlock_codemap(code_map);
		FQ_TRACE_EXIT(l);
        return(-1);
}

int get_codeval(fq_logger_t *l, codemap_t *m, char* code, char* dst)
{
        char *ptr;


		ASSERT(m);
		ASSERT(code);

		FQ_TRACE_ENTER(l);

        lock_codemap(m);

        ptr = fqlist_getval_unlocked(m->map, code);
        if ( ptr ) {
                strcpy(dst, ptr);
                unlock_codemap(m);
				FQ_TRACE_EXIT(l);
                return (0);
        }
        else {
                strcpy(dst, "");
                unlock_codemap(m);
				FQ_TRACE_EXIT(l);
                return (-1);
        }
}

int get_codekey( fq_logger_t *l, codemap_t *m, char* value, char* dst)
{
        char *ptr;

		FQ_TRACE_ENTER(l);

        lock_codemap(m);

        ptr = fqlist_getkey_unlocked(m->map, value);
        if ( ptr ) {
                strcpy(dst, ptr);
                unlock_codemap(m);
				FQ_TRACE_EXIT(l);
                return (0);
        }
        else {
                strcpy(dst, "");
                unlock_codemap(m);
				FQ_TRACE_EXIT(l);
                return (-1);
        }
}

int get_codekeyflag(fq_logger_t *l, codemap_t *m, char* value, char* dst, int* flag)
{
        char *ptr;

		FQ_TRACE_ENTER(l);

        lock_codemap(m);
        *flag = fqlist_getkeyflag_unlocked(m->map, value, &ptr);
        if ( ptr ) {
                if ( dst )
                        strcpy(dst, ptr);
                unlock_codemap(m);
				FQ_TRACE_EXIT(l);
                return (0);
        }
        else {
                if ( dst )
                        strcpy(dst, "");
                unlock_codemap(m);
				FQ_TRACE_EXIT(l);
                return (-1);
        }
}

void print_codemap(fq_logger_t *l, codemap_t* m, FILE* fp)
{
		FQ_TRACE_ENTER(l);

        lock_codemap(m);
        fprintf(fp, "Codemap '%s':\n", m->name);
        if ( m->map ) {
                fqlist_print(m->map, fp);
        }
        unlock_codemap(m);

		FQ_TRACE_EXIT(l);
}

int get_value_codemap_file(fq_logger_t *l, char *path, char* filename, char* src, char* dst)
{
    codemap_t *t;
    char buf[128];
    FILE* fp;

	FQ_TRACE_ENTER(l);
    ASSERT(dst);

    if ( !src ) {
		fq_log(l, FQ_LOG_ERROR, "There is no src value in parameters.");
		FQ_TRACE_EXIT(l);
        return(-1);
	}
    strcpy(dst, src);

    t = new_codemap(l, filename);
    if ( !t ) {
		fq_log(l, FQ_LOG_ERROR, "new_codemap('%s') failed.", filename);
		FQ_TRACE_EXIT(l);
        return(-1);
	}

    sprintf(buf, "%s/%s", path, filename);
    if ( read_codemap(l, t, buf) == 0 )
        (void)get_codeval(l, t, src, dst);

    free_codemap(l, &t);

	FQ_TRACE_EXIT(l);
    return(0);
}

int get_num_items( fq_logger_t *l, codemap_t* m )
{
	return(m->map->list->length);
}
