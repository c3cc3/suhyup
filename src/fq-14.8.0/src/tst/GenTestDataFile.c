/*
** GenTestData.c
** 2019/03/19 made by Gwisang.
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "fq_logger.h"
#include "fq_common.h"
#include "fq_sequence.h"
#include "fq_timer.h"


#define D_LOGFILE "/tmp/GenTestData.log"
#define D_USER_GEN_MSG_LEN (1020)

void usage(char *progname)
{
	printf("$ %s [data_size] [count] [path/fiiename] <enter>\n", progname);
	printf("$ %s 4096 100000 /tmp/Testdata.dat <enter>\n", progname);
}

int main(int ac, char **av)
{
	fq_logger_t *l = NULL;
	sequence_obj_t *sobj = NULL;
	int rc;
	FILE *fp=NULL;
	int i;

	if(ac != 4 ) {
		usage(av[0]);
		return(0);
	}

	rc = fq_open_file_logger(&l, D_LOGFILE, FQ_LOG_ERROR_LEVEL);
	CHECK(rc>0);

	rc = open_sequence_obj(l, ".", "test_data_seq", "TEST",  14, &sobj);
	CHECK( rc==TRUE );

	fp = fopen( av[3], "w");
	CHECK( fp != NULL );

	for(i=0; i< atoi(av[2]); i++) {
		char str_seq[21];
		int rc2;
		char *buf = NULL;

		rc2 = sobj->on_get_sequence(sobj, str_seq, sizeof(str_seq));
		CHECK( rc2 == TRUE );

		buf = calloc(1, sizeof(char) * D_USER_GEN_MSG_LEN);
		memset(buf, 'D',  D_USER_GEN_MSG_LEN -1);
		buf[D_USER_GEN_MSG_LEN-1] = '\n';
		memcpy(buf, str_seq, strlen(str_seq));

		fwrite(buf, 1, D_USER_GEN_MSG_LEN, fp);
		if( buf ) free(buf);
	}

	fq_log(l, FQ_LOG_INFO, "finished successfully!");
	if( fp ) fclose(fp);
	fq_close_file_logger(&l);

	return(0);
}
