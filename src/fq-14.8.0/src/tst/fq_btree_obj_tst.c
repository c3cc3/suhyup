#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "fq_common.h"
#include "fq_logger.h"
#include "fq_btree_obj.h"

//compares value of the new node against the previous node
static int CmpStr(const char *a, const char *b)
{
    return (strcmp (a, b));     // string comparison instead of pointer comparison
}

static char *input( void )
{
    /* max key size is 256 */
    char *buf;
    buf = calloc(256, sizeof(char));
    getprompt( "Please enter a string :", buf, 256);
    return(buf);
}

//displays menu for user
static void menu()
{
    printf("\nPress 'i' to insert an element\n");
    printf("Press 's' to search for an element\n");
    printf("Press 'p' to print the tree inorder\n");
    printf("Press 'f' to destroy current tree\n");
    printf("Press 'q' to quit\n");
}

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
            // obj->on_insert(value,  &obj->p_root, (Compare)CmpStr);
            obj->on_insert(l, value,  &obj->p_root, (Compare)strcmp);
			if(value) free(value);
        }
        else if( option[0] == 's' ) {
            value = input();
            // obj->on_search(value, obj->p_root, (Compare)CmpStr);     // no need for **
            obj->on_search(l, value, obj->p_root, (Compare)strcmp);     // no need for **
			if(value) free(value);
        }
        else if( option[0] == 'p' ) {
            obj->on_inorder(l, obj->p_root);
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
