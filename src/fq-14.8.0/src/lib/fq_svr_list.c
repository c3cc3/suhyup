#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "fq_common.h"
#include "fq_svr_list.h"
#include "fq_tokenizer.h"

/* This function is moved to fq_common.c */
#if 0
bool get_LR_value(char *src, char delimiter, char *left, char *right)
{
	char *p = NULL;
	char *start = src;
	char *end = src - strlen(src);
	int left_len;
	int right_len;


	p = strchr( src, delimiter);
	if( !p ) {
		return(false);
	}

	left_len = p - start;
	memcpy(left, src, left_len);
	left[left_len] = 0x00;

	right_len = (end-p) -1;
	memcpy(right, p+1, right_len);
	right[right_len] = 0x00;

	return(true);
}
#endif
#if 0
/* src: "kt:KoreaTelecom:/home/kt/enmq:snd_agnet.q" */
bool get_item_values( char *src, char *name, char *fullname, char *qpath, char *qname)
{
	char *p = src;
	char *delimiter = ":";
	tokened *tokened_header=NULL, *cur=NULL;

	tokenizer(src, delimiter, &tokened_header);
	cur = tokened_header;

	int i;
	for(i=0; (cur != NULL); i++) {
		char *buf=NULL;
		int size;

		size = (cur->end - cur->sta);
		buf = calloc(size+1, sizeof(char));
		memcpy(buf, cur->sta, size);

		if( cur->type != TOK_TOKEN_O ) {
			switch(i) {
				case 0:
					sprintf(name, "%s", buf);
					break;
				case 2:
					sprintf(fullname, "%s", buf);
					break;
				case 4:
					sprintf(qpath, "%s", buf);
					break;
				case 6:
					sprintf(qname, "%s", buf);
					break;
				default:
					break;
			}
		}

		cur=cur->next;
		SAFE_FREE(buf);
	}
	tokenizer_free( tokened_header );
	return(true);
}
#endif

static svr_t *
svr_new( char *ip, char *port)
{
	svr_t *t;

	t = (svr_t *)calloc(1, sizeof(svr_t));
	t->ip = strdup(ip);
	t->port = strdup(port);
	t->next = NULL;
	return(t);
}

static void
svr_free( svr_t *t )
{
	SAFE_FREE(t->ip);
	SAFE_FREE(t->port);
	SAFE_FREE(t);
}

svrlist_t *
svrlist_new()
{
	svrlist_t *t;

	t = (svrlist_t*) calloc(1, sizeof(svrlist_t));

	t->length = 0;
	t->head = t->tail = NULL;
	t->next = NULL;
	return(t);
}

void
svrlist_free(svrlist_t **t)
{
	svr_t *p, *q;
	if( !(*t) ) return;

	p = (*t)->head;
	while(p) {
		q = p;
		p = p->next;
		svr_free(q);
	}
	SAFE_FREE(*t);
}

svr_t *
svrlist_put( svrlist_t *t, char *ip, char *port)
{
	svr_t *p;

	p = svr_new( ip, port );
	if( !t->tail ) 
		t->head = t->tail = p;
	else {
		t->tail->next = p;
		t->tail = p;
	}
	(t->length)++;

	return(p);
}

svr_t *
svrlist_pop( svrlist_t *t )
{
	svr_t *p;

	p = t->head;
	if(!p ) return(NULL);

	t->head = p->next;
	(t->length)-- ;
	return(p);
}

void
svrlist_print(svrlist_t *t, FILE *fp)
{
	svr_t	*p;

	if( !t ) return;

	fprintf(fp, "svrlist contains %d elements\n", t->length);

	for( p=t->head; p != NULL; p=p->next) {
		fprintf(fp, "\tip='%s', port='%s'\n", p->ip, p->port);
		fflush(fp);
	}
}

static int get_svr_count(char *src)
{
	char *p = src;
	int count = 1;

	while(*p) {
		if( *p == ',' ) {
			count++;
			p++;
		}
		else p++;
	}

	return(count);
}

