/*
** monitor.c
** Description: This program sees status of all file queue semi-graphically.
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

#include "fqueue.h"
#include "shm_queue.h"
#include "fq_socket.h"
#include "screen.h"
#include "fq_file_list.h"
#include "fuser_popen.h"
#include "fq_manage.h"
#include "parson.h"
#include "fq_external_env.h"
#include "monitor.h"
#include "fq_hashobj.h"
#include "fq_monitor.h"


/* Customizing Area */
#define PRODUCT_NAME "M&Wise UMS Distributor"
/* Please type your ethernet device name with  $ ip addr command */
#define ETHERNET_DEVICE_NAME "eno1"

#define CURSOR_X_POSITION (50)  // key waiting cursor position
#define MAX_FILES_QUEUES (100)
#define USAGE_THRESHOLD (90.0)
#define DISK_THRESHOLD (1500000)

#define MONOTORING_WAITING_MSEC (1000000)

static bool check_alive_fq_mon_svr();
static void *main_thread( void *arg );
static void main_title(int y, char *fname);
static void disp_time(int y, int x);
static void disp_hostname_ip(int y, int x, int opened_q_cnt);
static void disp_queue_data_home_dir(int y, int x, char *qpath);
static void *waiting_key_thread(void *arg);
static void help(void);
static void disp_GetAvailableSpace(int y, int x, const char* path);
static void detail_view(char key);

char g_eth_name[16];

bool_t user_quit_flag = 0;

pthread_mutex_t mutex;
screen_t    s;
int y_status;
int get_y_position;
int current_sleep = 1;
int reset_flag=0;


char *g_path = NULL;
char *g_hash_path = NULL;
char *g_hash_name = "fq_mon_svr";
char fq_data_home[16+1];


int main(int ac, char **av)
{
    int rc;
    pthread_t thread_cmd, thread_main;
    int thread_id;

	/* shared memory */
    key_t     key=5678;
    int     sid = -1;
    int     size;

	if( ac == 1 ) {
		g_path = getenv("FQ_DATA_HOME");
		if( !g_path ) {
			getprompt("There is no FQ_DATA_HOME in env. Type your FQ_DATA_HOME: ", fq_data_home, 16);
			g_path = fq_data_home;
		}
	} else if( ac == 2 ) {
		g_path = av[1];
	} else {
		fprintf(stderr, "Usage: $ %s [FQ_DATA_HOME]", av[0]);
		exit(0);
	}

	g_hash_path = getenv("FQ_HASH_HOME");
	if(!g_hash_path) {
		fprintf(stderr, "There is no FQ_HASH_HINE in your env. Please run after setting.");
		exit(0);
	}

	bool tf;
	tf = check_alive_fq_mon_svr();
	if( tf == false ) {
		fprintf(stderr, "There is no fq_mon_svr(process). Run fq_mon_svr and restart it\n.");
		CHECK(tf);
		return(0);
	}

    rc = pthread_mutex_init (&mutex, NULL);
    if( rc !=0 ) {
        perror("pthread_mutex_init"), exit(0);
    }

    init_screen(&s);

    pthread_create(&thread_main, NULL, main_thread, &thread_id);
	sleep(2);
    pthread_create(&thread_cmd, NULL, waiting_key_thread, &thread_id);

    pthread_join(thread_cmd, NULL);

    end_screen(&s, 0);

    return(0);
}

#define MAX_COLUMNS 15
int	screen_format[MAX_COLUMNS+1] = { 1, 9, 25, 35, 45, 53, 62, 71, 79,  87,  97,  110, 124};
								   /*0, 1, 2,  3,  4,  5,  6,  7,  8,   9,   10,  11, 12*/

void sub_title( int py ) {
	int i;

	attron(A_BOLD);
	// attron(A_REVERSE);
	for(i=0; (i< MAX_COLUMNS && screen_format[i]>0) ; i++) {
		move(py, screen_format[i]);
		switch(i) {
			case 0:
				printw("%s", "NO");
				break;
			case 1:
				printw("%s", "QNAME");
				break;
			case 2:
				printw("%s", "MAX_RECO");
				break;
			case 3:
				printw("%s", "GAP");
				break;
			case 4:
				printw("%s", "Usage");
				break;
			case 5:
				printw("%s", "enQ_lock");
				break;
			case 6:
				printw("%s", "deQ_lock");
				break;
			case 7:
				printw("%s", "BIG");
				break;
			case 8:
				printw("%s", "TPS(en)");
				break;
			case 9:
				printw("%s", "TPS(de)");
				break;
			case 10:
				printw("%s", "en_sum");
				break;
			case 11:
				printw("%s", "de_sum");
				break;
			case 12:
				printw("%s", "Q_path");
				break;
			default:
				break;
		}
	}
	attroff(A_BOLD);
	// attron(A_REVERSE);
	return;
}

