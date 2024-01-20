#include <stdio.h>
#include "fq_common.h"
#include "fq_logger.h"
#include "fq_map_db.h"
#include "fq_sec_counter.h"

#define DB_FILE_PATH "."
#define DB_FILE_NAME "TST2"

void print_help(char *p)
{
	printf("Current DB name is '%s' in this program.\n", DB_FILE_NAME);

	printf("If there is no datafile and lock file('%s.map.DB', '.%s.flock' in directory('%s'), They will be created automatically.\n", DB_FILE_NAME, DB_FILE_NAME, DB_FILE_PATH);

	printf("Usage: $ %s -[i|u] [key] [value] <enter>\n", p);
	printf("Usage: $ %s -[d|r] [key] <enter>\n", p);
	printf("Usage: $ %s -a <enter>\n", p);
	printf("Usage: $ %s -s <enter>\n", p);
	printf("\t-i: insert.\n");
	printf("\t-d: delete.\n");
	printf("\t-u: update.\n");
	printf("\t-r: retrieve( get value ).\n");
	printf("\t-a: get all data.\n");
	printf("\t-s: show all data.\n");
	return;
}
int main(int ac, char **av)
{
	int i;
	int rc;
	map_db_obj_t *map_db_obj=NULL;
	fq_logger_t *l=NULL;
	char *logname="map_db_test.log";
	int     ch;
	char	cmd;
	char buf[MAX_VALUE_LEN+1];
	char	*key=NULL;
	char	*value=NULL;
	int		count=0;
	char	*all_string=NULL;
	char	*dst[MAX_RECORDS];
	
	
	if( ac < 2) {
		print_help(av[0]);
        return(0);
    }

	switch ( av[1][1] ) {
            case 'H':
            case 'h':
                print_help(av[0]);
                return(0);
            case 'i':
				cmd = 'i';
				if( ac != 4 ) {
					print_help(av[0]);
					return(0);
				}
				key = strdup(av[2]);
				value = strdup(av[3]);
                break;
            case 'd':
				if( ac != 3 ) {
					print_help(av[0]);
					return(0);
				}
				key = strdup(av[2]);
				cmd = 'd';
                break;
            case 'u':
				if( ac != 4 ) {
					print_help(av[0]);
					return(0);
				}
				key = strdup(av[2]);
				value = strdup(av[3]);
				cmd = 'u';
                break;
            case 'r':
				if( ac != 3 ) {
					print_help(av[0]);
					return(0);
				}
				key = strdup(av[2]);
				cmd = 'r';
                break;
            case 'a':
				if( ac != 2 ) {
					print_help(av[0]);
					return(0);
				}
				cmd = 'a';
                break;
            case 's':
				if( ac != 2 ) {
					print_help(av[0]);
					return(0);
				}
				cmd = 's';
                break;
            default:
                printf("[%c] is not available option.\n", ch);
                print_help(av[0]);
                return(0);
    }

	cmd = av[1][1];
	printf("cmd is [%c]\n", cmd);

	rc = fq_open_file_logger(&l, logname, FQ_LOG_TRACE_LEVEL);
    CHECK( rc > 0 );
	printf("log open success.\n");

	rc = open_map_db_obj( l, DB_FILE_PATH, DB_FILE_NAME, &map_db_obj);
    CHECK( rc == TRUE );
	printf("map_db open success.\n");
	
	switch(cmd) {
		case 'i': /* insert */
			printf("insert.\n");
			rc = map_db_obj->on_insert(l, map_db_obj, key, value);
			CHECK( rc == TRUE );
			break;
		case 'd': /* delete */
			printf("delete.\n");
			rc = map_db_obj->on_delete(l, map_db_obj, key);
			CHECK( rc == TRUE );
			break;
		case 'u': /* update */
			printf("update.\n");
			rc = map_db_obj->on_update(l, map_db_obj, key, value);
			CHECK( rc == TRUE );
			break;
		case 'r': /* retrieve */
			printf("retrieve.\n");
			rc = map_db_obj->on_retrieve(l, map_db_obj, key, buf);
			CHECK( rc == TRUE );
			printf("value is [%s]\n", buf);
			break;
		case 'a':
			printf("Get all string.\n");
			rc = map_db_obj->on_getlist(l, map_db_obj, &count, &all_string);
            CHECK( rc == TRUE );
            printf("count is [%d]\n", count);
			all_string[strlen(all_string)-1] = 0x00; /* remove last delimiter */
            printf("all stirng  is [%s]\n", all_string);

			rc = delimiter_parse(all_string, '`', count, dst);
			CHECK( rc == TRUE );
			for(i=0; i<count; i++) {
				printf("\t- '%s'\n", dst[i]);
				if(dst[i]) free(dst[i]);
			}

			if( all_string ) free(all_string);
            break;
		case 's':
			printf("Show data.\n");
			rc = map_db_obj->on_show(l, map_db_obj);
			CHECK(rc == TRUE);
			break;
		default:
			print_help(av[0]);
			goto stop;
	}

	printf("success rc=[%d]\n", rc);

stop:
	close_map_db_obj(l, &map_db_obj);
	fq_close_file_logger(&l);

	return(0);
}
