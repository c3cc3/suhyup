/*
** monitor.c
** Version4.0
** Description: This program sees status of all file queue semi-graphically.
** 
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <curses.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>

#include "fqueue.h"
#include "shm_queue.h"
#include "fq_socket.h"
#include "screen.h"
#include "fq_file_list.h"
#include "fuser_popen.h"
#include "fq_manage.h"
#include "parson.h"
#include "fq_external_env.h"
#include "wmonitor.h"
#include "fq_linkedlist.h"

#define HEAD_ROWS (6)

#define MONITOR_VERSION "3.0"

fq_logger_t *l = NULL;
size_t used_total_file_size=0;
screen_t s;
pthread_mutex_t mutex;
linkedlist_t	*ll=NULL;
int body_start_y =0; 

int winsize_columns=0;
int winsize_rows=0;
int current_x = 5;
int current_y = 0;
int opened = 0;
int available_disp_q_cnt;
bool	stop_key = false;
bool call_detail_flag = false;
int	detail_view_sn = 0;
char g_eth_name[16];

static void detail_view(linkedlist_t *ll, int sn);
static bool OpenFileQueues_and_MakeLinkedlist(fq_logger_t *l, fq_container_t *fq_c, linkedlist_t *ll, int winsize_columns, int winsize_rows);

typedef struct _mon mon_t;
struct _mon {
	int i;
	short x;
	short y;
	long before_en_sum;
	long before_de_sum;
	fqueue_obj_t *obj;
};

static bool my_scan_function( size_t value_sz, void *value)
{
	mon_t *tmp;
	tmp = (mon_t *) value;

	int x,y ;


	int my_x_sector = (tmp->i-1) / available_disp_q_cnt;

	y = body_start_y + ((tmp->i-1) % available_disp_q_cnt);
	x = my_x_sector * 65;
	int start_x = x;

	mvprintw(y, x, "%d", tmp->i); x += 3; x++;

	mvprintw(y, x, "%s", tmp->obj->qname); x += 16; x++;
	mvprintw(y, x, "%ld", tmp->obj->on_get_diff(0, tmp->obj)); x += 6; x++;

    clrtoeol();

	float usage;
	usage = tmp->obj->on_get_usage(0, tmp->obj);
	if(usage >= 20.0 && usage < 50.0) {
		init_pair(1, COLOR_GREEN, COLOR_BLACK);
		attrset(COLOR_PAIR(1)); /* set new color attribute */
		mvprintw(y, x, "%5.2f", usage); x += 6; x++;
		attrset(COLOR_PAIR(0)); /* set old color attribute */
	} else if( usage >= 50.0 && usage < 80.0 ) {
		init_pair(2, COLOR_YELLOW, COLOR_BLACK);
		attrset(COLOR_PAIR(2)); /* set new color attribute */
		mvprintw(y, x, "%5.2f", usage); x += 6; x++;
		attrset(COLOR_PAIR(0)); /* set old color attribute */
	} else if( usage >= 80.0 ) {
		init_pair(3, COLOR_RED, COLOR_BLACK);
		attrset(COLOR_PAIR(3)); /* set new color attribute */
		mvprintw(y, x, "%5.2f", usage); x += 6; x++;
		attrset(COLOR_PAIR(0)); /* set old color attribute */
	} else {
		mvprintw(y, x, "%5.2f", usage); x += 6; x++;
	}

	long en_tps=0;
	long de_tps=0;

	if( tmp->before_en_sum > 0 ) {
		en_tps = tmp->obj->h_obj->h->en_sum - tmp->before_en_sum;
		tmp->before_en_sum = tmp->obj->h_obj->h->en_sum;
	} else {
		tmp->before_en_sum = tmp->obj->h_obj->h->en_sum;
	}
	
	if( tmp->before_de_sum > 0 ) {
		de_tps = tmp->obj->h_obj->h->de_sum - tmp->before_de_sum;
		tmp->before_de_sum = tmp->obj->h_obj->h->de_sum;
	} else {
		tmp->before_de_sum = tmp->obj->h_obj->h->de_sum;
	}
	
	if( en_tps > 0 ) {
		init_pair(1, COLOR_GREEN, COLOR_BLACK);
		attrset(COLOR_PAIR(1));
		mvprintw(y, x, "%ld", en_tps); x += 4; x++;
		attrset(COLOR_PAIR(0));
	} else {
		mvprintw(y, x, "%ld", en_tps); x += 4; x++;
	}

	if( de_tps > 0 ) {
		init_pair(4, COLOR_GREEN, COLOR_BLACK);
		attrset(COLOR_PAIR(4));
		mvprintw(y, x, "%ld", de_tps); x += 4; x++;
		attrset(COLOR_PAIR(0));
	} else {
		mvprintw(y, x, "%ld", de_tps); x += 4; x++;
	}

	x += 5;

	long en_competition = tmp->obj->on_check_competition( l, tmp->obj, FQ_EN_ACTION);
	mvprintw(y, x, "%ld", en_competition); x += 6; x++;
	long de_competition = tmp->obj->on_check_competition( l, tmp->obj, FQ_DE_ACTION);
	mvprintw(y, x, "%ld", de_competition); x += 6; x++;
	mvprintw(y, start_x+64, "|");

	return(true);
}

