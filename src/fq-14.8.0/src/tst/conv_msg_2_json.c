#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>    // bool, true, false가 정의된 헤더 파일
#include "parson.h"     // parson.h 헤더 파일 포함
#include "fq_common.h"     // parson.h 헤더 파일 포함
#include "fq_tokenizer.h" // Tokenize()

#define DELIMITER '|'

#define TIME_POSITION 		0
#define HOSTNAME_POSITION 	1
#define SVCNAME_POSITION  	2
#define ERRORCODE_POSITION 3
#define TELNUMBER_POSITION 	4
#define CITY_POSITION 		5

int main()
{

	char *msg = "20170331:121030|raspi|CRM01|0000|010-7202-1516|Seoul|end";
	fq_token_t t;
	char *dst=NULL;

	int rc;
    JSON_Value *rootValue;
    JSON_Object *rootObject;

	fq_logger_t *l=NULL;
	int position;


	rc = system("free");
	CHECK(rc==0);

	printf("------------------------------------------------------------\n");
	printf("msg=[%s]\n", msg);

	rc = fq_open_file_logger(&l, "/tmp/conv_msg_2_json.log", FQ_LOG_ERROR_LEVEL);
    CHECK(rc=TRUE);

    rootValue = json_value_init_object();             // JSON_Value 생성 및 초기화
    rootObject = json_value_get_object(rootValue);    // JSON_Value에서 JSON_Object를 얻음

	if( Tokenize(l, msg, DELIMITER, &t) < 0 ) {
        printf("Tokenize() error.\n");
        return(-1);
    }


	position = TIME_POSITION;
	if( GetToken(l, &t, position, &dst) < 0 ) {
        printf("GetToken(TIME) error.\n");
		return(-2);
    }
    json_object_set_string(rootObject, "Time", dst);
	free(dst);


	position = HOSTNAME_POSITION;
	if( GetToken(l, &t, position, &dst) < 0 ) {
        printf("GetToken(HOSTNAME) error.\n");
		return(-3);
    }
    json_object_set_string(rootObject, "Hostname", dst);
	free(dst);


	position = SVCNAME_POSITION;
	if( GetToken(l, &t, position, &dst) < 0 ) {
        printf("GetToken(SVCNAME) error.\n");
		return(-3);
    }
    json_object_set_string(rootObject, "ServiceName", dst);
	free(dst);

	position = ERRORCODE_POSITION;
	if( GetToken(l, &t, position, &dst) < 0 ) {
        printf("GetToken(ERRORCODE) error.\n");
		return(-3);
    }
    json_object_set_string(rootObject, "ErrorCode", dst);
	free(dst);


	char *buf=NULL;
	size_t buf_size;


	printf("------------------------------------------------------------\n");
	buf_size = json_serialization_size(rootValue) + 1;
	buf = calloc(sizeof(char), buf_size);
	json_serialize_to_buffer(rootValue, buf, buf_size);
	printf("json(is not pretty)=[%s]\n", buf);
	free(buf);
	buf =NULL;

	printf("------------------------------------------------------------\n");
	buf = json_serialize_to_string_pretty(rootValue);
	printf("json(pretty)=[%s]\n", buf);
	free(buf);

    // JSON_Value를 사람이 읽기 쉬운 문자열(pretty)로 만든 뒤 파일에 저장
    json_serialize_to_file_pretty(rootValue, "result.json");

    json_value_free(rootValue);    // JSON_Value에 할당된 동적 메모리 해제

	printf("------------------------------------------------------------\n");
	rc = system("free");
	CHECK(rc==0);

	fq_close_file_logger(&l);

    return 0;
}
