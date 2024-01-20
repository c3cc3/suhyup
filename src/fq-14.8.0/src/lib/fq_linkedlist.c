/***************************************************************
 ** fq_linkedlist.c
 **/

#include <stdbool.h>
#include "config.h"
#include "fq_defs.h"
#include "fq_common.h"
#include "fq_linkedlist.h"
#include "fq_logger.h"

/*-------------------------------------------------------------------
 * linkedlist node operations
 * allocate a new node
 */
static ll_node_t
*ll_node_new(char* key, void* value, size_t sz_value)
{
	ll_node_t *t;

	t = (ll_node_t*)calloc(1, sizeof(ll_node_t));

	t->key   = (char *)strdup(key);
	
	t->value_sz = sz_value;
	t->value = malloc(sz_value+1);
	memcpy(t->value, value, sz_value);

	t->next = NULL;
	return (t);
}

static void
ll_node_free(ll_node_t *t)
{
	SAFE_FREE(t->key);
	if(t->value) {
		free(t->value);
		t->value = NULL;
	}
	SAFE_FREE(t);
}

/*-------------------------------------------------------------------
 * linkedlist operations
 * allocate a new head node.
 */
linkedlist_t*
linkedlist_new(char* key)
{
	linkedlist_t *t;

	t = (linkedlist_t*)calloc(1, sizeof(linkedlist_t));

	t->key  = (char *)strdup(key);
	t->length = 0;
	t->head = t->tail = NULL;
	t->next = NULL;

	return (t);
}

/*
** free all nodes of linkedlist and.
** free a linkedlist
*/
void
linkedlist_free(linkedlist_t **pt)
{
	ll_node_t *p, *q;

	if ( !(*pt) )
		return;

	p = (*pt)->head;

	while ( p ) {
		q = p;
		p = p->next;
		ll_node_free(q);
	}

	SAFE_FREE( (*pt)->key );
	SAFE_FREE( *pt );
}

/*
** put a new node to allocated linkedlist.
** This function permits that key is  duplicated.
*/
ll_node_t*
linkedlist_put(linkedlist_t *t, char *key, void *value, size_t sz_value)
{
	ll_node_t *p;

	p = ll_node_new(key, value, sz_value);
	if ( !t->tail )
		t->head = t->tail = p;
	else {
		t->tail->next = p;
		t->tail = p;
	}
	(t->length)++;
	return (p);
}

void
linkedlist_del(linkedlist_t *t, char *key)
{
	ll_node_t *p, *q;

	for ( p=q=t->head; p != NULL ; p = p->next ) {
		if ( STRCCMP((void *)p->key, (void *)VALUE(key)) == 0 ) {
			if ( p == q && p == t->tail ) {
				ll_node_free(p);
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
			ll_node_free(p);
			break;
		}
		q = p;
	}
	return;
}

ll_node_t *
linkedlist_find(fq_logger_t *l, linkedlist_t *t, char *key)
{
	ll_node_t *p;

	FQ_TRACE_ENTER(l);

	if( !t ) {
		FQ_TRACE_EXIT(l);
		return(NULL);
	}

	fq_log(l, FQ_LOG_DEBUG, " list '%s' contains %d elements.", t->key, t->length);

	p = t->head;

	while ( p ) {
		if( !(p->key) ) return(NULL);
		if( !HASVALUE(key))  return(NULL);


		if ( strncasecmp(p->key, key, strlen(key)) == 0 ) {
			fq_log(l, FQ_LOG_DEBUG, " found: Address of %s is \'%p\' value=[%s].", p->key, p->value, p->value);
			FQ_TRACE_EXIT(l);
			return (p);
		}

		FQ_TRACE_EXIT(l);
		p = p->next;
	}
	return (NULL);

}

void linkedlist_sort(linkedlist_t *t) 
{  
	ll_node_t *p;
	
	//Node current will point to head  
	ll_node_t *current = t->head;
	ll_node_t *index = NULL;  

	char *temp_key;  
	void *temp_value;
	size_t temp_value_sz;
	  
	if(t == NULL) {  
		return;  
	}  
	else {  
		while(current != NULL) {  
			//Node index will point to node next to current  
			index = current->next;  
			  
			while(index != NULL) {  
				//If current node's data is greater than index's node data, swap the data between them  
				if(strcmp(current->key, index->key) > 0) {  
					/* move current to tmp */
					temp_key = strdup(current->key);
					temp_value = calloc(temp_value_sz, sizeof(char));
					memcpy(temp_value, current->value, current->value_sz);
					temp_value_sz = current->value_sz;

					/* move index to current */
					current->key = strdup(index->key);
					current->value = calloc(index->value_sz, sizeof(char));
					memcpy(current->value, index->value, index->value_sz);
					current->value_sz = index->value_sz;
					if(index->key) free(index->key);
					if(index->value) free(index->value);

					/* move temp to index */
					index->key = strdup(temp_key);
					index->value = calloc(temp_value_sz, sizeof(char));
					memcpy(index->value, temp_value, temp_value_sz);
					index->value_sz = temp_value_sz;
					if(temp_key) free(temp_key);
					if(temp_value) free(temp_value);
				}  
				index = index->next;  
			}  
			current = current->next;  
		}      
	}  
}  

/**********************************************************
** print routines
*/
void
linkedlist_scan(linkedlist_t *t, FILE* fp)
{
	ll_node_t *p;

	if ( !t )
		return;

	fprintf(fp, " list '%s' contains %d elements\n", t->key, t->length);
	for ( p=t->head; p != NULL ; p = p->next ) {
		fprintf(fp, " Address of %s is \'%p\'.\n", p->key, p->value);
		fflush(fp);
	}
}

bool
linkedlist_callback(linkedlist_t *t, linkedlist_CB usrFP)
{
	ll_node_t *p;

	if ( !t )
		return false;

	for ( p=t->head; p != NULL ; p = p->next ) {
		bool usr_rc;
		usr_rc = usrFP( p->value_sz, p->value );
		if( usr_rc == false ) return false;
		else continue;
	}
	return true;
}

