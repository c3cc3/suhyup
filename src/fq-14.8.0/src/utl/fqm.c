#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include "fqueue.h"
#include "fq_manage.h"
#include "fq_info.h"
#include "fq_file_list.h"
#include "fq_scanf.h"

#if 0
typedef struct _queues_list_t queues_list_t;
struct _queues_list_t {
    char    qpath[256]; // deQ
    char    qname[256];
	int		mapping_num;
	int		multi_num;
	int		msglen;
};
#endif

static bool MakeLinkedList_from_list_file( fq_logger_t *l, char *list_file, char delimiter, linkedlist_t	*ll);
bool generate_fq_info_file_2( fq_logger_t *l, char *qpath, char *qname, int mapping_num, int multi_num, int msglen);

void Usage( char *progname )
{
		printf("Usage: $ %s [options] <enter>\n", progname);
		printf("Usage: $ %s -c [command] <enter>\n", progname);
		printf("Usage: $ %s -d [directory_name] [command] <enter>\n", progname);
		printf("Usage: $ %s -q [directory_name] [queue_name] [command] <enter>\n", progname);
		printf("Usage: $ %s -f [queue list file] [FQ_CREATE or FQ_UNLINK] <enter>\n", progname);
		printf("[commands]\n");
		printf("       FQ_GEN_INFO   : Generate a info file and add qname in ListFQ.info.\n");
		printf("       FQ_CREATE     : Create \n");
		printf("       FQ_UNLINK     : Unlink(delete) and remove qname in ListFQ.info. \n");
		printf("       FQ_DISABLE    : Disable \n");
		printf("       FQ_ENABLE     : Enable \n");
		printf("       FQ_RESET      : Reset \n");
		printf("       FQ_FLUSH      : Flush(de-queue) \n");
		printf("       FQ_INFO       : Info ( get size, usage...) \n");
		printf("       FQ_FORCE_SKIP : Force skip(de-queue one message) \n");
		printf("       FQ_DIAG       : Diagnostic \n");
		printf("       FQ_EXTEND     : Extend \n");
		printf("       FQ_DATA_VIEW  : view next data to be taken out(dequeu). \n");
		printf("       SHMQ_GEN_INFO : Generate a shmq info file  and add qname in ListSHMQ.info.\n");
		printf("       SHMQ_CREATE   : Shared Memeory Queue Create \n");
		printf("       SHMQ_UNLINK   : Shared Memeory Queue Unlink and remove qname in ListSHMQ.info.\n");
		printf("       SHMQ_RESET    : Reset \n");
		printf("       SHMQ_FLUSH    : Flush(de-queue) \n");
		printf("       FQ_EXPORT     : export queue_message to file. \n");
		printf("       FQ_IMPORT     : Please use /utl/mv_file_b_to_fq utility. \n");

		exit(0);
}

#define MAX_USERID_LENGTH (16)

