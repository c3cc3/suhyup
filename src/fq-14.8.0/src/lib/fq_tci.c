/***********************************************
* fq_tci.c
*
*/
#define FQ_TCI_C_VERSION "1.0.0"

#define _REENTERANT

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fq_tci.h"
#include "fq_common.h"

#include "fq_conf.h"
#include "fq_codemap.h"
#include "fq_cache.h"

#define _USE_UUID

static void make_postdata(aformat_t *w);
static int check_length(int code, char* src, char* dst, int len);
static int check_chars(int code, char* src, char* dst, int len);
static int fill_chars(int code, char* src, char* dst, int len);
static int del_chars(int code, char* src, char* dst, int len);
static int change_chars(int code, char* src, char* dst, int len);
static int add_string(int code, char* src, char* dst, int len);
static int ins_chars_to_date(int code, char* src, char* dst, int len);
static int to_upper(int code, char* src, char* dst, int len);
static int to_lower(int code, char* src, char* dst, int len);
static int to_number(int code, char* src, char* dst, int len);
static int fit_chars(int code, char* src, char* dst, int len);

tcidef_t _tci_defns [] = {
        { SAVE_N,       "N" },          /* 0 */
        { SAVE_L,       "L" },          /* 1 */
        { SAVE_S,       "S" },          /* 2 */
        { SAVE_LA,      "LA" },         /* 3 */
        { SAVE_SA,      "SA" },         /* 4 */

        { FILL_FIXED,   "FIXED" },      /* 5 */
        { FILL_REUSE,   "REUSE" },      /* 6 */
        { FILL_POST,    "POST" },       /* 7 */
        { FILL_CACHE,	"CACHE" },   	/* 8 */
        { FILL_STREAM,  "STREAM" },     /* 9 */

        { FILL_UUID, 	"SYS_UUID" },    	/* 10 */
        { FILL_DATE, 	"SYS_DATE" },    	/* 11 */
        { FILL_HOSTID,  "SYS_HOSTID" },    	/* 12 */

        { TYPE_STR,     "STR" },        /* 13 */
        { TYPE_PID,     "PID" },        /* 14 */
        { TYPE_CODESET, "CODESET" },    /* 15 */
        { TYPE_CLI_OP,  "CLI_OP" },     /* 16 */
        { TYPE_INT,     "INT" },        /* 17 */
        { TYPE_HEXA,    "HEXA" },       /* 18 */

        { TYPE_NO,      "N" },          /* 19 */
        { TYPE_YES,     "Y" },          /* 20 */

        { OUTPUT,       "@OUTPUT" },    /* 21 */
        { OUTPUT_P,     "@OUTPUT_P" },  /* 22 */
        { OUTPUT_TABLE, "@OUTPUT_TABLE" }, /* 23 */
        { OUTPUT_BLOCK, "@OUTPUT_BLOCK" }, /* 24 */
        { OUTPUT_UNION, "@OUTPUT_UNION" }, /* 25 */
		
        { -1, NULL }
};


int get_tcidef(int field, char* key)
{
        int i, from, to;

        assert(key);

        switch ( field ) {
                case SAVE : from = 0; to = 4; break;
                case FILL : from = 5; to = 12; break;
                case TYPE : from = 13; to = 18; break;
                case DISP : from = 19; to = 20; break;
                case BLOCK: from = 21; to = 25; break;
                default : return (-1);
        }

        for (i=from; i <= to; i++) {
                if ( STRCCMP(_tci_defns[i].key, key) == 0 )
                        return ( _tci_defns[i].type );
        }
        return(-1);
}

char*
get_tcikey(int type)
{
        int i = 0;

        if ( i < 0 )
                return(NULL);

        while ( _tci_defns[i].type >= 0 ) {
                if ( _tci_defns[i].type == type )
                        return ( _tci_defns[i].key );
                i++;
        }
        return (NULL);
}

qformat_t* new_qformat(char* name, char** ptr_errmesg)
{
	qformat_t* q=NULL;
	register int i;
	char    szErrMsg[1024];

	if ( !name || strlen(name)==0 ) {
		snprintf(szErrMsg, sizeof(szErrMsg), "File name not specified for new_qformat.[%s][%d]", __FILE__, __LINE__);
		*ptr_errmesg = (char *)strdup(szErrMsg);
		return ((qformat_t*)NULL);
	}

	q = (qformat_t*)calloc(1, sizeof(qformat_t));
	if( !q ) {
		snprintf(szErrMsg, sizeof(szErrMsg), "Memory overflow .[%s][%d]", __FILE__, __LINE__);
		*ptr_errmesg = (char *)strdup(szErrMsg);
		return ((qformat_t*)NULL);
	}

	q->count = 0;
	q->length = 0;
	for (i=0; i < MAX_QFIELD_COUNT; i++) {
		q->s[i] = NULL;
	}
	q->name = strdup(name);

	q->data = NULL;
	q->datalen = 0;

	return (q);
}

/*--------------------------------------------------------------
** clear_value_qformat 
** Return value : none
*/
void clear_value_qformat(qformat_t **q)
{
        register int k;

        if ( !(*q) )
                return;

        for (k=0; k < (*q)->count; k++) {
                if ((*q)->s[k]) {
#if 0
                        SAFE_FREE((*q)->s[k]->name);
                        SAFE_FREE((*q)->s[k]->deflt);
#endif
						SAFE_FREE((*q)->s[k]->value);
#if 0
                        SAFE_FREE((*q)->s[k]->conv);
                        SAFE_FREE((*q)->s[k]);
#endif
                }
		}
#if 0
        SAFE_FREE((*q)->name);
#endif
		SAFE_FREE((*q)->data);
}
/*--------------------------------------------------------------
** clear_value_qformat 
** Return value : none
*/
void free_qformat_values(qformat_t **q)
{
        register int k;

        if ( !(*q) )
                return;

        for (k=0; k < (*q)->count; k++) {
                if ((*q)->s[k]) {
						SAFE_FREE((*q)->s[k]->value);
						SAFE_FREE((*q)->s[k]->deflt);
						SAFE_FREE((*q)->s[k]->conv);
                }
		}
		SAFE_FREE((*q)->data);
}

/*--------------------------------------------------------------
** free_qformat - Free qformat_t structure
** Return value : none
*/
void free_qformat(qformat_t **q)
{
        register int k;

        if ( !(*q) )
                return;

        for (k=0; k < (*q)->count; k++) {
                if ((*q)->s[k]) {
						SAFE_FREE((*q)->s[k]->name);
						SAFE_FREE((*q)->s[k]->deflt);
						SAFE_FREE((*q)->s[k]->value);
						SAFE_FREE((*q)->s[k]->conv);
						SAFE_FREE((*q)->s[k]);
                }
		}
		SAFE_FREE((*q)->name);
		SAFE_FREE((*q)->data);
		SAFE_FREE(*q);
        *q = NULL;
}

/*--------------------------------------------------------------
** new_aformat : Allocate a aformat_t structure
** Argument    : char* name; // name of the format file
** Return value : aformat_t* if successful, NULL if fail
*/
aformat_t* new_aformat(char* name, char** ptr_errmesg)
{
        aformat_t *a=NULL;
        register int i;
        char    szErrMsg[1024];

        if ( !name || strlen(name)==0 ) {
                snprintf(szErrMsg, sizeof(szErrMsg), 
			"File name not specified for new_qformat.[%s][%d]", __FILE__, __LINE__);
                *ptr_errmesg = (char *)strdup(szErrMsg);
                return ((aformat_t*)NULL);
        }
        a = (aformat_t*)calloc(1, sizeof(aformat_t));
        if( !a ) {
                snprintf(szErrMsg, sizeof(szErrMsg), "memory overflow.[%s][%d]", __FILE__, __LINE__);
                *ptr_errmesg = (char *)strdup(szErrMsg);
                return ((aformat_t*)NULL);
        }

        a->count = 0;
        a->length = 0;
        for (i=0; i < MAX_ABLOCK_COUNT; i++)
                a->b[i] = NULL;
        a->name = strdup(name);

        a->data = NULL;
        a->datalen = 0;

		a->postdata = calloc( MAX_POSTDATA_LEN, sizeof(char));

        return (a);
}
/*--------------------------------------------------------------
** free_aformat - Free aformat_t structure
** Return value : none
*/
void free_aformat(aformat_t **a)
{
        register int l,k;
        ablock_t *b;

        if ( !(*a) )
                return;

        for (l=0; l < (*a)->count; l++) {
                if ( (*a)->b[l] ) {
                        SAFE_FREE((*a)->b[l]->name);
                        SAFE_FREE((*a)->b[l]->n_dat_var);
                        b = (*a)->b[l];
                        for (k=0; k < b->n_rec; k++) {
                                SAFE_FREE(b->s[k]->name);
                                SAFE_FREE(b->s[k]->value);
                                SAFE_FREE(b->s[k]->conv);
                                SAFE_FREE(b->s[k]);
                        }
                        SAFE_FREE((*a)->b[l]);
                }
		}
        SAFE_FREE((*a)->name);
        SAFE_FREE((*a)->data);
        SAFE_FREE((*a)->postdata);


		if( (*a)->cache_short) 
			cache_free( &(*a)->cache_short);

		if( (*a)->jv )
			json_value_free( (*a)->jv );

        SAFE_FREE(*a);
}
/*--------------------------------------------------------------
** free_aformat_values - Free aformat_t structure
** Return value : none
*/
void free_aformat_values(aformat_t **a)
{
        register int l,k;
        ablock_t *b;

        if ( !(*a) )
                return;

        for (l=0; l < (*a)->count; l++) {
                if ( (*a)->b[l] ) {
                        // SAFE_FREE((*a)->b[l]->name);
                        // SAFE_FREE((*a)->b[l]->n_dat_var);
                        b = (*a)->b[l];
                        for (k=0; k < b->n_rec; k++) {
                                // SAFE_FREE(b->s[k]->name);
                                SAFE_FREE(b->s[k]->value);
                                SAFE_FREE(b->s[k]->conv);
                                // SAFE_FREE(b->s[k]);
                        }
                        // SAFE_FREE((*a)->b[l]);
                }
		}

		if( (*a)->cache_short) 
			cache_clear( (*a)->cache_short);
}

