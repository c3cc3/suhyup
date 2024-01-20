/*
** gssi_test.c
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fq_gssi.h"
#include "fq_logger.h"
#include "fq_common.h"
#include "fq_cache.h"

int main(int ac, char **av)
{
	int rc;
	// char *template = "Hello %var5% ! %var2%, %var3%, %var4%, %var10%, %var6%, %name%, %cache sendate%, %email%," ;
	// char *template = "Hello %var5% ! %var2%, %var3%, %var4%, %var10%, %var6%, %name%, %email%, %cache senddate%" ;
	char *template = "%cache senddate%,  %var1%" ;
	char *var_data = "choigwisang|shinhanbank|31|gmail|man|ilsan|jerry|arrm";
	char *postdata = "name=choi&email=gwisang.choi@gmail.com";
	char dst[8192];
	char *logpath = ".";
	char *logname = "gssi_test.log";
	char *loglevel = "error";
	char logpathfile[256];
	fq_logger_t *l;
 	cache_t *cache_short = NULL;

	printf("template=[%s]\n", template);
	
	sprintf(logpathfile, "%s/%s", logpath, logname);
	rc = fq_open_file_logger(&l, logpathfile, get_log_level(loglevel));
	CHECK( rc > 0 );

loop:
	cache_short = cache_new('S', "Short term cache");

	
	char date[9], time[7];
	get_time(date, time);
	char send_time[16];
	sprintf(send_time, "%s%s", date, time);
	// cache_add(cache_short, "senddate", send_time, 0);
	// cache_update(cache_short, "kkk", "kkk-cache-value",0);
	cache_update(cache_short, "senddate", send_time, 0);

	rc = gssi_proc( l, template, var_data, postdata, cache_short, '|', dst, sizeof(dst));

	if( rc < 0 ) {
		printf("gssi_proc() error( not found tag ). rc=[%d]\n", rc);
		return(-1);
	}

	printf("Success!!!\n");
	printf("dst=[%s]\n", dst);
	
	cache_free( &cache_short );

	usleep(1000);
	// goto loop;

	return(0);
}
