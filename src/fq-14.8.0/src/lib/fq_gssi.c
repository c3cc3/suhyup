/*
** fq_gssi.c
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#include "fq_mem.h"
#include "fq_tokenizer.h"
#include "fq_gssi.h"
#include "fq_common.h"
int gssi_getarg(mem_obj_t *m, char* dst);

gssi_tag_type_t gssi_tags[NUM_GSSI_TAGS] = {
	// { "º¯¼ö0", 0},  /* 1 */
    { "var1", 0},  /* 1 */
    { "var2", 0},  /* 2 */
    { "var3", 0},  /* 4 */
    { "var4", 0},  /* 4 */
    { "var5", 0},  /* 5 */
    { "var6", 0},  /* 6 */
    { "var7", 0},  /* 7 */
    { "var8", 0},  /* 8 */
    { "var9", 0},  /* 9 */
    { "var10", 0},  /* 10 */
    { "var11", 0},  /* 11 */
    { "var12", 0},  /* 12 */
    { "var13", 0},  /* 13 */
    { "var14", 0},  /* 14 */
    { "var15", 0},  /* 15 */
    { "var16", 0},  /* 16 */
    { "var17", 0},  /* 17 */
    { "var18", 0},  /* 18 */
    { "var19", 0},  /* 19 */
    { "var20", 0},  /* 20 */
    { "var21", 0},  /* 21 */
    { "var22", 0},  /* 22 */
    { "var23", 0},  /* 23 */
    { "var24", 0},  /* 24 */
    { "var25", 0},  /* 25 */
    { "var26", 0},  /* 26 */
    { "var27", 0},  /* 27 */
    { "var28", 0},  /* 28 */
    { "var29", 0},  /* 29 */
    { "var30", 0},  /* 30 */
    { "var31", 0},  /* 31 */
    { "var32", 0},  /* 32 */
    { "var33", 0},  /* 33 */
    { "var34", 0},  /* 34 */
    { "var35", 0},  /* 35 */
    { "var36", 0},  /* 36 */
    { "var37", 0},  /* 37 */
    { "var38", 0},  /* 38 */
    { "var39", 0},  /* 39 */
    { "var40", 0},  /* 40 */
    { "var41", 0},  /* 41 */
    { "var42", 0},  /* 42 */
    { "var43", 0},  /* 43 */
    { "var44", 0},  /* 44 */
    { "var45", 0},  /* 45 */
    { "var46", 0},  /* 46 */
    { "var47", 0},  /* 47 */
    { "var48", 0},  /* 48 */
    { "var49", 0},  /* 49 */
    { "var50", 0},  /* 50 */
	{ "name", 1},  /* 51 */ /* reserved word */
	{ "button", 1},  /* 52 */ /* reserved word */
	{ "address", 1},  /* 53 */ /* reserved word */
	{ "part", 1},  /* 54 */ /* reserved word */
	{ "date", 1},  /* 55 */ /* reserved word */
	{ "time", 1},  /* 56 */ /* reserved word */
	{ "system", 1},  /* 57 */ /* reserved word */
	{ "IPadress", 1},  /* 58 */ /* reserved word */
	{ "email", 1},  /* 59 */ /* reserved word */
	{ "CODE", 1},    /* 60 */ /* reserved word */
	{ "cache", 2},  /* 61 */
	{ "jsonarray", 3}
};

static int get_tag_index_and_type( mem_obj_t *m , char *dst, int *tag_type)
{
	int len;
	char c;
	int i=0;
	int loop_cnt=0;
	
	for(len=0; len < MAX_TAG_SIZE; len++) {
		loop_cnt++;
		if( (c=m->mgetc(m)) == 0) {
			return(0);
		}
		if( isspace(c) ) break;

		if( c == '%' ) {
			break;
		}
		dst[len] = c;
	}
	dst[len] = 0x00;

	if( len == 0 || len == MAX_TAG_SIZE ) {
		return(-loop_cnt);
	}
	
	for (i=0; i < NUM_GSSI_TAGS ; i++) {
		if ( strncmp(gssi_tags[i].tag_name, dst, strlen(dst)) == 0 ) {
			*tag_type = gssi_tags[i].tag_type;
			return(i);
		}
	}
	return(-loop_cnt);
}

