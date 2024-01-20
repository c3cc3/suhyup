/*
** webgate.c
** Performance Testing Result : in localhost with WAS, 
** 4096 bytes , result: 23000 TPS
** memory leak checking -> passed 2023/04/19
**
** method to notation JSON -> from snake to camel
*/
#define FQ_SERVER_C_VERSION	"1.0.1"

#include <stdio.h>
#include <getopt.h>
#include <libgen.h>
#include "fq_param.h"
#include "fqueue.h"
#include "fq_socket.h"
#include "fq_logger.h"
#include "fq_common.h"
#include "fq_monitor.h"
#include "fq_protocol.h"
#include "fq_config.h"
#include "fq_gssi.h"
#include "fq_codemap.h"
#include "shm_queue.h"

#include "fq_monitor.h"
#include "fq_file_list.h"
#include "fq_linkedlist.h"
#include "fq_ratio_distribute.h"

#include "parson.h"
#include "fq_hashobj.h"
#include "fq_codemap.h"
#include "fq_scanf.h"



#define MAX_FILES_QUEUES (100)

int Sure_GetHash( fq_logger_t *l, hashmap_obj_t *hash_obj, char *key, char **value);



typedef struct _channel_ratio_obj_t channel_ratio_obj_t;
struct _channel_ratio_obj_t {
	char channel_name[16];
	ratio_obj_t	*ratio_obj;
};


typedef struct _admin_phone_map {
    char phone_no[12];
    char name[128];
} admin_phone_map_t;

static HashQueueInfo_t *get_a_queue_info_in_hashmap_local( fq_logger_t *l, char *hash_path, char *hash_name,  char *key);
bool MakeLinkedList_channel_co_ratio_obj_with_mapfile(fq_logger_t *l, char *mapfile, char delimiter, linkedlist_t *ll);

static bool MakeLinkedList_admin_phone_mapfile( fq_logger_t *l, char *mapfile, char delimiter, linkedlist_t	*ll);
bool get_admin_phone_no_list(JSON_Value *req_json_value, JSON_Object *req_json_object, char *svc_code, char **dst, int *dst_len);

int open_all_file_queues(fq_logger_t *l, fq_container_t *fq_c, int *opened_q_cnt);
void close_all_file_queues(fq_logger_t *l);
static bool check_alive_fq_mon_svr();
bool is_available_channel(fq_logger_t *l, char *channel);
static bool MakeLinkedList_admin_phone_mapfile( fq_logger_t *l, char *mapfile, char delimiter, linkedlist_t	*ll);
static bool is_exclude_queue_name( fq_logger_t *l, file_list_obj_t *exclude_list_obj, char *qname);

/* Global variables */
typedef struct Queues {
	fqueue_obj_t *obj;
	int shmq_flag;
} Queues_t;

typedef enum { NEW_RATIO=0, CO_DOWN, PROC_STOP } ctrl_cmd_t;
typedef struct _ctrl_msg_t ctrl_msg_t;
struct _ctrl_msg_t {
	char start_byte; /* 0x01 */
    ctrl_cmd_t   cmd; /* cmd : NEW_RATIO: 0  , CO_DOWN */
	char channel[3];
    char new_ratio_string[128];
    char co_name[2]; /* initial */
};

hashmap_obj_t *heartbeat_hash_obj=NULL;
file_list_obj_t *exclude_list_obj=NULL;


Queues_t Qs[MAX_FILES_QUEUES];
int opened_q_cnt = 0;
hashmap_obj_t *hash_obj=NULL;
char *ratio_ctrl_online_q_path=NULL;
char *ratio_ctrl_online_q_name=NULL;
fqueue_obj_t *ratio_ctrl_online_qobj;

char *ratio_ctrl_batch_q_path=NULL;
char *ratio_ctrl_batch_q_name=NULL;
fqueue_obj_t *ratio_ctrl_batch_qobj;

char *g_mon_hash_path = NULL;
char *g_mon_hash_name = NULL;
char *g_channel_co_ratio_mapfile = NULL;
char *g_fq_categories_codemap_file = NULL;
char *g_admin_phone_map_filename = NULL;
char *g_heartbeat_hash_map_path=NULL;
char *g_heartbeat_hash_map_name=NULL;
char *g_fq_pid_list_file=NULL;
char *g_distributor_count=NULL;
int i_distributor_count=0;
char *g_listfile_to_exclude=NULL;

codemap_t *fq_categories_codemap_t = NULL;

linkedlist_t *admin_phone_map_ll = NULL;

void init_Qs() {
	int i, rc;
	for(i=0; i<MAX_FILES_QUEUES; i++) {
		Qs[i].obj = NULL;
	}
}

fqueue_obj_t *find_fq_obj( char *qpath, char *qname)
{
	int i;
	for( i=0; i<opened_q_cnt; i++) {
		if( strcmp(Qs[i].obj->path, qpath)==0 &&
			strcmp(Qs[i].obj->qname, qname) ==0 ) {
			return( Qs[i].obj );
		}
	}
	return NULL;
}

static bool append_mypid_fq_pid_listfile_for_heartbeat( fq_logger_t *l, char *file )
{
	FILE *fp=NULL;

	fp = fopen(file, "a");
	if( fp == NULL) {
		fq_log(l, FQ_LOG_ERROR, "append_mypid_fq_pid_listfile('%s'). error.", file);
		return false;
	}
	fprintf(fp, "%d\n", getpid() );
	fflush(fp);
	fclose(fp);
	fq_log(l, FQ_LOG_INFO, "append_mypid_fq_pid_listfile('%s'). OK.", file);
	return true;
}

bool heartbeat_ums(fq_logger_t *l, hashmap_obj_t *hash_obj, char *progname)
{
	FQ_TRACE_ENTER(l);

	char str_pid[10];
	char str_bintime[16];

	sprintf(str_pid, "%d", getpid());

	time_t now;
	now = time(0);
	sprintf(str_bintime,  "%ld", now);

	int rc;
	rc = PutHash(l, hash_obj, str_pid, str_bintime);
	if( rc != TRUE ) {
		fq_log(l, FQ_LOG_ERROR, "PutHash(key='%s', value='%s')", str_pid, str_bintime);
		return false;
		// obj->on_clean_table(l, seq_check_hash_obj);
	}
	FQ_TRACE_EXIT(l);

	return true;
}

static int make_error_response( fq_logger_t *l, char *svc_code, char *err_code, char **dst);

/*
** Global Variables
*/
fq_logger_t *l=NULL;
char g_Progname[64];
char *conf=NULL;
/* server environment */
/* parameters */
char *bin_dir = NULL;
char *logname = NULL;
char *ip = NULL;
char *port = NULL;
char *header_type=NULL;
int  i_header_type;
char *loglevel = NULL;
char *err_report_format_file = NULL;
char *err_codemap_file = NULL;

int make_json_response_msg( char *svc_code , unsigned char **dst)
{
	char date[9], time[7];
	JSON_Value *rootValue = NULL;
	JSON_Object *rootObject = NULL;

	rootValue = json_value_init_object();
	rootObject = json_value_get_object(rootValue);

	get_time(date, time);
	json_object_set_string(rootObject, "date", date);
	json_object_set_string(rootObject, "time", time);
	json_object_set_string(rootObject, "resultCode", "0000");
	json_object_set_string(rootObject, "resultMessage", "This is a test message.");

	char *buf=NULL;
	size_t buf_size;

	buf_size = json_serialization_size(rootValue) + 1;
	buf = calloc(sizeof(char), buf_size);
	json_serialize_to_buffer( rootValue, buf, buf_size);

	debug_json(l, rootValue);

	*dst = (unsigned char *) strdup( buf );

	SAFE_FREE(buf);
	json_value_free(rootValue);

	return strlen(*dst);
}

/* 0000 */
bool test(JSON_Value *req_json_value, JSON_Object *req_json_object, char *svc_code, char **dst, int *dst_len)
{
	unsigned char *buf=NULL;
	int	len;

	printf("[%s].\n", __FUNCTION__);
	
    printf("test() called. svc_code = [%s]. \n",  svc_code);

	len = make_json_response_msg( svc_code , &buf);
	CHECK(len > 0);
	
	*dst = strdup(buf);
	// *dst = strdup("This is test response message.");

	*dst_len = strlen(*dst);
	return (true);
}

#include "fqueue.h"
#include "fq_manage.h"
#include "fq_linkedlist.h"