/*---------------------------------------------------------------------
** read_qformat - parse query message format
** Return value - 0 if successful, -1 o/w
** Remark - memory is bzero'ed and the values are set to defaults.
*/
int  read_qformat(qformat_t *w, char* filename, char** ptr_errmesg)
{
        FILE *fp=NULL;
        char buf[128];
        char *ptr, *last;
        int j, k, rc, ret=-1, line=1;
        qrecord_t *p=NULL;
        char szErrMsg[1024];

        assert(w);

        fp = fopen(filename,"r");
        if(fp == NULL)
        {
                w->count = 0;
                snprintf(szErrMsg, sizeof(szErrMsg), "Unable to open qformat file '%s'\n", filename);
                *ptr_errmesg = (char *)strdup(szErrMsg);
                return (EOF);
        }

        if ( (j=skip_comment(fp)) == EOF ) {
                snprintf(szErrMsg, sizeof(szErrMsg), "qformat file '%s' has no data.\n", filename);
                *ptr_errmesg = (char *)strdup(szErrMsg);
				SAFE_FP_CLOSE(fp);
                return (EOF);
        }
        line += j;
	k = 0; w->count = 0; w->length = 0;

        while ( 1 ) {
                rc = get_token(fp, buf, &j);
                if ( feof(fp) ) /* end of data entry */
                        break;
                if ( rc == EOF ) /* end of data entry */
                        break;
                line += rc;
                if ( j > 0 )
                        goto on_error;

                p = (qrecord_t *)calloc(1, sizeof(qrecord_t));
                assert(p);

                bzero((void*)p, sizeof(qrecord_t));

                SAFE_FREE(p->name); /* herediff */
                p->name = strdup(buf);          /* field #1 - name */

                rc = get_token(fp, buf, &j);
                if ( rc != 0 || j > 0 )
                        goto on_error;

                ptr = strtok_r(buf, "&", &last);/* field #2 - len&mem_size */
                                                /* (mem_size=optional) */
                p->len = atoi(ptr);
                ptr = strtok_r(NULL, "&", &last);
                if ( ptr != NULL )
                        p->mem_size = atoi(ptr);
                else
                        p->mem_size = p->len;

                if ( p->len > MAX_FIELD_SIZE ) {
                        snprintf(szErrMsg,  
				sizeof(szErrMsg),"Format field length too large (%s=%d)\n", p->name, p->len);
                        goto on_error;
                }
		
		w->length += p->len;

                rc = get_token(fp, buf, &j);
                if ( rc != 0 || j > 0 )
                        goto on_error;

                p->fill = get_tcidef(FILL, buf);        /* field #3 */
                                                /* fill method */
                if ( p->fill == -1 )
                        goto on_error;

                rc = get_token(fp, buf, &j);
                if ( rc != 0 || j > 0 )
                        goto on_error;

                /*--jssong------------------------------------------------*/
                /*
                We support the setting by double quotation in the default
                field to give a sequence of blanks, e.g. "   ".  Moreover,
                every fields can be specified by using double quatation,
                e.g. "This is a default value".  However, for the backward
                compatability, the value of a single "#" will be treated as
                a sequence of blanks ("#..." also by side effects).
                */
                if ( STRCMP(buf, "#") == 0 ) {
                        strcpy(buf, " ");
                }
                /*--jssong------------------------------------------------*/

                p->deflt = strdup(buf);         /* field #4 */
                                                /* default value */
                rc = get_token(fp, buf, &j);
                if ( rc != 0 || j > 0 )
                        goto on_error;

                p->type = get_tcidef(TYPE, buf);
                if ( p->type == -1 )
                        goto on_error;

                rc = get_token(fp, buf, &j);
                if ( rc < 0 )
                        goto on_error;

		p->save = get_tcidef(SAVE, buf); /* field #5 - save */
                if ( p->save == -1 )
                        goto on_error;


                rc = get_token(fp, buf, &j);
                if ( rc != 0 )
                        goto on_error;

                p->conv = strdup(buf);          /* field #6 - conv */

			
				/* new: 2011/09/02 by gwisang.choi for ShinHan */
				rc = get_token(fp, buf, &j);    /* field #7 - index of message */
				if( rc != 0 ) 
						goto on_error;
                p->msg_index = atoi(buf);

				rc = get_token(fp, buf, &j);    /* field #8 - index of message */
				if( rc != 0 ) 
						goto on_error;
                p->msg_len = atoi(buf);

                p->value = (char *)calloc(p->mem_size + 1, sizeof(char) );
                assert(p->value);
                memset(p->value, 0x00, p->mem_size+1);

                /*
                ** set the value to the default value
                */
                if ( p->type == TYPE_HEXA ) {
                        (void)str_gethexa(p->deflt, p->value);
                }
                else {
                        strcpy(p->value, p->deflt);
                }

                if ( k == MAX_QFIELD_COUNT-1 )
                        goto on_error;

                w->s[k++] = p;

                line += j;

        } /* while */

		SAFE_FP_CLOSE(fp);
        w->count = k;

        return (0);

    on_error:

		SAFE_FP_CLOSE(fp);
        snprintf(szErrMsg, sizeof(szErrMsg), "Invalid qformat spec in %s (line %d) [%s][%d]\n",
                w->name, line, __FILE__, __LINE__);
        w->count = 0;
        w->length = 0;

        if( ret <  0 ) {
                *ptr_errmesg = strdup(szErrMsg);
        }
        return (-1);
}

