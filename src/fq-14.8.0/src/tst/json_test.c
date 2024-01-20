#include <stdio.h>
#include "fq_json.h"
#include "fq_logger.h"
#include "fq_common.h"

#define HASVALUE(ptr)   ((ptr) && (strlen(ptr) > 0))

int main()
{
    int size;    // 문서 크기
    int i;
	int rc;
	fq_logger_t *l=NULL;

	rc = fq_open_file_logger(&l, "/tmp/json_test.log", FQ_LOG_TRACE_LEVEL);
	CHECK(rc);

	
    // 파일에서 JSON 문서를 읽음, 문서 크기를 구함
    char *doc = (char *)readFile( l, "example.json", &size);
    if (doc == NULL)
        return -1;

    JSON json = { 0, };    // JSON 구조체 변수 선언 및 초기화

    parseJSON( l, doc, size, &json);    // JSON 문서 파싱

#if 0
    for(i=0; i<TOKEN_COUNT;i++) {
        if( !HASVALUE(json.tokens[i].string) ) {
            continue;
        }
        printf("[%d]-th token: [%s].\n", i, json.tokens[i].string);
    }
#endif

    printf("Title: %s\n", getString(l, &json, "Title"));           // Title의 값 출력
    printf("Year: %d\n", (int)getNumber(l, &json, "Year"));        // Year의 값 출력
    printf("Runtime: %d\n", (int)getNumber(l, &json, "Runtime"));  // Runtime의 값 출력
    printf("Genre: %s\n", getString(l, &json, "Genre"));           // Genre의 값 출력
    printf("Director: %s\n", getString(l, &json, "Director"));     // Director의 값 출력

    printf("Actors:\n");
    int actors = getArrayCount(l, &json, "Actors");                // Actors 배열의 개수를 구함

    for ( i = 0; i < actors; i++)                            // 배열의 요소 개수만큼 반복
        printf("  %s\n", getArrayString(l, &json, "Actors", i));   // 인덱스를 지정하여 문자열을 가져옴

    printf("imdbRating: %f\n", getNumber(l, &json, "imdbRating")); // imdbRating의 값 출력

    freeJSON( l, &json);    // json 안에 할당된 동적 메모리 해제

    free(doc);    // 문서 동적 메모리 해제

	fq_close_file_logger(&l);

    return 0;
}

