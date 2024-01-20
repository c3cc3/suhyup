#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>    // bool, true, false가 정의된 헤더 파일
#include "parson.h"     // parson.h 헤더 파일 포함
#include "fq_common.h"     // parson.h 헤더 파일 포함

int main()
{
	int rc;
    JSON_Value *rootValue;
    JSON_Object *rootObject;

	rc = system("free");
	CHECK(rc==0);

    rootValue = json_value_init_object();             // JSON_Value 생성 및 초기화
    rootObject = json_value_get_object(rootValue);    // JSON_Value에서 JSON_Object를 얻음


    // 객체에 키를 추가하고 문자열 저장
    json_object_set_string(rootObject, "Title", "Inception");
    // 객체에 키를 추가하고 숫자 저장
    json_object_set_number(rootObject, "Year", 2010);
    json_object_set_number(rootObject, "Runtime", 148);
    // 객체에 키를 추가하고 문자열 저장
    json_object_set_string(rootObject, "Genre", "Sci-Fi");
    json_object_set_string(rootObject, "Director", "Christopher Nolan");

    json_object_dotset_string(rootObject, "Student.grade", "University");
    json_object_dotset_string(rootObject, "Student.class", "3-5");
    json_object_dotset_number(rootObject, "Student.number", 12);

    json_object_dotset_string(rootObject, "header.date", "20200302:121054");
    json_object_dotset_string(rootObject, "header.hostname", "ZEUS112");
    json_object_dotset_string(rootObject, "header.ip", "172.30.1.25");

    json_object_dotset_string(rootObject, "body.svcc", "iupd0001");
    json_object_dotset_string(rootObject, "body.userid", "c3cc3");
    json_object_dotset_number(rootObject, "body.amount", 153000);

    // 객체에 키를 추가하고 배열 생성
    json_object_set_value(rootObject, "Actors", json_value_init_array());
    // 객체에서 배열 포인터를 가져옴
    JSON_Array *actors = json_object_get_array(rootObject, "Actors");
    // 배열에 문자열 요소 추가
    json_array_append_string(actors, "Leonardo DiCaprio");
    json_array_append_string(actors, "Joseph Gordon-Levitt");
    json_array_append_string(actors, "Ellen Page");
    json_array_append_string(actors, "Tom Hardy");
    json_array_append_string(actors, "Ken Watanabe");

    // 객체에 키를 추가하고 숫자 저장
    json_object_set_number(rootObject, "imdbRating", 8.8);
    // 객체에 키를 추가하고 불 값 저장
    json_object_set_boolean(rootObject, "KoreaRelease", true);


	// array 에 key-value 값을 넣고자 할 때
	// json_array_append_value() 함수 이용


	char *buf=NULL;
	size_t buf_size;


	buf_size = json_serialization_size(rootValue) + 1;
	buf = calloc(sizeof(char), buf_size);
	
	json_serialize_to_buffer(rootValue, buf, buf_size);
	printf("buf=[%s]\n", buf);
	free(buf);
	buf =NULL;

	buf = json_serialize_to_string_pretty(rootValue);
	printf("buf=[%s]\n", buf);
	free(buf);

    // JSON_Value를 사람이 읽기 쉬운 문자열(pretty)로 만든 뒤 파일에 저장
    json_serialize_to_file_pretty(rootValue, "example.json");


    json_value_free(rootValue);    // JSON_Value에 할당된 동적 메모리 해제

	rc = system("free");
	CHECK(rc==0);

    return 0;
}
