/*
** This program is a sample to de_queue with Entrust File Libraries(version 8.0).
** retrieveMon.c
**
*/

#include <stdio.h>
#include "fqueue.h"
#include "fq_logger.h"
#include "fq_common.h"
#include "fq_timer.h"
#include "fq_tokenizer.h"

extern struct timeval tp1, tp2;
char g_ProgName[64];

const char *delimeter="`";
const char *delimeter_bub=",";

#include <stdio.h>
#include <sys/types.h>

#include "fqueue.h"
#include "fq_monitor.h"
#include "fq_common.h"

int main(int ac, char **av)  
{  
	int rc;
	fq_logger_t *l=NULL;
	char	*logname="/tmp/retrieveMon.log";
	monitor_obj_t 	*obj=NULL;
	int		use_rec_count;
	char	*all_string=NULL;

	printf("Compiled on %s %s\n", __TIME__, __DATE__);

	rc = fq_open_file_logger(&l, logname, FQ_LOG_TRACE_LEVEL);
	CHECK( rc > 0 );

	rc =  open_monitor_obj(l,  &obj);
	CHECK(rc==TRUE);

	while(1) {
		int size=0;
      	tokened *tokened_head=NULL,*cur=NULL;

		if( obj->on_getlist_mon(l, obj, &use_rec_count, &all_string) < 0 ) {
            fprintf(stderr, "on_getlist_mon() error.\n");
            break;
        }
        fprintf(stdout, "on_getlist_mon() OK.[%d][%s]\n", use_rec_count, all_string);

		tokenizer(all_string, delimeter, &tokened_head); /* extract 1 row from all_string by delimeter */
		cur=tokened_head;
		while(cur!=NULL){
			char *buf=NULL;

			size=(cur->end - cur->sta); /* length of 1 row */
			buf = calloc( size+1, sizeof(char));
			memcpy(buf, cur->sta, size);


			if( cur->type !=TOK_TOKEN_O ) {
				int sub_size=0;
				tokened *sub_tokened_head=NULL, *sub_cur=NULL;

				printf("[%s]\n", buf);

				tokenizer(buf, delimeter_bub, &sub_tokened_head);
				sub_cur = sub_tokened_head;
				while(sub_cur != NULL ) {
					char *sub_buf=NULL;

					sub_size = (sub_cur->end - sub_cur->sta);
					sub_buf=calloc( sub_size+1, sizeof(char));

					memcpy(sub_buf,sub_cur->sta,sub_size);

					if( sub_cur->type !=TOK_TOKEN_O ) {
						printf("\t-[%s]\n", sub_buf);
					}

					sub_cur = sub_cur->next;
					SAFE_FREE(sub_buf);
				}
				tokenizer_free(sub_tokened_head);
			}

			cur=cur->next;
			SAFE_FREE(buf);
		}
		tokenizer_free(tokened_head);
        SAFE_FREE(all_string); /* Don't forget it. */
        sleep(1);
	}

	rc = close_monitor_obj(l, &obj);
	CHECK(rc==TRUE);

	if( all_string ) free(all_string);

	fq_close_file_logger(&l);

	printf("OK.\n");
	return(rc);
}
