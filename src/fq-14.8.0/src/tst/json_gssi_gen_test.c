#include <stdio.h>
#include "fq_json.h"
#include "fq_logger.h"
#include "fq_common.h"
#include "fq_gssi.h"

#define HASVALUE(ptr)   ((ptr) && (strlen(ptr) > 0))

int main(int ac, char  **av)
{
    int filesize;    // 문서 크기
    int i;
	int rc;
	fq_logger_t *l=NULL;
	char *template = NULL;
	char dst[8192];
	char *var_data = "choigwisang|shinhanbank|31|gmail|man|ilsan|jerry|arrm"; /* order: 1:name,2:bank,3:age,4:email, 5:sex, 6:address,7:dogname,8:patname */
	char *postdata = "name=choi&email=gwisang.choi@gmail.com";
	cache_t	*cache_short=NULL;


	if( ac != 2 ) {
		printf("Usage: $ %s  [json_template_file] <enter>\n", av[0]);
		exit(0);
	}

	rc = fq_open_file_logger(&l, "/tmp/json_gssi_gen_test.log", FQ_LOG_ERROR_LEVEL);
	CHECK(rc);

	if(!template) {
		template = readFile(l, av[1], &filesize ); //  char *readFile( fq_logger_t *l, char *filename, int *readSize)
		CHECK(template);
		CHECK( filesize > 0 );
	}

	rc = gssi_proc( l, template, var_data, postdata, cache_short,  '|', dst, sizeof(dst));

	if( rc < 0 ) {
		printf("gssi_proc() error( not found tag ). rc=[%d]\n", rc);
		return(-1);
	}

	printf("Success!!!\n");
	printf("result: dst=[%s]\n", dst);

	fq_close_file_logger(&l);

    return 0;
}