int gssi_get_item(const char *str, const char *key, char *dst, int len)
{
	int i,j=0,length,len_key;
	char *p, *q;

	assert(dst);

	dst[0] = '\0';
	if ( !str || !key )
			return (-1);

	length  = strlen(str);
	len_key = strlen(key);

	for(i = 0 ; i < length+1 ; i++) {
	  if(str[i] == '&' || str[i] == 0 ) {
		if(!strncasecmp(str+j, key, len_key) && str[j+len_key] == '='){
			p = (char *)strchr(str+j, '=');
			if(p == NULL) return (-1);
			q = (char *)strchr(p+1,'&');
			if(q == NULL){
				strncpy(dst, p+1, FQ_MIN(str+length-p, len-1));
				dst[FQ_MIN(str+length-p,len-1)] = '\0';;
			}
			else {
				strncpy(dst,p+1,FQ_MIN(q-p-1, len-1));
				dst[FQ_MIN(q-p-1, len-1)] = '\0';;
			}
			return (0);
		}
		j = i+1;
	  }
	}
	return (-1);
}


int
gssi_var_recursive(mem_obj_t *m, char* dst)
{
    char tag[128], arg[128], buf[128];
    int rc, tag_type, tagid;

    *dst = '\0';
	tagid = get_tag_index_and_type( m, tag , &tag_type);
	if( tag_type != 2 ) {
		return(-1);
	}
	else {
        if ( (rc = gssi_getarg(m, arg)) == 0 ) {
			sprintf(dst, "%s", "AAA");
            // rc = gssi_variable(gssi, TAG_VAR, arg, dst);
            return(rc);
        }
		else  {
			return(-1);
		}
	}
		
}
int
gssi_getarg(mem_obj_t *m, char* dst)
{
    char ch, ch1='\0';
    char *arg;
	char *buf;
    char *ptr = dst;
    int rc;

    // arg = (char*)malloc(MAX_ARG_SIZE);
    // buf = (char*)malloc(MAX_ARG_SIZE);

    // ASSERT(arg);
    // ASSERT(buf);

    *ptr = '\0';
    while ( 1 ) {
        if ( (ch=m->mgetc(m)) == EOF ) {
            *ptr = '\0';
            // free(arg); free(buf);
            return(EOF);
        }
		
        if ( ch == '%' ) { /* end tag */
            // ptr--; *ptr = '\0';
            *ptr = '\0';
			return(0);
        }
        if ( ch == '\r' || ch == '\n' || ch == '\t' )
            ch = ' ';
        *ptr++ = ch;
    }
}

int gssi_proc( fq_logger_t *l, char *template, char *var_data, char *postdata, cache_t *cache_short,  char delimiter,  char *dst, int dst_buf_len)
{
	fq_token_t tok;
	mem_obj_t *m=NULL; // source
	mem_obj_t *t=NULL; // target

	FQ_TRACE_ENTER(l);
	
	if( Tokenize(l, var_data, delimiter, &tok) < 0 ) {
        fq_log(l, FQ_LOG_ERROR, "Tokenize() error.");
		FQ_TRACE_EXIT(l);
        return(-1);
    }

	open_mem_obj( l, &m , dst_buf_len);
	open_mem_obj( l, &t , dst_buf_len);

	m->mseek( m, 0);
	m->mcopy( m, template, strlen(template));

#ifdef _DEBUG
	m->mprint(m);
#endif
	m->mseek( m, 0);
	while( !m->meof( m ) ) {
		char  c;
		char tag[32];
		int tagid;
		char value[MAX_VALUE_SIZE];

		c = m->mgetc( m );
		if( c == '%' ) {
			int tag_type;

			memset(tag, 0x00, sizeof(tag));
			tagid = get_tag_index_and_type( m, tag , &tag_type);
			if( tagid < 0 ) {
				t->mputc( t, c);
				m->mback(m, -tagid);
				continue;
			}
			// fprintf(stdout, "tagid=[%d],  tag=[%s], tag_type =[%d]\n", tagid, tag, tag_type);
			fq_log(l, FQ_LOG_DEBUG, "tagid=[%d],  tag=[%s], tag_type =[%d]", tagid, tag, tag_type);
			if( tag_type == 0 ) { /* take a value from var_data */
				char *temp=NULL;

				memset(value, 0x00, sizeof(value));

				if( GetToken(l, &tok, tagid, &temp) < 0 ) {
					fq_log(l, FQ_LOG_ERROR,  "GetToken()() error. \n");
					// close_mem_obj(l, &m);
					// close_mem_obj(l, &t);
					// FQ_TRACE_EXIT(l);
					// return(-1);

					/* We will continue after filling with a space */
					/* In ShinHan Bank, want to it */
					t->mputc( t, ' ');
				}
				else {
					t->mcopy( t, temp, strlen(temp));
				}
				if( temp ) free(temp);
			}
			else if( tag_type == 1 ) { /* take a value from postdata */
				char value[128];

				if( gssi_get_item( postdata, tag, value, 128) < 0 ) {
					fq_log(l, FQ_LOG_ERROR, "get_time('%s') error. We don't have a value for [%s].", tag, tag);
					close_mem_obj(l, &m);
					close_mem_obj(l, &t);
					return(-1);
				}
				else {
					t->mcopy( t, value, strlen(value));
				}
			}
			else if( tag_type == 2 ) {
				char arg[128];
				gssi_getarg(m, arg);

				// printf("arg (variable name)----> %s\n", arg);
				/* get a value from cache */

				char *p;
				p = cache_getval_unlocked(cache_short, arg);
				if( p ) {
					t->mcopy( t, p, strlen(p));
				}
				else {
					fq_log(l, FQ_LOG_ERROR, "cache_getval_unlocked('%s') error. We don't have a value in cache.", arg);
					close_mem_obj(l, &m);
					close_mem_obj(l, &t);
					return(-1);
				}
			}
#if 1
			else if( tag_type == 3 ) {
				char arg[128];
				gssi_getarg(m, arg);
				// printf("json array key = [%s]\n", arg);
				int ii;
				int last = 2;
				for(ii=0; ii< last+1; ii++) {
					char buf[128];
					if( ii < last ) {
						sprintf(buf, "\"type-%d|kind|path|filename\",", ii);
					}
					else {
						sprintf(buf, "\"type-%d|kind|path|filename\"", ii);
					}
					t->mcopy( t, buf, strlen(buf));	
				}
				// m->mback(m, -1);
			}
#endif
			else {
				fq_log(l, FQ_LOG_ERROR, "unsupported tag: %s ", tag);
				close_mem_obj(l, &m);
				close_mem_obj(l, &t);
				FQ_TRACE_EXIT(l);
				return(-1);
			}
		}
		else {
			if( c == 0x00 ) break;
			t->mputc( t, c);
		}
	}

#ifdef _DEBUG
	m->mprint(m);
#endif

	t->mgets(t, dst);

	close_mem_obj(l, &m);
	close_mem_obj(l, &t);

	FQ_TRACE_EXIT(l);
	return(0);
}