/*---------------------------------------------------------------------
** read_aformat - parse response message format
** Arguments :
**      char *filename - name of the format file
**      aformat_t *w - pointer to a response message structure
** Return value - 0 if successful, -1 o/w
** Remark - memory is bzero'ed
*/
int  read_aformat(aformat_t *w, char* filename, char **ptr_errmesg)
{
	int rc = -1;
	char szErrMsg[1024];

	FILE *fp=NULL;
	char buf[128], buf1[128];
	char *ptr=NULL, *last=NULL;
	int i, j, k, line=1, num_block;
	arecord_t *p=NULL;
	ablock_t  *b=NULL;

	assert(w);

	fp = fopen(filename,"r");
	if(fp == NULL)
	{
		snprintf(szErrMsg, sizeof(szErrMsg), "read_aformat:Unable to open aformat file '%s'\n", VALUE(filename));
		goto L0;
	}

	if ( (j = skip_comment(fp)) == EOF ) {
		snprintf(szErrMsg, sizeof(szErrMsg), "read_aformat: aformat %s has no data\n", filename);
		goto L1;
	}

	line += j;

	/*
	SAFE_FREE( w->name );
	w->name = strdup(file);
	*/
	w->length = 0;

	num_block = w->count;
	k = 0;

	while ( 1 ) {
		rc = get_token(fp, buf, &j);
		if ( feof(fp) ) /* end of data entry */
				break;
		if ( rc == EOF )
				break;
		line += rc;

		/* Check if the beginning of a new block */
		if ( buf[0] == '@' ) {
			/* If previous block contains null record -> ignore */
			/* O/w, close the previous block */
			if ( k > 0 ) {
				if ( num_block == MAX_ABLOCK_COUNT-1 )
						goto L2;
				b->n_rec = k;
				w->b[num_block] = b;
				num_block++;
			}

			b = (ablock_t *)calloc(1, sizeof(ablock_t));
			assert(b);

			bzero((void*)b, sizeof(ablock_t));

			/* set block name */

			if ( STRCMP(buf, "@OUTPUT") == 0 ) {
				b->type = OUTPUT;
				b->name = strdup("");
			}
			else if ( STRCMP(buf, "@OUTPUT_P") == 0 ) {
				b->type = OUTPUT_P;
				snprintf(buf, sizeof(buf), "P%d", num_block);
				b->name = strdup(buf);
			}
			else if (STRCMP(buf, "@OUTPUT_BLOCK") == 0 ) {
				rc = get_token(fp, buf, &j);
				if ( rc != 0 || j > 0 )
						goto L2;
				b->type = OUTPUT_BLOCK;
				b->name = strdup(buf);
			}
			else if ( strncmp(buf, "@OUTPUT_BLOCK_", 14)==0 ) {
				/* for backward compatibility */
				b->type = OUTPUT_BLOCK;
				b->name = strdup(&buf[14]);
			}
			else if (STRCMP(buf, "@OUTPUT_TABLE") == 0 ) {
				/* for backward compatibility */
				b->type = OUTPUT_TABLE;
				snprintf(buf, sizeof(buf), "T%d", num_block);
				b->name = strdup(buf);
			}
			else if (STRCMP(buf, "@OUTPUT_UNION") == 0 ) {
				/* for backward compatibility */
				b->type = OUTPUT_UNION;
				snprintf(buf, sizeof(buf), "U%d", num_block);
				b->name = strdup(buf);
			}
			else {
#ifdef CHOI
				logging(stderr, "Invalid block name in aformat %s\n",filename);
#endif
				goto L2;
			}
			/* read block variables when OUTPUT_BLOCK */
			if ( (b->type == OUTPUT_BLOCK) ||
				 (b->type == OUTPUT_TABLE && j == 0) ) {

				rc = get_feol(fp, buf);
				if ( rc != 0 )
						goto L2;

				if ( getitem(buf, "#REPEAT", buf1) != 0 )
						goto L2;

				ptr = strchr(&buf[8], '&');
				if ( !ptr ) {   /* single value defined */
						if ( buf1[0] == '#' ) {
								b->n_max = b->n_dat =
										atoi(&buf1[1]);
								b->n_dat_var = strdup("");
						}
						else {
								b->n_max = b->n_dat = -1;
								b->n_dat_var = strdup(buf1);
						}
				}
				else {          /* two value specified */
						if ( buf1[0] == '#' ) {
								b->n_max = atoi(&buf1[1]);
								b->n_dat_var = strdup("");
						}
						else {
								goto L2;
								/* First value should be a */
								/* number */
						}
						if ( *(++ptr) == '#' ) {
								b->n_dat = atoi(++ptr);
								b->n_dat_var = strdup("");
						}
						else {
								b->n_dat = -1;
								b->n_dat_var = strdup(ptr);
						}
				}
				/* 2000/01/18 by choi */
				if( b->n_max > MAX_AFIELD_COUNT ) {
						b->n_max = MAX_AFIELD_COUNT;
#ifdef CHOI
						logging(stderr, "#REPEAT size is too large in the format file(%s)(%s)(%d)\n",
								VALUE(w->name), VALUE(b->name), b->n_max);
#endif
						b->n_max = MAX_AFIELD_COUNT;
				}
			}
			else {
					b->n_max = b->n_dat = 1;
					b->n_dat_var = strdup("");
			}
			k = 0;
			b->len = b->offset = 0;
			line++;
			continue;
		}
		else if (b == NULL) {
#ifdef CHOI
				logging(stderr, "aformat %s has no block identifier\n", VALUE(w->name));
#endif
				goto L2;
		}

		/* read individual record spec */

		p = (arecord_t *)calloc(1, sizeof(arecord_t));
		assert(p);

		bzero((void*)p, sizeof(arecord_t));

		SAFE_FREE(p->name); /* herediff */
		p->name = strdup(buf);          /* field #1 - name */

		rc = get_token(fp, buf, &j);
		if ( rc != 0 || j > 0 ) {
				goto L2;
		}
		ptr = strtok_r(buf, "&", &last);        /* field #2 */
								/* len&mem_size (mem_size=optional) */
		p->len = atoi(ptr);
		ptr = strtok_r(NULL, "&", &last);
		if ( ptr != NULL )
				p->mem_size = atoi(ptr);
		else
				p->mem_size = p->len;

		if ( p->len > MAX_FIELD_SIZE ) {
#ifdef CHOI
				logging(stderr, "Format field length too large (%s=%d)\n",
						VALUE(p->name), p->len);
#endif
				goto L2;
		}

		rc = get_token(fp, buf, &j);
		if ( rc != 0 || j > 0) {
				goto L2;
		}
		p->type = get_tcidef(TYPE, buf);        /* field #3 - type */
		if ( p->type == -1 ) {
				goto L2;
		}

		rc = get_token(fp, buf, &j);
		if ( rc != 0 || j > 0 ) {
				goto L2;
		}

		p->save = get_tcidef(SAVE, buf);        /* field #4 - save */
		if ( p->save == -1 ) {
				goto L2;
		}

		rc = get_token(fp, buf, &j);
		if ( rc != 0 && rc != -1 ) {
				goto L2;
		}
		p->conv = strdup(buf);  /* field #5 - conversion method */


		rc = get_token(fp, buf, &j);
		if ( rc != 0 ) {
				goto L2;
		}

		p->dspl = get_tcidef(DISP, buf);        /* field #6 - display */
		if ( p->dspl == -1 ) {
				goto L2;
		}

		p->value = NULL;

		b->len += p->len;

		if ( k == MAX_AFIELD_COUNT-1 ) {
				goto L2;
		}

		b->s[k++] = p;

		line += j;

    } /* while */

	if ( k > 0 ) {
		if ( num_block == MAX_ABLOCK_COUNT-1 ) goto L2;

		b->n_rec = k;
		w->b[num_block] = b;
		num_block++;
	}
	SAFE_FP_CLOSE(fp);
	w->count = num_block;

	/*
	** Compute aformat size with base block
	*/
	k = 0;
	for ( i=0; i < w->count; i++ ) {
		if ( w->b[i]->type == OUTPUT_UNION ) {
			if ( k == 0 )
					w->length += w->b[i]->len;
			k++;
		}
		else {
			k = 0;
			w->length += w->b[i]->len;
		}
	}
	return (0);

L2:
	snprintf(szErrMsg, 
	sizeof(szErrMsg),"Invalid aformat spec in %s (line %d)\n", VALUE(w->name), line);
	w->count = 0;
L1:
	SAFE_FP_CLOSE(fp);

L0:
	if( rc < 0 ) {
		*ptr_errmesg = (char *)strdup( szErrMsg );
	}
	return (rc);
}

void print_qformat(qformat_t* w)
{
        register int i, k;
        char val[MAX_FIELD_SIZE+1];
        char    tmpbuf[1024*10];
        char    tmpbuf2[1024*10];

        assert(w);

		memset(val, 0x00, MAX_FIELD_SIZE+1);
		memset(tmpbuf, 0x00, 10240);
		memset(tmpbuf2, 0x00, 10240);
	snprintf(tmpbuf, sizeof(tmpbuf), 
		"\nSENT summery. \n------------------------------\n Qformat '%s' - records=%d, length=%d\n", 
		VALUE(w->name), w->count, w->length);

        strcat(tmpbuf2, tmpbuf);

        for (i=0; i < w->count; i++) {
                if ( !w->s[i]->value )
                        strcpy(val, "");
                else if (w->s[i]->type == TYPE_PID) {
                        k = strlen(w->s[i]->value);
                        memset(val, '*', k);
                        val[k] = '\0';
                } else {
					strcpy(val, w->s[i]->value);
				}
                snprintf(tmpbuf, sizeof(tmpbuf), "\t'%s': %d '%s' [%s] '%s' '%s' '%s' [%s]\n",
                        VALUE(w->s[i]->name),
                        w->s[i]->len,
                        VALUE(get_tcikey(w->s[i]->fill)),
                        VALUE(w->s[i]->deflt),
                        VALUE(get_tcikey(w->s[i]->type)),
                        VALUE(get_tcikey(w->s[i]->save)),
                        VALUE(w->s[i]->conv),
                        val);
                strcat(tmpbuf2, tmpbuf);
        }
        strcat(tmpbuf2, "------------------------------\n\n");

		fprintf(stdout, "%s\n\n", tmpbuf2);
		fprintf(stdout, "[%s]\n\n", w->data);
		fprintf(stdout, "len=[%d]\n\n", w->datalen);
#ifdef CHOI
        logging(1, "%s", tmpbuf2);
#endif
}

void print_aformat(aformat_t *w)
{
        char    tmpbuf[1024*10];
        char    tmpbuf2[1024*10];

        assert(w);
        memset(tmpbuf, '\0', sizeof(tmpbuf));
        memset(tmpbuf2, '\0', sizeof(tmpbuf2));

#ifdef _DEBUG

printf("<p align=\"center\"><b>Aformat Summary</b></p>\n");
printf("<table width=\"75\%\" border=\"1\" align=\"center\">\n");
printf("  <tr> \n");
printf("    <td> \n");
printf("      <div align=\"center\"><font size=\"-1\">화일명</font></div>\n");
printf("    </td>\n");
printf("    <td> \n");
printf("      <div align=\"center\"><font size=\"-1\">총 블럭수</font></div>\n");
printf("    </td>\n");
printf("    <td> \n");
printf("      <div align=\"center\"><font size=\"-1\">총길이</font></div>\n");
printf("    </td>\n");
printf("  </tr>\n");
printf("  <tr> \n");
printf("    <td><font size=\"-1\">%s</font></td>\n", w->name);
printf("    <td><font size=\"-1\">%d</font></td>\n", w->count);
printf("    <td><font size=\"-1\">%d</font></td>\n", w->length);
printf("  </tr>\n");
printf("</table>\n");

        for (i=0; i < w->count; i++) {
                b = w->b[i];

                snprintf(tmpbuf, sizeof(tmpbuf), "Block %d: %s %s %d %d %d %d(%s)\n",
                        i, VALUE(get_tcikey(b->type)),
                        VALUE((char *)b->name), VALUE((char *)b->n_rec), b->len, b->n_max,
                        b->n_dat, VALUE(b->n_dat_var));

printf("<table width=\"75\%\" border=\"1\" align=\"center\">\n");
printf("  <tr> \n");
printf("    <td colspan=\"6\"><font size=\"-1\">%s</td>\n", tmpbuf);
printf("  </tr>\n");

                /* n_rec : number of fields in the block */
                for (k=0; k < b->n_rec; k++) {
                        if ( !b->s[k]->value )
                                strcpy(val, "");
                        else if (b->s[k]->type == TYPE_PID) {
                                j = strlen(VALUE(b->s[k]->value));
                                memset(val, '*', j);
                                val[j] = '\0';
                        } else strcpy(val, b->s[k]->value);
                        snprintf( tmpbuf, sizeof(tmpbuf), "\t%s: %d %s %s [%s] %s [%s]\n",
                                VALUE(b->s[k]->name),
                                b->s[k]->len,
                                VALUE(get_tcikey(b->s[k]->type)),
                                VALUE(get_tcikey(b->s[k]->save)),
                                VALUE(b->s[k]->conv),
                                VALUE(get_tcikey(b->s[k]->dspl)),
                                VALUE(val));

printf("  <tr> \n");
printf("    <td><font size=\"-1\">%s</td>\n", VALUE(b->s[k]->name));
printf("    <td><font size=\"-1\">%d</td>\n", b->s[k]->len);
printf("    <td><font size=\"-1\">%s</td>\n", VALUE(get_tcikey(b->s[k]->type)));
printf("    <td><font size=\"-1\">%s</td>\n", VALUE(get_tcikey(b->s[k]->save)));
printf("    <td><font size=\"-1\">%s</td>\n", VALUE(b->s[k]->conv));
printf("    <td><font size=\"-1\">%s</td>\n", VALUE(get_tcikey(b->s[k]->dspl)));
printf("    <td><font size=\"-1\">%s</td>\n", VALUE(val));
printf("  </tr>\n");

                }
        }

printf("</table>\n");
printf("<p>&nbsp; </p>\n");
printf("</body>\n");
printf("</html>\n");

#endif
}

