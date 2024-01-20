/***************************************************************
 ** fq_cache.c
 **/
#define FQ_CACHE_C_VERSION "1.0.0"

#include <stdbool.h>
#include "config.h"
#include "fq_defs.h"
#include "fq_common.h"
#include "fq_cache.h"

#define _FALSE 0
#define _TRUE  1

/*-------------------------------------------------------------------
 * node operations
 */
node_t
*node_new(char* key, char* value, int flag)
{
	node_t *t;

	t = (node_t*)calloc(1, sizeof(node_t));

	t->key   = (char *)strdup(key);
	t->value = (char *)strdup(value);
	t->flag = flag;
	t->next = NULL;
	return (t);
}

void
node_free(node_t *t)
{
	SAFE_FREE(t->key);
	SAFE_FREE(t->value);
	SAFE_FREE(t);
}

/*-------------------------------------------------------------------
 * list operations
 */
tlist_t*
list_new(char type, char* key)
{
	tlist_t *t;

	t = (tlist_t*)calloc(1, sizeof(tlist_t));

	t->key  = (char *)strdup(key);
	t->type = type;
	t->length = 0;
	t->head = t->tail = NULL;
	t->next = NULL;

	return (t);
}

void
list_Free(tlist_t **pt)
{
	node_t *p, *q;

	if ( !(*pt) )
		return;

	p = (*pt)->head;

	while ( p ) {
		q = p;
		p = p->next;
		node_free(q);
	}

	SAFE_FREE( (*pt)->key );
	SAFE_FREE( *pt );
}

void
list_clear(tlist_t *t)
{
	node_t *p, *q;

	p = t->head;
	while ( p ) {
		q = p;
		p = p->next;
		node_free(q);
	}
	t->length = 0;
	t->head = t->tail = NULL;
}

node_t*
list_find(tlist_t *t, char *key)
{
	node_t *p;

	if( !t ) return(NULL);
	p = t->head;

	while ( p ) {
		/* here111 */

		if( !(p->key) ) return(NULL);
		if( !HASVALUE(key))  return(NULL);

		/*
		if ( STRCCMP(p->key, key) == 0 )
		*/
		if ( strncasecmp(p->key, key, strlen(key)) == 0 )
			return (p);

		p = p->next;
	}
	return (NULL);
}

node_t*
list_put(tlist_t *t, char *key, char *value, int flag)
{
	node_t *p;


	p = node_new(key, value, flag);
	if ( !t->tail )
		t->head = t->tail = p;
	else {
		t->tail->next = p;
		t->tail = p;
	}
	(t->length)++;
	return (p);
}

node_t*
list_push(tlist_t *t, char* key, char *value, int flag)
{
	node_t *p;

	if( t->length >= MAX_LIST_QUEUE_SIZE ) {
		return(NULL); /* queue is full. */ 
	}
	p = node_new(key, value, flag);
	if ( !t->head )
		t->head = t->tail = p;
	else {
		p->next = t->head;
		t->head = p;
	}
	(t->length)++;
	return (p);
}

node_t*
list_pop(tlist_t *t)
{
	node_t *p;


	p = t->head;
	if ( !p )
		return (NULL);

	t->head = p->next;
	(t->length)--;

	return (p);
}

node_t*
list_Add(tlist_t *t, char *key, char *value, int flag)
{
	if ( !list_find(t, key) )
		return (list_put(t, key, value, flag));
	else 
		return((node_t *)NULL);
}

void
list_Delete(tlist_t *t, char *key)
{
	node_t *p, *q;

	for ( p=q=t->head; p != NULL ; p = p->next ) {
		if ( STRCCMP((void *)p->key, (void *)VALUE(key)) == 0 ) {
			if ( p == q && p == t->tail ) {
				node_free(p);
				t->head = t->tail = NULL;
				t->length = 0;
				break;
			}
			if ( p == q )
				t->head = p->next;
			else if ( p == t->tail )
				t->tail = q;
			q->next = p->next;
			(t->length)--;
			node_free(p);
			break;
		}
		q = p;
	}
}

node_t*
list_update(tlist_t *t, char *key, char* value, int flag)
{
	node_t* p;

	ASSERT(t);

	p = list_find(t, key);
	if ( p ) {
		SAFE_FREE(p->value);
		p->value = (char *)strdup(value);
		ASSERT(p->value);
		p->flag  = flag;
		return (p);
	}
	else
		return (list_put(t, key, value, flag));
}

char*
list_getkey(tlist_t *t, char *value)
{
        node_t *p;

        ASSERT(t);

        if ( !value )
                return(NULL);

        for ( p=t->head; p != NULL ; p = p->next )
                if ( p->value && STRCCMP((void *)p->value, (void *)value) == 0 ) {
                        return (p->key);
                }
        return (NULL);
}

