#define FQ_FSTAT_C_VERSION "1.0.0"

#include <stdio.h>
#include <pthread.h>
#include <dirent.h>

#include "fq_common.h"
#include "fq_logger.h"
#include "fq_fstat.h"
#include "fq_linkedlist.h"

static int get_mode_typeOfFile(mode_t mode)
{
    return (mode & S_IFMT);
}

static char *
get_str_typeOfFile(mode_t mode)
{
    switch (mode & S_IFMT) {
    case S_IFREG:
        return("regular file");
    case S_IFDIR:
        return("directory");
    case S_IFCHR:
        return("character-special device");
    case S_IFBLK:
        return("block-special device");
    case S_IFLNK:
        return("symbolic link");
    case S_IFIFO:
        return("FIFO");
    case S_IFSOCK:
        return("UNIX-domain socket");
    }
    return("unknown file type.");
}

static int get_int_typeOfFile(mode_t mode)
{
    switch (mode & S_IFMT) {
                case S_IFREG:
                        return(1);
                case S_IFDIR:
                        return(2);
                case S_IFCHR:
                        return(3);
                case S_IFBLK:
                        return(4);
                case S_IFLNK:
                        return(5);
                case S_IFIFO:
                        return(6);
                case S_IFSOCK:
                        return(7);
                default:
                        return(-1);
    }
}
static int my_select(const struct dirent *s_dirent)
{ /* filter */
    int g_my_select = DT_DIR;
    int g_my_select_mask = 0;

    if(s_dirent != ((struct dirent *)0)) {
        if((s_dirent->d_type & g_my_select) == g_my_select_mask)
            return(1);
    }
    return(0);
}

int fq_scandir_and_make_linked_list_with_sub_string(const char *s_path, int sub_dir_name_include_flag, char *find_sub_string,  linkedlist_t *ll)
{
	int s_check, s_index;
	struct dirent **s_dirlist;
	int found_count = 0;

	if( sub_dir_name_include_flag ) {
		s_check = scandir(s_path, (struct dirent ***)(&s_dirlist), 0, alphasort);
		printf("included sub-directoies.\n");
	}
	else {
		s_check = scandir(s_path, (struct dirent ***)(&s_dirlist), my_select, alphasort);
		printf("is not included sub-directoies.\n");
	}

	if(s_check >= 0) {
		(void)fprintf(stdout, "scandir result=%d\n", s_check);
		for(s_index = 0;s_index < s_check;s_index++) {
			struct stat fbuf;
			char fullname[512];

			sprintf(fullname, "%s/%s", s_path, (char *)s_dirlist[s_index]->d_name);

			if( stat(fullname, &fbuf) < 0 ) {
				fprintf(stderr, "stat() error. resson=[%s]\n", strerror(errno));
				continue;
			}

			(void)fprintf(stdout, "[%-30.30s] (%12ld)bytes. mode=[%d] Type=[%s] \n", 
					(char *)s_dirlist[s_index]->d_name, fbuf.st_size, get_mode_typeOfFile(fbuf.st_mode), get_str_typeOfFile(fbuf.st_mode) );

			char *value = "test";
			ll_node_t *node = NULL;
			if( find_sub_string ) {
				char *p = NULL;
			
				p = strstr( (char *)s_dirlist[s_index]->d_name, find_sub_string);
				if( p ) {
					ll_node_t *node = linkedlist_put(ll, (char *)s_dirlist[s_index]->d_name , value, strlen(value)+1);
					found_count++;
				}
			}
			else {
				ll_node_t *node = linkedlist_put(ll, (char *)s_dirlist[s_index]->d_name , value, strlen(value)+1);
				found_count++;
			}
			free((void *)s_dirlist[s_index]);
		}
		
		if(s_dirlist != ((void *)0)) {
			free((void *)s_dirlist);
		}
		(void)fprintf(stdout, "\x1b[1;35mTOTAL result\x1b[0m=%d\n", s_check);
	}
	return( found_count );
}
bool exist_queue_in_filelist( fq_logger_t *l, char *qname, linkedlist_t *files_ll )
{

	ll_node_t *p=NULL;
    fq_log(l, FQ_LOG_DEBUG, " list '%s' contains %d elements\n", files_ll->key, files_ll->length);
    for ( p=files_ll->head; p != NULL ; p = p->next ) {

		char *q = NULL;
		q = strstr(p->key, qname);
		int key_len;
		key_len = strlen(p->key);

		if( q &&  key_len > 18  ) { // found 
			return true;
		}
    }
	return false;
}

#if 0
int main(int ac, char **av)
{
	linkedlist_t *ll;
	int found_count = 0;

	ll = linkedlist_new("list_of_files");

	found_count = fq_scandir_and_make_linked_list_with_sub_string( av[1], 0, "MCA2HOST", ll);
	// found_count = fq_scandir_and_make_linked_list_with_sub_string( av[1], 0, 0, ll); // all scan

	printf( "found_count = [%d]\n", found_count);

	ll_node_t *node = linkedlist_find(0, ll , "extract_files_from_dir.c");
	if( node ) { //found
		printf("key='%s', value='%s'\n", node->key, (char *)node->value);
	}

	ll_node_t *p;
    fprintf(stdout, " list '%s' contains %d elements\n", ll->key, ll->length);
    for ( p=ll->head; p != NULL ; p = p->next ) {
        fprintf(stdout, " Address of %s is \'%p\'.\n", p->key, p->value);
    }
}
#endif