static void make_postdata(aformat_t *w)
{
		int i, j, k;
        ablock_t  *b=NULL;
		char	val[1024], buf[2048];

        assert(w);

		if(w->postdata) free(w->postdata);

		w->postdata = calloc( MAX_POSTDATA_LEN, sizeof(char));

        for (i=0; i < w->count; i++) {
                b = w->b[i];

                /* n_rec : number of fields in the block */
                for (k=0; k < b->n_rec; k++) {
						memset(val, 0x00, sizeof(val));
                        if ( !b->s[k]->value )
                                strcpy(val, "");
                        else if (b->s[k]->type == TYPE_PID) {
                                j = strlen(b->s[k]->value);
                                memset(val, '*', j);
                                val[j] = '\0';
                        } else {
								strcpy(val, b->s[k]->value);
						}
						memset(buf, 0x00, sizeof(buf));
						sprintf(buf, "%s=%s&", b->s[k]->name, val);

						strcat(w->postdata, buf);
                }
        }
}
static void make_json(aformat_t *w)
{
	register int i,j,k,offset=0;
	ablock_t	*b;
	char *str, buf[MAX_FIELD_SIZE];
	JSON_Object *jo=NULL;

	if(!w) return;

	w->jv = json_value_init_object();
	jo = json_value_get_object(w->jv);

	for(i=0; i<w->count; i++) {
		b = w->b[i];
		//printf("block_name = '%s'\n", b->name);
		if( !(b->type == OUTPUT_BLOCK || b->type == OUTPUT_TABLE) ) {
			// printf("[%d]-th OUTPUT is not BLOCK or TABLE.\n", i);
			int field_offset = 0;
			int y;
			for(y=0; y<b->n_rec; y++) {
				char field[MAX_FIELD_SIZE];
				memcpy(field, w->data+b->offset+field_offset, b->s[y]->len);
				field[b->s[y]->len] = 0x00;
				//printf("[%d]-th field: name='%s', value='%s'.\n", y, b->s[y]->name, field);
				json_object_set_string( jo, b->s[y]->name, field);
				field_offset += b->s[y]->len;
			}
			continue;
		}
		else {
			//printf("[%d]-th OUTPUT is BLOCK or TABLE, it has loop values, loop_cnt is [%d].\n", i, b->n_dat);
			offset = b->offset;

			json_object_set_value(jo, b->name, json_value_init_array());
			JSON_Array *jblock = json_object_get_array(jo, b->name);

			for(j=0; j<b->n_dat; j++) {
				int rec_len = 0;
				for(k=0; k<b->n_rec; k++) {
					rec_len += b->s[k]->len;
				}
				// print("rec_len = '%d'\n", rec_len); // length of sum of fields in a block.
				char record[1024];
				memcpy(record, w->data+offset, rec_len);
				record[rec_len] = 0x00;
				// printf("[%d]-th record: '%s'\n", record);
				JSON_Value *sub_jv;
				JSON_Object *sub_jo;
				sub_jv = json_value_init_object();
				sub_jo = json_value_get_object(sub_jv);

				int field_offset = 0;
				for(k=0; k<b->n_rec; k++) {
					char field[MAX_FIELD_SIZE];
					memcpy(field, record+field_offset, b->s[k]->len);
					field[b->s[k]->len] = 0x00;
					// printf("\t [%d]-th field: name='%s', value='%s'.\n", b->s[k]->name, field);
					json_object_set_string(sub_jo, b->s[k]->name, field);
					field_offset += b->s[k]->len;
				}
				json_array_append_value(jblock, sub_jv);
				offset += rec_len;
			}
		}
	}
}

/*
** This parses multi-codes, and put integer to codeval buffer.
*/
static int parse_codeseq(char* codes, int *codeval)
{
	char *p = (char*)codes, *q, buf[80];
	int i, j;

	for (i=0; i < MAX_MULTI_CONVERSION; i++) {
		if ( !p )
			break;
		if ( (q=strchr(p, '+')) ) {
			strncpy(buf, p, q-p);
			buf[q-p	] = '\0';
			p = q+1;
		}
		else {
			strcpy(buf, p);
			p = NULL;
		}
		codeval[i] = atoi(buf);
	}
	return(i);
}

static int do_convert( int code, char *src, char *dst, int len, char *fn)
{
	char* p = src;
	char* q = dst;
	int rc;

	assert(src);
	assert(dst);

	*q = '\0';

	if ( code >= CHECK_LENGTH_FROM && code <= CHECK_LENGTH_TO )
		return (check_length(code, p, q, len));

	switch ( code ) {
		case CHECK_FIELD_LENGTH:
			return(check_length(code, p, q, len));
		case CHECK_NUMERIC_ONLY:
		case CHECK_ALPHA_ONLY:
			return(check_chars(code, p, q, len));

		case FILL_SPACE:
		case FILL_SPACE_LEFT:
		case FILL_SPACE_RIGHT:
		case FILL_ZERO_LEFT:
			return(fill_chars(code, p, q, len));

		case DEL_SPACE:
			return(del_chars(code, p, q, len));

		case CHG_PLUS_TO_SPACE:
			return(change_chars(code, p, q, len));

		case ADD_HAN_WON:
		case ADD_HAN_DOLLOR:
		case ADD_CHAR_DOLLOR:
		case ADD_CHAR_PERCENT: 
			return(add_string(code, p, q, len));

		case INS_SLASH_TO_DATE:
		case INS_HYPHEN_TO_DATE:
			return(ins_chars_to_date(code, p, q, len));

		case TO_UPPER: return(to_upper(code, p, q, len));
		case TO_LOWER: return(to_lower(code, p, q, len));
		case TO_NUMBER: return(to_number(code, p, q, len));

		case FIT_LEFT:
			return(fit_chars(code, p, q, len));

		default:
			strcpy(dst,p);
			return(-CODE_UNDEFINED);
	}
}	


int format_conversion( char** str, char* fn, int len, char* codes)
{
	char *dst, *buf, *org;
	int i, n, rc;
	int codeval[MAX_MULTI_CONVERSION];

	n = parse_codeseq(codes, codeval);

	if( n == 0 || codeval[0] == 0 ) {
		return(0);
	}

	dst = calloc( MAX_FIELD_SIZE, sizeof(char)); assert(dst);
	buf = calloc( MAX_FIELD_SIZE, sizeof(char)); assert(buf);
	org = strdup(*str);

	strcpy(dst, VALUE(*str));

	for (i=0; i < n; i++) {
		/* ignore conversion code zero */
		if ( codeval[i] == 0 ) continue;

		if ( codeval[i] == STOP_CONV ) break;

		if ( codeval[i] == STOP_CONV_IF_BLANK ) {
			if ( str_isblank(*str) )		
				break;
			else
				continue;
		}

		rc = do_convert(codeval[i], *str, dst, len, fn);
		if( rc == 0 ) {
			SAFE_FREE(*str);
			*str = STRDUP(dst);
		}
		else {
			printf("do_convert(%s, %d) error!\n", *str, codeval[i]);
		}

	}

	SAFE_FREE(*str);
	*str = STRDUP(dst);

	SAFE_FREE(org);
	SAFE_FREE(dst);
	SAFE_FREE(buf);

	return(0);

}
void fill_qformat(qformat_t *w, char* postdata, fqlist_t *t, char *bytestream)
{
	register int i;

	assert(w);

	for (i=0; i < w->count; i++) {
		if( w->s[i]->fill == FILL_POST ) {
			char *buf=NULL;
				
			buf = calloc(MAX_FIELD_SIZE+1, sizeof(char));

			if ( postdata && get_item(postdata, w->s[i]->name, buf, strlen(postdata)+1) == 0 ) {
				 SAFE_FREE( w->s[i]->value ); // free default value
				 w->s[i]->value = strdup(buf);
			}

			SAFE_FREE(buf);
		}
		else if( t && (w->s[i]->fill == FILL_CACHE) ) {
			char *p=NULL;

			if( (p=fqlist_getval_unlocked(t, w->s[i]->name)) != NULL ) {
				SAFE_FREE ( w->s[i]->value ); // free default value
				w->s[i]->value = STRDUP(p);
			}	
		}
		else if( bytestream && w->s[i]->fill == FILL_STREAM ) {
			char *p=NULL;

			SAFE_FREE( w->s[i]->value ); // free default value

			p = calloc(w->s[i]->len+1, sizeof(char));
			memset(p, 0x00, w->s[i]->len+1);
			memcpy(p, &bytestream[w->s[i]->msg_index], w->s[i]->msg_len);
			w->s[i]->value = STRDUP(p);
			SAFE_FREE(p);
		}

#ifdef _USE_UUID
		else if( w->s[i]->fill == FILL_UUID ) {
			char buf[37];

			get_uuid(buf, sizeof(buf));

			SAFE_FREE( w->s[i]->value );

			w->s[i]->value = strdup(buf);

		}
#endif
		else if( w->s[i]->fill == FILL_DATE) {
			char date[9];

			get_time(date, NULL);
			SAFE_FREE( w->s[i]->value ); 

			w->s[i]->value = strdup(date);
		}
		else if( w->s[i]->fill == FILL_HOSTID ) {
			char	hostid[36];

			get_hostid( hostid );
			SAFE_FREE( w->s[i]->value );

			w->s[i]->value = strdup(hostid);
		}

		if( w->s[i]->conv ) {
			if ( format_conversion(&w->s[i]->value, w->s[i]->name, w->s[i]->len, w->s[i]->conv) < 0 ) {
				printf("format_conversion(%s) error!\n", w->s[i]->conv);
			}
		}
	}
}

