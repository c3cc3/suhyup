#include <stdio.h>
#include "fq_tci.h"
#include "fq_common.h"

#include "fq_conf.h"
#include "fq_codemap.h"
#include "fq_cache.h"

int main(int ac, char **av)
{
	int rc, len;
	char	*errmsg=NULL;

	qformat_t *q=NULL;
	aformat_t *a=NULL;
	char	*qformat_file="qformat.sample";
	char	*aformat_file="aformat.sample";
	char	buf_length[9];
	char	buf_gen_ymd[9];
	char	buf_gen_hms[14];
	char	buf_node_id[16];
	char	buf_NAME[11];

	/* output */
	unsigned char	*packet=NULL;

	/* sources: postdata, received body data, cache(t), SYS_DATE, SYS_UUID */
	char	*postdata = "NAME=Gwisang&AGE=47";
	char	*body="0123456788901234567890123456789012345678901234567890123456789";
	fqlist_t  *t=NULL;
	
	q = new_qformat(qformat_file, &errmsg);
	CHECK(q);

	step_print( "new_qformat()", "OK");

	rc=read_qformat( q, qformat_file, &errmsg);
	CHECK(rc >= 0);
	step_print( "read_qformat()", "OK");

	a = new_aformat(aformat_file, &errmsg);
	CHECK(a);
	step_print( "new_aformat()", "OK");

	rc = read_aformat(a, aformat_file, &errmsg);
	if(rc < 0 ) {
		printf("read_aformat() error. errmsg=[%s]\n", errmsg);
		goto stop;
	}
	step_print( "read_aformat()", "OK");

loop:
	t = fqlist_new('A', "TEST");
	CHECK(t);

	// fqlist_add(t, "gen_hms", "121212.001234", 0);
	fqlist_update(t, "gen_hms", "121212.001234", 0);

	fill_qformat(q, postdata, t, body);
	step_print( "fill_qformat()", "OK");

	len = assoc_qformat(q, NULL);
	CHECK(len > 0 );
	step_print( "assoc_qformat()", "OK");

	packet = malloc(len+1);
	memset(packet, 0x00, len+1);
	memcpy(packet, q->data, q->datalen);

	printf("-----< qformat output value >------\n");
	printf("\t-data:[%s]\n", q->data);
	printf("\t-len=[%d]\n", q->datalen);
	/*
	** Send data to other server or Meessaging Queue
	*/

	/*
	** Receive data from other server by TCP or Messaging Queue.
	*/
	a->data = strdup(q->data);
	a->datalen = q->datalen;

	len = assoc_aformat(a, &errmsg);
	if( len < 0 ) {
		printf("assoc_aformat() error. errmsg=[%s]\n", errmsg);
		goto stop;
	}
	step_print( "assoc_aformat()", "OK");
	printf("We make a->postdata with a->data by assoc_aformat().\n");
	printf("postdata: %s\n", a->postdata);

	get_avalue(a, "length", buf_length);
	get_avalue(a, "gen_ymd", buf_gen_ymd);
	get_avalue(a, "gen_hms", buf_gen_hms);
	get_avalue(a, "node_id", buf_node_id);
	str_lrtrim(buf_node_id);
	get_avalue(a, "NAME", buf_NAME);
	str_lrtrim(buf_NAME);

	step_print( "get_avalue()", "OK");

	printf("-----< aformat values >------\n");
	printf("\t- length=[%s]\n", buf_length);
	printf("\t- gen_ymd=[%s]\n", buf_gen_ymd);
	printf("\t- gen_hms=[%s]\n", buf_gen_hms);
	printf("\t- node_id=[%s]\n", buf_node_id);
	printf("\t- NAME=[%s]\n", buf_NAME);

	printf("success!!!\n");
	if(t) fqlist_free(&t);

	if( a->data ) {
		free(a->data);
		a->data = NULL;
	}

	SAFE_FREE(errmsg);
	SAFE_FREE(packet);
	usleep(1000);
	goto loop;

stop:	

	if(q) free_qformat(&q);
	if(a) free_aformat(&a);

	printf("good bye...\n");
	return(0);
}
