/*
** fq_map.c
*/
#define FQ_MAP_C_VERSION "1.0,0"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>

#include "fq_map.h"
#include "fq_common.h"

/*
 * [ ] token
 */
static int get_token_blank(FILE* fp, char *dst, int* line)
{
    char *s = dst;
    int k = 0;


    *s = '\0';
    *line = 0;

    /* skip preceding spaces and comment lines */
     while ( !feof(fp) ) {
		*s = getc(fp);
		if ( *s == ' ' || *s == '\t' )
			continue;
		else if ( *s == '\n' ) {
			k += (1+skip_comment(fp));
			continue;
		}
		else
			break;
	}
	if ( feof(fp) ) {        /* no token */
#ifdef _TOKEN_DEBUG
        fprintf(stderr, "no token.\n");
#endif
		return (EOF);
	}


	/* token found */
	if ( *s == '[' ) {                     /* quote opened */
		while ( !feof(fp) ) {
			*s = getc(fp);
			if ( *s == '\t' ) {
				*s = ' ';
			} else if ( *s == '\n' ) {
				*s = '\0';
				*line = 1+skip_comment(fp);
				return (k);     /* quote not closed */

			} else if ( *s == ']' ) { /* quote closed */
				*s = '\0';
				while  ( !feof(fp) ) {
					*s = getc(fp);
					if ( *s == ' ' || *s == '\t' )
						continue;
					else if ( *s == '\n' ) {
						*s = '\0';
						*line = 1+skip_comment(fp);
						return (k);
					}
					else {
						ungetc(*s, fp);
						*s = '\0';
						return (k);
					}
				}
				return (k);
			}
			s++;
		}
	} else {
		while ( !feof(fp) ) {
			*(++s) = getc(fp);
			if ( *s == ' ' || *s == '\t' ) {
				while  ( !feof(fp) ) {
					*s = getc(fp);
					if ( *s == ' ' || *s == '\t' )
						continue;
					else if ( *s == '\n' ) {
						*s = '\0';
						*line = 1+skip_comment(fp);
						return (k);
					}
					else {
						ungetc(*s, fp);
						*s = '\0';
						return (k);
					}
				}
				if ( *s ) {
					*(++s) = '\0';
					return (k);
				}
			} else if ( *s == '\n' ) {
				*s = '\0';
				*line = 1+skip_comment(fp);
				return (k);
			}
		}
		if ( *s ) {
			*(++s) = '\0';
			return (k);
		}
	}
	if ( *s ) {
		*(++s) = '\0';
		return (k);
	}
	return (EOF);
}
map_format_t* new_map_format(const char* name)
{
	map_format_t* q;
	register int i;

	if ( !name || strlen(name)==0 ) {
		printf("FILE name not specified for new_map_ormat\n");
		return ((map_format_t*)NULL);
	}
	q = (map_format_t*)calloc(1, sizeof(map_format_t));

#ifdef _DEFINE_ASSERT
	ASSERT(q);
#endif
	q->count = 0;
	for (i=0; i < MAX_MAP_LINE; i++) {
		q->s[i] = NULL;
	}
	q->name = strdup(name);

	return (q);
}

void free_map_format(map_format_t **q)
{
	register int k;

	if ( !(*q) )
		return;

	for (k=0; k < (*q)->count; k++) {
		if ((*q)->s[k]) {
			SAFE_FREE((*q)->s[k]->key);
			SAFE_FREE((*q)->s[k]->group_id);
			SAFE_FREE((*q)->s[k]->grep_string);
			SAFE_FREE((*q)->s[k]->run_cmd);
			SAFE_FREE((*q)->s[k]->option_string);
			SAFE_FREE((*q)->s[k]->description);
			SAFE_FREE((*q)->s[k]->bin_dir);
			SAFE_FREE((*q)->s[k]);
		}
	}
	SAFE_FREE((*q)->name);
	SAFE_FREE(*q);
}