/* 0001 */
bool all_q_status(JSON_Value *req_json_value, JSON_Object *req_json_object, char *svc_code, char **dst, int *dst_len)
{
	int i;
	char date[9], time[7];
	JSON_Value *rootValue = NULL;
	JSON_Object *rootObject = NULL;

	printf("[%s].\n", __FUNCTION__);

	get_time(date, time);
	rootValue = json_value_init_object();
	rootObject = json_value_get_object(rootValue);

	json_object_set_string(rootObject, "resultCode", "0000");
	json_object_set_string(rootObject, "date", date);
	json_object_set_string(rootObject, "time", time);
	json_object_set_number(rootObject, "countOfQueues", opened_q_cnt);

	json_object_set_value(rootObject, "queues", json_value_init_array());
	JSON_Array *Queues = json_object_get_array(rootObject, "queues");

	for( i=0; i<opened_q_cnt; i++) {
#if 0
		char record[256];
		sprintf( record,"%s|%s|%s|%d|%ld|%f", 
			Qs[i].obj->path,
			Qs[i].obj->qname,
			Qs[i].obj->h_obj->h->desc,
			Qs[i].obj->h_obj->h->max_rec_cnt,
			Qs[i].obj->h_obj->h->en_sum-Qs[i].obj->h_obj->h->de_sum, // qi.diff
			Qs[i].obj->on_get_usage(NULL, Qs[i].obj)
		);
		json_array_append_string(Queues, record);
#else
		JSON_Value *subValue = NULL;
		JSON_Object *subObject = NULL;

		
		subValue = json_value_init_object();
		subObject = json_value_get_object(subValue);

		json_object_set_number(subObject, "seq", i);
		json_object_set_string(subObject, "qPath", Qs[i].obj->path);
		json_object_set_string(subObject, "qName", Qs[i].obj->qname);
		json_object_set_string(subObject, "description", Qs[i].obj->h_obj->h->desc);
		json_object_set_number(subObject, "maxRecCnt", Qs[i].obj->h_obj->h->max_rec_cnt);
		json_object_set_number(subObject, "gap", Qs[i].obj->h_obj->h->en_sum-Qs[i].obj->h_obj->h->de_sum);
		json_object_set_number(subObject, "usage", Qs[i].obj->on_get_usage(NULL, Qs[i].obj));
		json_object_set_number(subObject, "status", Qs[i].obj->h_obj->h->status);

		int categories = 0;
		char code_value[8];

		char codemap_key[256];
		sprintf(codemap_key, "%s/%s",  Qs[i].obj->path, Qs[i].obj->qname);

		int rc;
		rc = get_codeval(l, fq_categories_codemap_t, codemap_key, code_value );
		
		fq_log(l, FQ_LOG_DEBUG, "codemap_key=[%s], code_value=[%s] rc=%d",  
			codemap_key, code_value, rc );
		if( rc == 0 ) { // found
			categories = atoi(code_value);
		}
		else {
			categories = 99; // undefined category.
		}

		json_object_set_number(subObject, "categories", categories);


		char key[256];
		sprintf(key, "%s/%s", Qs[i].obj->path, Qs[i].obj->qname);

#if 1
		HashQueueInfo_t *hash_qi =  get_a_queue_info_in_hashmap_local( l, g_mon_hash_path, g_mon_hash_name,  key);
		
		if( !hash_qi ) {
			fq_log(l, FQ_LOG_ERROR, "Can't find a queue('%s', '%s) infomation in hash(%s).", Qs[i].obj->path, Qs[i].obj->qname, g_mon_hash_name);

			json_object_set_number(subObject, "enTps", hash_qi->en_tps);
			json_object_set_number(subObject, "deTps", hash_qi->de_tps);
		}
		else {
			fq_log(l, FQ_LOG_INFO, "Successfully, We get a queue('%s', '%s') info in haah.", Qs[i].obj->path, Qs[i].obj->qname );
			json_object_set_number(subObject, "enTps", 0);
			json_object_set_number(subObject, "deTps", 0);
		}
#else
			json_object_set_number(subObject, "enTps", 0);
			json_object_set_number(subObject, "deTps", 0);

#endif
			
		json_array_append_value(Queues, subValue);
#endif
	}

	char *buf=NULL;
	size_t buf_size;
	buf_size = json_serialization_size(rootValue) + 1;
	buf = calloc(sizeof(char), buf_size);
	json_serialize_to_buffer( rootValue, buf, buf_size);

	debug_json(l, rootValue);

	*dst = (unsigned char *) strdup( buf );


	SAFE_FREE(buf);
	json_value_free(rootValue);

char *g_channel_ratio_mapfile = NULL;
	*dst_len = strlen(*dst);
    printf("svc_code = [%s]. \n",  svc_code);
	return (true);
}

/* 0002 */
bool get_ratio(JSON_Value *req_json_value, JSON_Object *req_json_object, char *svc_code, char **dst, int *dst_len)
{
	int i;
	char date[9], time[7];
	JSON_Value *rootValue = NULL;
	JSON_Object *rootObject = NULL;
	char hash_key[32];

	printf("[%s].\n", __FUNCTION__);

	linkedlist_t *channel_co_ratio_obj_ll = linkedlist_new("channel_co_ratio_obj_list");
	char map_delimiter = '|';
	bool tf;

	tf = MakeLinkedList_channel_co_ratio_obj_with_mapfile(l, g_channel_co_ratio_mapfile, map_delimiter, channel_co_ratio_obj_ll);
	CHECK(tf);
	fq_log(l, FQ_LOG_INFO, "MakeLinkedList channel_co_ratio map ok.");
	


	get_time(date, time);
	rootValue = json_value_init_object();
	rootObject = json_value_get_object(rootValue);

	json_object_set_string(rootObject, "resultCode", "0000");
	json_object_set_string(rootObject, "date", date);
	json_object_set_string(rootObject, "time", time);
	json_object_set_number(rootObject, "NumberOfChannels", channel_co_ratio_obj_ll->length);

	char *ratio_string=NULL;

	ll_node_t *node_p = NULL;

	json_object_set_value(rootObject, "Channels", json_value_init_array());
	JSON_Array *Channels = json_object_get_array(rootObject, "Channels");

	for( node_p=channel_co_ratio_obj_ll->head; (node_p != NULL); (node_p=node_p->next) ) {
		channel_ratio_obj_t *tmp;
		tmp = (channel_ratio_obj_t *) node_p->value;

		fq_log(l, FQ_LOG_INFO, "tmp->find_key = '%s'.", tmp->channel_name);
		
		int rc;
		char hash_search_key[256];

		sprintf(hash_search_key, "%s_RATIO", tmp->channel_name);

		rc = Sure_GetHash(l, hash_obj, hash_search_key, &ratio_string);
		if( rc == TRUE ) {
			// json_object_set_string(rootObject, hash_search_key, ratio_string);
			JSON_Value *subValue = NULL;
			JSON_Object *subObject = NULL;
			subValue = json_value_init_object();
			subObject = json_value_get_object(subValue);

			json_object_set_string(subObject, "CHANNEL", tmp->channel_name);
			json_object_set_string(subObject, "RATIO", ratio_string);

			json_array_append_value(Channels, subValue);
			SAFE_FREE(ratio_string);
		}
	}

	char *buf=NULL;
	size_t buf_size;
	buf_size = json_serialization_size(rootValue) + 1;

	buf = calloc(sizeof(char), buf_size);
	json_serialize_to_buffer( rootValue, buf, buf_size);

	debug_json(l, rootValue);

	*dst = (unsigned char *) strdup( buf );

	SAFE_FREE(buf);
	json_value_free(rootValue);

	linkedlist_free(&channel_co_ratio_obj_ll);

	*dst_len = strlen(*dst);
    printf("svc_code = [%s]. \n",  svc_code);
	return (true);
}

bool is_available_channel(fq_logger_t *l, char *channel)
{
	linkedlist_t *channel_co_ratio_obj_ll = linkedlist_new("channel_co_ratio_obj_list");
	char map_delimiter = '|';
	bool tf;

	tf = MakeLinkedList_channel_co_ratio_obj_with_mapfile(l, g_channel_co_ratio_mapfile, map_delimiter, channel_co_ratio_obj_ll);
	CHECK(tf);
	fq_log(l, FQ_LOG_INFO, "MakeLinkedList channel_co_ratio map ok.");

	ll_node_t *node_p = NULL;

	for( node_p=channel_co_ratio_obj_ll->head; (node_p != NULL); (node_p=node_p->next) ) {
		channel_ratio_obj_t *tmp;
		tmp = (channel_ratio_obj_t *) node_p->value;
		fq_log(l, FQ_LOG_INFO, "tmp->find_key = '%s'.", tmp->channel_name);
		if( strcmp(tmp->channel_name, channel) == 0 ) {

			linkedlist_free(&channel_co_ratio_obj_ll);
			return true;
		}
	}

	linkedlist_free(&channel_co_ratio_obj_ll);

	return false;
}