int
list_getkeyflag(tlist_t *t, char *value, char** key)
{
        node_t *p;

        ASSERT(t);

        *key = NULL;
        if ( !value )
                return (_FALSE);

        for ( p=t->head; p != NULL ; p = p->next )
                if ( p->value && STRCCMP((void *)p->value, (void *)value) == 0 ) {
                        *key = p->key;
                        return (p->flag);
                }
        return (_FALSE);
}

char*
list_getval(tlist_t *t, char *key)
{
        node_t *p;

        ASSERT(t);

        if ( (p = list_find(t, key)) )
                return (p->value);

        return (NULL);
}

int
list_getflag(tlist_t *t, char *key, char **value)
{
        node_t *p;

        ASSERT(t);

        if ( (p = list_find(t, key)) ) {
                *value = p->value;
                return (p->flag);
        }
        *value = NULL;
        return (_TRUE);
}

/*********************************************************************
** fqlist operations
*/
void
fqlist_lock(fqlist_t *t)
{
        ASSERT(t);
        pthread_mutex_lock(&t->lock);
}

void
fqlist_unlock(fqlist_t *t)
{
        ASSERT(t);
        pthread_mutex_unlock(&t->lock);
}

fqlist_t*
fqlist_new(char type, char* key)
{
        fqlist_t *t;

        t = (fqlist_t*)calloc(1, sizeof(fqlist_t));
        ASSERT(t);

        t->list = list_new(type, key);
        pthread_mutex_init(&t->lock, NULL);

        return (t);
}

void
fqlist_free(fqlist_t **pt)
{
        if ( !(*pt) )
                return;

        pthread_mutex_lock(&(*pt)->lock);
        list_Free(&(*pt)->list);
        pthread_mutex_unlock(&(*pt)->lock);
        pthread_mutex_destroy(&(*pt)->lock);
        SAFE_FREE(*pt);
}

void
fqlist_clear(fqlist_t *t)
{
        if ( !t )
                return;

        pthread_mutex_lock(&t->lock);
        list_clear(t->list);
        pthread_mutex_unlock(&t->lock);
}

void
fqlist_put(fqlist_t *t, char *key, char *value, int flag)
{
        ASSERT(t);

        pthread_mutex_lock(&t->lock);
        (void)list_put(t->list, key, value, flag);
        pthread_mutex_unlock(&t->lock);
}

/*
** add function do not permit duplication.
*/
int
fqlist_add(fqlist_t *t, char *key, char *value, int flag)
{
        ASSERT(t);
		node_t *p;

        pthread_mutex_lock(&t->lock);
        p = list_Add(t->list, key, value, flag);
		if( p )  {
			pthread_mutex_unlock(&t->lock);
			return(0);
		}
		else {
			pthread_mutex_unlock(&t->lock);
			return(-1);
		}
}

node_t *
fqlist_push(fqlist_t *t, char *key, char *value, int flag)
{
	node_t	*p=NULL;
	ASSERT(t);

	pthread_mutex_lock(&t->lock);
	p = list_push(t->list, key, value, flag);
	pthread_mutex_unlock(&t->lock);

	return(p);
}

node_t*
fqlist_pop(fqlist_t *t)
{
        node_t* p;

        ASSERT(t);

        pthread_mutex_lock(&t->lock);
        p = list_pop(t->list);
        pthread_mutex_unlock(&t->lock);
        return (p);
}

void
fqlist_delete(fqlist_t *t, char *key)
{
        ASSERT(t);

        pthread_mutex_lock(&t->lock);
        list_Delete(t->list, key);
        pthread_mutex_unlock(&t->lock);
}

void
fqlist_update(fqlist_t *t, char *key, char* value, int flag)
{
        ASSERT(t);

        pthread_mutex_lock(&t->lock);
        (void)list_update(t->list, key, value, flag);
        pthread_mutex_unlock(&t->lock);
}

/*
** The following functions provides no locking mechanism
** Locking/unlocking are user's responsibility
*/
node_t*
fqlist_find_unlocked(fqlist_t *t, char *key)
{
        ASSERT(t);

        return ( list_find(t->list, key) );
}

char*
fqlist_getkey_unlocked(fqlist_t *t, char *value)
{
        ASSERT(t);

        return ( list_getkey(t->list, value) );
}

int
fqlist_getkeyflag_unlocked(fqlist_t *t, char *value, char** key)
{
        ASSERT(t);

        return ( list_getkeyflag(t->list, value, key) );
}

char*
fqlist_getval_unlocked(fqlist_t *t, char *key)
{
        ASSERT(t);

        return ( list_getval(t->list, key) );
}