int  read_map_format(map_format_t *w, const char* filename)
{
	FILE *fp=NULL;
	char buf[1024];
	int j, k, rc, line=1, field_count=0;
	map_record_t *p;

#ifdef _DEFINE_ASSERT 
	ASSERT(w);
#endif
	fp = fopen(filename,"r");
	if(fp == NULL)
	{
		w->count = 0;
		fprintf(stderr, "Unable to open mapformat file '%s'\n", VALUE(filename));
		return (EOF);
	}

	if ( (j=skip_comment(fp)) == EOF ) {
		fprintf(stderr, "mapformat %s has no data\n", filename);
		SAFE_FP_CLOSE(fp);
		return (EOF);
	}

	line += j;

	k = 0; w->count = 0;

	while ( 1 ) {
		field_count = 0;
		/*******************************************/
		rc = get_token_blank(fp, buf, &j);
		if ( rc == EOF ) /* end of data entry */
			break;
		line += rc;
		if ( j > 0 ) {
			printf("get_token error. field=[1]\n");
			goto on_error;
		}

		p = (map_record_t *)calloc(1,sizeof(map_record_t));
#ifdef _DEFINE_ASSERT
		ASSERT(p);
#endif

		bzero((void*)p, sizeof(map_record_t));

		field_count++;
		p->key = strdup(buf);          /* field #1 - key */


		/*******************************************/
		rc = get_token_blank(fp, buf, &j);
		if ( rc != 0 || j > 0 ) {
			fprintf(stdout, "get_token error. field=[2]\n");
			goto on_error;
		}
		field_count++;
		p->group_id = strdup(buf);          /* field #2 - group_id */
		/*******************************************/


		/*******************************************/
		rc = get_token_blank(fp, buf, &j);
		if ( rc != 0 || j > 0 ) {
			fprintf(stdout, "get_token error. field=[3]\n");
			goto on_error;
		}
		field_count++;
		p->grep_string = strdup(buf);          /* field #3 - grep_string */
		/*******************************************/


		/*******************************************/
		rc = get_token_blank(fp, buf, &j);
		if ( rc != 0 || j > 0 ) {
			fprintf(stdout, "get_token error. field=[4]\n");
			goto on_error;
		}
		field_count++;
		p->run_cmd = strdup(buf);			/* field #4 - run_cmd */
		/*******************************************/


		/*******************************************/
		rc = get_token_blank(fp, buf, &j);
		if ( rc != 0 || j > 0 ) {
			fprintf(stdout, "get_token error. field=[5]\n");
			goto on_error;
		}
		field_count++;
		if( STRCMP(buf, "NULL") != 0 ) {
			p->option_string = strdup(buf);    /* field #5  - option_string */
		}
		else {
			p->option_string = NULL;
		}
		/*******************************************/

			/*******************************************/
		rc = get_token_blank(fp, buf, &j);
		if ( rc != 0 || j > 0  ) { 
			fprintf(stdout, "get_token error. field=[6]\n");
			goto on_error;
		}
		field_count++;
		p->description = strdup(buf);             /* field #6 - description */
		/*******************************************/


		/*******************************************/
		rc = get_token_blank(fp, buf, &j);
		if ( rc != 0 || j > 0 ) { 
			fprintf(stdout, "get_token error. field=[7]\n");
			goto on_error;
		}
		field_count++;
		p->pid_index = atoi(buf);     		/* field #7 - pid_index */
			/*******************************************/


		/*******************************************/
		rc = get_token_blank(fp, buf, &j);
		if ( rc != 0 ) { 
			fprintf(stdout, "get_token error. field=[7]\n");
			goto on_error;
		}
		field_count++;
		p->bin_dir = strdup(buf);     		/* field #8 - bin_directory */
			/*******************************************/
	
	
#ifdef _MAP_DEBUG
		fprintf(stderr, "one line read end. fields=[%d] sure? \n", field_count);
#endif

		p->pid = -1;


		if ( k == MAX_MAP_LINE-1 ) {
			printf("too many line.[%d]\n", k);
			goto on_error;
		}
		w->s[k++] = p;
		line += j;
	} /* while */

	SAFE_FP_CLOSE(fp);
	w->count = k;

	return (0);

on_error:
	fclose(fp);
#ifdef _MAP_DEBUG
	printf("Invalid map_format spec in %s (line %d)\n", w->name, line);
#endif
	w->count = 0;
	return (-1);
}

void print_map_format(map_format_t* w)
{
	register int i;

#ifdef _DEFINE_ASSERT
	ASSERT(w);
#endif

#ifdef _DEBUG
	fprintf(stdout, "map format file '%s' - records=[%d]\n", VALUE(w->name), w->count);
#endif

	for (i=0; i < w->count; i++) {
		printf("key = [%s]\n\t-group_id: [%s]\n\t-grep_string: [%s]\n\t-run_cmd: [%s]\n\t-option_string:[%s]\n\t-description: [%s]\n\t-pid_index: [%d]\n\t-pid = [%d]\n\t-bin-dir =[%s]\n",
		w->s[i]->key,
		w->s[i]->group_id,
		w->s[i]->grep_string,
		w->s[i]->run_cmd,
		w->s[i]->option_string,
		w->s[i]->description,
		w->s[i]->pid_index,
		w->s[i]->pid, 
		w->s[i]->bin_dir);
	}
}
