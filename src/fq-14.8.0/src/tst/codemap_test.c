#include <stdio.h>
#include "fq_codemap.h"
#include "fq_logger.h"
#include "fq_common.h"

int main(int ac , char **av)
{
	codemap_t *t=NULL;
	int rc;
	char filename[128];
	char value[128];
	char key[128];
	fq_logger_t *l=NULL;

	if( ac != 2 ) {
		printf("Usage; $ %s codemap_file <enter>\n", av[0]);
		return(0);
	}

	rc = fq_open_file_logger(&l, "codemap_test.log", FQ_LOG_TRACE_LEVEL);
	if(rc <0) {
		fprintf(stderr, "fq_open_file_logger() error.\n");
		return(0);
	}

	sprintf(filename, "%s", av[1]);
	t = new_codemap(l, "bankcode");

	if( (rc=read_codemap(l, t, filename)) < 0 ) {
		fprintf(stderr, "readcode_map('%s') failed\n", filename);
		return(FALSE);
	}

	rc = get_codeval(l, t, "02", value);
	CHECK(rc == 0);
	printf("value=[%s]\n", value);

	rc = get_codekey(l, t, "hana", key);
	CHECK(rc == 0);
	printf("key=[%s]\n", key);

	printf("Total items = [%d]\n", get_num_items(0x00, t));

	print_codemap(l, t, stdout);

	free_codemap(l, &t);

	fq_close_file_logger(&l);
	return(TRUE);
}