/* 0003 */
bool set_ratio(JSON_Value *req_json_value, JSON_Object *req_json_object, char *svc_code, char **dst, int *dst_len)
{
	int i;
	char date[9], time[7];
	JSON_Value *rootValue = NULL;
	JSON_Object *rootObject = NULL;
	char hash_key[32];

	printf("[%s].\n", __FUNCTION__);

	char channel[3];
	sprintf(channel, "%s", json_object_get_string(req_json_object, "Channel"));

	if( !is_available_channel(l, channel) ) {
		fq_log(l, FQ_LOG_ERROR, "'%s' is undefined channel .", channel);
		return false;
	}

	char NewRatio[128];
	sprintf(NewRatio, "%s", json_object_get_string(req_json_object, "NewRatio"));
	if( strlen(NewRatio) < 8 ) {
		printf("There is no NewRatio value.\n");
		fq_log(l, FQ_LOG_ERROR, "illegal request: There is no NewRatio value.");
		return false;
	}

/*
	if( !HASVALUE( NewRatio ) ) {
		fq_log(l, FQ_LOG_ERROR, "illegal request: There is no NewRatio value.");
		return false;
	}
*/
	printf("NewRatio=[%s]\n", NewRatio);
	
	/* Update NewRatio */
	char put_key[128];
	sprintf(put_key, "%s_RATIO", channel);
	int rc;

	rc = PutHash(l, hash_obj, put_key, NewRatio);
	CHECK(rc == TRUE);

	fq_log(l, FQ_LOG_EMERG, "PutHash( key='%s', value='%s') success.", put_key, NewRatio);

	/* make a ratio ctrl json message and put it to control queue */
	ctrl_msg_t  *ctrl_msg = calloc(1, sizeof(ctrl_msg_t));	

	ctrl_msg->start_byte = 0x01;
	sprintf(ctrl_msg->channel, "%s", channel);
	ctrl_msg->cmd = NEW_RATIO;
	sprintf(ctrl_msg->new_ratio_string, "%s", NewRatio);

	CHECK(ratio_ctrl_online_qobj);

	int ii;
	for( ii=0; ii< i_distributor_count; ii++) {
		/* online ctrl */
		while(1) {
			int rc;
			long l_seq=0L;
			long run_time=0L;

			rc =  ratio_ctrl_online_qobj->on_en(l, ratio_ctrl_online_qobj, EN_NORMAL_MODE, (unsigned char *)ctrl_msg, sizeof(ctrl_msg_t)+1, sizeof(ctrl_msg_t), &l_seq, &run_time );
			if( rc == EQ_FULL_RETURN_VALUE )  {
				fq_log(l, FQ_LOG_EMERG, "Queue('%s', '%s') is full.\n", 
					ratio_ctrl_online_qobj->path, ratio_ctrl_online_qobj->qname);
				usleep(100000);
				continue;
			}
			else if( rc < 0 ) { 
				fq_log(l, FQ_LOG_ERROR, "Queue('%s', '%s'): on_en() error.\n", ratio_ctrl_online_qobj->path, ratio_ctrl_online_qobj->qname);
				return(rc);
			}
			else {
				fq_log(l, FQ_LOG_EMERG, "(online) [%d]-th Ratio control message is enqueued to [%s]. len(rc) = [%d].", ii,  ratio_ctrl_online_qobj->qname, rc);
				break;
			}
		}
	}
	/* batch control */
	while(1) {
		int rc;
		long l_seq=0L;
		long run_time=0L;

		rc =  ratio_ctrl_batch_qobj->on_en(l, ratio_ctrl_batch_qobj, EN_NORMAL_MODE, (unsigned char *)ctrl_msg, sizeof(ctrl_msg_t)+1, sizeof(ctrl_msg_t), &l_seq, &run_time );
		if( rc == EQ_FULL_RETURN_VALUE )  {
			fq_log(l, FQ_LOG_EMERG, "Queue('%s', '%s') is full.\n", 
				ratio_ctrl_batch_qobj->path, ratio_ctrl_batch_qobj->qname);
			usleep(100000);
			continue;
		}
		else if( rc < 0 ) { 
			fq_log(l, FQ_LOG_ERROR, "Queue('%s', '%s'): on_en() error.\n", ratio_ctrl_batch_qobj->path, ratio_ctrl_batch_qobj->qname);
			return(rc);
		}
		else {
			fq_log(l, FQ_LOG_EMERG, "(batch) Ratio control message is enqueued to [%s]. len(rc) = [%d].", ratio_ctrl_batch_qobj->qname, rc);
			break;
		}
	}

	get_time(date, time);
	rootValue = json_value_init_object();
	rootObject = json_value_get_object(rootValue);

	json_object_set_string(rootObject, "resultCode", "0000");
	json_object_set_string(rootObject, "date", date);
	json_object_set_string(rootObject, "time", time);

	char *buf=NULL;
	size_t buf_size;
	buf_size = json_serialization_size(rootValue) + 1;

	buf = calloc(sizeof(char), buf_size);
	json_serialize_to_buffer( rootValue, buf, buf_size);

	debug_json(l, rootValue);

	*dst = (unsigned char *) strdup( buf );

	SAFE_FREE(buf);
	json_value_free(rootValue);

	*dst_len = strlen(*dst);
    printf("svc_code = [%s]. \n",  svc_code);
	return (true);
}

