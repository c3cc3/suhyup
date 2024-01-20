/*
** fq_tokenizer.c
*/
#define FQ_TOKENIZER_C_VERSION "1.0.0"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "fq_logger.h"
#include "fq_tokenizer.h"

int tokenizer(const char *str, const char *token, tokened **tokened_head)
{
  tokened *cur=NULL,*prev=NULL;
  char chr;
  int i=0,cnt=0;
  int token_len=0;
  char *last_token_occur=NULL;

  if(str==NULL){
    return 1;
  }

  if(token==NULL){
    return 2;
  }

  token_len=strlen(token);
  last_token_occur=(char *)str;

  while((chr=*(str+cnt))!='\0'){

    for(i=0;token_len>i;++i){

      if(chr==token[i]){

        if(str+cnt>last_token_occur){

          cur=(tokened *)malloc(sizeof(tokened));
          memset(cur,0x0,sizeof(tokened));
          cur->type=TOK_TOKEN_X;
          cur->sta=last_token_occur;
          cur->end=(char *)(str+cnt);

          if((*tokened_head)==NULL){

            (*tokened_head)=cur;

          }else{

            prev->next=cur;

          }

          prev=cur;

        }

        cur=(tokened *)malloc(sizeof(tokened));
        memset(cur,0x0,sizeof(tokened));
        cur->type=TOK_TOKEN_O;
        cur->sta=(char *)(str+cnt);
        cur->end=(char *)(str+cnt+1);

        if((*tokened_head)==NULL){

          (*tokened_head)=cur;

        }else{

          prev->next=cur;

        }

        prev=cur;

        last_token_occur=(char *)(str+cnt+1);

        break;

      }

    }

    ++cnt;

  }

  if(str+cnt>last_token_occur){

    cur=(tokened *)malloc(sizeof(tokened));
    memset(cur,0x0,sizeof(tokened));
    cur->type=TOK_TOKEN_X;
    cur->sta=last_token_occur;
    cur->end=(char *)(str+cnt);

    if((*tokened_head)==NULL){

      (*tokened_head)=cur;

    }else{

      prev->next=cur;

    }

    prev=cur;

  }

  return 0;
}

int tokenizer_free(tokened *tokened_head)
{

  tokened *cur=NULL,*next=NULL;

  cur=tokened_head;

  while(cur!=NULL){

    next=cur->next;
    if(cur) {
		free(cur); cur=NULL;
	}
    cur=next;

  }
  return 0;

}

/*
** return 
	0 > ret:  error.
	0 < ret:  normal
	0 == ret: three is no value;
*/

int Tokenize(fq_logger_t *l, char *src, char delimiter, fq_token_t *t)
{
	char *p=NULL, *q=NULL;
	int idx=0;

	FQ_TRACE_ENTER(l);

	if( !src  || !t ) {
		fq_log(l, FQ_LOG_ERROR, "parameter error.");
		FQ_TRACE_EXIT(l);
		return(-1);
	}

	t->src = src;
	p = src;

	t->n_delimiters = 0;
	t->delimiter = delimiter;
	t->pos[idx++] = p;

	while(1) {
		q = strchr(p, delimiter);
		if( q ) {
			t->pos[idx++] = q+1;
			t->n_delimiters++;
			p = q+1;
			if( t->n_delimiters >= MAX_FILEDS ) {
				fq_log(l, FQ_LOG_ERROR, "Too many delimiters");
				FQ_TRACE_EXIT(l);
				return(-2);
			}
		}
		else {
			t->pos[idx] = p;
			break;
		}
	}
	fq_log(l, FQ_LOG_DEBUG, "n_delimiters=[%d]\n", t->n_delimiters);

	FQ_TRACE_EXIT(l);

	return 0;
}

int GetToken(fq_logger_t *l, fq_token_t *t, int position, char **dst)
{
	char *p=NULL, *q=NULL, *d=NULL;
	char *pos[MAX_FILEDS+1];
	int  idx=0;
	int  datalen=0;

	FQ_TRACE_ENTER(l);

	if( !t ) {
		FQ_TRACE_EXIT(l);
		return( -1 );
	}

	if( position < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "[%d] is not available position value( 0 <= Position ).", position );
		FQ_TRACE_EXIT(l);
		return(-2);
	}

	if( position > t->n_delimiters ) {
		fq_log(l, FQ_LOG_ERROR, "Too big position value [%d], Last position value is [%d], ", position, t->n_delimiters);
		FQ_TRACE_EXIT(l);
		return(-2);
	}
	
	if( position == t->n_delimiters ) {
		datalen = strlen(t->pos[position]);
	}
	else {
		datalen = (t->pos[position+1] - t->pos[position]) - 1;
	}

	fq_log(l, FQ_LOG_DEBUG, "datalen=[%d]", datalen);

	d = calloc( datalen+1, sizeof(char));

	assert(d);

	memcpy(d, t->pos[position], datalen);

	*dst = d;

	FQ_TRACE_EXIT(l);
	return(datalen);
}

#if 0
int main(int ac, char **av)
{
	char *test = "000|||333|444|555|";
	char *dst=NULL;
	fq_token_t t;

	if( ac != 2 ) {
		printf("Usage: $ %s [position] <enter>\n");
		return(0);
	}

	if( Tokenize(test, '|', &t) < 0 ) {
		printf("Tokenize() error.\n");
		return(-1);
	}

	if( GetToken(&t, atoi(av[1]), &dst) < 0 ) {
		printf("GetToken() error.\n");
		return(-2);
	}
	else {
		printf("dst=[%s]\n", dst);
		free(dst);
		return(0);
	}
}
#endif