void fill_qformat_from_list(qformat_t *w, fqlist_t *t)
{
    register int i;
    assert(w);
    assert(t);

    for (i=0; i < w->count; i++) {
		if( w->s[i]->fill != FILL_FIXED ) {
    		char *p=NULL;

			if( (p=fqlist_getval_unlocked(t, w->s[i]->name)) != NULL ) {
				SAFE_FREE(w->s[i]->value );
				w->s[i]->value = STRDUP(p);
			}	
		}
	}
}

/*
** 전용클라이언트에서는 이함수를 부른다.
** PFM_VERSION
*/
void fill_qformat2(qformat_t *w, char* src)
{
        register int i;
        char *buf=NULL;
        char    tmpbuf[1024];
        char    tmpbuf2[1024];

        assert(w);

        if ( !src )
                return;


        memset(tmpbuf, '\0', sizeof(tmpbuf));

        buf = (char*)calloc(MAX_FIELD_SIZE+1, sizeof(char));
        assert(buf);

        strcat(tmpbuf, "\nPOSTDATA fill summery.\n------------------------------\n");

        for (i=0; i < w->count; i++) {
                if ( get_item2(src, w->s[i]->name, buf, strlen(src)+1) == 0 && w->s[i]->fill != FILL_FIXED) {

                        SAFE_FREE( w->s[i]->value );

                        w->s[i]->value = STRDUP(buf);
                        memset(tmpbuf2, '\0', sizeof(tmpbuf2));
                        snprintf(tmpbuf2, 
				sizeof(tmpbuf2), "\t'%s'\t\t'%s'\n", VALUE(w->s[i]->name), VALUE(w->s[i]->value));
                        strcat(tmpbuf, tmpbuf2);
                }
        }
        strcat(tmpbuf, "------------------------------\n\n");

#ifdef CHOI
        logging(1, "%s", tmpbuf);
#endif
		SAFE_FREE(buf);

}


int get_avalue(aformat_t *w, char *name, char *dst)
{
        int i, k, offset, index, n, l;
        char varname[80];
        ablock_t *b;
	
	assert(dst);

	if ( !w || !w->data || !name )
                return (-1);
	if ( get_arrayindex(name, varname, &index) == 0 ) {
		/* find variable in the block */
            for (i=0; i < w->count; i++) {
                b = w->b[i];
                if ( b->type != OUTPUT_BLOCK && b->type != OUTPUT_TABLE )
                        continue;
                offset = 0;
                for (k=0; k < b->n_rec; k++) {
                        if ( STRCMP(b->s[k]->name, varname) == 0 ) {
                                /* found -- check range */
                                if ( index < 0 || index > b->n_dat ) {
                                        strcpy(dst, "?");
#ifdef CHOI
                                        logging(stderr, "Out of range in '%s' - ignored\n", name);
#endif
                                }
                                n = b->offset+b->len*index+offset;
                                l = b->s[k]->len;

                                if ( n+l <= w->datalen ) {
                                        strncpy(dst, &w->data[n], l);
                                        dst[l] = '\0';
                                }
                                else
                                        strcpy(dst, "?");

#ifdef CHOI
                                ptr = strdup(dst);
				/*
                                ** Format Conversion
                                */
                                if ( format_conversion(0, &ptr,
                                        b->s[k]->name,
                                        b->s[k]->len,
                                        b->s[k]->conv, NULL) == 0 ) {
                                        strcpy(dst, ptr);
                                }
								SAFE_FREE(ptr);
#endif
                                return (0);
			}
                        offset += b->s[k]->len;
                }
            }

#ifdef CHOI
            logging(stdout, "Specified array var %s not found in aformat %s\n",
                    name, w->name);
#endif
            return (-1);
        }

	for (i=0; i < w->count; i++) {
                b = w->b[i];
                if ( b->type == OUTPUT_BLOCK || b->type == OUTPUT_TABLE )
                        continue;
                offset = 0;
                for (k=0; k < b->n_rec; k++) {
                        if ( STRCMP(b->s[k]->name, name) == 0 ) {
                                n = b->offset+offset;
                                l = b->s[k]->len;

                                if ( n+l <= w->datalen ) {
                                        strncpy(dst, &w->data[n], l);
                                        dst[l] = '\0';
                                }
                                else
                                        strcpy(dst, "?");
#ifdef CHOI
				ptr = STRDUP(dst);
                                /*
                                ** Format Conversion
                                */
                                if ( format_conversion(0, &ptr, b->s[k]->name, b->s[k]->len,
                                        b->s[k]->conv, NULL) == 0 ) {
                                        strcpy(dst, ptr);
                                }
								SAFE_FREE(ptr);
#endif
                                return (0);
                        }
                        offset += b->s[k]->len;
                }
        }
        /*
        logging(stdout, "Specified var %s not found in aformat %s\n", name, w->name);
        */

        return (-1);
}


/****************************************************************************
 ** get_avaluep - Get the value of a named field in the aformat_t
 **/
int get_avaluep(aformat_t *w, int bindex, int rindex, int lindex, char* dst)
{
        int  n, l;
        ablock_t *b;

        assert(w);

        strcpy(dst, " ");

        if ( !w->data )
                return (-1);

        if ( bindex >= w->count )
                return (-1);

        b = w->b[bindex];

        if ( rindex >= b->n_rec || lindex >= b->n_dat )
                return (-1);

        if ( b->type == OUTPUT_BLOCK || b->type == OUTPUT_TABLE ) {
                n = b->offset + b->len*lindex + b->s[rindex]->offset;
                l = b->s[rindex]->len;
                if ( n+l <= w->datalen ) {
                        strncpy(dst, &w->data[n], l);
                        dst[l] = '\0';
                }
                else
                        strcpy(dst, "?");
        }
        else {
                n = b->offset + b->s[rindex]->offset;
                l = b->s[rindex]->len;
                if ( n+l <= w->datalen ) {
                        strncpy(dst, &w->data[n], l);
                        dst[l] = '\0';
                }
                else
                        strcpy(dst, "?");
        }

#ifdef CHOI
	ptr = strdup(dst);
        if ( format_conversion(0, &ptr, b->s[rindex]->name, b->s[rindex]->len,
                b->s[rindex]->conv, NULL) == 0 ) {
                strcpy(dst, ptr);
        }
		SAFE_FREE(ptr);
#endif

        return (0);
}


/****************************************************************************
 ** get_avalue0 - Get the value of a named field in the aformat_t
 **/
int get_avalue0(aformat_t *w, char* name, char* dst)
{
        int bindex, rindex;

        assert(dst);

        strcpy(dst, " ");

        if ( !w || !w->data )
                return (-1);

        if ( (rindex=find_afieldp(w, name, &bindex)) != -1 ) {
                if ( get_avaluep(w, bindex, rindex, 0, dst) == 0 )
                        return(0);
        }
        return(-1);
}

/****************************************************************************
 ** assoc_qformat
 **
 ** This function associates the qformat_t data to a byte stream.
 ** The byte stream will be sent to the remote bank host when a
 ** transaction is requested.
 ** Argument : qformat_t *w;  // query message format with values
 **            char* delimiter;
 ** Return   : length of byte stream if successful, -1 o/w
 **/
#if 0
int assoc_qformat(qformat_t *w, char* delimiter)
{
        int i, offset=0, delim_len;
        char *buf, *value;

        if ( !w )
                return (-1);

        buf = (char*)calloc(MAX_QMSGSTREAM_LEN, sizeof(char));
        if ( !buf ) {
#ifdef CHOI
                logging(stderr, "Unable allocate memory in assoc_qformat\n");
#endif
                return(-1);
        }

	if ( delimiter && strlen(delimiter) > 0 )
                delim_len = strlen(delimiter);
        else
                delim_len = 0;

        if ( delim_len > 0 ) {
            for (i=0; i < w->count; i++) {
                value = VALUE(w->s[i]->value);
                snprintf(buf+offset, sizeof(buf+offset), "%s%s", value, delimiter);
                offset += (strlen(value) + delim_len);
            }
            buf[offset] = '\0';
        }
        else {
            for (i=0; i < w->count; i++) {
                value = VALUE(w->s[i]->value);
                memset(buf+offset, ' ', w->s[i]->len);
                memcpy(buf+offset, value,FQ_MIN(strlen(value),w->s[i]->len));
                offset += w->s[i]->len;
            }
            buf[offset] = '\0';
        }

	/*-----jssong------------------------------------------*/
        /* ASCII mode transmission only in this version. */
        /*-----jssong------------------------------------------*/

		// if(w->data) free(w->data); // here: gwisang.choi 2011/09/06 for looping.
		SAFE_FREE(w->data);

        w->data = STRDUP(buf);
        w->datalen = offset;
		SAFE_FREE(buf);
        return (offset);
}
#else

