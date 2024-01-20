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

#define HASHOBJ_TST_C_VERSION "1.0,1"

typedef struct _input_param_t {
    char progname[64];
    char *logname;
    char *log_level; /* trace, debug, info, error, emerg  */
    char *hash_path;
    char *hash_name;
	char *key;
} input_param_t;

void init_in_params(input_param_t *in_params) 
{
	in_params->progname[0] = 0x00;
	in_params->logname = 0x00;
	in_params->log_level = 0x00;
	in_params->hash_path = 0x00;
	in_params->hash_name = 0x00;
	in_params->key = 0x00;

	return;
}

void print_help(char *p)
{
	char    *data_home=NULL;

	printf("Compiled on %s %s\n", __TIME__, __DATE__);

    data_home = getenv("FQ_HASH_HOME");
    if( !data_home ) {
		fprintf(stderr, "Please add FQ_HASH_HOME in your .bashrc or .profile.\n");
		return;
    }

	printf("\n\nUsage  : $ %s [-V] [-h] [-p hash_path] [-q hash_name] [-l logname] [-o loglevel]<enter>\n", p);
	printf("\t	-V: version \n");
	printf("\t	-h: help \n");
	printf("\t	-l: logfilename \n");
	printf("\t	-p: hash full path \n");
	printf("\t	-q: hash name \n");
	printf("\t	-o: log level ( trace|debug|info|error|emerg )\n");
	printf("example: $ %s -l /tmp/hashobj_test.log -p %s -q fq_mon_svr -o debug <enter>\n",  p, data_home);
	printf("example: $ %s -l /tmp/hashobj_test.log -p %s -q history -o debug <enter>\n",  p, data_home);
	printf("example: $ %s -l /tmp/hashobj_test.log -p %s -q ums -o debug <enter>\n",  p, data_home);
	return;
}
void print_version(char *p)
{
    printf("\nversion: %s.\n\n", HASHOBJ_TST_C_VERSION);
    return;
}

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

	input_param_t   in_params;

	printf("Compiled on %s %s\n", __TIME__, __DATE__);

	if( ac < 2 ) {
		print_help(av[0]);
        return(0);
    }
	init_in_params( &in_params );

	int ch;
	while(( ch = getopt(ac, av, "VvHh:l:p:q:o:")) != -1) {
		switch(ch) {
			case 'H':
            case 'h':
                print_help(av[0]);
                return(0);
            case 'V':
            case 'v':
                print_version(av[0]);
                return(0);
            case 'l':
                in_params.logname = strdup(optarg);
				break;
			case 'p':
				in_params.hash_path = strdup(optarg);
				break;
			case 'q':
				in_params.hash_name = strdup(optarg);
				break;
			case 'o':
				in_params.log_level = strdup(optarg);
				break;
			default:
				printf("ch=[%c]\n", ch);
				print_help(av[0]);
                return(0);
		}
	}

	if( !in_params.logname || !in_params.hash_path || !in_params.hash_name || !in_params.log_level) {
		fprintf(stderr, "ERROR: logname[%s], path[%s], qname[%s], log_level[%s] are mandatory.\n", 
			in_params.logname, in_params.hash_path, in_params.hash_name, in_params.log_level );
		print_help(av[0]);
		return(-1);
	}

	int i_log_level = get_log_level( in_params.log_level);
	rc = fq_open_file_logger(&l, in_params.logname, i_log_level);
    CHECK( rc > 0 );

	/* For creating, Use /utl/ManageHashMap command */
    rc = OpenHashMapFiles(l, in_params.hash_path, in_params.hash_name, &hash_obj);
    CHECK(rc==TRUE);

	Scan_Hash(l, hash_obj);

	hash_obj->h->on_print(l, hash_obj->h);

	while(1) {
		put_key = calloc(hash_obj->h->h->max_key_length+1, sizeof(char));
		get_key = calloc(hash_obj->h->h->max_key_length+1, sizeof(char));
		put_value = calloc(hash_obj->h->h->max_data_length+1, sizeof(char));
		get_value = calloc(hash_obj->h->h->max_data_length+1, sizeof(char));

		getprompt("Which do you want?(put/get/del/mon/con/end) select one:", select_str, 3);
		if( strncmp( select_str, "put", 3) == 0 ) {
			getprompt("Type a key for put : ", put_key, hash_obj->h->h->max_key_length);
			getprompt("Type a value for put : ", put_value, hash_obj->h->h->max_data_length);
			rc = PutHash(l, hash_obj, put_key, put_value);
			CHECK(rc==TRUE);
		} else if( strncmp( select_str, "get", 3) == 0 ) {
			getprompt("Type a key for get : ", get_key, hash_obj->h->h->max_key_length);
			rc = GetHash(l, hash_obj, get_key, &get_value);
			if( rc == TRUE ) {
				printf("found: value=[%s]\n", get_value);
			} else {
				printf("not found.\n");
			}
		} else if( strncmp( select_str, "con", 3) == 0 ) {
			getprompt("Type a key for get : ", get_key, hash_obj->h->h->max_key_length);
			while(1) {
				rc = GetHash(l, hash_obj, get_key, &get_value);
				if( rc == TRUE ) {
					printf("found: value=[%s]\n", get_value);
					sleep(1);
				} else {
					printf("not found.\n");
					break;
				}
			}
		} else if( strncmp( select_str, "del", 3) == 0 ) {
			getprompt("Type a key for del : ", get_key, hash_obj->h->h->max_key_length);
			rc = DeleteHash( l, hash_obj, get_key);
			if( rc == TRUE ) {
				printf("[%s] is deleted.\n", get_key);
			} else {
				printf("not found.\n");
			}
		} else if( strncmp(select_str, "end", 3) == 0 ) {
			printf("Good bye!\n");
			break;
		} else if( strncmp(select_str, "mon", 3) == 0 ) {
			while(1) {
				hash_obj->h->on_print(l, hash_obj->h);
#if 1
				
				time_t rawtime;
 				struct tm * timeinfo;
  				time ( &rawtime );
  				timeinfo = localtime ( &rawtime );
				char *time_str = asctime(timeinfo);
				time_str[strlen(time_str)-1] = '\0';
				printf("----------------------------------------------------------------------[%s]\n\n", time_str);
#else
				time_t now = time(NULL);
				char *time_str = ctime(&now);
				time_str[strlen(time_str)-1] = '\0';
				printf("----------------------------------------------------------------------[%s]\n\n", time_str);

#endif
				sleep(1);
			}
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