#if 0
/*
** Server Side Including Processing
**
** SSIP(templateCache, sMsgBody, DbRibMail->sVarData, stSndMsg);
** templateCache : load_template_fmt_db (empty structure buffer)
** sMsgBody : template
** sVarData : user_Data
** stSndMsg : target_message
*/
int SSIP(fq_logger_t *l, load_template_fmt_db *tContent, char *sTempContents, char *varData, tmpl_msg_fmt *trgt_msg) 
{
	char dst[8192];
	int rc;
#if 0
	fq_token_t t;
	char *sndphnid=NULL;
	char *subject=NULL;
#endif
	
	FQ_TRACE_ENTER(l);

	replace_CR_NL_to_space( varData );

	memset( tContent->content, 0x00, sizeof(tContent->content) ) ;
	strcpy( tContent->content, sTempContents ) ;

	/*
	** fill template with varData
	*/
	rc = gssi_proc(l, sTempContents, varData, NULL, '|', dst, sizeof(dst));
	if( rc < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "gssi_proc() error.! rc=[%d]", rc);
		FQ_TRACE_EXIT(l);
		return(-1);
	}
	sprintf(trgt_msg->sendmsg, "%s", dst);
	fq_log(l, FQ_LOG_DEBUG, "SSIP() result: len=[%d] ,'%s'", strlen(dst), dst);

#if 0
	if( Tokenize(l, varData, '|', &t) < 0 ) {
		fq_log(l, FQ_LOG_DEBUG, "Tokenize() error.");
		FQ_TRACE_EXIT(l);
		return(-2);
	}

	/*
	** bring sndphnid from varData
	*/
	if( GetToken(l, &t, 0, &sndphnid) < 0 ) {
		fq_log(l, FQ_LOG_DEBUG, "GetToken(%d-th) error.");
                return(-3);
    }
	sprintf(trgt_msg->sndphnid, "%s", sndphnid);

	/*
	** bring subject from varData
	*/
	if( GetToken(l, &t, 1, &subject) < 0 ) {
		fq_log(l, FQ_LOG_DEBUG, "GetToken(%d-th) error.");
                return(-3);
	}
	sprintf(trgt_msg->subject, "%s", subject);

	SAFE_FREE(sndphnid);
	SAFE_FREE(subject);
#else
	sprintf(trgt_msg->sndphnid, "%s", " ");
	sprintf(trgt_msg->subject, "%s", "kakao");
#endif

	FQ_TRACE_EXIT(l);

	return 0;
}

void replace_CR_NL_to_space( char *src )
{
	int i;

	if( !src ) return;

	for( i=0; i < strlen(src) ; i++) {
		if ( src[i] == '\n'|| src[i] == '\r' ) 
			src[i] = 0x20; /* space */
	}
	return;
}
#endif