/* 0004 */
bool channel_co_down(JSON_Value *req_json_value, JSON_Object *req_json_object, char *svc_code, char **dst, int *dst_len)
{
	int i;
	char date[9], time[7];
	JSON_Value *rootValue = NULL;
	JSON_Object *rootObject = NULL;
	char hash_key[32];

	printf("[%s].\n", __FUNCTION__);

	char channel[3];
	sprintf(channel, "%s", json_object_get_string(req_json_object, "Channel"));

	if( !is_available_channel(l, channel) ) {
		fq_log(l, FQ_LOG_ERROR, "'%s' is undefined channel .", channel);
		return false;
	}

	char co_initial[2];
	sprintf(co_initial, "%s", json_object_get_string(req_json_object, "co_initial"));

	if( strlen(co_initial) != 1 ) {
		printf("There is no co_initial value.\n");
		fq_log(l, FQ_LOG_ERROR, "illegal request: There is no co_initial value.");
		return false;
	}

	printf("co_initial=[%s]\n", co_initial);
	
	/* make a ratio ctrl json message and put it to control queue */
	ctrl_msg_t  *ctrl_msg = calloc(1, sizeof(ctrl_msg_t));	
	sprintf(ctrl_msg->channel, "%s", channel);
	ctrl_msg->cmd = CO_DOWN;
	sprintf(ctrl_msg->co_name, "%s", co_initial);

	CHECK(ratio_ctrl_online_qobj);

	while(1) {
		int rc;
		long l_seq=0L;
		long run_time=0L;

		rc =  ratio_ctrl_online_qobj->on_en(l, ratio_ctrl_online_qobj, EN_NORMAL_MODE, (unsigned char *)ctrl_msg, sizeof(ctrl_msg_t)+1, sizeof(ctrl_msg_t), &l_seq, &run_time );
		if( rc == EQ_FULL_RETURN_VALUE )  {
			fq_log(l, FQ_LOG_EMERG, "Queue('%s', '%s') is full.\n", 
				ratio_ctrl_online_qobj->path, ratio_ctrl_online_qobj->qname);
			usleep(100000);
			continue;
		}
		else if( rc < 0 ) { 
			fq_log(l, FQ_LOG_ERROR, "Queue('%s', '%s'): on_en() error.\n", ratio_ctrl_online_qobj->path, ratio_ctrl_online_qobj->qname);
			return(rc);
		}
		else {
			fq_log(l, FQ_LOG_EMERG, "enqueued ums_msg len(rc) = [%d].", rc);
			break;
		}
	}

	get_time(date, time);
	rootValue = json_value_init_object();
	rootObject = json_value_get_object(rootValue);

	json_object_set_string(rootObject, "resultCode", "0000"); // success
	json_object_set_string(rootObject, "date", date);
	json_object_set_string(rootObject, "time", time);

	char *buf=NULL;
	size_t buf_size;
	buf_size = json_serialization_size(rootValue) + 1;

	buf = calloc(sizeof(char), buf_size);
	json_serialize_to_buffer( rootValue, buf, buf_size);

	debug_json(l, rootValue);

	*dst = (unsigned char *) strdup( buf );

	SAFE_FREE(buf);
	json_value_free(rootValue);

	*dst_len = strlen(*dst);
    printf("svc_code = [%s]. \n",  svc_code);
	return (true);
}
/* 0005 */
bool proc_stop(JSON_Value *req_json_value, JSON_Object *req_json_object, char *svc_code, char **dst, int *dst_len)
{
	int i;
	char date[9], time[7];
	JSON_Value *rootValue = NULL;
	JSON_Object *rootObject = NULL;
	char hash_key[32];

	printf("[%s].\n", __FUNCTION__);

	/* make a ratio ctrl json message and put it to control queue */
	ctrl_msg_t  *ctrl_msg = calloc(1, sizeof(ctrl_msg_t));	
	ctrl_msg->cmd = PROC_STOP;

	CHECK(ratio_ctrl_online_qobj);

	while(1) {
		int rc;
		long l_seq=0L;
		long run_time=0L;

		rc =  ratio_ctrl_online_qobj->on_en(l, ratio_ctrl_online_qobj, EN_NORMAL_MODE, (unsigned char *)ctrl_msg, sizeof(ctrl_msg_t)+1, sizeof(ctrl_msg_t), &l_seq, &run_time );
		if( rc == EQ_FULL_RETURN_VALUE )  {
			fq_log(l, FQ_LOG_EMERG, "Queue('%s', '%s') is full.\n", 
				ratio_ctrl_online_qobj->path, ratio_ctrl_online_qobj->qname);
			usleep(100000);
			continue;
		}
		else if( rc < 0 ) { 
			fq_log(l, FQ_LOG_ERROR, "Queue('%s', '%s'): on_en() error.\n", ratio_ctrl_online_qobj->path, ratio_ctrl_online_qobj->qname);
			return(rc);
		}
		else {
			fq_log(l, FQ_LOG_EMERG, "enqueued ums_msg len(rc) = [%d].", rc);
			break;
		}
	}

	get_time(date, time);
	rootValue = json_value_init_object();
	rootObject = json_value_get_object(rootValue);

	json_object_set_string(rootObject, "resultCode", "0000"); // success
	json_object_set_string(rootObject, "date", date);
	json_object_set_string(rootObject, "time", time);

	char *buf=NULL;
	size_t buf_size;
	buf_size = json_serialization_size(rootValue) + 1;

	buf = calloc(sizeof(char), buf_size);
	json_serialize_to_buffer( rootValue, buf, buf_size);

	debug_json(l, rootValue);

	*dst = (unsigned char *) strdup( buf );

	SAFE_FREE(buf);
	json_value_free(rootValue);

	*dst_len = strlen(*dst);
    printf("svc_code = [%s]. \n",  svc_code);
	return (true);
}
/* 0006 */
bool queue_disable(JSON_Value *req_json_value, JSON_Object *req_json_object, char *svc_code, char **dst, int *dst_len)
{
	int i;
	char date[9], time[7];
	JSON_Value *rootValue = NULL;
	JSON_Object *rootObject = NULL;
	char hash_key[32];

	printf("[%s].\n", __FUNCTION__);

	char qpath[256];
	sprintf(qpath, "%s", json_object_get_string(req_json_object, "Qpath"));

	if( strlen( qpath ) < 2 ) {
		printf("There is no qpath in your request.\n");
		fq_log(l, FQ_LOG_ERROR, "illegal request: There is no Qpath value.");
		return false;
	}
	printf("Qpath=[%s]\n", qpath);

	char qname[32];
	sprintf(qname, "%s", json_object_get_string(req_json_object, "Qname"));
	if( strlen(qname) <  2 ) {
		printf("There is no Qname value.\n");
		fq_log(l, FQ_LOG_ERROR, "illegal request: There is no Qname value.");
		return false;
	}
	printf("Qname=[%s]\n", qname);

	// find a queue object
	fqueue_obj_t *qobj;

	qobj = find_fq_obj( qpath , qname);
	if( !qobj ) {
		printf("Cannot find a queue obj .\n");
		fq_log(l, FQ_LOG_ERROR, "illegal request: Cannot find a qeuue obj.");
		return false;
	}

	int rc;
	rc = qobj->on_disable(l, qobj);
	if( rc != TRUE ) {
		fq_log(l, FQ_LOG_ERROR, "Disable queue is Failed.");
		return false;

	}
	fq_log(l, FQ_LOG_EMERG, "%s, %s is disabled by manager.", qpath, qname);
	
	get_time(date, time);
	rootValue = json_value_init_object();
	rootObject = json_value_get_object(rootValue);

	json_object_set_string(rootObject, "resultCode", "0000"); // success
	json_object_set_string(rootObject, "date", date);
	json_object_set_string(rootObject, "time", time);

	char *buf=NULL;
	size_t buf_size;
	buf_size = json_serialization_size(rootValue) + 1;

	buf = calloc(sizeof(char), buf_size);
	json_serialize_to_buffer( rootValue, buf, buf_size);

	debug_json(l, rootValue);

	*dst = (unsigned char *) strdup( buf );

	SAFE_FREE(buf);
	json_value_free(rootValue);

	*dst_len = strlen(*dst);
    printf("svc_code = [%s]. \n",  svc_code);
	return (true);
}
/* 0007 */
bool queue_enable(JSON_Value *req_json_value, JSON_Object *req_json_object, char *svc_code, char **dst, int *dst_len)
{
	int i;
	char date[9], time[7];
	JSON_Value *rootValue = NULL;
	JSON_Object *rootObject = NULL;
	char hash_key[32];

	printf("[%s].\n", __FUNCTION__);

	char qpath[256];
	sprintf(qpath, "%s", json_object_get_string(req_json_object, "Qpath"));

	if( strlen( qpath ) < 2 ) {
		printf("There is no qpath in your request.\n");
		fq_log(l, FQ_LOG_ERROR, "illegal request: There is no Qpath value.");
		return false;
	}
	printf("Qpath=[%s]\n", qpath);

	char qname[32];
	sprintf(qname, "%s", json_object_get_string(req_json_object, "Qname"));
	if( strlen(qname) <  2 ) {
		printf("There is no Qname value.\n");
		fq_log(l, FQ_LOG_ERROR, "illegal request: There is no Qname value.");
		return false;
	}
	printf("Qname=[%s]\n", qname);

	// find a queue object
	fqueue_obj_t *qobj;

	qobj = find_fq_obj( qpath , qname);
	if( !qobj ) {
		printf("Cannot find a queue obj .\n");
		fq_log(l, FQ_LOG_ERROR, "illegal request: Cannot find a qeuue obj.");
		return false;
	}

	int rc;
	rc = qobj->on_enable(l, qobj);
	if( rc != TRUE ) {
		fq_log(l, FQ_LOG_ERROR, "Enable queue is Failed.");
		return false;

	}
	fq_log(l, FQ_LOG_EMERG, "%s, %s is enabled by manager.", qpath, qname);
	
	get_time(date, time);
	rootValue = json_value_init_object();
	rootObject = json_value_get_object(rootValue);

	json_object_set_string(rootObject, "resultCode", "0000"); // success
	json_object_set_string(rootObject, "date", date);
	json_object_set_string(rootObject, "time", time);

	char *buf=NULL;
	size_t buf_size;
	buf_size = json_serialization_size(rootValue) + 1;

	buf = calloc(sizeof(char), buf_size);
	json_serialize_to_buffer( rootValue, buf, buf_size);

	debug_json(l, rootValue);

	*dst = (unsigned char *) strdup( buf );

	SAFE_FREE(buf);
	json_value_free(rootValue);

	*dst_len = strlen(*dst);
    printf("svc_code = [%s]. \n",  svc_code);
	return (true);
}
/* 0008 */
bool queue_detail(JSON_Value *req_json_value, JSON_Object *req_json_object, char *svc_code, char **dst, int *dst_len)
{
	int i;
	char date[9], time[7];
	JSON_Value *rootValue = NULL;
	JSON_Object *rootObject = NULL;
	char hash_key[32];


	fq_log(l, FQ_LOG_INFO, "0008(queue_detail) function start.");

	char qpath[256];
	memset(qpath, 0x00, sizeof(qpath));
	sprintf(qpath, "%s", json_object_get_string(req_json_object, "Qpath"));

	if( strlen( qpath ) < 2 ) {
		printf("There is no qpath in your request.\n");
		fq_log(l, FQ_LOG_ERROR, "illegal request: There is no Qpath value.");
		return false;
	}

	char qname[32];
	memset(qname, 0x00, sizeof(qname));
	sprintf(qname, "%s", json_object_get_string(req_json_object, "Qname"));
	if( strlen(qname) <  2 ) {
		printf("There is no Qname value.\n");
		fq_log(l, FQ_LOG_ERROR, "illegal request: There is no Qname value.");
		return false;
	}

	// find a queue info from hash
	int rc = TRUE;
	char key[256*2];
	sprintf(key, "%s/%s", qpath, qname);
	HashQueueInfo_t *hash_qi =  get_a_queue_info_in_hashmap_local( l, g_mon_hash_path, g_mon_hash_name,  key);
	
	if( !hash_qi ) {
		fq_log(l, FQ_LOG_ERROR, "Can't find a queue infomation in hash(%s).", g_mon_hash_name);
		return false;
	}

	fq_log(l, FQ_LOG_INFO, "Successfully, We get a queue('%s', '%s') info in haah.", qpath, qname );
	
	get_time(date, time);
	rootValue = json_value_init_object();
	rootObject = json_value_get_object(rootValue);

	json_object_set_string(rootObject, "resultCode", "0000"); // success
	json_object_set_string(rootObject, "date", date);
	json_object_set_string(rootObject, "time", time);

	json_object_dotset_string(rootObject, "queue.path", hash_qi->qpath);
	json_object_dotset_string(rootObject, "queue.name", hash_qi->qname);
	json_object_dotset_number(rootObject, "queue.maxRecCnt", hash_qi->max_rec_cnt);
	json_object_dotset_number(rootObject, "queue.msgLen", hash_qi->msglen);
	json_object_dotset_number(rootObject, "queue.gap", hash_qi->gap);
	json_object_dotset_number(rootObject, "queue.usage", hash_qi->usage);
	json_object_dotset_number(rootObject, "queue.enTps", hash_qi->en_tps);
	json_object_dotset_number(rootObject, "queue.deTps", hash_qi->de_tps);
	json_object_dotset_number(rootObject, "queue.enSum", hash_qi->en_sum);
	json_object_dotset_number(rootObject, "queue.deSum", hash_qi->de_sum);
	json_object_dotset_number(rootObject, "queue.status", hash_qi->status);
	json_object_dotset_number(rootObject, "queue.shmqFlag", hash_qi->shmq_flag);
	json_object_dotset_string(rootObject, "queue.desc", hash_qi->desc);

	int categories = 0;
	char code_value[8];

	char codemap_key[256];
	sprintf(codemap_key, "%s/%s",  hash_qi->qpath, hash_qi->qname);

	rc = get_codeval(l, fq_categories_codemap_t, codemap_key, code_value );
	
	fq_log(l, FQ_LOG_DEBUG, "codemap_key=[%s], code_value=[%s] rc=%d",  
			codemap_key, code_value, rc );

	if( rc == 0 ) { // found
		categories = atoi(code_value);
	}
	else {
		categories = 99; // undefined category.
	}
	json_object_dotset_number(rootObject, "queue.categories", categories);

	char *p_last_en_time;
	p_last_en_time = asc_time(hash_qi->last_en_time);
	json_object_dotset_string(rootObject, "queue.lastEnTime", p_last_en_time);
	SAFE_FREE(p_last_en_time);

	char *p_last_de_time;
	p_last_de_time = asc_time(hash_qi->last_de_time);
	json_object_dotset_string(rootObject, "queue.lastDeTime", p_last_de_time);
	SAFE_FREE(p_last_de_time);

	char *buf=NULL;
	size_t buf_size;
	buf_size = json_serialization_size(rootValue) + 1;

	buf = calloc(sizeof(char), buf_size);
	json_serialize_to_buffer( rootValue, buf, buf_size);

	debug_json(l, rootValue);

	*dst = (unsigned char *) strdup( buf );

	SAFE_FREE(buf);
	json_value_free(rootValue);

	*dst_len = strlen(*dst);
    printf("svc_code = [%s]. \n",  svc_code);
	return (true);
}
/* 0009 */
bool move_queue_XA(JSON_Value *req_json_value, JSON_Object *req_json_object, char *svc_code, char **dst, int *dst_len)
{
	int i;
	char date[9], time[7];
	JSON_Value *rootValue = NULL;
	JSON_Object *rootObject = NULL;

	fq_log(l, FQ_LOG_INFO, "0009(move_queue_XA) function start.");

	char from_qpath[256];
	memset(from_qpath, 0x00, sizeof(from_qpath));
	sprintf(from_qpath, "%s", json_object_get_string(req_json_object, "from_Qpath"));

	if( strlen( from_qpath ) < 2 ) {
		printf("There is no from_Qpath in your request.\n");
		fq_log(l, FQ_LOG_ERROR, "illegal request: There is no Qpath value.");
		return false;
	}

	char from_qname[32];
	memset(from_qname, 0x00, sizeof(from_qname));
	sprintf(from_qname, "%s", json_object_get_string(req_json_object, "from_Qname"));
	if( strlen(from_qname) <  2 ) {
		printf("There is no from_Qname value.\n");
		fq_log(l, FQ_LOG_ERROR, "illegal request: There is no Qname value.");
		return false;
	}

	char to_qpath[256];
	memset(to_qpath, 0x00, sizeof(to_qpath));
	sprintf(to_qpath, "%s", json_object_get_string(req_json_object, "to_Qpath"));

	if( strlen( to_qpath ) < 2 ) {
		printf("There is no to_Qpath in your request.\n");
		fq_log(l, FQ_LOG_ERROR, "illegal request: There is no Qpath value.");
		return false;
	}

	char to_qname[32];
	memset(to_qname, 0x00, sizeof(to_qname));
	sprintf(to_qname, "%s", json_object_get_string(req_json_object, "to_Qname"));
	if( strlen(to_qname) <  2 ) {
		printf("There is no to_Qname value.\n");
		fq_log(l, FQ_LOG_ERROR, "illegal request: There is no Qname value.");
		return false;
	}
	
	int move_req_number = json_object_get_number(req_json_object, "MoveReqNumber");

	int moved_count;
	moved_count = MoveFileQueue_XA( l, from_qpath, from_qname, to_qpath, to_qname, move_req_number);
	if( moved_count < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "MoveFileQueue_XA('%s', '%s' -> '%s', '%s') failed.", from_qpath, from_qname, to_qpath, to_qname);
		return false;
	}

	fq_log(l, FQ_LOG_INFO, "Successsfully, Queue is moved from ('%s', '%s') to ('%s', '%s'),", from_qpath, from_qname, to_qpath, to_qname);

	get_time(date, time);
	rootValue = json_value_init_object();
	rootObject = json_value_get_object(rootValue);

	json_object_set_string(rootObject, "resultCode", "0000"); // success
	json_object_set_string(rootObject, "date", date);
	json_object_set_string(rootObject, "time", time);
	json_object_set_number(rootObject, "MovedCount", moved_count);

	char *buf=NULL;
	size_t buf_size;
	buf_size = json_serialization_size(rootValue) + 1;
	buf = calloc(sizeof(char), buf_size);
	json_serialize_to_buffer( rootValue, buf, buf_size);

	debug_json(l, rootValue);

	*dst = (unsigned char *) strdup( buf );

	SAFE_FREE(buf);
	json_value_free(rootValue);

	*dst_len = strlen(*dst);

    fq_log(l, FQ_LOG_INFO, "Success!!, svc_code = [%s]. \n",  svc_code);

	return (true);
}

