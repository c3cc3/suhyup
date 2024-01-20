/*
 * $(HOME)/src/utl/enFQ.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include <dlfcn.h>

#include "fqueue.h"
#include "fq_common.h"

int main(int ac, char **av)  
{  
	int rc;
	void *handle;
	char *home = getenv("HOME");
	char	lib_path[256];
	char *p=NULL;
	int	 (*hello_fq)();
	char	*error=NULL;
	
	// first time check if path is not set
	if ((p = getenv("LD_LIBRARY_PATH"))==NULL) {
		//set it and re-execute the process

		printf("There is no LD_LIBRARY_PATH. I'll set LD_LIBRARY_PATH and restart.\n");
		sprintf(lib_path, "%s/tst11/lib",  home);
		setenv("LD_LIBRARY_PATH", lib_path, 1);
		execl(av[0], av[0], NULL);
	}
	else {
		printf("LD_LIBRARY_PATH='%s'\n", p);
	}

	// open the shared library the second time
	handle = dlopen("libfq.so", RTLD_LAZY);
	if (!handle) {
		fprintf(stderr, "dlopen() error. '%s'\n", dlerror());
		// fputs (dlerror(), stderr);
		return(-1);
	}

#if 0
	double (*cosine)(double);

	cosine = dlsym(handle, "cos");
    if ((error = dlerror()) != NULL)  {
    	fprintf (stderr, "dlsym() error(cos).");
        	exit(1);
    }
    printf ("%f\n", (*cosine)(2.0));

#else
	hello_fq = dlsym(handle, "hello_fq");
    if ((error = dlerror()) != NULL)  {
    	fprintf (stderr, "dlsym() error(cos).");
        	exit(1);
    }
	rc = (*hello_fq)();
	CHECK(rc == TRUE);
    printf ("Success\n");
#endif

	dlclose(handle);

	printf("Good bye !\n");

	return(0);
}