#define HEAD_WIDE 65

bool print_head(fq_logger_t *l, int *body_start_y)
{
    char    szTime[30];
    long    sec, usec;
	int	y=0, x=0;

    memset(szTime, 0x00, sizeof(szTime));
    MicroTime(&sec, &usec, szTime);
    move(y++,x);
    clrtoeol();
	winsize_rows = get_row_winsize();
	winsize_columns = get_col_winsize();
    mvprintw(0, 0, "%s -refresh interval(%d)sec. cols=%d, rows=%d version=[%s]", szTime, 1, winsize_columns, winsize_rows, MONITOR_VERSION);

	char hostname[64];
	int fd;
	struct ifreq ifr;
	char IPbuffer[32];
	// char *eth_name = "eno1";

	gethostname(hostname, 64);

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	ifr.ifr_addr.sa_family = AF_INET;
	snprintf(ifr.ifr_name, IFNAMSIZ, "%s", g_eth_name);
	// int snprintf(char *str, size_t size, const char *format, ...);
	ioctl(fd, SIOCGIFADDR, &ifr);

	sprintf(IPbuffer, "%s", inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));
	close(fd);

    mvprintw(y++, 0, "- Hostname: %s ", hostname); 
    mvprintw(y++, 0, "- IP address: %s ,  Number of Queues: %d", IPbuffer, opened);

	char *fq_data_home = getenv("FQ_DATA_HOME");
	if( !fq_data_home ) {
		fq_log(l, FQ_LOG_ERROR, "There is no FQ_DATA_HOME in your env values.");
		return false;
	}
	mvprintw(y++, 0, "- FQ_DATA_HOME: %s,  Allocated Disk: %ld", fq_data_home, used_total_file_size);

	
    char    Center_Title[128];
	int		solution_no;


    sprintf(Center_Title, "[ SAMSUNG LogCollector FileQueue Status. library version %s ] hotkey: 'quit='q', detail= SN number enter", FQUEUE_LIB_VERSION);
	init_pair(2, COLOR_YELLOW, COLOR_BLACK);
	attrset(COLOR_PAIR(2)); /* set new color attribute */
    center_disp(y++, Center_Title);
    draw_bar(y++, 0);
	attrset(COLOR_PAIR(0)); /* set old color attribute */

	attron(A_BOLD);

	int title_rows = y;
	available_disp_q_cnt = winsize_rows - title_rows;
	available_disp_q_cnt = available_disp_q_cnt -2;

	int can_disp_queues = available_disp_q_cnt * (winsize_columns/HEAD_WIDE);
	int need_title_counts = (opened / available_disp_q_cnt)+1;

	 mvprintw(y++, 0, "opend=[%d], title_rows=[%d], available_disp_q_cnt=[%d], can_disp_queues=[%d], need_title_count=[%d], body_start_y=[%d], HEAD_WIDE=[%d]", 
			opened, title_rows, available_disp_q_cnt, can_disp_queues, need_title_counts, y+2, HEAD_WIDE);

	int i;
	for(i=0; i<need_title_counts; i++) {
		x = i*HEAD_WIDE;
		mvprintw(y, x, "%s", "SN");  x+=3; x++;
		mvprintw(y, x, "%s", "QNAME"); x += 16; x++;
		mvprintw(y, x, "%s", "GAP"); x += 6; x++;
		mvprintw(y, x, "%s", "Usage"); x += 6; x++;
		mvprintw(y, x, "%s", "in/out(TPS)"); x += 13; x++;
		mvprintw(y, x, "%s", "in/out(lock)"); x += 4; x++;
	}
	y++;
	
	*body_start_y = y;