typedef struct _svc_funcptr_map_t {
	char *svc_code;
	bool (*func_ptr)(JSON_Value *, JSON_Object *, char *, char **, int *);
} svc_funcptr_map_t;

svc_funcptr_map_t svc_func_map_table[] = {
	{"0000", &test},
	{"0001", &all_q_status},
	{"0002", &get_ratio},
	{"0003", &set_ratio},
	{"0004", &channel_co_down},
	{"0005", &proc_stop},
	{"0006", &queue_disable},
	{"0007", &queue_enable},
	{"0008", &queue_detail},
	{"0009", &move_queue_XA},
	{"0010", &get_admin_phone_no_list},
	{ NULL, 0x00 }
};

int get_func_index( char *svc_code )
{
	int i;
	assert(svc_code);

	for(i=0; svc_func_map_table[i].svc_code; i++) {
		if( strcmp( svc_code, svc_func_map_table[i].svc_code) == 0 ) {
			return i;
		}
	}
	return (-1); // not found
}

void sig_exit_handler( int signal )
{
    printf("sig_pipe_handler() \n");
	exit(EXIT_FAILURE);
}

int CB_function( unsigned char *data, int len, unsigned char *resp, int *resp_len)
{
	char *resp_message = NULL;
	char *err_code = "1000";
	char svc_code[256];

	fprintf(stdout, "CB called. [%s] len=[%d]\n", data+4, len);
	fq_log(l, FQ_LOG_INFO, "CB called.");

	/*
	** Warning : Variable data has value of header.so If you print data to sceeen, you can't see real data.
	** so you have to add 4bytes(header length) for saving and printing.
	*/
	fq_log(l, FQ_LOG_INFO, "len=[%d] [%-s]\n", len, data+4);

	/* processing */
	/* 1. Analysis request ( JSON by parson library )
	** 2. Select a job function with service_code.
	** 2. Make a response date and return in the service function.(JSON Type )
	*/


	char *json_request_msg = calloc( len-4+1, sizeof(unsigned char));
	CHECK(json_request_msg);
	json_request_msg = strdup( data+4 );

	/* Make a JSON RootValue with  a UMS message */
	JSON_Value *ums_JSONrootValue = NULL;
	ums_JSONrootValue = json_parse_string(json_request_msg);
	if( !ums_JSONrootValue ) { // We throw it.
		printf("illegal json format.\n");
		fq_log(l, FQ_LOG_ERROR, "illegal json format. We throw it.\n");
		sprintf(svc_code, "%s", "Unknown");
		goto error_return;
	}
	// CHECK( ums_JSONrootValue );

	/* Make a JSON Object with a JSONRootValue.*/
	JSON_Object *ums_JSONObject = NULL;
	ums_JSONObject = json_value_get_object(ums_JSONrootValue); 
	CHECK( ums_JSONObject  );

	printf("JSON parsing success!.\n");

	sprintf(svc_code, "%s", json_object_get_string(ums_JSONObject, "svc_code"));
	printf("svc_code = '%s'\n", svc_code);

	int func_index = get_func_index( svc_code );
	if( func_index < 0 ) {
		err_code = "2000";
		goto error_return;
	}
	printf("func_index = [%d]\n", func_index );

	char *output = NULL;
	int output_len = 0;

	/* service call */
	bool tf = svc_func_map_table[func_index].func_ptr( ums_JSONrootValue, ums_JSONObject, svc_code, &output, &output_len );
	if( tf == true ) {
		printf("svc func result: '%s' len=%d.\n", output, output_len);
		memcpy(resp, output, output_len);
		*resp_len = output_len;
	}
	else {
		err_code = "3000"; 
		goto  error_return;
	}

#if 0

	char buf_phone_no[20];
	sprintf(buf_phone_no, "%s", json_object_get_string(ums_JSONObject, "Phone_no"));

	int	ums_seq;
	ums_seq = (int)json_object_get_number(ums_JSONObject, "ums_seq");

	char buf_template[2048];
	sprintf(buf_template, "%s", json_object_get_string(ums_JSONObject, "Template"));
	printf("Template=%s\n", buf_template);

	char buf_var_data[2048];
	sprintf(buf_var_data, "%s", json_object_get_string(ums_JSONObject, "Vardata"));
	printf("Vardata=%s\n", buf_var_data);

	sprintf(channel, "%s", json_object_get_string(ums_JSONObject, "Channel"));
	printf("Channel=%s\n", channel);

	bool GssiFlag = json_object_get_boolean(ums_JSONObject, "GssiFlag");
	printf("GssiFlag=%d\n", GssiFlag);
#endif

	/* make a response data ( JSON SSIP ) */

	fq_log(l, FQ_LOG_DEBUG, "response =[%s] resp_len=[%d]", resp, *resp_len);

	json_value_free(ums_JSONrootValue);
	SAFE_FREE(json_request_msg);

	return(*resp_len);

error_return:
	*resp_len = make_error_response( l, svc_code, err_code,  &resp_message );
	CHECK(*resp_len > 0);
	memcpy(resp, resp_message, *resp_len);

	SAFE_FREE(json_request_msg);
	SAFE_FREE(resp_message);

	return(*resp_len);
}