int main(int ac, char **av)
{
	fq_container_t *fq_c=NULL;
	fq_cmd_t cmd;
	int rc;
	int ch;
	char	*directory_name=NULL;
	char	*queue_name=NULL;
	int		log_level = 0;
	fq_logger_t *l=NULL;

	if( (ac != 3) && (ac != 4) && (ac != 5) ) {
		Usage(av[0]);
	}

	char 	username[MAX_USERID_LENGTH];
	char 	logname[256];
	cuserid(username);
	sprintf( logname, "/tmp/fqm_%s.log", username);

	rc = fq_open_file_logger(&l, logname, log_level);
    CHECK( rc > 0 );

	fq_log(l, FQ_LOG_INFO, "fqm Version (%s). Compiled %s %s.", FQUEUE_LIB_VERSION, __DATE__, __TIME__);

	if( strcmp(av[1], "-f") != 0 ) {
		if( load_fq_container(l, &fq_c) != TRUE ) {
				fprintf(stderr, "load_fq_container() error. for detail, See your logfile('%s')\n", logname); 
				goto STOP;
		}
	}

	while( (ch=getopt(ac, av, "fcdq")) != -1 ) {
		switch(ch) {
			case 'f':
				if( ac != 4 ) Usage(av[0]);

				linkedlist_t *ll;
				bool tf;

				cmd = get_cmd( av[3]);

				ll = linkedlist_new("queues_list");
				tf = MakeLinkedList_from_list_file( l, av[2], '|', ll);
				CHECK(tf);
				ll_node_t *node_p = NULL;
				int t_no=0;

				for( node_p=ll->head; (node_p != NULL); (node_p=node_p->next, t_no++) ) {
					queues_list_t	*tmp;
					tmp = (queues_list_t *) node_p->value;
					 printf("\n[%d]-th queue: [%s] [%s] [%d] [%d] [%d]\n",
						t_no,
						tmp->qpath,
						tmp->qname,
						tmp->mapping_num,
						tmp->multi_num,
						tmp->msglen);
					if( cmd == FQ_CREATE ) {
						// create info file
						tf = generate_fq_info_file_2( l, tmp->qpath, tmp->qname, tmp->mapping_num, tmp->multi_num, tmp->msglen);
						if(tf == false ) {
							printf("Failed! Check directory_name or queue_name. See log file for detail infomation.\n");
							goto STOP;
						}
						printf("\t - info file('%s.Q.info' creation successful!\n", tmp->qname);

						fq_log(l, FQ_LOG_DEBUG, "info file('%s.Q.info') creation successful.", tmp->qname);

						// add a qname to ListFQ.info
						tf =  append_new_qname_to_ListInfo( l, tmp->qpath, tmp->qname);
						if(tf == false ) {
							printf("Failed! Already exist queue info. See log file for detail infomation.\n");
							goto STOP;
						}
						printf("\t - Successfully added a new queue('%s') to the list file(%s)!\n", tmp->qname, FQ_LIST_FILE_NAME);
						fq_log(l, FQ_LOG_DEBUG, "Successfully added a new queue('%s') to the list file(%s).", tmp->qname, FQ_LIST_FILE_NAME);
						// make datafile and headerfile

						rc = fq_queue_manage_with_queues_list_t( l, tmp, cmd);

						fprintf(stdout,"result: rc=[%d]\n",rc);
						if( rc < 0 ) {
							printf("\t - Failed to create a new queue('%s', '%s').\n", tmp->qpath, tmp->qname);
							goto STOP;
						}
						printf("\t - Successfully created a new queue('%s', '%s').\n", tmp->qpath, tmp->qname);
					
					} 
					else if( cmd == SHMQ_CREATE ) {
						// create info file
						tf = generate_fq_info_file_2( l, tmp->qpath, tmp->qname, tmp->mapping_num, tmp->multi_num, tmp->msglen);
						if(tf == false ) {
							printf("Failed! Check directory_name or queue_name. See log file for detail infomation.\n");
							goto STOP;
						}
						printf("\t - info file('%s.Q.info' creation successful!\n", tmp->qname);

						fq_log(l, FQ_LOG_DEBUG, "info file('%s.Q.info') creation successful.", tmp->qname);

						// add a qname to ListFQ.info
						tf =  append_new_qname_to_shm_ListInfo( l, tmp->qpath, tmp->qname);
						if(tf == false ) {
							printf("Failed! Already exist queue info. See log file for detail infomation.\n");
							goto STOP;
						}
						printf("\t - Successfully added a new queue('%s') to the list file(%s)!\n", 
								tmp->qname, SHMQ_LIST_FILE_NAME);
						fq_log(l, FQ_LOG_DEBUG, "Successfully added a new queue('%s') to the list file(%s).", 
								tmp->qname, SHMQ_LIST_FILE_NAME);

						// make datafile and headerfile
						rc = fq_queue_manage_with_queues_list_t( l, tmp, cmd);
						fprintf(stdout,"result: rc=[%d]\n",rc);
						if( rc < 0 ) {
							printf("\t - Failed to create a new shm queue('%s', '%s').\n", tmp->qpath, tmp->qname);
							goto STOP;
						}
						printf("\t - Successfully created a new shm queue('%s', '%s').\n", tmp->qpath, tmp->qname);
					} 
					
					else if( cmd == FQ_UNLINK ) {
						// unlink info
						char info_filename[1024];

						sprintf(info_filename,"%s/%s.Q.info", tmp->qpath, tmp->qname);
						rc = unlink(info_filename);
						CHECK(rc == 0 );

						
						// delete a line in ListFQ.info
						tf = delete_qname_to_ListInfo( l, 0,  tmp->qpath, tmp->qname); // 0: file queue, 1: shmq
						if(tf == false ) {
                            printf("Failed! to delete a qname in ListInfo. See log file for detail infomation.\n");
                            goto STOP;
                        }

						// unlink : remove a queue in ListFQ.info and remove header and body files.
						rc = fq_queue_manage_with_queues_list_t( l, tmp,  cmd);
						fprintf(stdout,"result: rc=[%d]\n",rc);
						if( rc < 0 ) {
							printf("\t - Failed to delete a file queue('%s', '%s').\n", tmp->qpath, tmp->qname);
							goto STOP;
						}
						printf("\t - Successfully deleted a file queue('%s', '%s').\n", tmp->qpath, tmp->qname);
					}
					else if( cmd == SHMQ_UNLINK ) {
						// unlink info
						char info_filename[1024];

						sprintf(info_filename,"%s/%s.Q.info", tmp->qpath, tmp->qname);
						rc = unlink(info_filename);
						CHECK(rc == 0 );

						
						// delete a line in ListFQ.info
						tf = delete_qname_to_ListInfo( l, 1, tmp->qpath, tmp->qname); // 1: SHMQ
						if(tf == false ) {
                            printf("Failed! to delete a qname in ListInfo. See log file for detail infomation.\n");
                            goto STOP;
                        }

						// unlink : remove a queue in ListFQ.info and remove header and body files.
						rc = fq_queue_manage_with_queues_list_t( l, tmp,  cmd);
						fprintf(stdout,"result: rc=[%d]\n",rc);
						if( rc < 0 ) {
							printf("\t - Failed to delete a shm queue('%s', '%s').\n", tmp->qpath, tmp->qname);
							goto STOP;
						}
						printf("\t - Successfully deleted a shm queue('%s', '%s').\n", tmp->qpath, tmp->qname);
					}
					else { // not support
						fprintf(stderr, "Not support command in -f option.\n"); 
						break;
					}
				}
				if( ll ) linkedlist_free(&ll);

				break;

			case 'c': // container( all )
				printf("-c option\n");
				if( ac != 3 ) Usage(av[0]);
				cmd = get_cmd( av[2]);
				if( cmd < 0 ) break;


				if( cmd == FQ_RESET ) {
					char yesno[2];
					getprompt("Warning: You can lost a first message. for safety, After RESET, You have to restart your processes. continue>(y/n):", yesno, 1); 
					if( yesno[0] == 'n' || yesno[0] == 'N' ) {
						return 0;
					}
				}

				rc = fq_container_manage(l, fq_c, cmd);
				fprintf(stdout,"result: rc=[%d]\n",rc);
				
				break;
			case 'd': // directory
				printf("ch=[%c] optarg=[%s] optind=[%d] \n", ch, optarg, optind);
				if( ac != 4 ) Usage(av[0]);
				directory_name = av[2];
				cmd = get_cmd(av[3]);
				if( cmd < 0 ) break;

				if( cmd == FQ_RESET ) {
					char yesno[2];
					getprompt("Warning: You can lost a first message. for safety, After RESET, You have to restart your processes. continue>(y/n):", yesno, 1); 
					if( yesno[0] == 'n' || yesno[0] == 'N' ) {
						return 0;
					}
				}

				rc = fq_directory_manage(l, fq_c, directory_name, cmd);
				fprintf(stdout,"result: rc=[%d]\n",rc);
				break;
			case 'q': // queue
				printf("ch=[%c] optarg=[%s] optind=[%d] \n", ch, optarg, optind);
				if( ac != 5 ) Usage(av[0]);
				directory_name = av[2];
				queue_name = av[3];
				if(strlen(queue_name) > 16) {
					printf("Qname max size is 16.");
					goto STOP;
				}
				cmd = get_cmd(av[4]);
				if( cmd < 0 ) break;

				printf("cmd = [%d]\n", cmd);

				if( cmd == FQ_GEN_INFO ) {
					bool_t tf;
					fq_info_t *t=NULL;

					t  = new_fq_info( "info");

					tf = generate_fq_info_file( l, directory_name, queue_name, t);
					if(tf == false ) {
						printf("Failed! Check directory_name or queue_name. See log file for detail infomation.\n");
						free_fq_info(&t);
						goto STOP;
					}
					free_fq_info(&t);

					tf = check_qname_in_listfile(l, directory_name, queue_name);
					if( tf == false ) {
						printf("Failed! Already exist queue in [%s]. See log file for detail infomation.\n",
							FQ_LIST_FILE_NAME);
						goto STOP;

					}
					tf =  append_new_qname_to_ListInfo( l, directory_name, queue_name);
					if(tf == false ) {
						printf("Failed! Already exist queue info. See log file for detail infomation.\n");
						goto STOP;
					}
					printf("Success generated '%s.Q.info and added qname in '%s'!\n", queue_name, FQ_LIST_FILE_NAME );
				}
				else if( cmd == SHMQ_GEN_INFO ) {
					fq_info_t *t=NULL;
					bool_t tf;

					t  = new_fq_info( "info");
					
					tf = generate_fq_info_file( l, directory_name, queue_name, t);
					if(tf == false ) {
						printf("Failed! Check directory_name or queue_name. See log file for detail infomation.\n");
						free_fq_info(&t);
						goto STOP;
					}
					free_fq_info(&t);

					tf = check_qname_in_shm_listfile(l, directory_name, queue_name);
					if( tf == false ) {
						printf("Failed! Already exist queue in [%s]. See log file for detail infomation.\n",
							FQ_LIST_FILE_NAME);
						goto STOP;

					}
					tf =  append_new_qname_to_shm_ListInfo( l, directory_name, queue_name);
					if(tf == false ) {
						printf("Failed! Already exist queue info. See log file for detail infomation.\n");
						goto STOP;
					}
					printf("Success generated '%s.Q.info and added qname in '%s'!\n", queue_name, SHMQ_LIST_FILE_NAME );
				}
				else if( cmd == FQ_DATA_VIEW ) {
					char *buf = 0x00;
					int return_length;
					long return_seq;
					bool big_flag;
					int rc;

					rc = ViewFileQueue(l, directory_name, queue_name, &buf, &return_length, &return_seq, &big_flag);
					printf("rc = %d result='%s'\n", rc, (rc>=0) ? "success": "failed");

					if( rc == 0 ) {
						printf("There is no data.(empty)\n");
					}
					else if( rc> 0 ) {
						printf("buf='%s', length='%d', seq='%ld'\n", buf, return_length, return_seq);
						if( big_flag == true ) {
							// char shell_cmd[1024];

							// sprintf(shell_cmd, "cat %s", buf);
							// system( shell_cmd );
							printf("This is a big message. You can view it with cat command.\n");
						}
					}
					else {
						printf("ViewFileQueue() error. See logfile.\n");
					}
				}
				else {
					if( cmd == FQ_RESET ) {
						char yesno[2];
						getprompt("Warning: You can lost a first message. for safety, After RESET, You have to restart your processes. continue>(y/n):", yesno, 1); 
						if( yesno[0] == 'n' || yesno[0] == 'N' ) {
							return 0;
						}
					}

					rc = fq_queue_manage( l, fq_c, directory_name, queue_name, cmd);
					fprintf(stdout,"result: rc=[%d]\n",rc);
					if( rc < 0 ) {
						fprintf(stderr, "failed.\n");
					}
					else {
						fprintf(stderr, "success.\n");
					}
				}
				break;
			default:
				Usage(av[0]);
		}
	}

STOP:
	if( l )  fq_close_file_logger(&l);
	if(fq_c) fq_container_free(&fq_c);

	return(0);
}
static bool MakeLinkedList_from_list_file( fq_logger_t *l, char *list_file, char delimiter, linkedlist_t	*ll)
{
	file_list_obj_t *filelist_obj=NULL;
	FILELIST *this_entry=NULL;
	int rc;
	int t_no;

	FQ_TRACE_ENTER(l);

	fq_log(l, FQ_LOG_DEBUG, "file='%s'" , list_file);

	rc = open_file_list(l, &filelist_obj, list_file);
	CHECK( rc == TRUE );

	printf("count = [%d]\n", filelist_obj->count);

	// filelist_obj->on_print(filelist_obj);

	this_entry = filelist_obj->head;
	for( t_no=0;  (this_entry->next && this_entry->value); t_no++ ) {
		char *errmsg = NULL;
		// printf("this_entry->value=[%s]\n", this_entry->value);

		char	qpath[256]; 
		char	qname[256];
		char	mapping_num[5]; 
		char	multi_num[6];
		char	msglen[8];

		int cnt;

		cnt = fq_sscanf(delimiter, this_entry->value, "%s%s%s%s%s", 
			qpath, qname, mapping_num, multi_num, msglen);

		CHECK(cnt == 5);

		fq_log(l, FQ_LOG_INFO, "qpath='%s', qname='%s', mapping_num='%s',  multi_num='%s', msglen='%s'.", 
			qpath, qname, mapping_num, multi_num, msglen);

		queues_list_t *tmp=NULL;
		tmp = (queues_list_t *)calloc(1, sizeof(queues_list_t));

		char	key[11];
		memset(key, 0x00, sizeof(key));
		sprintf(key, "%010d", t_no);

		sprintf(tmp->qpath, "%s", qpath);
		sprintf(tmp->qname, "%s", qname);
		tmp->mapping_num =  atoi(mapping_num);
		tmp->multi_num = atoi(multi_num);
		tmp->msglen = atoi(msglen);

		ll_node_t *ll_node=NULL;
		ll_node = linkedlist_put(ll, key, (void *)tmp, sizeof(char)+sizeof(queues_list_t));
		CHECK(ll_node);

		this_entry = this_entry->next;
	}

	if( filelist_obj ) close_file_list(l, &filelist_obj);

	fq_log(l, FQ_LOG_INFO, "ll->length is [%d]", ll->length);

	FQ_TRACE_EXIT(l);
	return true;
}