int assoc_qformat(qformat_t *w, char* delimiter)
{
	int i, offset=0, delim_len;
	char *buf, *value;

	if ( !w )
			return (-1);

	buf = (char*)calloc(w->length+1, sizeof(char));
	if ( !buf ) {
			return(-1);
	}

	if ( delimiter && strlen(delimiter) > 0 )
                delim_len = strlen(delimiter);
        else
                delim_len = 0;

        if ( delim_len > 0 ) {
            for (i=0; i < w->count; i++) {
                value = VALUE(w->s[i]->value);
                snprintf(buf+offset, sizeof(buf+offset), "%s%s", value, delimiter);
                offset += (strlen(value) + delim_len);
            }
            buf[offset] = '\0';
        }
        else {
            for (i=0; i < w->count; i++) {
                value = VALUE(w->s[i]->value);
                memset(buf+offset, ' ', w->s[i]->len);
                memcpy(buf+offset, value,FQ_MIN(strlen(value),w->s[i]->len));
                offset += w->s[i]->len;
            }
            buf[offset] = '\0';
        }

		SAFE_FREE(w->data);

        w->data = STRDUP(buf);
        w->datalen = offset;
		SAFE_FREE(buf);
        return (offset);
}
#endif

/****************************************************************************
 ** save_qvalue
 ** Save the field values of 'S' or 'L' type to the given cache
 ** Argument : qformat_t *w;            // query message format with values
 **            cache_t* cache_short;    // cache to save short term value
 **            cache_t* cache_long;     // cache to save long term value
 **/
void save_qvalue(qformat_t* w, cache_t* cache_short, cache_t* cache_long)
{

        if ( !w )
                return;

#ifdef CHOI
        for (i=0; i < w->count; i++) {
                if ( w->s[i]->save == SAVE_S && cache_short ) {
                        cache_update(cache_short,w->s[i]->name,w->s[i]->value,0);
                }
                else if ( w->s[i]->save == SAVE_L && cache_long ) {
                        cache_update(cache_long, w->s[i]->name,w->s[i]->value,0);
                }
	}
#endif
}

/****************************************************************************
 ** save_avalue
 ** Save the field values of 'S' or 'L' type to the given cache
 ** Argument : aformat_t *w;     // response message format with values
 **            cache_t* cache_short;    // cache to save short term value
 **            cache_t* cache_long;     // cache to save long term value
 **/
void save_avalue(aformat_t* w, cache_t* cache_short, cache_t* cache_long)
{
#if 0
	char    *str;

	if ( !w ) 	
		return;

	for (i=0; i < w->count; i++) {
		b = w->b[i];
		if ( b->type == OUTPUT_BLOCK || b->type == OUTPUT_TABLE )
			continue;

		for (j=0; j < b->n_rec; j++) {
			if ( b->s[j]->save == SAVE_S && cache_short ) {
				cache_update(cache_short, b->s[j]->name, b->s[j]->value, 0);
			}
			else if ( b->s[j]->save == SAVE_L && cache_long ) {
				cache_update(cache_long, b->s[j]->name, b->s[j]->value, 0);
			}
		}
	}
#endif
}

/****************************************************************************
 ** assoc_aformat
 **
 ** This function associates the byte stream which received from
 ** the bank host resulted in a transaction, with the corresponding
 ** aformat TCI format.
 ** Argument : aformat_t *w;   // pointer to aformat_t
 ** Return   : 0 if successful, -1 o/w
 **/
int assoc_aformat(aformat_t *w, char** ptr_errmesg)
{
	char szErrMsg[1024];
	int i, j, k, offset=0;
	int union_size = 0;
	char buf[MAX_FIELD_SIZE+1];
	ablock_t *b;

	if ( !w->data ) {
			snprintf( szErrMsg, sizeof(szErrMsg),  "Empty received data. [%s][%d].", __FILE__, __LINE__ );
			*ptr_errmesg  = (char *)strdup(szErrMsg);
			return (-1);
	}

	for (i=0; i < w->count; i++) {
		b = w->b[i];

		if ( i > 0 && w->b[i-1]->type == OUTPUT_UNION && w->b[i]->type != OUTPUT_UNION ) {
			offset += union_size;
			union_size = 0;
		}

		b->offset = offset;

		/*
		** Set repetition counter and max counter if necessary
		*/
		if ( b->n_dat_var[0] ) {
			/* 990317 -- wild length support */
			if ( STRCMP(b->n_dat_var, "*") == 0 ) {
					if ( i != w->count-1 )
							return(-1);
					b->n_dat = (w->datalen - offset)/b->len;
#ifdef CHOI
					logging(1, "** blen=%d *** n_dat=%d ** datalen=%d ** offset=%d **\n",
							b->len, b->n_dat, w->datalen, offset);
#endif
					continue;
			}
			/* 990317 -- wild length support */
			if ( get_avalue(w, b->n_dat_var, buf) < 0 ) {
					snprintf( szErrMsg, sizeof(szErrMsg),  "Not found field[%s] from received data. [%s][%d].",
									b->n_dat_var, __FILE__, __LINE__ );
					*ptr_errmesg  = (char *)strdup(szErrMsg);
					return (-1);
			}
			b->n_dat = atoi(buf);
		}
		if ( b->n_max < 0 )
			b->n_max = b->n_dat;


		/*
		** Increase offset if necessary
		*/
		if ( b->type == OUTPUT_UNION )
			union_size = FQ_MAX(union_size, b->len);
		else
			offset += (b->n_max * b->len);
		/*
		** Set offset value for individual records of the block
		** note that b->s[0]->offset = 0 and b->len already computed.
		*/
		for (j=0, k=0; j < b->n_rec; j++) {
			b->s[j]->offset = k;
			k += b->s[j]->len;
		}
	}

	/*
	** Calculate length of the aformat data
	*/

	w->length = 0;
	for (i=0; i < w->count; i++) {
		b = w->b[i];
		if ( !(i > 0 && w->b[i-1]->type == OUTPUT_UNION) )
			w->length += (b->len * b->n_dat);
	}
	if ( w->datalen < w->length ) {
		char* ptr;

		ptr = (char*)calloc(w->length, sizeof(char));
		memset(ptr, ' ', w->length);
		memcpy(ptr, w->data, w->datalen);
		SAFE_FREE(w->data);
		w->data = ptr;
		w->datalen = w->length;
	}


	/*
	** fill the values with data according to computed offset
	*/
	for (i=0; i < w->count; i++) {
		b = w->b[i];
		for (j=0, k=0; j < b->n_rec; j++) {
			strncpy(buf, w->data + b->offset + b->s[j]->offset, b->s[j]->len);
			buf[b->s[j]->len] = '\0';
			str_trim(buf);
			SAFE_FREE(b->s[j]->value);
			b->s[j]->value = strdup(buf);

			/* We put all data into short cache */
			// cache_update(w->cache_short, b->s[j]->name, b->s[j]->value, 0);
		}
	}

	// make_postdata(w);
	// make_json(w);

	/*
	** byte stream size check - We allow underflow (V2.4)
	*/
	/*******************************
	if ( w->datalen < w->length ) {
		logging(stdout, "WARNING: Size of response message underflow (%d:%d)\n", w->datalen, w->length);
		return (-1);
	}
	********************************/

	return (0);
}

/******************************************************************************
 ** find_atable
 ** This function finds a table block in the aformat
 ** Return: index of the block if successful, -1 o/w.
 **/
int find_atable(aformat_t *w)
{
        int i;
        for (i=0; i < w->count; i++) {
                if ( w->b[i]->type == OUTPUT_TABLE )
                        return (i);
        }
        return (-1);
}

/******************************************************************************
 ** find_ablock
 ** This function finds a named block in the aformat
 ** Return: index of the block if successful, -1 o/w.
 **/
int find_ablock(aformat_t *w, char *name)
{
        int i;
        for (i=0; i < w->count; i++) {
                if ( w->b[i]->type == OUTPUT_BLOCK &&
                     STRCMP(w->b[i]->name, name) == 0)
                        return (i);
        }
        return (-1);
}

/******************************************************************************
 ** find_afield
 ** This function finds a named field in the specified block of aformat
 ** Return: index of the field if successful, -1 o/w.
 **/
int find_afield(aformat_t *w, int block_index, char *name)
{
        int i;
        for (i=0; i < w->b[block_index]->n_rec; i++) {
                if ( STRCMP(w->b[block_index]->s[i]->name, name) == 0)
                        return (i);
        }
        return (-1);
}

/******************************************************************************
 ** find_afieldp
 ** This function finds a named field in the OUTPUT and OUTPUT_P block
 ** Return: index of the field if successful, -1 o/w.
 **/
int find_afieldp(aformat_t *w, char *name, int* block_index)
{
        int i, k;
        for (k=0; k < w->count; k++) {
                if ( !(w->b[k]->type == OUTPUT || w->b[k]->type == OUTPUT_P) )
                        continue;
                for (i=0; i < w->b[k]->n_rec; i++) {
                        if ( STRCMP(w->b[k]->s[i]->name, name) == 0 ) {
                                *block_index = k;
                                return (i);
                        }
                }
        }
        return (-1);
}

/******************************************************************************
 ** find_qfield
 ** This function finds a named field in the specified block of qformat
 ** Return: index of the field if successful, -1 o/w.
 **/
int find_qfield(qformat_t *w, char *name)
{
        int i;
        for (i=0; i < w->count; i++) {
                if ( STRCMP(w->s[i]->name, name) == 0)
                        return (i);
        }
        return (-1);
}

/******************************************************************************
 ** get_msgcode
 ** This function finds the value of AMSG_CODE from the first block of an
 ** aformat with the given byte stream.
 ** Return: 0 if successful, -1 if fail
 **/