static int make_error_response( fq_logger_t *l, char *svc_code, char *err_code, char **dst)
{
	int n = 0;

	int rc;
	char resp_SSIP_result[65536];
	int file_len;
	unsigned char *json_format_string = NULL;
	char *errmsg = NULL;
	cache_t	*cache_short_for_gssi;
	codemap_t	*t=NULL;
	char err_message[1024];

	t = new_codemap(l, "err_code_map");
	rc = read_codemap(l, t, err_codemap_file);
	CHECK(rc >= 0);

	rc = get_codeval(l, t, err_code, err_message);
	CHECK(  rc == 0 );

	cache_short_for_gssi = cache_new('S', "short term cache");

	char Date[9];
	char Time[7];
	char date_time[16];

	get_time(Date, Time);
	sprintf(date_time,  "%s:%s", Date, Time);
	cache_update(cache_short_for_gssi, "date_time", date_time, 0 );
	cache_update(cache_short_for_gssi, "svc_code", svc_code, 0 );
	cache_update(cache_short_for_gssi, "err_code", err_code, 0 );
	cache_update(cache_short_for_gssi, "err_message", err_message, 0 );

	rc = LoadFileToBuffer( err_report_format_file, &json_format_string, &file_len,  &errmsg);
	if( errmsg ) printf("errmsg -> '%s' filename='%s'\n", errmsg, err_report_format_file);
	CHECK(rc > 0);

	rc = gssi_proc( l, json_format_string, "", "", cache_short_for_gssi, '|', resp_SSIP_result, sizeof(resp_SSIP_result));
	if( rc < 0 ) {
		printf("gssi_proc() error( not found tag ). rc=[%d]\n", rc);
		return(-1);
	}

	printf("send SSIP Success!!!\n");
	printf("send SSIP result: =[%s], len=[%ld]\n", resp_SSIP_result, strlen(resp_SSIP_result));

	CHECK( strlen(resp_SSIP_result) > 0 );
	printf("JSON Aseemble OK. len=[%ld]\n", strlen(resp_SSIP_result));

	if(t) free_codemap(l, &t);

	*dst = strdup(resp_SSIP_result);

	return (strlen(resp_SSIP_result));
}

void print_help(char *p)
{
	printf("\n\nUsage  : $ %s [-V] [-h] [-f config_file] [-i ip] [-p port] [-l logname] [-o loglevel] <enter>\n", p);
	printf("\t	-V: version \n");
	printf("\t	-h: help \n");
	printf("\t	-f: config_file \n");
	printf("\t	-p: port \n");
	printf("\t	-l: logfilename \n");
	printf("\t  -o: log level ( trace|debug|info|error|emerg )\n");
	printf("example: $ %s -i 127.0.0.1 -p 6666 -l /tmp/%s.log -o debug <enter>\n", p, p);
	printf("example: $ %s -f FQ_server_solaris.conf <enter>\n", p);
	return;
}

int main(int ac, char **av)
{
	codemap_t *t=NULL; // categories codemap

	TCP_server_t x;
	fq_container_t *fq_c = NULL;
	char *hash_map_dir = NULL;
	char *hash_map_name = NULL;

	int	i_loglevel = 0;
	int	ack_mode = 1; /* ACK 모드: FQ_server는 항상 1로 설정한다. */

	int	rc = -1;
	int	ch;

	printf("Compiled on %s %s source program version=[%s]\n\n\n", __TIME__, __DATE__, FQ_SERVER_C_VERSION);

	getProgName(av[0], g_Progname);

	while(( ch = getopt(ac, av, "Hh:p:Q:l:i:P:o:f:")) != -1) {
		switch(ch) {
			case 'H':
			case 'h':
				print_help(av[0]);
				return(0);
			case 'i':
				ip = strdup(optarg);
				break;
			case 'p':
				port = strdup(optarg);
				break;
			case 'l':
				logname = strdup(optarg);
				break;
			case 'o':
				loglevel = strdup(optarg);
				break;
			case 'f':
				conf = strdup(optarg);
				break;
			default:
				print_help(av[0]);
				return(0);
		}
	}

	if(HASVALUE(conf) ) { /* If there is a config file ... */
		config_t *t=NULL;
		char	buffer[128];

		t = new_config(conf);

		if( load_param(t, conf) < 0 ) {
			fprintf(stderr, "load_param() error.\n");
			return(EXIT_FAILURE);
		}
		/* server environment */
		get_config(t, "BIN_DIR", buffer);
		bin_dir = strdup(buffer);
		CHECK(bin_dir);

		/* 사용자가 정한 항목을 읽을 경우 */
		get_config(t, "FQ_SERVER_IP", buffer);
		ip = strdup(buffer);
		CHECK(ip);

		get_config(t, "FQ_SERVER_PORT", buffer);
		port = strdup(buffer);
		CHECK(port);

		get_config(t, "HEADER_TYPE", buffer);
		header_type = strdup(buffer);
		CHECK(header_type);
		if( strcmp(header_type, "BINARY") == 0 ) {
			i_header_type = BINARY_HEADER_TYPE;
		}
		else if( strcmp(header_type, "ASCII") == 0 ) {
            i_header_type = ASCII_HEADER_TYPE;
        }
		else {
			printf("[%s] is unsupported header type.(BINARY or ASCII).\n", header_type);
			exit(0);	
		}

		get_config(t, "LOG_NAME", buffer);
		logname = strdup(buffer);
		CHECK(logname);

		get_config(t, "LOG_LEVEL", buffer);
		loglevel = strdup(buffer);
		CHECK(loglevel);

		get_config(t, "ERR_REPORT_FORMAT_FILE", buffer);
		err_report_format_file = strdup(buffer);
		CHECK(err_report_format_file);

		get_config(t, "ERR_CODEMAP_FILE", buffer);
		err_codemap_file = strdup(buffer);
		CHECK(err_codemap_file);

		get_config(t, "HASH_MAP_DIR", buffer);
		hash_map_dir = strdup(buffer);
		CHECK(hash_map_dir);

		g_mon_hash_path = strdup(buffer);

		get_config(t, "HASH_MAP_NAME", buffer);
		hash_map_name = strdup(buffer);
		CHECK(hash_map_name);

		get_config(t, "RATIO_CTRL_ONLINE_Q_PATH", buffer);
		ratio_ctrl_online_q_path = strdup(buffer);
		CHECK(ratio_ctrl_online_q_path);

		get_config(t, "RATIO_CTRL_ONLINE_Q_NAME", buffer);
		ratio_ctrl_online_q_name = strdup(buffer);
		CHECK(ratio_ctrl_online_q_name);

		get_config(t, "RATIO_CTRL_BATCH_Q_PATH", buffer);
		ratio_ctrl_batch_q_path = strdup(buffer);
		CHECK(ratio_ctrl_batch_q_path);

		get_config(t, "RATIO_CTRL_BATCH_Q_NAME", buffer);
		ratio_ctrl_batch_q_name = strdup(buffer);
		CHECK(ratio_ctrl_batch_q_name);

		get_config(t, "MON_HASH_PATH", buffer);
		g_mon_hash_path = strdup(buffer);
		CHECK(g_mon_hash_path);

		get_config(t, "MON_HASH_NAME", buffer);
		g_mon_hash_name = strdup(buffer);
		CHECK(g_mon_hash_path);

		get_config(t, "CHANNEL_CO_RATIO_MAPFILE", buffer);
		g_channel_co_ratio_mapfile = strdup(buffer);
		CHECK(g_channel_co_ratio_mapfile);

		get_config(t, "FQ_CATEGORIES_CODEMAP_FILE", buffer);
		g_fq_categories_codemap_file = strdup(buffer);
		CHECK(g_fq_categories_codemap_file);

		get_config(t, "ADMIN_PHONE_MAP_FILENAME", buffer);
		g_admin_phone_map_filename = strdup(buffer);
		CHECK(g_admin_phone_map_filename);

		get_config(t, "HEARTBEAT_HASH_MAP_PATH", buffer);
		g_heartbeat_hash_map_path = strdup(buffer);
		CHECK(g_heartbeat_hash_map_path);

		get_config(t, "HEARTBEAT_HASH_MAP_NAME", buffer);
		g_heartbeat_hash_map_name = strdup(buffer);
		CHECK(g_heartbeat_hash_map_name);

		get_config(t, "FQ_PID_LIST_FILE", buffer);
		g_fq_pid_list_file = strdup(buffer);
		CHECK(g_fq_pid_list_file);

		get_config(t, "DISTRIBUTOR_COUNT", buffer);
		g_distributor_count = strdup(buffer);
		CHECK(g_distributor_count);
		i_distributor_count = atoi(g_distributor_count);


		get_config(t, "LISTFILE_TO_EXCLUDE", buffer);
		g_listfile_to_exclude = strdup(buffer);

		free_config(&t);
	}

	/* Checking mandatory parameters */
	if( !ip || !port || !logname || !loglevel) {
		print_help(av[0]);
		printf("User input: ip=[%s] port=[%s] logname=[%s] loglevel=[%s]\n", 
			VALUE(ip), VALUE(port), VALUE(logname), VALUE(loglevel));
		return(0);
	}

	/* open fq_mon_svr hashmap */
	g_mon_hash_path = getenv("FQ_HASH_HOME");
	if(!g_mon_hash_path) {
		fprintf(stderr, "There is no FQ_HASH_HINE in your env. Please run after setting.");
		exit(0);
	}

	bool tf;
	tf = check_alive_fq_mon_svr();
	if( tf == false ) {
		fprintf(stderr, "There is no fq_mon_svr(process). Run fq_mon_svr and restart it\n.");
		// CHECK(tf);
		return(0);
	}
	/*
	** make background daemon process.
	*/
	// daemon_init();

	signal(SIGPIPE, SIG_IGN);


	init_Qs();

	rc = load_fq_container(l, &fq_c);
	if( rc != TRUE ) {
		fq_log(l, FQ_LOG_ERROR, "load_fq_container() error.");
		return(0);
	}

	/********************************************************************************************
	** Open a file for logging
	*/
	i_loglevel = get_log_level(loglevel);
	rc = fq_open_file_logger(&l, logname, i_loglevel);
	CHECK( rc > 0 );
	/**********************************************************************************************/

	/* Make a exclude linkedlist */
	rc = open_file_list(l, &exclude_list_obj, g_listfile_to_exclude);
    CHECK( rc == TRUE );
	fq_log(l, FQ_LOG_INFO, "Count of exclude list is [%d]\n", exclude_list_obj->count);

	rc = open_all_file_queues(l, fq_c, &opened_q_cnt);
	if(rc != 0 ) {
		fq_log(l, FQ_LOG_ERROR, "open_all_file_queues() error.");
		return(0);
	}

	/* For creating, Use /utl/ManageHashMap command */
	rc = OpenHashMapFiles(l, hash_map_dir, hash_map_name, &hash_obj);
	CHECK(rc==TRUE);
	fq_log(l, FQ_LOG_INFO, "ums HashMap open ok.");


	/* find a ctrl queue obj in table */
	ratio_ctrl_online_qobj = find_fq_obj( ratio_ctrl_online_q_path , ratio_ctrl_online_q_name);
	CHECK(ratio_ctrl_online_qobj);

	ratio_ctrl_batch_qobj = find_fq_obj( ratio_ctrl_batch_q_path , ratio_ctrl_batch_q_name);
	CHECK(ratio_ctrl_batch_qobj);

	fq_categories_codemap_t = new_codemap(l, "fq_categories");
	CHECK(fq_categories_codemap_t);
	if( (rc=read_codemap(l, fq_categories_codemap_t, g_fq_categories_codemap_file)) < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "readcode_map('%s') failed\n", g_fq_categories_codemap_file);
		return(0);
    }

	/* make a linkedlist phone map */
	admin_phone_map_ll = linkedlist_new("admin_phone_list");
	tf = MakeLinkedList_admin_phone_mapfile( l, g_admin_phone_map_filename, 0x20, admin_phone_map_ll );
	if( tf == false ) {
		fq_log(l, FQ_LOG_ERROR, "MakeLinkedList_admin_phone_mapfile('%s') failed\n", g_fq_categories_codemap_file);
		return(0);
	}



	tf = OpenHashMapFiles(l, g_heartbeat_hash_map_path, g_heartbeat_hash_map_name, &heartbeat_hash_obj);
	if(tf == false) {
		fq_log(l, FQ_LOG_ERROR, "Can't open ums hashmap(path='%s', name='%s'). Please check hashmap.", 
				g_heartbeat_hash_map_path, g_heartbeat_hash_map_name);
		return(0);
	}

	fq_log(l, FQ_LOG_INFO, "ums HashMap(path='%s', name='%s') open ok.", g_heartbeat_hash_map_path, g_heartbeat_hash_map_name);

	if( bin_dir && conf ) {
		/**********************************************************************************************
		*/
		signal(SIGPIPE, sig_exit_handler); /* retry 시에는 반드시 헨들러를 재 구동하여야 한다 */
		signal(SIGTERM, sig_exit_handler); /* retry 시에는 반드시 헨들러를 재 구동하여야 한다 */
		signal(SIGQUIT, sig_exit_handler); /* retry 시에는 반드시 헨들러를 재 구동하여야 한다 */
		/************************************************************************************************/
	}

	make_thread_sync_CB_server( l, &x, ip, atoi(port), i_header_type, 4, ack_mode, CB_function);
	// make_thread_sync_CB_server( l, &x, ip, atoi(port), BINARY_HEADER_TYPE, 4, ack_mode, CB_function);
	// make_thread_sync_CB_server( l, &x, ip, atoi(port), ASCII_HEADER_TYPE, 4, ack_mode, CB_function);

	fq_log(l, FQ_LOG_EMERG, "FQ server (entrust Co.version %s) start compiled [%s][%s]!!!.(listening ip=[%s] port=[%s]", 
		FQ_SERVER_C_VERSION, __DATE__, __TIME__, ip, port);

	// We do not use heartbeat in webgate
	// append_mypid_fq_pid_listfile_for_heartbeat(l, g_fq_pid_list_file);

	pause();

	close_all_file_queues(l);

	free_codemap(l, &fq_categories_codemap_t);

	rc = CloseHashMapFiles(l, &hash_obj);
    CHECK(rc==TRUE);

    fq_close_file_logger(&l);
	exit(EXIT_FAILURE);
}
/*
** This function open all file queues and create each monitoring thread.
*/
int open_all_file_queues(fq_logger_t *l, fq_container_t *fq_c, int *opened_q_cnt) {
	dir_list_t *d=NULL;
	fq_node_t *f;
	int opened = 0;

	for( d=fq_c->head; d!=NULL; d=d->next) {
		for( f=d->head; f!=NULL; f=f->next) {
			int rc;

			fq_log(l, FQ_LOG_DEBUG, "OPEN: [%s/%s].", d->dir_name, f->qname);

			if( is_exclude_queue_name( l, exclude_list_obj, f->qname) == true) {
				fq_log(l, FQ_LOG_INFO, "We exclude [%s] in monitoring.", f->qname);
				continue;
			}
			
			char *p=NULL;
			if( (p= strstr(f->qname, "SHM_")) == NULL ) {
				rc =  OpenFileQueue(l, NULL, NULL, NULL, NULL, d->dir_name, f->qname, &Qs[opened].obj);
			} 
			else {
				rc =  OpenShmQueue(l, d->dir_name, f->qname, &Qs[opened].obj);
				Qs[opened].shmq_flag = 1;
			}
			if( rc != TRUE ) {
				fq_log(l, FQ_LOG_ERROR, "OpenFileQueue('%s') error.", f->qname);
				return(-1);
			}
			fq_log(l, FQ_LOG_DEBUG, "%s:%s open success.", d->dir_name, f->qname);

			opened++;
		}
	}

	fq_log(l, FQ_LOG_INFO, "Number of opened filequeue is [%d].", opened);
	*opened_q_cnt = opened;
	return(0);
}