char* info_keywords[NUM_FQ_INFO_ITEM] = {
		"QNAME",			/* 0 */
        "QPATH",			/* 1 */
        "MSGLEN",			/* 2 */
		"MMAPPING_NUM",		/* 3 */
		"MULTI_NUM",		/* 4 */
		"DESC",				/* 5 */
		"XA_MODE_ON_OFF",	/* 6 */
		"WAIT_MODE_ON_OFF", /* 7 */
		"MASTER_USE_FLAG", /* 8 */
		"MASTER_HOSTNAME" /* 9 */
};

#if 1 
bool generate_fq_info_file_2( fq_logger_t *l, char *qpath, char *qname, int mapping_num, int multi_num, int msglen)
{
	int i;
	FILE *wfp = NULL;
	char info_filename[256];
	fq_info_t *t=NULL;

	FQ_TRACE_ENTER(l);

	t  = new_fq_info( "info");

	CHECK(t);

	sprintf(info_filename, "%s/%s.Q.info", qpath, qname);

	/* Check Already existing */
	if( is_file(info_filename) == TRUE ) {
		fq_log(l, FQ_LOG_INFO, "Already [%s] is exist.", info_filename);
		return true;
	}

	if( (wfp = fopen( info_filename, "w") ) == NULL) { 
		fq_log(l, FQ_LOG_ERROR, "fopen() error. reason=[%s].", strerror(errno));
		return false;
	}

	for (i=0; i<NUM_FQ_INFO_ITEM; i++) {
		switch (i) {
			case 0:
				t->item[i].str_value = strdup(qname);
				t->item[i].int_value = -1;
				break;
			case 1:
				t->item[i].str_value = strdup(qpath);
				t->item[i].int_value = -1;
				break;
			case 2:
				t->item[i].int_value = msglen;
				break;
			case 3:
				t->item[i].int_value = mapping_num;
				break;
			case 4:
				t->item[i].int_value = multi_num;
				break;
			case 5:
				t->item[i].str_value = strdup(qname);
				t->item[i].int_value = -1;
				break;
			case 6:
				t->item[i].str_value = strdup("ON");
				t->item[i].int_value = -1;
				break;
			case 7:
				t->item[i].str_value = strdup("OFF");
				t->item[i].int_value = -1;
				break;
		}
		if( t->item[i].int_value < 0 ) {
			fprintf(wfp, "%s = \"%s\"\n", info_keywords[i], VALUE(t->item[i].str_value) );
		}
		else {
			fprintf(wfp, "%s = \"%d\"\n", info_keywords[i], t->item[i].int_value);
		}
	}
	fflush(wfp);
	fclose(wfp);

	if(t) free_fq_info(&t);

	FQ_TRACE_EXIT(l);

	return true;
}
#endif