int
fqlist_getflag_unlocked(fqlist_t *t, char *key, char** value)
{
        ASSERT(t);

        return ( list_getflag(t->list, key, value) );
}

/*********************************************************************
** container operations
*/
void
container_lock(container_t *t)
{
        ASSERT(t);

        pthread_mutex_lock(&t->lock);
}

void
container_unlock(container_t *t)
{
        ASSERT(t);

        pthread_mutex_unlock(&t->lock);
}

container_t*
container_new(char* name)
{
        container_t *t;

        t = (container_t*)calloc(1, sizeof(container_t));
        ASSERT(t);

        t->name = (char *)strdup(name);
        ASSERT(t->name);
        t->length = 0;
        t->head = t->tail = NULL;

        pthread_mutex_init(&t->lock, NULL);
        return (t);
}

void
container_free(container_t **pt)
{
        tlist_t *p, *q;

        if ( !(*pt) )
                return;

        pthread_mutex_lock(&(*pt)->lock);
        p = (*pt)->head;
        while ( p ) {
                q = p;
                p = p->next;
                list_Free(&q);
        }
        SAFE_FREE( (*pt)->name );
        pthread_mutex_unlock(&(*pt)->lock);
        pthread_mutex_destroy(&(*pt)->lock);

        SAFE_FREE( *pt );
}

void
container_clear(container_t *t)
{
        tlist_t *p, *q;

        ASSERT(t);

        pthread_mutex_lock(&t->lock);
        p = t->head;
        while ( p ) {
                q = p;
                p = p->next;
                list_Free(&q);
        }
        t->length = 0;
        t->head = t->tail = NULL;
        pthread_mutex_unlock(&t->lock);
}

/* Jssong 2000.2.22: Return value void -> tlist_t */
tlist_t*
container_put(container_t *t, char type, char* key)
{
        tlist_t *p;

        ASSERT(t);

        pthread_mutex_lock(&t->lock);
        p = container_put_unlocked(t, type, key);
        pthread_mutex_unlock(&t->lock);

        return(p);
}

/* Jssong 2000.2.22: Return value void -> tlist_t */
tlist_t*
container_add(container_t *t, char type, char* key)
{
        tlist_t *p;

        ASSERT(t);

        pthread_mutex_lock(&t->lock);
        if ( !container_find_unlocked(t, key) ) {
                p = container_put_unlocked(t, type, key);
		}
		else {
				p = (tlist_t *)NULL;
		}

        pthread_mutex_unlock(&t->lock);

        return(p);
}

tlist_t*
container_list_update(container_t *t, char type, char* key, char* itemkey, char *value, int flag )
{
	tlist_t *c;
	tlist_t *p;

	ASSERT(t);

	pthread_mutex_lock(&t->lock);
	if ( !(c=container_find_unlocked(t, key)) ) {
		p = container_put_unlocked(t, type, key);
        (void)list_update(p, itemkey, value, flag);
		pthread_mutex_unlock(&t->lock);
		return(p);
	}
	else {
		list_update(c, itemkey, value, flag);
		pthread_mutex_unlock(&t->lock);
		return(c);
	}
}

void
container_additem(container_t *t, char* key, char* itemkey, char* value, int flag)
{
        tlist_t *p;

        ASSERT(t);

        pthread_mutex_lock(&t->lock);
        p = container_find_unlocked(t, key);
        if ( p )
                (void)list_Add(p, itemkey, value, flag);
        pthread_mutex_unlock(&t->lock);
}

void
container_putitem(container_t *t, char* key, char* itemkey, char* value, int flag)
{
        tlist_t *p;

        ASSERT(t);

        pthread_mutex_lock(&t->lock);
        p = container_find_unlocked(t, key);
        if ( p )
                (void)list_put(p, itemkey, value, flag);
        pthread_mutex_unlock(&t->lock);
}

void
container_delkey(container_t *t, char *key)
{
        tlist_t *p, *q;

        ASSERT(t);

        pthread_mutex_lock(&t->lock);
        for ( p=q=t->head; p != NULL ; p = p->next ) {
                if ( STRCCMP((void *)p->key, (void *)VALUE(key)) == 0 )
                {
                        if ( p == q && p == t->tail ) {
                                list_Free(&p);
                                t->head = t->tail = NULL;
                                t->length = 0;
                                break;
                        }
                        if ( p == q )
                                t->head = p->next;
                        else if ( p == t->tail )
                                t->tail = q;
                        q->next = p->next;
                        (t->length)--;
                        list_Free(&p);
                        break;
                }
                q = p;
        }
        pthread_mutex_unlock(&t->lock);
}

