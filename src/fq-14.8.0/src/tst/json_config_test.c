#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "json_config.h"
#include "fq_logger.h"
#include "fq_common.h"
#include "json_config.h"

int main(int ac, char **av)
{
	json_config_t *jc=NULL;
	bool	tf; /* true/false */
	int i;

	if( ac != 2) {
		printf("Usage: $ %s [config filename] <enter>\n", av[0]);
		return(0);
	}

	char *config_filename = av[1];

	tf = open_json_config(&jc, config_filename);
	TEST( tf == true );

	jc->on_print(jc);

	/* Access array */
	JSON_Array *array = json_object_get_array(jc->JObject, "Actors");
    for (i = 0; i < json_array_get_count(array); i++)
    {
        printf("[%d]-th:  %s\n", i, json_array_get_string(array, i));
    }

	/* Get inner value */
	char *code = (char *)json_object_dotget_string(jc->JObject, "Maps.KBStar");
	TEST(code != NULL);
	printf("bank code=[%s]\n", code);

	/* Get number from inner map */
	i = (int)json_object_dotget_number(jc->JObject, "Codes.A001");
	printf("code=[%d]\n", i);

	/* Get float value */
	float rate = (float)json_object_get_number(jc->JObject, "imdbRating");
	printf("rate=[%f]\n", rate);

	fq_log(jc->l, FQ_LOG_ERROR, "TEST");

	close_json_config(jc);

	return(0);
}
