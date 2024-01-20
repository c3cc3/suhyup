#include "fq_logger.h"
#include "fq_common.h"
#include "fq_tokenizer.h"
#include "fq_codemap.h"

int main(int ac, char **av)
{
	char *test = "20170331:121030|raspi|CRM01|0000|010-7202-1516|Seoul|end";
	char *dst=NULL;
	codemap_t *c=NULL;
	fq_token_t t;
	fq_logger_t *l=NULL;
	int rc;
	char key[16];
	char value[128];
	int  nth_position;
	char *codemap_filename=NULL;
	int i;

	if( ac != 2 ) {
		printf("Usage: $ %s [codemap_filename] <enter>\n", av[0]);
		return(0);
	}

	rc = fq_open_file_logger(&l, "/tmp/tokenizer_test.log", FQ_LOG_ERROR_LEVEL);
	CHECK(rc=TRUE);


	c = new_codemap(l, "CRM.MSG");
	CHECK(c);


	codemap_filename  = av[1];

	rc=read_codemap(l, c, codemap_filename);
	CHECK(rc >= 0);
		

	if( Tokenize(l, test, '|', &t) < 0 ) {
		printf("Tokenize() error.\n");
		return(-1);
	}

#if 0
	/* get position value with name  in code_map */
	rc = get_codekey(l, c, "SERVICE_CODE", key);
	CHECK(rc == 0);

	nth_position = atoi(key);
	CHECK(nth_position >= 0);

	if(nth_position > c->map->list->length) {
		fprintf(stderr, "Position value of a map is error.\n");
		return(-1);
	}

	if( GetToken(l, &t, nth_position, &dst) < 0 ) {
		printf("GetToken() error.\n");
		rc = -2;
	}
	else {
		printf("dst=[%s]\n", dst);
		free(dst);
		rc = 0;
	}
#endif

	/* compare code_map and the message */
	if( t.n_delimiters+1 != c->map->list->length) {
		printf("Check map file[%s] or message protocols. \n",  codemap_filename);
		return(-1);
	}

	/* print all data */
	printf("----- All fields ----- \n");
	for(i=0; i<c->map->list->length; i++) {
		char sz_key[128];
		char sz_value[256];

		sprintf(sz_key, "%02d", i);
		get_codeval(l, c, sz_key, sz_value);
		// printf("sz_key[%s], sz_value=[%s]\n", sz_key, sz_value);

		if( GetToken(l, &t, i, &dst) < 0 ) {
			printf("GetToken() error.\n");
			rc = -2;
		}
		else {
			printf("[%02d]-th :  name=[%s] dst=[%s]\n", i, sz_value, dst);
			free(dst);
			rc = 0;
		}
	}


	fq_close_file_logger(&l);

	return(rc);
}