#if 0
	if( opened > (winsize_rows-HEAD_ROWS) ) {
		x = winsize_columns / 2;
		mvprintw(y, x, "%s", "SN");  x+=2; x++;
		mvprintw(y, x, "%s", "QNAME"); x += 16; x++;
		mvprintw(y, x, "%s", "GAP"); x += 6; x++;
		mvprintw(y, x, "%s", "Usage"); x += 6; x++;
		mvprintw(y, x, "%s", "in/out(TPS)"); x += 13; x++;
		mvprintw(y, x, "%s", "in/out(lock)"); x += 4; x++;
	}
#endif

	attroff(A_BOLD);

	refresh();

	return true;
}

static void *waiting_key_thread(void *arg)
{
    int key;
	char number_key[4];
	int number_key_cnt = 0;

	memset(number_key, 0x00, sizeof(key));

	sleep(2);
    while(1) {
		int n=0;

		pthread_mutex_lock(&mutex);
		move( winsize_rows, winsize_columns);
		n = get_number_win();
		fq_log(l, FQ_LOG_EMERG, "n=[%d]",n);
		if(n > 0 && n < opened+1 ) {
			call_detail_flag = true;
			detail_view_sn = n;
			// detail_view(ll, n);
			memset(number_key, 0x00, sizeof(key));
			pthread_mutex_unlock(&mutex);
			while(call_detail_flag == true) {
				usleep(10000);
			}
			continue;
		}
		else if( n == 999 ) { /* quit : 'q' */
			stop_key = true;
			break;
        }
		else {
			call_detail_flag = false;
			pthread_mutex_unlock(&mutex);
			continue;
		}
    }

    endwin();
    exit(0);
	// pthread_mutex_unlock(&mutex);
    // pthread_exit((void *)0);
}

static fqueue_obj_t *find_obj_in_linkedlist ( linkedlist_t *ll, int sn)
{
	ll_node_t *p;
	mon_t *tmp=NULL;

	for(p=ll->head; p!=NULL; p=p->next) {
		tmp = (mon_t *) p->value;
		if(tmp->i == sn) {
			return(tmp->obj);
		}
	}
	return(NULL);
}

#include<sys/utsname.h>   /* Header for 'uname'  */