int display_screen( int py, HashQueueInfo_t Array[], int index)  {
	int i;

	move(py, 0); clrtoeol(); /* Clear to the end of the line */

	for(i=0; (i< MAX_COLUMNS && screen_format[i]>0) ; i++) {
		move(py, screen_format[i]);
		switch(i) {
			case 0:
				if( index >= 0 && index <=9 ) {
					if( Array[index].status == 0 ) {
						printw("%d(%c)", index, index+48);
					}
					else {
						init_pair(1, COLOR_RED, COLOR_BLACK);
						attrset(COLOR_PAIR(1));
						printw("%d(%c)-X", index, index+48);
						attrset(COLOR_PAIR(0));
					}
				}
				else { 
					if( Array[index].status == 0 ) {
						printw("%d(%c)", index, index+55);
					}
					else {
						init_pair(1, COLOR_RED, COLOR_BLACK);
						attrset(COLOR_PAIR(1));
						printw("%d(%c)", index, index+55);
						attrset(COLOR_PAIR(0));
					}
				}
				break;
			case 1:
				if(Array[index].shmq_flag) 
					printw("%s (s)", Array[index].qname);
				else
					printw("%s (f)", Array[index].qname);
				break;
			case 2:
				printw("%d", Array[index].max_rec_cnt);
				break;
			case 3:
				printw("%d", Array[index].gap);
				break;
			case 4:
				if( Array[index].usage > USAGE_THRESHOLD ) {
					init_pair(1, COLOR_RED, COLOR_BLACK); /* init_pair(pairNum,fg,bg) */
					attrset(COLOR_PAIR(1)); /* set new color attribute */
					printw("%5.2f", Array[index].usage);
					attrset(COLOR_PAIR(0)); /* set old color attribute */
				} else if( Array[index].usage > 50 ) {
					init_pair(2, COLOR_YELLOW, COLOR_BLACK); /* init_pair(pairNum,fg,bg) */
					attrset(COLOR_PAIR(2)); /* set new color attribute */
					printw("%5.2f", Array[index].usage);
					attrset(COLOR_PAIR(0)); /* set old color attribute */
				} else if( Array[index].usage > 20 ) {
					init_pair(3, COLOR_GREEN, COLOR_BLACK); /* init_pair(pairNum,fg,bg) */
					attrset(COLOR_PAIR(3)); /* set new color attribute */
					printw("%5.2f", Array[index].usage);
					attrset(COLOR_PAIR(0)); /* set old color attribute */
				} else {
					printw("%5.2f", Array[index].usage);
				}
				break;
			case 5:
				printw("%ld", Array[index].en_competition);
				break;
			case 6:
				printw("%ld", Array[index].de_competition);
				break;
			case 7:
				printw("%d", Array[index].big);
				break;
			case 8:
				if( Array[index].en_tps > 0 ) {
					init_pair(3, COLOR_GREEN, COLOR_BLACK); /* init_pair(pairNum,fg,bg) */
					attrset(COLOR_PAIR(3)); 
					printw("%d", Array[index].en_tps);
					attrset(COLOR_PAIR(0)); /* set old color attribute */
				} else {
					printw("%d", Array[index].en_tps);
				}
				break;
			case 9:
				if( Array[index].en_tps > 0 ) {
                    attrset(COLOR_PAIR(3)); /* set new color attribute */
                    printw("%d", Array[index].de_tps);
                    attrset(COLOR_PAIR(0)); /* set old color attribute */
                } else {
                    printw("%d", Array[index].de_tps);
                }
				break;
			case 10:
				printw("%ld", Array[index].en_sum);
				break;
			case 11:
				printw("%ld", Array[index].de_sum);
				break;
			case 12:
				printw("%s", Array[index].qpath);
				break;
			default:
				break;
		}
	}
				
	return(0);
}
#define MAX_USERID_LENGTH 16