int get_msgcode(aformat_t *w, int index, int size, char* dst, char** ptr_errmesg)
{
        int     rc = -1;
        int i, j, k=0, offset=0, block=0, len;
        char    szErrMsg[1024];

        j = index;

        if ( j < 0 ) {
                snprintf(szErrMsg, sizeof(szErrMsg),  "get_msgcode:msgcode index is bad.[%s][%d].", __FILE__, __LINE__);
                goto L0;
        }
        if( j > w->b[0]->n_rec ) {
                snprintf(szErrMsg,  sizeof(szErrMsg), "get_msgcode: msgcode index is big.[%s][%d].", __FILE__, __LINE__);
                goto L0;
        }

        for (i=0; i < j; i++) {
                if ( k == w->b[block]->n_rec ) {
                        block++;
                        k = 0;
                }
                offset += w->b[block]->s[k]->len;
                k++;
        }

        len = size;

        if ( len < 0 ) {
                if ( k >= w->b[block]->n_rec )
                        len = w->b[++block]->s[0]->len;
                else
                        len = w->b[block]->s[k]->len;
        }

        if ( offset+len <= w->datalen ) {
                strncpy(dst, w->data+offset, len);
                *(dst+len) = '\0';
                rc = 0;
        }
	else {
                snprintf(szErrMsg,  sizeof(szErrMsg),
			 "get_errcode:errcode index is bad or received_data.[%s][%d].", __FILE__, __LINE__);
                goto L0;
        }

L0:
        if( rc < 0 ) {
                *ptr_errmesg = (char *)strdup(szErrMsg);
        }

        return (rc);
}

/******************************************************************************
 ** get_errcode
 ** This function finds the value of ERR_CODE from the first block of an
 ** aformat with the given byte stream.
 ** Return: 0 if successful, -1 if fail
 **/
int get_errcode(aformat_t *w, int index, int size, char* dst, char** ptr_errmesg)
{
        int     rc = -1;
        int i, j, len, offset=0;
        char    szErrMsg[1024];


        j = index;

        if( j < 0 ) {
                snprintf(szErrMsg, sizeof(szErrMsg),  "get_errcode:errcode index is bad.[%s][%d].", __FILE__, __LINE__);
                goto L0;
        }

        if( j > w->b[0]->n_rec-1 ) {
                snprintf(szErrMsg,  sizeof(szErrMsg), 
			"get_errcode:errcode index is too big.[%s][%d].", __FILE__, __LINE__);
                goto L0;
        }

        for (i=0; i < j; i++)
                offset += w->b[0]->s[i]->len;

        if ( (len = size) < 0 )
                len = w->b[0]->s[j]->len;

        if ( offset+len <= w->datalen ) {
                strncpy(dst, w->data+offset, len);
                *(dst+len) = '\0';
                rc = 0;
        }
        else {
                snprintf(szErrMsg, sizeof(szErrMsg),  "get_errcode:errcode index is bad.[%s][%d].", __FILE__, __LINE__);
                goto L0;
        }

	L0:
        if( rc < 0 ) {
                *ptr_errmesg = (char *)strdup(szErrMsg);
        }

        return (rc);
}
	
/****************************************************************************
 ** save_ablock
 ** Save the field values in a block to the given cache
 ** Argument : aformat_t *w;     // response message format with values
 **            container_t* cache_option; // cache containter
 **/
void save_ablock(aformat_t* w, container_t* cache_option)
{
#ifdef CHOI
        register int i, j, k, offset=0;
        ablock_t *b;
        char *str, buf[MAX_FIELD_SIZE];

        if ( !w )
                return;

        for (i=0; i < w->count; i++) {
                b = w->b[i];
                if ( !(b->type == OUTPUT_BLOCK || b->type == OUTPUT_TABLE) )
                        continue;

                for (k=0; k < b->n_rec; k++) {
                        if ( !(b->s[k]->save == SAVE_L || b->s[k]->save == SAVE_LA ||
                               b->s[k]->save == SAVE_S || b->s[k]->save == SAVE_SA) )
                                continue;

                        if ( b->s[k]->save == SAVE_LA || b->s[k]->save == SAVE_SA )
                                container_delkey(cache_option, b->s[k]->name);

                        container_add(cache_option, b->s[k]->save, b->s[k]->name);

                        offset = b->offset + b->s[k]->offset;

			 for (j=0; j < b->n_dat; j++) {
                                if ( offset+b->s[k]->len <= w->datalen ) {
                                        strncpy(buf, w->data+offset, b->s[k]->len);
                                        buf[b->s[k]->len] = '\0';
                                }
                                else
                                        strcpy(buf, "");

                                str = STRDUP(buf);
                                (void)format_conversion(0, &str, b->s[0]->name, b->s[0]->len,
                                        b->s[k]->conv, NULL);

                                if ( b->s[k]->save == SAVE_LA || b->s[k]->save == SAVE_SA )
                                        container_putitem(cache_option, b->s[k]->name, str, NULL, 0);
                                else
                                        container_additem(cache_option, b->s[k]->name, str, NULL, 0);

								SAFE_FREE(str);

                                offset += b->len;
                        }
                }
        }
#endif
}

int get_errmsg(char *errcode, char *dst, char** ptr_errmesg)
{
#ifdef CHOI
	char buf[MAX_FIELD_SIZE];
	char    szErrMsg[1024];
	char    *errmesg = NULL;

	assert(errcode);
	assert(dst);

	*dst = '\0';
	*ptr_errmesg = (char *)strdup("백엔드오류코드");

	if ( get_codeval(_errcodemap, errcode, buf, &errmesg) == 0 ) {
			strcpy(dst, buf);
			SAFE_FREE(errmesg);
			return(0);
	}

	snprintf(szErrMsg, sizeof(szErrMsg), "In errcodmap, Can't found errcode[%s]. cause: [%s] [%s][%d]",
			errcode, errmesg, __FILE__, __LINE__);
	*ptr_errmesg = (char *)strdup(szErrMsg);

	SAFE_FREE(errmsg)l

	return(-1);
#endif
	return(0);

}

/*
** Check length
*/
static int check_length(int code, char* src, char* dst, int len)
{
	assert(src);
	assert(dst);

	strcpy(dst, src);
	if ( code >= CHECK_LENGTH_FROM && code <= CHECK_LENGTH_TO ) {
		if ( strlen(src) == code ) 
			return(0);
		return (-code);
	}
	else if ( code == CHECK_FIELD_LENGTH ) {
		if ( strlen(src) == len ) 
			return(0);
		return (-code);
	}
	return(-code);
}

/*
** character type checking
*/
static int check_chars(int code, char* src, char* dst, int len)
{
	char *p = src;
	register int i;


	assert(src);
	assert(dst);

	strcpy(dst, src);
	switch ( code ) {
		case CHECK_NUMERIC_ONLY:
			while ( *p ) {
				if (!isdigit(*p)) 
					return (-code);
				p++;
			}
			return(0);

		case CHECK_ALPHA_ONLY:
			while ( *p ) {
				if ( !isalpha(*p) )
					return(-code);
				p++;
			}
			return(0);
		default:
			return(-code);
	}
}
/*
** Fill the destination with given characters
*/
static int fill_chars(int code, char* src, char* dst, int len)
{
	char *p = src;
	char *q = dst;
	char ch;
	register int i, k;

	assert(src);
	assert(dst);

	switch ( code ) {
		
	case FILL_SPACE: 
		for (i=0; i < len; i++, q++)
			*q = ' ';
		*q = '\0';
		return(0);
	
	case FILL_SPACE_LEFT: 
		if ( (k = strlen(src)) > len )
			return(-code);
		for (i=0; i < len-k; i++, q++)
			*q = ' ';
		for (; i < len; i++, p++, q++)
			*q = *p;
		*q = '\0';
		return(0);
	
	case FILL_SPACE_RIGHT: 
		if ( (k = strlen(src)) > len )
			return(-code);
		for (i=0; i < k; i++, p++, q++)
			*q = *p;
		for (; i < len; i++, q++)
			*q = ' ';
		*q = '\0';
		return(0);

	case FILL_ZERO_LEFT: 
		if ( (k = strlen(src)) > len )
			return(-code);
		for (i=0; i < len-k; i++, q++)
			*q = '0';
		for (; i < len; i++, p++, q++)
			*q = *p;
		*q = '\0';
		return(0);
	case DISPLAY_PWD: 
		for (i=0; i < len; i++, q++)
			*q = '*';
		*q = '\0';
		return(0);

	default:
		return(-code);

	}
}
/*
** Delete characters
*/
static int del_chars(int code, char* src, char* dst, int len)
{
	char *p = src;
	char *q = dst;
	char ch;
	register int i, j, k;

	assert(src); 
	assert(dst); 

	switch ( code ) { 
	case DEL_SPACE: 
	case DEL_COMMA: 
		switch ( code ) {
			case DEL_SPACE:  ch = ' '; break;
			case DEL_COMMA:  ch = ','; break;
		}
		k = strlen(p);
		for (i=0; i < k; i++, p++) {
			if ( *p == ch )
				continue;
			else {
				*q = *p; q++;
			}
		}
		*q = '\0';
		return(0);
		k = strlen(p);
		p += 2;
		for( i=0; i < (k-2); i++ ) {
			*q = *p;
			q++;
			p++;
		}
		*q = '\0';
		return(0);
	default:
		return(-code);
	}
}

/*
** Change characters
*/
static int change_chars(int code, char* src, char* dst, int len)
{
	char *p = src;
	char *q = dst;
	char ch, ch2;
	register int i, j, k;

	ASSERT(src);
	ASSERT(dst);

	switch ( code ) {
		case CHG_PLUS_TO_SPACE:  	ch = '+'; ch2 = ' '; break;
		case CHG_ZERO_TO_SPACE:    	ch = '0'; ch2 = ' '; break;
	}

	k = strlen(p);
	for (i=0; i < k; i++, p++, q++) {
		if ( *p == ch )
			*q = ch2;
		else {
			*q = *p;
		}
	}
	*q = '\0';
	return(0);
}

