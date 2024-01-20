/*
** cache_test.c
*/
#include <stdio.h>
#include "fq_cache.h"

int main(int ac, char **av)
{
	cache_t	*cache_short = NULL;
	cache_t	*cache_long = NULL;
	cache_t *cache_sys = NULL;
	int rc;

	char *p;

	cache_short = cache_new('S', "Short term cache");
	cache_long = cache_new('L', "Long term cache");
	cache_sys   = cache_new('P', "Session Information");

	cache_update(cache_short, "NAME", "gwisang.choi",0);
	cache_update(cache_short, "NAME", "gwisang.choi",0);

	p = cache_getval_unlocked(cache_short, "NAME");
	printf("p=[%s]\n", p);

	printf("first add same key\n");
	rc = cache_add( cache_short, "NAME1", "gwisang.choi", 0);
	if( rc < 0 ) {
		printf("cache_add() error.(dup)\n");
	}
	else {
		printf("cache_add() success.\n");
	}

	printf("second add same key\n");
	rc = cache_add( cache_short, "NAME1", "gwisang.choi", 0);
	if( rc < 0 ) {
		printf("cache_add() error.(dup)\n");
	}
	else {
		printf("cache_add() success.\n");
	}

	cache_free( &cache_short );
	cache_free( &cache_long );
	cache_free( &cache_sys );

	return(0);
}