static void *main_thread( void *arg )
{
    int     cmd_y;
    int     y_err=1;
    int     rc;
	int status=0;
	int y=0;
	int	port;
	int px=0, py=0;
	int	fd, lockfd;
	char *path = NULL;
	char ListInfoFile[256];
	fq_logger_t *l=NULL;
	int opened_q_cnt = 0;
	unsigned char buf[65536];
	long l_seq, run_time;
	external_env_obj_t *external_env_obj = NULL;

	fq_container_t *fq_c=NULL;

	char username[MAX_USERID_LENGTH];
	cuserid(username);

	char logfilename[256];
	sprintf(logfilename, "/tmp/fqmon_%s.log", username);
	rc = fq_open_file_logger(&l, logfilename, FQ_LOG_ERROR_LEVEL);
	CHECK(rc == TRUE);

	open_external_env_obj( l, g_path, &external_env_obj);
	if( external_env_obj ) {
		external_env_obj->on_get_extenv( l, external_env_obj, "eth", g_eth_name);
		fq_log(l, FQ_LOG_EMERG, "eth='%s'.", g_eth_name);
	}
	else {
		sprintf(g_eth_name, "%s", "eth0");
	}

	clear();
			

	while(1) {
		int i;

		if( user_quit_flag ) break;
		py = 0;
		y_status = get_row_winsize()-2;
		cmd_y = get_row_winsize();

        status = pthread_mutex_lock (&mutex);

        if( status !=0 ) {
            mvprintw(10, 1, "pthread_mutex_lock error.");
			break;
        }

		disp_time(py++, 0);
		disp_hostname_ip(py++,0, opened_q_cnt);
		disp_queue_data_home_dir( py++,0, g_path);
		disp_GetAvailableSpace(py++,0, g_path);

        main_title(py++, "LogCollector File Queue monitoring");

		px = 1;
		py++;

		sub_title(py);
		py++;

		linkedlist_t *ll = linkedlist_new("file queue linkedlist.");
		hashmap_obj_t *hash_obj=NULL;
		rc = OpenHashMapFiles(l, g_hash_path, g_hash_name, &hash_obj);
		CHECK(rc==TRUE);

		bool tf = MakeLinkedList_filequeues(l, ll);
		CHECK(tf);

		HashQueueInfo_t Array[ll->length+1];
		tf = Make_Array_from_Hash_QueueInfo_linkedlist(l, hash_obj, ll, Array );
		CHECK(tf);
	
		/* display body values */
		for(i=0;  i<ll->length; i++) {

			int waiting_msec = 100000;
			int text_color = COLOR_WHITE;

			rc = display_screen( py, Array, i);
			CHECK( rc == 0 );

			py++;
		}
		
		draw_bar(py++, 0);

        move(get_y_position, CURSOR_X_POSITION);
        refresh();

        pthread_mutex_unlock (&mutex);


		if(ll) linkedlist_free(&ll);
		if(hash_obj) CloseHashMapFiles(l, &hash_obj);

        sleep(1); /* If you remove it, you will get core-dump */
    }

    move(y_err, 0); clrtoeol();

	// close_all_file_queues(l);

	if(l) fq_close_file_logger(&l);

stop:
	if(!user_quit_flag) {
		clear();
		mvprintw(0, 0 , "fatal: See logfile(/tmp/monitor.log). Good bye!  Press 'q'.");
		refresh();
		getch();
		end_screen(&s, 0);
		exit(0);
	}

    pthread_exit((void *)0);
}

#include<sys/utsname.h>   /* Header for 'uname'  */

static void help(void)
{
    int x=0, y=0;
	struct utsname uname_pointer;

	uname(&uname_pointer);

    clear();

    mvprintw(y++, x, "Semi-graphic Viewer version %s, Copyright (c) 2019 through 2010, Gwisang Choi", FQUEUE_LIB_VERSION);
    mvprintw(y++, x, "Compiled on %s %s ", __TIME__, __DATE__);
	y++;
	mvprintw(y++, x, "-----< How to connect to developer  >-------");
    mvprintw(y++, x, "Connect to Author -> Mobile: %s,  e-mail: %s ", "010-7202-1516", "gwisang.choi@gmail.com");
    mvprintw(y++, x, "Connect to Company(CLANG Co.) -> Tel: %s", "031-916-1516");

	y++;
	mvprintw(y++, x, "-----< Environment of compiled system >-------");
	mvprintw(y++, x, "- System name : %s", uname_pointer.sysname);
	mvprintw(y++, x, "- Nodename    : %s", uname_pointer.nodename);
	mvprintw(y++, x, "- Release     : %s", uname_pointer.release);
	mvprintw(y++, x, "- Version     : %s", uname_pointer.version);
	mvprintw(y++, x, "- Machine     : %s", uname_pointer.machine);
 	// printf("Domain name - %s \n", uname_pointer.domainname);
	y++;
	mvprintw(y++, x, "-----< Configurations of Program >-------");
	mvprintw(y++, x, "- MAX_FILES_QUEUES       : %d", MAX_FILES_QUEUES);
	mvprintw(y++, x, "- USAGE_THRESHOLD        : %f", USAGE_THRESHOLD);
	mvprintw(y++, x, "- DISK_THRESHOLD(blocks) : %d", DISK_THRESHOLD);

    y++;

    mvprintw(y++,x, "Available command charactors.");
    mvprintw(y++,x, "q\t -quit");
    mvprintw(y++,x, "r\t -reset");
    mvprintw(y++,x, "h or ?\t -help: show this text");

    y++;
    y++;
    y++;
    attrset(A_REVERSE);
    mvprintw(y,x, "Hit any key to continue: ");
    attrset(0);
    refresh();
    getch();
    clear();
    return;
}

