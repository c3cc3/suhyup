/*
** qsort.c:17:5: error: 'for' loop initial declarations are only allowed in C99 mode
** qsort.c:17:5: note: use option -std=c99 or -std=gnu99 to compile your code
*/
#if 0
#include <stdio.h>
#include <stdlib.h>
int comp (const void * elem1, const void * elem2) 
{
    int f = *((int*)elem1);
    int s = *((int*)elem2);
    if (f > s) return  1;
    if (f < s) return -1;
    return 0;
}
int main(int argc, char* argv[]) 
{
    int x[] = {4,5,2,3,1,0,9,8,6,7};

    qsort (x, sizeof(x)/sizeof(*x), sizeof(*x), comp);

    for (int i = 0 ; i < 10 ; i++)
        printf ("%d ", x[i]);

    return 0;
}
#else
#include <stdio.h>
#include <stdlib.h>
int comp (const void * elem1, const void * elem2) 
{
    int f = *((int*)elem1);
    int s = *((int*)elem2);
    if (f > s) return  1;
    if (f < s) return -1;
    return 0;
}
int main(int argc, char* argv[]) 
{
	int i;
	int array_length;
    int x[] = {4,5,2,3,1,0,9,8,6,7,100,201,53,46,76,45,3,88,77,2,33,44,1020};

	array_length = sizeof(x)/sizeof(*x);
    qsort (x, array_length, sizeof(*x), comp);

    for (i = 0 ; i < array_length ; i++)
        printf ("%d ", x[i]);

	printf("\n");
    return 0;
}
#endif
#if 0
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int
cmpstringp(const void *p1, const void *p2)
{
   /* The actual arguments to this function are "pointers to
	  pointers to char", but strcmp(3) arguments are "pointers
	  to char", hence the following cast plus dereference */

   return strcmp(* (char * const *) p1, * (char * const *) p2);
}

int
main(int argc, char *argv[])
{
   int j;

   if (argc < 2) {
	   fprintf(stderr, "Usage: %s <string>...\n", argv[0]);
	   exit(EXIT_FAILURE);
   }

   qsort(&argv[1], argc - 1, sizeof(char *), cmpstringp);

   for (j = 1; j < argc; j++)
	   puts(argv[j]);
   exit(EXIT_SUCCESS);
}
#endif