static int add_string(int code, char* src, char* dst, int len)
{
	char *p = src;
	char *q = dst;
	char add_str[10];

	assert(src);
	assert(dst);

	*dst = '\0';
	switch ( code ) {
	case ADD_HAN_WON:	strcpy(add_str, "원"); break;
	case ADD_HAN_DOLLOR:	strcpy(add_str, "달러"); break;
	case ADD_CHAR_DOLLOR:	strcpy(add_str, "＄"); break;
	case ADD_CHAR_PERCENT:  strcpy(add_str, "%"); break;
#if 0
	case ADD_HAN_IL:	strcpy(add_str, "일"); break;
	case ADD_HAN_GUN:	strcpy(add_str, "건"); break;
	case ADD_HAN_HOI:	strcpy(add_str, "회"); break;
	case ADD_HAN_GOA:	strcpy(add_str, "좌"); break;
	case ADD_HAN_GYEWOL:	strcpy(add_str, "개월"); break;
	case ADD_HAN_NYEN:	strcpy(add_str, "년"); break;
	case ADD_HAN_KANG:	strcpy(add_str, "강"); break;
	case ADD_HAN_KA:	strcpy(add_str, "가"); break;
	case ADD_CHAR_YEN:	strcpy(add_str, "￥"); break;
	case ADD_CHAR_WON:	strcpy(add_str, "￦"); break;
	case ADD_HAN_MANWON:	strcpy(add_str, "만원"); break;
#endif
	}

	strcpy(dst, src);
	strcat(dst, add_str);
	return(0);
}

static int ins_chars_to_date(int code, char* src, char* dst, int len)
{
	char *p = src;
	char *q = dst;
	char ch;
	register int i;

	assert(src);
	assert(dst);

	switch ( code ) {
		case INS_SLASH_TO_DATE: 	ch = '/'; break;
		case INS_HYPHEN_TO_DATE: 	ch = '-'; break;
	}

	strcpy(dst, src);

	if ( len > 4 ) {
		for (i=0; i < len-4; i++, p++, q++) 
			*q = *p;
		*q = ch; q++;
	}

	for (i=0; i < 2; i++, p++, q++) 
		*q = *p;
	*q = ch; q++;
	for (i=0; i < 2; i++, p++, q++) 
		*q = *p;
	*q = '\0';

	return(0);
}

static int to_upper(int code, char* src, char* dst, int len)
{
	char* p = src;
	char* q = dst;
	register int i;

	assert(src);
	assert(dst);

	for (i=0; i < strlen(src); i++, p++, q++)
		*q = toupper(*p);
	*q = '\0';
	return (0);
}

static int to_lower(int code, char* src, char* dst, int len)
{
	char* p = src;
	char* q = dst;
	register int i;

	assert(src);
	assert(dst);

	for (i=0; i < strlen(src); i++, p++, q++)
		*q = tolower(*p);
	*q = '\0';
	return (0);
}

static int to_number(int code, char* src, char* dst, int len)
{
	long long ll;

	assert(src);
	assert(dst);

	ll = strtoll(src, NULL, 10);
	sprintf(dst, "%lld", ll);
	return(0);
}

/*
** fit characters
*/
static int fit_chars(int code, char* src, char* dst, int len)
{
	char *p = src;
	char *q = dst;

	assert(src);
	assert(dst);

	switch(code) {
	case FIT_LEFT:
		strcpy(q, p);
		q[len] = '\0';
		return(0);
		
	default:
		return(-code);
	}
}

void save_aformat_to_cache(aformat_t *w)
{
		int i;

        assert(w);

		w->cache_short = cache_new('S', w->name);
	
        for (i=0; i < w->count; i++) {
				ablock_t *b;
				int k;

                b = w->b[i];

#if 0
                snprintf(tmpbuf, sizeof(tmpbuf), "Block %d: %s %s %d %d %d %d(%s)\n",
                        i, 
						VALUE(get_tcikey(b->type)),
                        VALUE((char *)b->name), 
						VALUE((char *)b->n_rec), 
						b->len, 
						b->n_max,
                        b->n_dat, 
						VALUE(b->n_dat_var));
#endif

                /* n_rec : number of fields in the block */
                for (k=0; k < b->n_rec; k++) {
                        if ( !b->s[k]->value ) {
								cache_update(w->cache_short, VALUE(b->s[k]->name), "",0);							
                        } 
						else {
								cache_update(w->cache_short, VALUE(b->s[k]->name), b->s[k]->value ,0);
						}
                }
        }
}

/*
** Warning : You must free a return pointer(aformat_t) after using.
*/
aformat_t *parsing_stream_message( fq_logger_t *l, conf_obj_t *cfg, codemap_t *svc_code_map,  const char *msg, int msglen)
{
	int rc;
	int svc_code_offset, svc_code_length;
	char *svc_code = NULL;
	char aformat_filename[256];
	aformat_t	*a = NULL;
	char	*errmsg = NULL;

	FQ_TRACE_ENTER(l);

	if( !svc_code_map ) {
		fq_log(l, FQ_LOG_ERROR, "There is no a value in svc_code_map parameter.");
		goto stop;
	}

	svc_code_offset = cfg->on_get_int(cfg, "SVC_CODE_OFFSET");
	svc_code_length = cfg->on_get_int(cfg, "SVC_CODE_LENGTH");

	/* get a service code from message */	
	svc_code = calloc(svc_code_length+1, sizeof(char));
	memcpy(svc_code, &msg[svc_code_offset], svc_code_length);
	svc_code[svc_code_length] = 0x00;

	fq_log(l, FQ_LOG_DEBUG, "service code = [%s]\n", svc_code );

	/* get a format filename from service_code_map with service_code */
	rc = get_codeval(l, svc_code_map, svc_code, aformat_filename);
	if( rc != 0 ) {
		fq_log(l, FQ_LOG_ERROR, "service code error.! Can't find a aformat filename in service_code_map.");
		goto stop;
	}
	fq_log(l, FQ_LOG_DEBUG, "aformat_filename = [%s]\n", aformat_filename );

	char aformat_file_path[256];
	svc_code_length = cfg->on_get_str(cfg, "FORMAT_FILE_PATH", aformat_file_path);
	if( !HASVALUE(aformat_file_path)) {
		fq_log(l, FQ_LOG_ERROR, "Can't find (FORMAT_FILE_PATH)path of a aformat filename in your config.");
		goto stop;
	}
	char aformat_path_filename[1024];
	sprintf(aformat_path_filename, "%s/%s", aformat_file_path, aformat_filename);

	/* memory allocation for aformat read */
	a = new_aformat(aformat_path_filename, &errmsg);
	if( !a ) {
		fq_log(l, FQ_LOG_ERROR, "new_aformat('%s') error. reason=[%s].", aformat_path_filename, errmsg);
		goto stop;
	}

	/* make a template with a aformat file */
	rc = read_aformat(a, aformat_path_filename, &errmsg);
	if( rc < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "read_aformat('%s') error. reason=[%s].", aformat_filename, errmsg);
		goto stop;
	}
	
	a->data = strdup(msg);
	a->datalen = msglen;

	/* fill values to aformat_t ( each field and make postdata type. */
	rc = assoc_aformat(a, &errmsg);
	if( rc != 0 ) {
		fq_log(l, FQ_LOG_ERROR, "assoc_aformat('%s') error. reason=[%s].", aformat_filename, errmsg);
		goto stop;
	}

	save_aformat_to_cache(a);
	
	SAFE_FREE(svc_code);
	SAFE_FREE(errmsg);

	FQ_TRACE_EXIT(l);
	
	return(a);
stop:
	SAFE_FREE(svc_code);
	SAFE_FREE(errmsg);
	
	if(a) free_aformat(&a);

	FQ_TRACE_EXIT(l);

	return(NULL);
}

void print_aformat_stdout(aformat_t *w)
{
	char    tmpbuf[1024*10];
	char    tmpbuf2[1024*10];
	int 	i;

	assert(w);
	memset(tmpbuf, '\0', sizeof(tmpbuf));
	memset(tmpbuf2, '\0', sizeof(tmpbuf2));

	printf("TCI parsing result ...\n");
	printf("name: %s \n", w->name);
	printf("count: %d \n", w->count);
	printf("length: %d \n",  w->length);
	for (i=0; i < w->count; i++) {
		int k;
		ablock_t *b;	

		b = w->b[i];

		snprintf(tmpbuf, sizeof(tmpbuf), "Block %d: %s %s %d %d %d %d(%s)\n",
				i, VALUE(get_tcikey(b->type)),
				VALUE((char *)b->name), b->n_rec, b->len, b->n_max,
				b->n_dat, VALUE(b->n_dat_var));

		printf( "%s\n", tmpbuf);

		/* n_rec : number of fields in the block */
		for (k=0; k < b->n_rec; k++) {
				char val[2048];
				int j;

				if ( !b->s[k]->value )
						strcpy(val, "");
				else if (b->s[k]->type == TYPE_PID) {
						j = strlen(VALUE(b->s[k]->value));
						memset(val, '*', j);
						val[j] = '\0';
				} else  {
					strcpy(val, b->s[k]->value);
				}

				snprintf( tmpbuf, sizeof(tmpbuf), "\t%s: %d %s %s [%s] %s [%s]\n",
						VALUE(b->s[k]->name),
						b->s[k]->len,
						VALUE(get_tcikey(b->s[k]->type)),
						VALUE(get_tcikey(b->s[k]->save)),
						VALUE(b->s[k]->conv),
						VALUE(get_tcikey(b->s[k]->dspl)),
						VALUE(val));
				printf( "%s\n" , tmpbuf);
		}
	}
}