static void detail_view(char key)
{

    int x=0, y=0;
	struct utsname uname_pointer;
	int i;
	long i_before_en = 0;
	long i_before_de = 0;

	uname(&uname_pointer);

    clear();

	if( key >= '0' && key <= '9')
		i = key - 48;
	else
		i = key - 55;

	while(1) {
		int input_key;
		char *fuser_out = NULL;
		int rc;

		clear();

		linkedlist_t *ll = linkedlist_new("file queue linkedlist.");
		hashmap_obj_t *hash_obj=NULL;

		rc = OpenHashMapFiles(NULL, g_hash_path, g_hash_name, &hash_obj);
		CHECK(rc==TRUE);

		bool tf = MakeLinkedList_filequeues(NULL, ll);
		CHECK(tf);

		HashQueueInfo_t Array[ll->length+1];
		tf = Make_Array_from_Hash_QueueInfo_linkedlist(NULL, hash_obj, ll, Array );

		CHECK(tf);
		disp_time(y++, x);
		mvprintw(y++, x, "%s", "---------------< Status of File Queue>--------------");
		mvprintw(y++, x, "- path        : %s", Array[i].qpath);
		mvprintw(y++, x, "- qname       : %s", Array[i].qname);
		mvprintw(y++, x, "- description : %s", Array[i].desc);
		mvprintw(y++, x, "- message size: %d", Array[i].msglen);
		mvprintw(y++, x, "- records     : %d", Array[i].max_rec_cnt);
		mvprintw(y++, x, "- income(TPS) : %d", Array[i].en_tps);
		mvprintw(y++, x, "- outcome(TPS): %d", Array[i].de_tps);
		mvprintw(y++, x, "- income      : %ld", Array[i].en_sum);
		mvprintw(y++, x, "- outcome     : %ld", Array[i].de_sum);
		mvprintw(y++, x, "- Gap(en-de)  : %d", Array[i].gap);
		mvprintw(y++, x, "- Usage       : %-5.2f %%", Array[i].usage);
		mvprintw(y++, x, "- BIG files   : %d", Array[i].big);
		mvprintw(y++, x, "- Status      : %d", Array[i].status);
		char *p = asc_time(Array[i].last_en_time);;
		mvprintw(y++, x, "- last_en_time: %s", p);
		free(p);
		p =  asc_time(Array[i].last_de_time);
		mvprintw(y++, x, "- last_de_time: %s", p);
		free(p);

		y++;
		mvprintw(y,x, "Press space key for refreshing or quit(q)");
		refresh();

		if(ll) linkedlist_free(&ll);
		if(hash_obj) CloseHashMapFiles(NULL, &hash_obj);

		input_key = getch();

		if(input_key == 'q') break;
		else {
			y = 0;
			continue;
		}
	}


    refresh();
    clear();
    refresh();
    return;
}


static void main_title(int y, char *fname)
{
    int		i;
    char    Center_Title[128];
	int		solution_no;

    sprintf(Center_Title, "[ %s FileQueue Status. library version %s [%s][%s] ]", 
			PRODUCT_NAME, FQUEUE_LIB_VERSION, __DATE__, __TIME__);

    center_disp(y, Center_Title);
    draw_bar(y+1, 0);

    return;
}

