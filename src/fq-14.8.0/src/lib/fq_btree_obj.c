#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "fq_common.h"
#include "fq_logger.h"
#include "fq_btree_obj.h"

static bool on_insert(fq_logger_t *l, char* key, struct btree_node **leaf, Compare cmp);
static void on_inorder(fq_logger_t *l, struct btree_node *root);
static void on_search(fq_logger_t *l, char* key, struct btree_node *leaf, Compare cmp);
static void on_delete(fq_logger_t *l, struct btree_node** leaf);

static bool on_insert(fq_logger_t *l, char* key, struct btree_node **leaf, Compare cmp)
{
    int res;

	FQ_TRACE_ENTER(l);

    if( *leaf == NULL ) {
        *leaf = (struct btree_node*) malloc( sizeof( struct btree_node ) );
        (*leaf)->value = malloc( strlen (key) +1 );     // memory for key
        strcpy ((*leaf)->value, key);                   // copy the key
        (*leaf)->p_left = NULL;
        (*leaf)->p_right = NULL;
		fq_log(l, FQ_LOG_DEBUG, "new node for %s" , key);
    } else {
        res = cmp (key, (*leaf)->value);
        if( res < 0)
            on_insert(l, key, &(*leaf)->p_left, cmp);
        else if( res > 0)
            on_insert(l, key, &(*leaf)->p_right, cmp);
        else                                            // key already exists
			fq_log(l, FQ_LOG_DEBUG,  "Key '%s' already in tree.", key);
    }
	FQ_TRACE_EXIT(l);
	return true;
}

//recursive function to print out the tree inorder
static void on_inorder(fq_logger_t *l, struct btree_node *root)
{
	FQ_TRACE_ENTER(l);
    if( root != NULL ) {
        on_inorder(l,root->p_left);
        fq_log(l, FQ_LOG_DEBUG, "   %s.", root->value);     // string type
        on_inorder(l,root->p_right);
    }
	FQ_TRACE_EXIT(l);
}

//searches elements in the tree
void on_search(fq_logger_t *l, char* key, struct btree_node* leaf, Compare cmp)  // no need for **
{
    int res;

	FQ_TRACE_ENTER(l);

    if( leaf != NULL ) {
        res = cmp(key, leaf->value);
        if( res < 0)
            on_search(l, key, leaf->p_left, cmp);
        else if( res > 0)
            on_search(l, key, leaf->p_right, cmp);
        else
            fq_log(l, FQ_LOG_DEBUG, "'%s' found!", key);     // string type
    }
    else fq_log(l, FQ_LOG_DEBUG, "Not in tree.");

	FQ_TRACE_EXIT(l);
    return;
}

static void on_delete(fq_logger_t *l, struct btree_node** leaf)
{
	FQ_TRACE_ENTER(l);
    if( *leaf != NULL ) {
        on_delete(l, &(*leaf)->p_left);
        on_delete(l, &(*leaf)->p_right);
        free( (*leaf)->value );         // free the key
        free( (*leaf) );
    }
	FQ_TRACE_EXIT(l);
}

bool OpenBTree(fq_logger_t *l, fq_btree_obj_t **obj)
{
	fq_btree_obj_t	*rc=NULL;

	FQ_TRACE_ENTER(l);
	
	rc = (fq_btree_obj_t *)calloc(1, sizeof(fq_btree_obj_t));
	if(!rc) {
		fq_log(l, FQ_LOG_ERROR, "calloc() error.");
		FQ_TRACE_EXIT(l);
		return false;
	}

	rc->l = l;
	rc->length = 0;
    rc->p_root = NULL;

	rc->on_insert = on_insert;
	rc->on_inorder = on_inorder;
	rc->on_search = on_search;
	rc->on_delete = on_delete;

	*obj = rc;
	
	FQ_TRACE_EXIT(l);
	
	return true;
}
bool CloseBTree(fq_logger_t *l, fq_btree_obj_t *obj)
{
	struct btree_node **leaf = &obj->p_root;

	FQ_TRACE_ENTER(l);

    if( *leaf != NULL ) {
        obj->on_delete(l, &(*leaf)->p_left);
        obj->on_delete(l, &(*leaf)->p_right);
        free( (*leaf)->value );         // free the key
        free( (*leaf) );
    }

	if(obj) {
		free(obj);
		obj = NULL;
	}
	return true;
	FQ_TRACE_EXIT(l);
}

//compares value of the new node against the previous node
int CmpStr(const char *a, const char *b)
{
    return (strcmp (a, b));     // string comparison instead of pointer comparison
}

char *input( void )
{
	/* max key size is 256 */
	char *buf;
	buf = calloc(256, sizeof(char));
	getprompt( "Please enter a string :", buf, 256);
	return(buf);
}


#if 0
int main()
{
    char *value=NULL;
    char option[2];
	fq_logger_t *l=NULL;

	bool tf;
	fq_btree_obj_t *obj=NULL;
	tf = OpenBTree(l, &obj);
	CHECK(tf);

    while( option[0] != 'q' ) {
        //displays menu for program
        menu();

        //gets the char input to drive menu
		getprompt("Which do you want?", option, 1);

        if( option[0] == 'i') {
            value = input();
            printf ("Inserting %s\n", value);
            obj->on_insert(value,  &obj->p_root, (Compare)CmpStr);
			if(value) free(value);
        }
        else if( option[0] == 's' ) {
            value = input();
            obj->on_search(value, obj->p_root, (Compare)CmpStr);     // no need for **
			if(value) free(value);
        }
        else if( option[0] == 'p' ) {
            obj->on_inorder(obj->p_root);
        }
        else if( option[0] == 'f' ) {
            CloseBTree(l, obj);
            printf("Tree destroyed. Good bye.\n");
			break;
        }
        else if( option[0] == 'q' ) {
            printf("Quitting");
        }
    }
	return 0;
}
#endif
