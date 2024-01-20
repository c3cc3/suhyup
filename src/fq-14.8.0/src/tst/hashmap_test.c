/*
 * A unit test and example of how to use the simple C hashmap
 */

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "fq_hashmap.h"
#include "fq_common.h"

#define KEY_MAX_LENGTH (256)
#define KEY_PREFIX ("somekey")
#define KEY_COUNT (1024)

typedef struct data_struct_s
{
    char key_string[KEY_MAX_LENGTH];
    int number;
} data_struct_t;

int main(char* argv, int argc)
{
    int index;
    int error;
    map_t mymap;
    char key_string[KEY_MAX_LENGTH];
    data_struct_t* value;
	char dump_filename[128];
	int init_elements = 256;
	int max_key_length = KEY_MAX_LENGTH;
    
    mymap = hashmap_new(256, max_key_length);
	step_print("hashmap_new()", "OK");

    /* First, populate the hash map with ascending values */
    for (index=0; index<KEY_COUNT; index+=1)
    {
        /* Store the key string along side the numerical value so we can free it later */
        value = malloc(sizeof(data_struct_t));
        snprintf(value->key_string, KEY_MAX_LENGTH, "%s%d", KEY_PREFIX, index);
        value->number = index;

        error = hashmap_put(mymap, value->key_string, value, sizeof(data_struct_t));
		// printf("put(): key=[%s] [%d]-th\n", value->key_string, value->number);
        assert(error==MAP_OK);
    }
	step_print("hashmap_put() testing", "OK");

	sprintf(dump_filename, "%s", "/tmp/hashmap.dump");
	// hashmap_dump(mymap, dump_filename);
	step_print("hashmap_dump() testing", "OK");

    /* Now, check all of the expected values are there */
    for (index=0; index<KEY_COUNT; index+=1)
    {
        snprintf(key_string, KEY_MAX_LENGTH, "%s%d", KEY_PREFIX, index);

        error = hashmap_get(mymap, key_string, (void**)(&value));
		printf("hashmap_get(): value:[%s] number=[%d]\n", value->key_string, value->number);
        
        /* Make sure the value was both found and the correct number */
        assert(error==MAP_OK);
        assert(value->number==index);
    }
	step_print("hashmap_get() testing", "OK");
    
    /* Make sure that a value that wasn't in the map can't be found */
    snprintf(key_string, KEY_MAX_LENGTH, "%s%d", KEY_PREFIX, KEY_COUNT);

    error = hashmap_get(mymap, key_string, (void**)(&value));
        
    /* Make sure the value was not found */
    assert(error==MAP_MISSING);
	step_print("hashmap_get() testing", "OK");

    /* Free all of the values we allocated and remove them from the map */
    for (index=0; index<KEY_COUNT; index+=1)
    {
        snprintf(key_string, KEY_MAX_LENGTH, "%s%d", KEY_PREFIX, index);

        error = hashmap_get(mymap, key_string, (void**)(&value));
        assert(error==MAP_OK);

        error = hashmap_remove(mymap, key_string);
        assert(error==MAP_OK);

        free(value);        
    }
	step_print("hashmap_remove() testing", "OK");
    
    /* Now, destroy the map */
    hashmap_free(mymap);
	step_print("hashmap_free() testing", "OK");

    return 1;
}