static void *waiting_key_thread(void *arg)
{
    int key, cmd_y;

    cmd_y = get_row_winsize();
    y_status = cmd_y-2;

    while(1) {
        int status;
        char szCmd[128];

        move(cmd_y-1, 0);
        get_y_position = cmd_y-1;
        clrtoeol();
        mvprintw(cmd_y-1, 0, "%s", "Type a key[h(help), hotkey(for detail), q(quit)]>");
        move(cmd_y-1, 57);
        key = getch();

        status = pthread_mutex_lock (&mutex);
        if( status !=0 ) {
            mvprintw(10, 1, "pthread_mutex_lock error.");
            pthread_exit(0);
        }
		switch(key) {
            case 'q':
				user_quit_flag = True;
                pthread_mutex_unlock (&mutex);
				sleep(1);
				// close_all_file_queues(NULL);
                goto L0;
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
			case 'A':
			case 'B':
			case 'C':
			case 'D':
			case 'E':
			case 'F':
			case 'G':
			case 'H':
			case 'I':
			case 'J':
			case 'K':
			case 'L':
			case 'M':
			case 'N':
			case 'O':
			case 'P':
			case 'Q':
			case 'R':
			case 'S':
			case 'T':
			case 'U':
			case 'V':
			case 'W':
			case 'X':
			case 'Y':
			case 'Z':
				detail_view(key);
				clear();
				break;
            case 'h':
            case '?':
                help();
                break;
            case 'r':
				reset_flag=1;
                break;
            default:
                mvprintw(y_status, 0, "Error: [%c] is invalid key.", key);
                break;
        }
        refresh();
        pthread_mutex_unlock (&mutex);
    }
L0:
    pthread_exit((void *)0);
}

static void disp_time(int y, int x)
{
    char    szTime[30];
    long    sec, usec;

    memset(szTime, 0x00, sizeof(szTime));
    MicroTime(&sec, &usec, szTime);
    move(y,x);
    clrtoeol();
    mvprintw(y,x, "%s -refresh interval(%d)", szTime, current_sleep);
    return;
}

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>

static void disp_hostname_ip(int y, int x, int opened_q_cnt)
{
	char hostname[64];
	int fd;
	struct ifreq ifr;
	char IPbuffer[32];

	gethostname(hostname, 64);

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	ifr.ifr_addr.sa_family = AF_INET;
	snprintf(ifr.ifr_name, IFNAMSIZ, "%s", g_eth_name);
	// int snprintf(char *str, size_t size, const char *format, ...);
	ioctl(fd, SIOCGIFADDR, &ifr);

	sprintf(IPbuffer, "%s", inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));
	close(fd);

    move(y,x);
    clrtoeol();
    mvprintw(y,x, "- Hostname: %s,  - IP address: %s - Number of Queues: %d", hostname, IPbuffer, opened_q_cnt);
    return;
}

static void disp_queue_data_home_dir(int y, int x, char *qpath)
{
	attron(A_BOLD);
    mvprintw(y,x, "FQ_DATA_HOME: %s, FQ_HASH_HOME: %s, HASH_NAME: %s", qpath, g_hash_path, g_hash_name);
	attroff(A_BOLD);
    return;
}
#include <sys/statvfs.h>

static void disp_GetAvailableSpace(int y, int x, const char *path)
{
	struct statvfs stat;

	if (statvfs(path, &stat) != 0) {
		return;
	}

	if( stat.f_bavail < DISK_THRESHOLD) {
		attrset(COLOR_PAIR(1)); /* set new color attribute */
		// mvprintw(y,x, "Available Disk Space: %ld", stat.f_bsize);
		mvprintw(y,x, "Available Disk Space: %ld blocks.(1 block=%ld)", stat.f_bavail, stat.f_bsize);
		// mvprintw(y,x, "Available Disk Space: %ld", (stat.f_bsize * stat.f_bavail));
		attrset(COLOR_PAIR(0)); /* set old color attribute */
	} else {
		mvprintw(y,x, "Available Disk Space: %ld blocks.(1 block=%ld)", stat.f_bavail, stat.f_bsize);
	}

    return;
}
static bool check_alive_fq_mon_svr()
{
	char    CMD[512];
	char    buf[1024];
	FILE    *fp=NULL;

	sprintf(CMD, "ps -ef| grep fq_mon_svr | grep fq_mon_svr.conf | grep -v grep | grep -v tail | grep -v vi");
	if( (fp=popen(CMD, "r")) == NULL) {
		printf("popen() error.\n");
		return false;
	}
	while(fgets(buf, 1024, fp) != NULL) {
		/* found */
		printf("line: %s\n", buf);
		pclose(fp);
		return true;
	}
	/* not found */
	pclose(fp);
	return false;
}
