#include "parson.h"
#include "unistd.h"
int main()
{

#if 0
	JSON_Value *json = json_value_init_object(); // make a json
	JSON_Object *root = json_value_get_object(json); // get oot pointer

	json_object_set_string(root, "type", "reply"); // put string to root

	JSON_Value *list = json_value_init_array(); // make array
	JSON_Array *array = json_value_get_array(list); // get array pointer
	json_array_append_string(array, "Str"); // p put string to array
	json_array_append_number(array,1);
	json_array_append_number(array,2);
	json_array_append_number(array,3);

 	json_object_set_value(root, "list", list); // put array to root
 	// json_object_dotset_value(root, "info.list", list);

	printf("JSON: %s\n", json_serialize_to_string_pretty(json));

#else

// loop:
	JSON_Value *json = json_value_init_object(); // make a json
	JSON_Object *root = json_value_get_object(json); // get oot pointer
    JSON_Value *branch = json_value_init_array();

    JSON_Array *array_contents = json_value_get_array(branch);

 	json_object_set_string(root, "mms_title", "download_list"); // put array to root
 	json_object_set_string(root, "mms_message", "We send list that you can download."); // put array to root
 	json_object_set_number(root, "contents_count", 3); // put array to root

	int i;
	JSON_Value *array_one = json_value_init_object();
	JSON_Object *leaf_object = json_value_get_object(array_one);

	for(i=0; i<3; i++) {

		json_object_set_number(leaf_object,"sn",i);
		json_object_set_string(leaf_object,"type","TXT");
		json_object_set_string(leaf_object,"kind","T");
		json_object_set_string(leaf_object,"path","/umsnas/contents");
		json_object_set_string(leaf_object,"filename","duc.txt");
		json_array_append_value(array_contents, array_one);
	}

 	json_object_set_value(root, "contents_array", branch); // put array to root

	char *dst = json_serialize_to_string_pretty(json);
	printf("JSON: %s\n", dst);
	json_free_serialized_string(dst);


	json_value_free(json);
	usleep(1000);
//	goto loop;

#endif

	return 0;
}
