/***********************************************
* fq_conf.c
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include <errno.h>
#include <sys/stat.h>

#include "fq_conf.h"
#include "fq_common.h"
#include "fq_cache.h"

#define FQ_MASK_C_VERSION "1.0.0"

static int on_get_str( conf_obj_t *, char *, char *);
static char on_get_char( conf_obj_t *, char *);
static int on_get_int( conf_obj_t *, char *);
static float on_get_float( conf_obj_t *, char *);
static long on_get_long( conf_obj_t *, char *);
static void on_print_conf( conf_obj_t *);
static int read_config(fqlist_t *t, const char* filename);

int open_conf_obj( char *filename, conf_obj_t **obj)
{
	conf_obj_t *rc;

	assert(filename);

	rc = (conf_obj_t *)calloc(1, sizeof(conf_obj_t));

	if(rc) {
		rc->filename = strdup(filename);
		
		rc->items = fqlist_new('C', "items");

		rc->item_num = read_config(rc->items, filename);
		if( rc->item_num < 0 ) {
			goto STOP;
		}

		rc->on_get_str = on_get_str;
		rc->on_get_char = on_get_char;
		rc->on_get_int = on_get_int;
		rc->on_get_float = on_get_float;
		rc->on_get_long = on_get_long;
		rc->on_print_conf = on_print_conf;

		*obj = rc;
		return(TRUE);
	}

STOP:

	SAFE_FREE( (*obj) );

	return(FALSE);
}


/*-----------------------------------------------------------------------
**  read_config : Read configuration file
**     Argument : char *filename;   - configuration file name 
**    Operation : It reads the specified configuration file which contains
**       'keyword = value' type assignments for each line. The settings
**       are stored in a pairwise linked list called 'config'.
*/
static int read_config(fqlist_t *t, const char* filename)
{
        FILE *fp=NULL;
        struct stat stbuf;
        char buf[1024], keyword[128], value[1024];
        int  line=1;
		int	 items=0;

        if ( !t || !filename )
                return (-1);

        if ( stat(filename, &stbuf) != 0 ) {
                fprintf(stderr, "Can't access file '%s', %s\n", filename, strerror(errno));
                return(-1);
        }

        fp = fopen(filename, "r");
        if ( fp == NULL ) {
                fprintf(stderr, "Can't open config %s, %s\n", filename, strerror(errno));
                return(-1);
		}
		while ( !feof(fp) ) {
				line += skip_comment(fp);

				if ( get_feol(fp, buf) == -1 )
						break;

				if ( !buf[0] || (strlen(buf) < 2) ) {
						line++;
						continue;
				}
				if ( get_assign(buf, keyword, value) != 0 ) {
						fprintf(stderr, "Invalid configuration setting at line %d at %s\n", line, filename);
						SAFE_FP_CLOSE(fp);
						return(-2);
				}
				fqlist_add(t, keyword, value, 0);
				items++;
				line++;
		}
		SAFE_FP_CLOSE(fp);

        // fprintf(stdout, "Loading %s ok (items=%d)\n", filename, items);

        return(items);
}

#define FOUND_CONF		1
#define NOT_FOUND_CONF	-9999

static int on_get_str( conf_obj_t *obj, char *keyword, char *value)
{
	char *ptr=NULL;

	ptr = fqlist_getval_unlocked(obj->items, keyword);
	if( ptr ) {
		strcpy(value, ptr);
		return(FOUND_CONF);
	}
	else {
		return(NOT_FOUND_CONF);
	}
}

static int on_get_int( conf_obj_t *obj, char *keyword)
{
	char *ptr=NULL;
	char	value[10];

	memset(value, 0x00, sizeof(value));

	ptr = fqlist_getval_unlocked(obj->items, keyword);
	if( ptr ) {
		strcpy(value, ptr);
		return(atoi(value));
	}
	else {
		return(NOT_FOUND_CONF);
	}
}

static float on_get_float( conf_obj_t *obj, char *keyword)
{
	char *ptr=NULL;
	char	value[10];

	memset(value, 0x00, sizeof(value));

	ptr = fqlist_getval_unlocked(obj->items, keyword);
	if( ptr ) {
		strcpy(value, ptr);
		return(atof(value));
	}
	else {
		return(NOT_FOUND_CONF);
	}
}

static long on_get_long( conf_obj_t *obj, char *keyword)
{
	char *ptr=NULL;
	char	value[24];

	memset(value, 0x00, sizeof(value));

	ptr = fqlist_getval_unlocked(obj->items, keyword);
	if( ptr ) {
		strcpy(value, ptr);
		return(atol(value));
	}
	else {
		return(NOT_FOUND_CONF);
	}
}

static char on_get_char( conf_obj_t *obj, char *keyword)
{
	char *ptr=NULL;
	char	c;

	ptr = fqlist_getval_unlocked(obj->items, keyword);
	if( ptr ) {
		c = *ptr;
		return(c);
	}
	else {
		return(0x00);
	}
}

int close_conf_obj(conf_obj_t **obj)
{
	SAFE_FREE( (*obj)->filename );

	fqlist_free(&(*obj)->items);

	SAFE_FREE( (*obj) );

	return(TRUE);
}
static void on_print_conf( conf_obj_t *obj)
{
	fprintf(stdout, "Config '%s':\n", VALUE(obj->filename));
	fprintf(stdout, "number of items: [%d] \n", obj->item_num);
	fqlist_print(obj->items, stdout);
	
	return;
}