void close_all_file_queues(fq_logger_t *l) {
	int i;

	for(i=0; Qs[i].obj; i++) {
		if( Qs[i].obj  && (Qs[i].shmq_flag == 0)) CloseFileQueue(l, &Qs[i].obj);
		else if( Qs[i].obj  && (Qs[i].shmq_flag == 1)) CloseShmQueue(l, &Qs[i].obj);
	}
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

bool MakeLinkedList_channel_co_ratio_obj_with_mapfile(fq_logger_t *l, char *mapfile, char delimiter, linkedlist_t *ll)
{
	file_list_obj_t *filelist_obj=NULL;
	FILELIST *this_entry=NULL;
	int rc;
	int t_no;

	rc = open_file_list(l, &filelist_obj, mapfile);
	CHECK( rc == TRUE );

	printf("count = [%d]\n", filelist_obj->count);

	// filelist_obj->on_print(filelist_obj);

	this_entry = filelist_obj->head;
	for( t_no=0;  (this_entry->next && this_entry->value); t_no++ ) {
		printf("this_entry->value=[%s]\n", this_entry->value);

		bool tf;
		char channel_name[16];
		char co_ratio_string[255];
		tf = get_LR_value(this_entry->value, delimiter, channel_name, co_ratio_string);
		printf("channel_name='%s', ratio_string='%s'.\n", channel_name, co_ratio_string);


		// For webgate , We initialize hash value.
		//
		// char hash_put_key[128];
		// sprintf(put_key, "%s_RATIO", channel_name);
		// rc = PutHash(l, hash_obj, put_key, co_ratio_string);
    	// CHECK(rc == TRUE);


		char fields_delimiter=',';
		char value_delimiter = ':';

		channel_ratio_obj_t *tmp=NULL;
		tmp = (channel_ratio_obj_t *)calloc(1, sizeof(channel_ratio_obj_t));
		sprintf(tmp->channel_name, "%s", channel_name);

		tf =  open_ratio_distributor( l, co_ratio_string, fields_delimiter, value_delimiter, &tmp->ratio_obj);
		CHECK(tf);

		ll_node_t *ll_node=NULL;
		ll_node = linkedlist_put(ll, channel_name, (void *)tmp, sizeof(char)+sizeof(ratio_obj_t));
		CHECK(ll_node);

		// close_ratio_distributor(l, &tmp->ratio_obj);
		this_entry = this_entry->next;
	}
	if( filelist_obj ) close_file_list(l, &filelist_obj);


	printf("ll->length is [%d]\n", ll->length);

	return true;
}

static bool find_a_queue_info( fq_logger_t *l, hashmap_obj_t *hash_obj,  linkedlist_t *ll, char *key, HashQueueInfo_t *dst )
{
	FQ_TRACE_ENTER(l);

	ll_node_t *p;
	int	array_index;
	
	for(p=ll->head, array_index=0; p!=NULL; p=p->next, array_index++) {
		if( strcmp(key, p->key) != 0 ) {
			continue;
		}

		char *ts1=strdup(p->key);
		char *ts2=strdup(p->key);

		char *qpath=dirname(ts1);
		char *qname=basename(ts2);

		sprintf(dst->qpath, "%s", qpath);
		sprintf(dst->qname, "%s", qname);

		free(ts1);
		free(ts2);

		int rc;
		char *hash_value = NULL;
		rc = GetHash(l, hash_obj, p->key, &hash_value);

		if( rc == TRUE ) {
			fqlist_t *ll = fqlist_new('A', "delimiter");
			CHECK(ll);

			int value_count;
			value_count = MakeListFromDelimiterMsg( ll, hash_value, '|', 0 );
			fq_log(l, FQ_LOG_DEBUG, "value count = %d", value_count);

			tlist_t *t = ll->list;
			node_t *p;

			int t_no=0;
			for (  p = t->head ; p != NULL ; p=p->next, t_no++ ) {
				switch(t_no) {
					case 0:
						dst->msglen = atoi(p->value);
						break;
					case 1:
						dst->max_rec_cnt = atoi(p->value);
						break;
					case 2:
						dst->gap = atoi(p->value);
						break;
					case 3:
						dst->usage = atof(p->value);
						break;
					case 4:
						dst->en_competition = atol(p->value);
						break;
					case 5:
						dst->de_competition = atol(p->value);
						break;
					case 6:
						dst->big = atoi(p->value);
						break;
					case 7:
						dst->en_tps = atoi(p->value);
						break;
					case 8:
						dst->de_tps = atoi(p->value);
						break;
					case 9:
						dst->en_sum = atol(p->value);
						break;
					case 10:
						dst->de_sum = atol(p->value);
						break;
					case 11:
						dst->status = atoi(p->value);
						break;
					case 12:
						dst->shmq_flag = atoi(p->value);
						break;
					case 13:
						sprintf(dst->desc, "%s", p->value);
						break;
					case 14:
						dst->last_en_time = atol(p->value);
						break;
					case 15:
						dst->last_de_time = atol(p->value);
						break;
					default:
						break;

				}
			} // for end.
			if(ll) fqlist_free(&ll);
			SAFE_FREE(hash_value);

			break;
		} else {
			fq_log(l, FQ_LOG_ERROR,  "[%s] is not found in hashmap.", p->key);
		}

	}// for end.

	if( dst->msglen > 0 ) {
		FQ_TRACE_EXIT(l);
		return true;
	}
	else {
		FQ_TRACE_EXIT(l);
		return false;
	}
}
static HashQueueInfo_t *get_a_queue_info_in_hashmap_local( fq_logger_t *l, char *hash_path, char *hash_name,  char *key)
{
	HashQueueInfo_t *p=NULL;

	FQ_TRACE_ENTER(l);

	p = calloc(1, sizeof(HashQueueInfo_t));
	linkedlist_t *ll = linkedlist_new("file queue linkedlist.");
	CHECK(ll);

	hashmap_obj_t *hash_obj=NULL;
	int rc;
	rc = OpenHashMapFiles(l, hash_path, hash_name, &hash_obj);
    CHECK(rc==TRUE);

	bool tf = MakeLinkedList_filequeues(l, ll);
	CHECK(tf);

	p = (HashQueueInfo_t *)calloc(1, sizeof(HashQueueInfo_t));
	CHECK(p);

	tf = find_a_queue_info(l, hash_obj, ll, key, p);
	if( tf == false ) {
		if(hash_obj) CloseHashMapFiles(l, &hash_obj);
		if(ll) linkedlist_free(&ll);

		FQ_TRACE_EXIT(l);
		return NULL;
	}
	// CHECK(tf);

	if(hash_obj) CloseHashMapFiles(l, &hash_obj);

	if(ll) linkedlist_free(&ll);

	FQ_TRACE_EXIT(l);

	return(p);
}

/* 0010 */
bool get_admin_phone_no_list(JSON_Value *req_json_value, JSON_Object *req_json_object, char *svc_code, char **dst, int *dst_len)
{
	int i;
	char date[9], time[7];
	JSON_Value *rootValue = NULL;
	JSON_Object *rootObject = NULL;

	printf("[%s].\n", __FUNCTION__);

	get_time(date, time);
	rootValue = json_value_init_object();
	rootObject = json_value_get_object(rootValue);

	json_object_set_string(rootObject, "resultCode", "0000");
	json_object_set_string(rootObject, "date", date);
	json_object_set_string(rootObject, "time", time);
	json_object_set_number(rootObject, "countOfQueues", opened_q_cnt);

	json_object_set_value(rootObject, "phoneList", json_value_init_array());
	JSON_Array *Phones = json_object_get_array(rootObject, "Phones");


	ll_node_t *node_p = NULL;
	int t_no;
	for( node_p=admin_phone_map_ll->head, t_no=0; (node_p != NULL); (node_p=node_p->next, t_no++) ) {
		admin_phone_map_t *tmp;
		tmp = (admin_phone_map_t *) node_p->value;
		fq_log(l, FQ_LOG_INFO, "We send SMS to Admin phone no:: [%s] name: [%s]\n",
			tmp->phone_no,
			tmp->name);

		JSON_Value *subValue = NULL;
		JSON_Object *subObject = NULL;
		
		subValue = json_value_init_object();
		subObject = json_value_get_object(subValue);

		json_object_set_string(subObject, "phoneNo", tmp->phone_no);
		json_object_set_string(subObject, "name", tmp->name);

		json_array_append_value(Phones, subValue);
	}

	char *buf=NULL;
	size_t buf_size;
	buf_size = json_serialization_size(rootValue) + 1;
	buf = calloc(sizeof(char), buf_size);
	json_serialize_to_buffer( rootValue, buf, buf_size);

	debug_json(l, rootValue);

	*dst = (unsigned char *) strdup( buf );


	SAFE_FREE(buf);
	json_value_free(rootValue);

	*dst_len = strlen(*dst);
    printf("svc_code = [%s]. \n",  svc_code);
	return (true);
}
static bool MakeLinkedList_admin_phone_mapfile( fq_logger_t *l, char *mapfile, char delimiter, linkedlist_t	*ll)
{
	file_list_obj_t *filelist_obj=NULL;
	FILELIST *this_entry=NULL;
	int rc;
	int t_no;

	FQ_TRACE_ENTER(l);


	fq_log(l, FQ_LOG_DEBUG, "mapfile='%s'" , mapfile);

	rc = open_file_list(l, &filelist_obj, mapfile);
	CHECK( rc == TRUE );

	printf("count = [%d]\n", filelist_obj->count);

	// filelist_obj->on_print(filelist_obj);

	this_entry = filelist_obj->head;
	for( t_no=0;  (this_entry->next && this_entry->value); t_no++ ) {
		char *errmsg = NULL;
		printf("this_entry->value=[%s]\n", this_entry->value);

		char	phone_no[12]; // SEND, RECV
		char	name[128]; // deQ

		int cnt;

		cnt = fq_sscanf(delimiter, this_entry->value, "%s%s", phone_no, name);

		CHECK(cnt == 2);

		fq_log(l, FQ_LOG_INFO, "phone_no='%s', name='%s'.", phone_no, name);

		admin_phone_map_t *tmp = NULL;
		tmp = (admin_phone_map_t *)calloc(1, sizeof(admin_phone_map_t));

		char	key[11];
		memset(key, 0x00, sizeof(key));
		sprintf(key, "%010d", t_no);

		sprintf(tmp->phone_no, "%s", phone_no);
		sprintf(tmp->name, "%s", name);

		ll_node_t *ll_node=NULL;
		ll_node = linkedlist_put(ll, key, (void *)tmp, sizeof(char)+sizeof(admin_phone_map_t));
		CHECK(ll_node);

		this_entry = this_entry->next;
	}

	if( filelist_obj ) close_file_list(l, &filelist_obj);
	
	fq_log(l, FQ_LOG_INFO, "ll->length is [%d]", ll->length);

	FQ_TRACE_EXIT(l);
	return true;
}
int Sure_GetHash( fq_logger_t *l, hashmap_obj_t *hash_obj, char *key, char **value)
{
	int rc;
	char *out=NULL;
	
	FQ_TRACE_ENTER(l);

reget:
	rc = GetHash( l, hash_obj, key, &out);
	if( rc == TRUE) {
		if( out[0] == 0x00) {
			SAFE_FREE(out);	
			usleep(100000);
			goto reget;
		}
		*value = strdup(out);
	}
	else {
		return FALSE;
	}

	SAFE_FREE(out);
	FQ_TRACE_EXIT(l);

	return(TRUE);
}

static bool is_exclude_queue_name( fq_logger_t *l, file_list_obj_t *exclude_list_obj, char *qname)
{
  FILELIST *this_entry=NULL;

  FQ_TRACE_ENTER(l);

  if( exclude_list_obj->head == NULL) {
  	FQ_TRACE_EXIT(l);
	return false;
  }

  this_entry = exclude_list_obj->head;

  while (this_entry->next && this_entry->value ) {
	if( strcmp(this_entry->value, qname) == 0 ) {
		FQ_TRACE_EXIT(l);
		return true;
	}
	this_entry = this_entry->next;
  }

  FQ_TRACE_EXIT(l);
  return false;
}