void
container_deltype(container_t *t, char type)
{
        tlist_t *p, *q;

        ASSERT(t);

        p = q = t->head;

        pthread_mutex_lock(&t->lock);
        while ( p ) {
                if ( p->type == type || type == '*' ) {
                        if ( p == q && p == t->tail ) {
                                list_Free(&p);
                                t->head = t->tail = NULL;
                                t->length = 0;
                                pthread_mutex_unlock(&t->lock);
                                return;
                        }
                        if ( p == t->tail ) {
                                t->tail = q;
                                q->next = NULL;
                                list_Free(&p);
                                (t->length)--;
                                pthread_mutex_unlock(&t->lock);
                                return;
                        }
                        else if ( p == q ) {
                                t->head = p->next;
                                list_Free(&p);
                                (t->length)--;
                                p=q=t->head;
                        }
                        else {
                                q->next = p->next;
                                list_Free(&p);
                                (t->length)--;
                                p = q->next;
                        }
                }
                else {
                        q = p;
                        p = p->next;
                }
        }
        pthread_mutex_unlock(&t->lock);
}

void
container_delitem(container_t *t, char* key, char* itemkey)
{
        tlist_t *p;

        ASSERT(t);

        pthread_mutex_lock(&t->lock);
        p = container_find_unlocked(t, key);
        if ( p )
                list_Delete(p, itemkey);
        pthread_mutex_unlock(&t->lock);
}

/*
** The following functions provides no locking mechanism
** Locking/unlocking are user's responsibility
*/
/* Jssong 2000.2.22: Return value void -> tlist_t */
tlist_t*
container_put_unlocked(container_t *t, char type, char* key)
{
        tlist_t *p;

        ASSERT(t);

        p = list_new(type, key);
        if ( !t->tail )
                t->head = t->tail = p;
        else {
                t->tail->next = p;
                t->tail = p;
        }
        (t->length)++;

        return(p);
}

tlist_t*
container_find_unlocked(container_t *t, char *key)
{
        tlist_t *p;

        ASSERT(t);

        for ( p=t->head; p != NULL ; p = p->next )
                if ( STRCCMP((void *)p->key, (void *)VALUE(key)) == 0 ) {
                        return (p);
                }
        return (NULL);
}

/**********************************************************
** print routines
*/
void
list_print(tlist_t *t, FILE* fp)
{
	node_t *p;

	if ( !t )
		return;

	fprintf(fp, " list '%s' type '%c' contains %d elements\n",
			t->key, t->type, t->length);
	for ( p=t->head; p != NULL ; p = p->next ) {
		fprintf(fp, "  %s = '%s\' %d\n", p->key, p->value, p->flag);
		fflush(fp);
	}
}

void
fqlist_print(fqlist_t *t, FILE* fp)
{
        if ( !t )
                return;
        pthread_mutex_lock(&t->lock);
        list_print(t->list, fp);
        pthread_mutex_unlock(&t->lock);
}

void
container_print(container_t *t, FILE* fp)
{
        tlist_t *p;
        if ( !t )
                return;
        pthread_mutex_lock(&t->lock);
        fprintf(fp, "container '%s' contains %d elements\n", t->name, t->length);
        for ( p=t->head; p != NULL ; p = p->next )
                list_print(p, fp);
        pthread_mutex_unlock(&t->lock);
}

/*
** This function can parse stream when delimiter is null(0x00)
*/
int MakeListFromDelimiterMsg( fqlist_t *li, char *a, char delimiter, int idx )
{
	char buf[128], key[10];
	int  i=0;
	char *p;

	memset(buf, 0x00, sizeof(buf));
	memset(key, 0x00, sizeof(key));

	p = a;
	if(!(*p) ) return(idx);
	while( *p != delimiter ) {
		if( *p == 0x00 ) { /* last */
			sprintf(key, "%03d", idx);
			fqlist_add(li, key, buf, 0); /* flag 1: number value 0: non-number */
			//printf("[%s]\n", buf);
			return(++idx);
		}
		buf[i++] = *p;
		p = p+1;
	}
	sprintf(key, "%03d", idx);
	fqlist_add(li, key, buf, 0);
	// printf("[%d]-th: [%s]\n", idx,buf);
	(void)MakeListFromDelimiterMsg(li, a+i+1, delimiter, idx+1);
}

// typedef int (*ListCBtype)( char *, char *, int);

int fqlist_CB( fqlist_t *li, ListCBtype userfunction )
{
	tlist_t *t = li->list;
	node_t *p;

	if( !t ) return -1;
	for ( p=t->head; p != NULL ; p = p->next ) {
		bool usr_rc;
		usr_rc = userfunction( p->key, p->value, p->flag);
		if( usr_rc == false ) return false;
	}
	return true;
}