static void detail_view(linkedlist_t *ll, int sn)
{
    int x=0, y=0;
	struct utsname uname_pointer;
	int i;
	long i_before_en = 0;
	long i_before_de = 0;
	fqueue_obj_t *obj=NULL;

	obj = find_obj_in_linkedlist(ll, sn);
	if( obj == NULL ) {
		clear();
		mvprintw(0, 0, "Can't found file queue object. Press any key.");
		getch();
		return;
	}

	uname(&uname_pointer);

	while(1) {
		int input_key;
		char *fuser_out = NULL;
		int rc;

		clear();
		// disp_time(y++, x);
		mvprintw(y++, x, "%s", "---------------< Status of File Queue>--------------");
		mvprintw(y++, x, "- path        : %s", obj->path);
		mvprintw(y++, x, "- qname       : %s", obj->qname);
		mvprintw(y++, x, "- description : %s", obj->h_obj->h->desc);
		mvprintw(y++, x, "- header file : %s", obj->h_obj->name);
		mvprintw(y++, x, "- data   file : %s", obj->d_obj->name);
		mvprintw(y++, x, "- message size: %ld", obj->h_obj->h->msglen);
		mvprintw(y++, x, "- file size   : %ld", (long int)obj->h_obj->h->file_size);
		mvprintw(y++, x, "- multi num   : %ld", (long int)obj->h_obj->h->multi_num);
		mvprintw(y++, x, "- mapping num : %ld", (long int)obj->h_obj->h->mapping_num);
		mvprintw(y++, x, "- mapping len : %ld", (long int)obj->h_obj->h->mapping_len);
		mvprintw(y++, x, "- records     : %d", obj->h_obj->h->max_rec_cnt);
		mvprintw(y++, x, "- income(TPD) : %ld-(%ld)", obj->h_obj->h->en_sum, (i_before_en>0)?(obj->h_obj->h->en_sum-i_before_en): i_before_en);
		i_before_en = obj->h_obj->h->en_sum;
		mvprintw(y++, x, "- outcome(TPD): %ld-(%ld)", obj->h_obj->h->de_sum, (i_before_de>0)?(obj->h_obj->h->de_sum-i_before_de): i_before_de);
		i_before_de = obj->h_obj->h->de_sum;
		mvprintw(y++, x, "- diff        : %ld", obj->on_get_diff(NULL, obj));
		mvprintw(y++, x, "- BIG files   : %d", obj->h_obj->h->current_big_files);
	
		char *p=NULL;
		mvprintw(y++, x, "- last en time: %s", (p=asc_time(obj->h_obj->h->last_en_time)));
		if(p) free(p);
		mvprintw(y++, x, "- last de time: %s", (p=asc_time(obj->h_obj->h->last_de_time)));
		if(p) free(p);

		rc = fuser_popen( obj->d_obj->name, &fuser_out);
		if( rc > 0 ) {
			mvprintw(y++, x, "- using pids  : %s", fuser_out);
			SAFE_FREE(fuser_out);
		}
		else {
			mvprintw(y++, x, "- using pids  : %s", "none");
		}

		y++;
		mvprintw(y,x, "Press space key for refreshing or quit(q)");
		refresh();

		input_key = getch();

		if(input_key == 'q') break;
		else {
			y = 0;
			continue;
		}
	}
	fq_log(l, FQ_LOG_EMERG, "exit detail_view().");
    refresh();
    clear();
    refresh();
    return;
}

int main(int ac, char **av)
{
    int rc;
	fq_container_t *fq_c=NULL;

	/* shared memory */
    key_t     key=5678;
    int     sid = -1;
    int     size;
	external_env_obj_t *external_env_obj = NULL;

	rc = fq_open_file_logger(&l, "./monitor.log", FQ_LOG_TRACE_LEVEL);
    CHECK(rc == TRUE);

	/* here error : core dump */
	rc = load_fq_container(l, &fq_c);
	CHECK( rc == TRUE );

	ll = linkedlist_new("monitor");

	winsize_columns = get_col_winsize();
	winsize_rows = get_row_winsize();

	bool tf;
	tf = OpenFileQueues_and_MakeLinkedlist(l, fq_c, ll, winsize_columns, winsize_rows);
	CHECK(tf == true);

	char *fq_data_home = getenv("FQ_DATA_HOME");
	if( !fq_data_home ) {
		fq_log(l, FQ_LOG_ERROR, "There is no FQ_DATA_HOME in your env values.");
		return false;
	}

	open_external_env_obj( l, fq_data_home, &external_env_obj);
	if( external_env_obj ) {
		external_env_obj->on_get_extenv( l, external_env_obj, "eth", g_eth_name);
	}

#if 0
	/* check screen size */
	if( winsize_columns < 120 ) { 
		printf("Please increase your terminal columns size.(more than 120)\n");
		exit(0);
	}
	if( (winsize_rows-HEAD_ROWS) < (opened/2 + 1) ) {
		printf("Please increase your terminal rows size.(more than %d)\n", (opened/2)+HEAD_ROWS);
		exit(0);
	}
#endif

	init_screen(&s);

	pthread_mutex_init(&mutex, NULL);
	fq_log(l, FQ_LOG_DEBUG, "winsize_columns=[%d], winsize_rows=[%d].\n", winsize_columns, winsize_rows);

#if 1
	pthread_t thread_cmd;
	int thread_id;
	pthread_attr_t attr;

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_create(&thread_cmd, &attr, waiting_key_thread, &thread_id);
#endif

	int old_row_wsize=0, old_col_wsize=0;
    int cur_row_wsize=0, cur_col_wsize=0;

	while(1) {
		cur_row_wsize = get_row_winsize();
        cur_col_wsize = get_col_winsize();

		if( old_row_wsize == 0 || old_col_wsize == 0 ) {
            old_row_wsize = cur_row_wsize;
            old_col_wsize = cur_col_wsize;
        }
        else {
            if( (old_row_wsize != cur_row_wsize) || (old_col_wsize != cur_col_wsize) ) {
                fq_log(l, FQ_LOG_INFO, "Changed: rows=[%d], cols=[%d].", cur_row_wsize, cur_col_wsize);
                clear();
                refresh();
            }

            old_row_wsize = cur_row_wsize;
            old_col_wsize = cur_col_wsize;
		}
		
		if(call_detail_flag) {
			detail_view(ll, detail_view_sn);
			call_detail_flag = false;
			continue;
		}
		else {
			// pthread_mutex_lock(&mutex);
			/*
			** Head
			*/

			print_head(l, &body_start_y);

			int ch;
			linkedlist_callback(ll, my_scan_function );

			if(stop_key == true ) { 	
				break;
			}
			else { 
				sleep(1);
			}
			// pthread_mutex_unlock(&mutex);
		}
	}

	if(ll) linkedlist_free(&ll);

	if(l) fq_close_file_logger(&l);

	end_screen(&s, 0);
    return(0);
}

