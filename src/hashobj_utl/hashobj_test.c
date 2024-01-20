/*
** hashobj_test.c
** Descriptions:
	hash ojbect can use as simplae Database.
	Saved data don't delete when A process was stopped.
	It is difference to cache.
*/

#include <stdio.h>
#include <fq_hashobj.h>
#include <fq_common.h>
#include <fq_logger.h>

int main(int ac, char **av)
{
	int rc;
	fq_logger_t *l=NULL;
	hashmap_obj_t *hash_obj=NULL;
	char *logname = "/tmp/hashobj_test.log";
	char *put_key=NULL;
	char *get_key=NULL;
	char *put_value=NULL;
	char *get_value=NULL;
	char	select_str[4];

	char *hash_path = NULL;
	char *hash_name = NULL;

	if(ac != 3 ) {
		printf("Usage: $ %s [hash_path] [hash_name] <enter>\n", av[0]);
		printf("ex   : $ %s /ums/hash fq_mon_svr <enter>\n", av[0]);
		exit(0);
	}

	rc = fq_open_file_logger(&l, logname, FQ_LOG_TRACE_LEVEL);
    CHECK( rc > 0 );

	hash_path = av[1];
	hash_name = av[2];

	/* For creating, Use /utl/ManageHashMap command */
    rc = OpenHashMapFiles(l, hash_path, hash_name, &hash_obj);
    CHECK(rc==TRUE);

	printf("========================= %s( length = %d, elements=[%d)) ================================\n", hash_obj->name, hash_obj->h->h->table_length, hash_obj->h->h->curr_elements);
	int data_count = 0;
	int index;
	for(index=0; index<hash_obj->h->h->table_length; index++) {

		char  *key=NULL;
		unsigned char *value=NULL;
		unsigned char *p=NULL;

		key = calloc(hash_obj->h->h->max_key_length+1, sizeof(char));
		value = calloc(hash_obj->h->h->max_data_length+1, sizeof(unsigned char));

		rc = hash_obj->on_get_by_index(l, hash_obj, index, key, value);
		if( rc == MAP_MISSING ) {
			SAFE_FREE(key);
			SAFE_FREE(value);
			continue;
		}
		else {
			p = (unsigned char *)value;
			data_count++;
			printf( "key=[%s], value=[%s]\n", key, p);
		}
	}
	printf("record count=[%d]\n", data_count);

	while(1) {
		put_key = calloc(hash_obj->h->h->max_key_length+1, sizeof(char));
		get_key = calloc(hash_obj->h->h->max_key_length+1, sizeof(char));
		put_value = calloc(hash_obj->h->h->max_data_length+1, sizeof(char));
		get_value = calloc(hash_obj->h->h->max_data_length+1, sizeof(char));

		getprompt("Which do you want?(put/get/del/end/mon) select one:", select_str, 3);
		if( strncmp( select_str, "put", 3) == 0 ) {
			getprompt("Type a key for put : ", put_key, hash_obj->h->h->max_key_length);
			getprompt("Type a value for put : ", put_value, hash_obj->h->h->max_data_length);
			rc = PutHash(l, hash_obj, put_key, put_value);
			CHECK(rc==TRUE);
		} else if( strncmp( select_str, "get", 3) == 0 ) {
			getprompt("Type a key for get : ", get_key, hash_obj->h->h->max_key_length);

			hash_obj->flock->on_flock(hash_obj->flock);
			rc = GetHash(l, hash_obj, get_key, &get_value);
			if( rc == TRUE ) {
				printf("found: value=[%s]\n", get_value);
			} else {
				printf("not found.\n");
			}
			hash_obj->flock->on_funlock(hash_obj->flock);

		} else if( strncmp( select_str, "del", 3) == 0 ) {
			getprompt("Type a key for del : ", get_key, hash_obj->h->h->max_key_length);
			rc = DeleteHash( l, hash_obj, get_key);
			if( rc == TRUE ) {
				printf("[%s] is deleted.\n", get_key);
			} else {
				printf("not found.\n");
			}
		} else if( strncmp( select_str, "mon", 3) == 0 ) {
			getprompt("Type a key for monitoring : ", get_key, hash_obj->h->h->max_key_length);
			while(1) {
				rc = GetHash(l, hash_obj, get_key, &get_value);
				if( rc == TRUE ) {
					if( get_value[0] == 0x00 ) {
						printf("ERROR: value is null\n");
						usleep(100000);
						continue;
					}

					printf("found: value=[%s]\n", get_value);
					usleep(1000000);
				} else {
					printf("not found.\n");
				}
			}
		} else if( strncmp(select_str, "end", 3) == 0 ) {
			printf("Good bye!\n");
			break;
		} else {
			printf("Unknown keyword.\n");
			continue;
		}
		SAFE_FREE(put_key);
		SAFE_FREE(put_value);
		SAFE_FREE(get_key);
		SAFE_FREE(get_value);
	}
	SAFE_FREE(put_key);
	SAFE_FREE(put_value);
	SAFE_FREE(get_key);
	SAFE_FREE(get_value);

	rc = CloseHashMapFiles(l, &hash_obj);
    CHECK(rc==TRUE);

    fq_close_file_logger(&l);

	return 0;
}