static bool get_item_values(char *src, char *ip, char *port)
{
	char *p = src;
	char *delimiter = ":";
	tokened *tokened_head = NULL, *cur = NULL;

	tokenizer(src, delimiter, &tokened_head);
	cur = tokened_head;

	int i;
	for(i=0; (cur != NULL); i++) {
		char *buf=NULL;
		int size;

		size = (cur->end - cur->sta);
		buf = calloc(size+1, sizeof(char));
		memcpy(buf, cur->sta, size);

		if(cur->type != TOK_TOKEN_O ) {
			switch(i) {
				case 0:
					sprintf(ip, "%s", buf);
					break;
				case 2:
					sprintf(port, "%s", buf);
					break;
				default:
					break;
			}
		}
		cur=cur->next;
		SAFE_FREE(buf);
	}
	if(tokened_head) tokenizer_free(tokened_head);
	return(true);
}

#define MAX_SVR_LIST 100
bool make_svr_list( fq_logger_t *l, char *src, svrlist_t *t )
{
	int count, rc;
	char *dst[MAX_SVR_LIST];

	memset(dst, 0x00, sizeof(dst));

	count = get_svr_count(src);

	fq_log(l, FQ_LOG_DEBUG, "delimiter_parse() begin, count=[%d], src=[%s].", count, src);

	rc = delimiter_parse(src, ',', count, dst);
	if(rc == FALSE) {
		fq_log(l, FQ_LOG_ERROR, "delimiter_parse('%s') error.", src);
		return(false);
	}
	fq_log(l, FQ_LOG_DEBUG, "delimiter_parse() OK.");

	int i;
	for(i=0; i<MAX_SVR_LIST; i++) {
		if( HASVALUE(dst[i]) ) {
			char ip[16], port[7];
			svr_t	*p = NULL;

			fq_log(l, FQ_LOG_DEBUG, "[%d]-th dst=[%s].", i, dst[i]);

			rc = get_item_values( dst[i], ip, port);
			if(rc == false ) {
				fq_log(l, FQ_LOG_ERROR, "get_LR_value() error.");
				return(false);
			}

			p = svrlist_put ( t, ip, port );
			if( !p ) {
				fq_log(l, FQ_LOG_ERROR, "svrlist_put() error.");
				break;
			}
		}
	}

	if(l->level == FQ_LOG_DEBUG_LEVEL) {
		fq_file_logger_t *p = l->logger_private;

		svrlist_print(t, p->logfile);
	}

	return(true);
}
/*
** This is a example for using  svrlist_t(linked list of svr_t(structure))
*/
#if 0
bool connect_to_server(fq_logger_t *l, tcp_client_obj_t **socket_obj, snd_agent_conf_t *my_c)
{
	svrlist_t *svrlist = NULL;
	tcp_client_obj_t *obj = NULL;

	int rc;

retry:
	svrlist = NULL;
	svrlist = svrlist_new();
	
	rc= make_svr_list(l, my_c->server_list, svrlist);

	if(rc == false ) {
		fq_log(l, FQ_LOG_ERROR, "make_svr_list(src='%s') error.", my_c->server_list);
		return false;
	}

	fq_log(l, FQ_LOG_DEBUG, "make_svr_list() OK.");

	while(1) {
		svr_t *svr = NULL;

		svr = svrlist_pop(svrlist);
		if( svr ) {
			rc = open_tcp_client_obj(l, svr->ip, svr->port, BINARY_HEADER, 4, &obj);
			if( rc != TRUE ) {
				fq_log(l, FQ_LOG_ERROR, "open_tcp_client_obj('%s', '%s') error. ", svr->ip, svr->port);
				sleep(2);
				continue;
			}
			fq_log(l, FQ_LOG_DEBUG, "Connected to server(%s:%s).", svr->ip, svr->port);
			break;
		}
		else {
			fq_log(l, FQ_LOG_ERROR, "There is no alive server. We will retry after 5 second.");
			sleep(5);

			if(svrlist) svrlist_free(&svrlist);
			goto retry;
		}
	}
	if(svrlist) svrlist_free(&svrlist);

	*socket_obj = obj;
	return(true);
}
#endif