static bool OpenFileQueues_and_MakeLinkedlist(fq_logger_t *l, fq_container_t *fq_c, linkedlist_t *ll, int winsize_columns, int winsize_rows)
{
	dir_list_t *d=NULL;
	fq_node_t *f;
	ll_node_t *ll_node = NULL;
	int sn=1;

	for( d=fq_c->head; d!=NULL; d=d->next) {
		for( f=d->head; f!=NULL; f=f->next) {
			fqueue_obj_t *obj=NULL;
			int rc;
			mon_t *tmp = NULL;

			obj = calloc(1, sizeof(fqueue_obj_t));

			fq_log(l, FQ_LOG_DEBUG, "OPEN: [%s/%s].", d->dir_name, f->qname);
			
			char *p=NULL;
			if( (p= strstr(f->qname, "SHM_")) == NULL ) {
				rc =  OpenFileQueue(l, NULL, NULL, NULL, NULL, d->dir_name, f->qname, &obj);
			} 
			else {
				rc =  OpenShmQueue(l, d->dir_name, f->qname, &obj);
			}
			if( rc != TRUE ) {
				fq_log(l, FQ_LOG_ERROR, "OpenFileQueue('%s') error.", f->qname);
				return(false);
			}
			fq_log(l, FQ_LOG_DEBUG, "%s:%s open success.", d->dir_name, f->qname);

			/*
			** Because of shared memory queue, We change it to continue.
			*/
			// CHECK(rc == TRUE);

			char	key[36];
			sprintf(key, "%s/%s", d->dir_name, f->qname);
			tmp = calloc(1, sizeof(mon_t));
			tmp->i = sn++;
			if( sn > (winsize_rows-HEAD_ROWS) ) {
				tmp->y = (sn % (winsize_rows-HEAD_ROWS)) + HEAD_ROWS;
				tmp->x = winsize_columns / 2;
			}
			else {
				tmp->y = (sn-1)+HEAD_ROWS;
				tmp->x = 0;
			}

			tmp->before_en_sum = obj->h_obj->h->en_sum;
			tmp->before_de_sum = obj->h_obj->h->de_sum;

			tmp->obj = obj;

            used_total_file_size += obj->h_obj->h->file_size;

			size_t value_size=sizeof(int)+sizeof(short)*2+sizeof(long)*2+sizeof(fqueue_obj_t);

			ll_node = linkedlist_put(ll, key, (void *)tmp, value_size);

			if(!ll_node) {
				fq_log(l, FQ_LOG_ERROR, "linkedlist_put('%s', '%s') error.", d->dir_name, f->qname);
				return(false);
			}

			// printf("tmp.i=[%d], name->[%s/%s], tmp->obj->h_obj->h->msglen=[%zu]\n", 
			// 	tmp->i, tmp->obj->path, tmp->obj->qname, tmp->obj->h_obj->h->msglen);

			if(tmp) free(tmp);

			opened++;
		}
	}

	fq_log(l, FQ_LOG_INFO, "Number of opened filequeue is [%d].", opened);

	return(true);
}
