#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "parson.h"
#include "fq_logger.h"
#include "fq_common.h"
#include "json_config.h"

static int on_print( json_config_t *obj )
{
	char *out = json_serialize_to_string_pretty(obj->JValue);
	printf("---------<%s>------------\n", obj->filename);
	printf("%s\n", out);
	printf("-----------------------------\n");
	if(out) free(out);
  	return 0;
}

bool open_json_config( json_config_t **json_config, char *config_filename)
{
	json_config_t *jc;
	JSON_Value *JValue;
    JSON_Object *JObject;

	jc = calloc( 1, sizeof(json_config_t));

	TEST( config_filename != NULL);

	jc->filename = strdup(config_filename);

	JValue = json_parse_file(jc->filename);
	TEST(JValue != NULL);

	JObject = json_value_get_object( JValue);
	TEST(JObject != NULL);

	/* check default items */
	/* log filename */
	char *log_filename = (char *)json_object_dotget_string(JObject, "log.LogFilename");
	jc->log_filename = strdup(log_filename);
	TEST( jc->log_filename != NULL);
	/* log level */
	char *log_level = (char *)json_object_dotget_string(JObject, "log.LogLevel");
	TEST( log_level != NULL);
	jc->log_level = get_log_level(log_level);

	int rc = fq_open_file_logger(&jc->l, jc->log_filename, jc->log_level);
	TEST( rc > 0 );

	jc->JValue = JValue;
	jc->JObject = JObject;

	jc->on_print = on_print;

	*json_config = jc;

	// json_value_free(jc->JValue);

	return(true);
}

int close_json_config( json_config_t *jc )
{
	if(jc->l) fq_close_file_logger( &jc->l);
	if(jc->filename) free(jc->filename);
	if(jc->log_filename) free(jc->log_filename);
	json_value_free(jc->JValue);
	free(jc);
}
