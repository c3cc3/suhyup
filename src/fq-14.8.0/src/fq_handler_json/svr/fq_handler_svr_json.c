/*
** FQ_Protocol_svr_json.c
** This program has objects of all queues
** 
*/
#define FQ_PROTOCOL_SVR_JSON__C_VERSION	"1.0.1"

#include <stdio.h>
#include <getopt.h>
#include "fq_param.h"
#include "fqueue.h"
#include "shm_queue.h"
#include "fq_socket.h"
#include "fq_logger.h"
#include "fq_common.h"
#include "fq_monitor.h"
#include "fq_protocol.h"
#include "fq_config.h"
#include "parson.h"
#include "fq_file_list.h"
#include "fq_manage.h"
#include "fq_utl.h"
#include "fq_scanf.h"
#include "fq_handler_conf.h"
/*
** Session Control
*/
#define MAX_SESSION_LIMIT       1000 /* concurent users, If you decide this value according to available memory size. */
#define SKEY_SIZE				32
#define MAX_OPEN_QUEUES	1000

void make_empty_response_msg(fq_logger_t *l, int error_code , char *skey, JSON_Value *r_jsonValue, JSON_Object *r_jsonObject, unsigned char *resp, int *resp_len );
void make_full_response_msg(fq_logger_t *l, int error_code , char *skey, JSON_Value *r_jsonValue, JSON_Object *r_jsonObject, unsigned char *resp, int *resp_len );
void make_success_response_msg( fq_logger_t *l, int req, int DEQU_rc, int sid, JSON_Value *jsonValue, JSON_Object *jsonObject, unsigned char *resp, char *msg, int *resp_len);
void make_fail_response_msg(fq_logger_t *l, int error_code , char *skey, JSON_Value *r_jsonValue, JSON_Object *r_jsonObject, unsigned char *resp, int *resp_len );
void *session_view_thread( void *arg );
static int handler_eventlog(fq_logger_t *l, char *eventlog_filename, ...);
static bool MakeLinkedList_check_filesystem_mapfile(char *mapfile, char delimiter, linkedlist_t *ll);
bool CheckFileSystem( fq_handler_conf_t *my_conf, linkedlist_t *check_filesystem_ll);

struct _thread_param {
    fq_logger_t *l;
};

linkedlist_t *ll_qobj = NULL;

int g_session_timeout_sec;

typedef struct _thread_param thread_param_t;

typedef struct {
	char    skey[SKEY_SIZE+1];
	char	*qpath;
	char	*qname;
	long	tot_req;
	char*   client_addr;
	time_t  last_access_time;
	pthread_cond_t  cond;
} sess_t;

typedef struct {
	pthread_mutex_t lock;
	sess_t		*sess;
	time_t  	set_time;
} SessionTable;

typedef struct {
	int	current_users;
	SessionTable GlbSm[MAX_SESSION_LIMIT+1];
}SessionStage;

SessionStage SS;

int set_skey(fq_logger_t *l, sess_t *s);
int trylock_session(fq_logger_t *l, int sid);
int lock_session(fq_logger_t *l, int sid);

/*
** global variables
*/
int last_sid = 0;
int n_users=0;
fq_logger_t *l=NULL;
char restart_cmd[256];
char g_Progname[64];
int	error_code = 0;
char *g_eventlog_filename;

pthread_mutex_t lock_nusers = PTHREAD_MUTEX_INITIALIZER;

int trylock_session(fq_logger_t *l, int sid)
{
	int rc;

	FQ_TRACE_ENTER(l);

	rc = pthread_mutex_trylock(&SS.GlbSm[sid].lock);

	FQ_TRACE_EXIT(l);
	return(rc);
}

int lock_session(fq_logger_t *l, int sid)
{
	FQ_TRACE_ENTER(l);

	ASSERT(sid > 0);
	pthread_mutex_lock(&SS.GlbSm[sid].lock);

	FQ_TRACE_EXIT(l);
	return(0);
}

int unlock_session(fq_logger_t *l, int sid)
{
	int rc=-1;

	FQ_TRACE_ENTER(l);

	ASSERT(sid > 0);
	rc = pthread_mutex_unlock(&SS.GlbSm[sid].lock);

	FQ_TRACE_EXIT(l);

	return(rc);
}

sess_t* get_session(fq_logger_t *l, int sid)
{
	sess_t *s=NULL;

	FQ_TRACE_ENTER(l);

	ASSERT(sid > 0);
	s= SS.GlbSm[sid].sess;

	FQ_TRACE_EXIT(l);
	return(s);
}

void init_sess( fq_logger_t *l )
{
	int i, rc;

	FQ_TRACE_ENTER(l);

	SS.current_users = 0;

	for (i=0; i <= MAX_SESSION_LIMIT; i++) {
			rc = pthread_mutex_init(&SS.GlbSm[i].lock, NULL);
			ASSERT(rc ==0);
			SS.GlbSm[i].set_time = time(0);
	}

	FQ_TRACE_EXIT(l);

	return;
}

sess_t* sess_new(fq_logger_t *l, char *client_addr, char *qpath, char *qname)
{
	sess_t *s=NULL;

	FQ_TRACE_ENTER(l);

	fq_log(l, FQ_LOG_INFO, "sess_new().");

	s = (sess_t*)malloc(sizeof(sess_t));

	if( !s ) {
		fq_log(l, FQ_LOG_ERROR, "sess_new: Can't allocate memory for new sess, %s.", strerror(errno));
		FQ_TRACE_EXIT(l);
		return( (sess_t*)NULL );
	}

	s->client_addr = (char *)strdup(client_addr);
	s->qpath = (char *)strdup(qpath);
	s->qname = (char *)strdup(qname);

	bzero((void*)s->skey, SKEY_SIZE+1);
	s->tot_req     = 0;
	s->last_access_time  = time(0);

	pthread_cond_init(&s->cond, NULL);

	FQ_TRACE_EXIT(l);
	return s;
}

/*
** sess_free : Free sess_t structure
*/
void sess_free(fq_logger_t *l, sess_t **s_ptr)
{
	FQ_TRACE_ENTER(l);

	if ( ! (*s_ptr) ) {
		FQ_TRACE_EXIT(l);
		return;
	}

	SAFE_FREE( (*s_ptr)->client_addr );
	SAFE_FREE( (*s_ptr)->qpath );
	SAFE_FREE( (*s_ptr)->qname );

	pthread_cond_destroy(&((*s_ptr)->cond));

	SAFE_FREE( *s_ptr );

	(*s_ptr) = NULL;
	FQ_TRACE_EXIT(l);

	return;
}

void sess_remove_unlocked(fq_logger_t *l, int sid, int eventid, int sn, int rc);
void sess_remove(fq_logger_t *l, int sid, int eventid, int sn, int rc);

int sess_findskey(fq_logger_t *l, char* skey)
{
	int i;

	FQ_TRACE_ENTER(l);
	ASSERT(skey);
	for (i=1; i <= MAX_SESSION_LIMIT; i++) {
		if ( SS.GlbSm[i].sess && STRCMP(SS.GlbSm[i].sess->skey, skey) == 0 ) {
			SS.GlbSm[i].sess->last_access_time  = time(0);
			SS.GlbSm[i].set_time = time(0);
			FQ_TRACE_EXIT(l);
			return(i);
		}
	}
	FQ_TRACE_EXIT(l);
	return(-1);
}

void sess_remove_unlocked(fq_logger_t *l, int sid, int eventid, int sn, int rc)
{

	FQ_TRACE_ENTER(l);

	ASSERT( sid > 0 );

	if ( SS.GlbSm[sid].sess ) {
		sess_free(l, &SS.GlbSm[sid].sess);
	}

	pthread_mutex_lock( &lock_nusers );
	SS.current_users--;
	n_users--;
	pthread_mutex_unlock( &lock_nusers );

	FQ_TRACE_EXIT(l);
}

int sess_add_unlocked(fq_logger_t *l, char* skey, char *client_addr, char *qpath, char *qname)
{
	int     rc=-1;
	int  i, new_sid;
	sess_t *s=NULL;
	char    sz_date[16], sz_time[9], sz_month[16];

	FQ_TRACE_ENTER(l);

	memset(sz_date, '\0', sizeof(sz_date));
	memset(sz_time, '\0', sizeof(sz_time));
	memset(sz_month, '\0', sizeof(sz_month));

	new_sid = last_sid + 1;

	for (i=1; i <= MAX_SESSION_LIMIT; i++, new_sid++) {
		if ( new_sid > MAX_SESSION_LIMIT )
			new_sid = 1;    /* We do not use session no 0 */

		if ( !SS.GlbSm[new_sid].sess ) {
			fq_log(l, FQ_LOG_INFO, "session; empty (%d-th)", new_sid);
			pthread_mutex_lock(&SS.GlbSm[new_sid].lock);

			s = sess_new(l, client_addr, qpath, qname);
			if( !s ) {
				fq_log(l, FQ_LOG_ERROR, "sess_new() error.");
				goto L1;
			}

			pthread_mutex_lock( &lock_nusers );
			n_users++;
			pthread_mutex_unlock( &lock_nusers );

			SS.GlbSm[new_sid].sess = s;
			SS.GlbSm[new_sid].set_time = time(0);
			SS.GlbSm[new_sid].sess->last_access_time = time(0);

			pthread_mutex_unlock(&SS.GlbSm[new_sid].lock);
			last_sid = new_sid;
			rc = new_sid;
			fq_log(l, FQ_LOG_DEBUG, "New sesion OK.[%d]\n", rc);

			goto L1;
		} else {
			fq_log(l, FQ_LOG_INFO, "session; reuse check. (%d-th)", i);
			/* 120초 이상된 세션을 잡아쓰기 */
			if( (SS.GlbSm[new_sid].sess->last_access_time+g_session_timeout_sec) < time(0)) {
				fq_log(l, FQ_LOG_INFO, "This is a expired session.");

				sess_free(l, &SS.GlbSm[new_sid].sess);

				pthread_mutex_lock(&SS.GlbSm[new_sid].lock);

				s = sess_new(l, client_addr, qpath, qname);
				if( !s ) {
					fq_log(l, FQ_LOG_ERROR, "sess_new() error.");
					goto L1;
				}

				SS.GlbSm[new_sid].sess = s;
				SS.GlbSm[new_sid].sess->last_access_time = time(0);
				SS.GlbSm[new_sid].set_time = time(0);
				pthread_mutex_unlock(&SS.GlbSm[new_sid].lock);
				last_sid = new_sid;
				rc = new_sid;
				fq_log(l, FQ_LOG_DEBUG, "New sesion OK(re-uesed old session).[%d]\n", rc);
				goto L1;
			}
			else {
				fq_log(l, FQ_LOG_INFO, "This is a available session.");
			}
		}
	}
	fq_log(l, FQ_LOG_ERROR, "%s. Table Scan count=[%d].", "There is no empty room. Too many clients", i);
L1:
	FQ_TRACE_EXIT(l);

	return(rc);
}

int sess_register_unlocked(fq_logger_t *l, char *skey, char *qpath, char *qname)
{
	sess_t *s=NULL;
	int i, new_sid = -1;

	FQ_TRACE_ENTER(l);

	if ( !HASVALUE(skey) ) {
		fq_log(l, FQ_LOG_DEBUG, "There is no skey.");
		goto new_session;
	}

	for (i=1; i <= MAX_SESSION_LIMIT; i++) {
		s = SS.GlbSm[i].sess;
		if ( s && STRCMP(skey, s->skey) == 0 ) {
			new_sid = i;
			SS.GlbSm[new_sid].set_time = time(0);
			SS.GlbSm[new_sid].sess->last_access_time = time(0);
			break;
		}
	}

new_session:
	if ( new_sid < 0 ) {
		SS.current_users++;
		fq_log(l, FQ_LOG_DEBUG, "current_users=[%d],", SS.current_users);
		new_sid = sess_add_unlocked( l, skey, "127.0.0.1", qpath, qname);
		if ( new_sid > 0 ) {
			set_skey(l, SS.GlbSm[new_sid].sess);
		}
		else {
			fq_log(l, FQ_LOG_ERROR, "sess_add_unlocked() error.");
		}
	}
	else {
		SS.GlbSm[new_sid].set_time = time(0);
		SS.GlbSm[new_sid].sess->last_access_time = time(0);
	}

	FQ_TRACE_EXIT(l);

	return(new_sid);
}

int set_skey(fq_logger_t *l, sess_t *s)
{
	struct timeval tp;
	char buf[128], tmpbuf[80];
	int i;

	FQ_TRACE_ENTER(l);

	ASSERT(s);

	gettimeofday(&tp, (void*)NULL);
	sprintf(buf, "%ld%ld", tp.tv_sec, tp.tv_usec);
	for (i=0; i<5; i++) {
			sprintf(tmpbuf, "%d", rand());
			strcat(buf, tmpbuf);
	}
	strncpy(s->skey, buf, SKEY_SIZE);
	s->skey[SKEY_SIZE] = '\0';

	FQ_TRACE_EXIT(l);

	return(0);
}

pthread_mutex_t s_lock  = PTHREAD_MUTEX_INITIALIZER;

void lock_sesstable()
{
	pthread_mutex_lock(&s_lock);
}

void unlock_sesstable()
{
	pthread_mutex_unlock(&s_lock);
}

int sess_register(fq_logger_t *l, char* skey, char *qpath,  char *qname)
{
	int rc = -1;

	FQ_TRACE_ENTER(l);

	lock_sesstable();
	rc = sess_register_unlocked(l, skey, qpath, qname);
	if( rc < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "sess_register_unlocked() error.");
	}
	unlock_sesstable();

	FQ_TRACE_EXIT(l);
	return(rc);
}

void sess_remove(fq_logger_t *l, int sid, int eventid, int sn, int rc)
{
	FQ_TRACE_ENTER(l);

	lock_sesstable();

	if ( get_session(l, sid) ) {
			sess_remove_unlocked(l, sid, eventid, sn, rc);
	}
	unlock_session(l, sid);

	unlock_sesstable();

	FQ_TRACE_EXIT(l);
}

void sig_exit_handler( int signal )
{
    printf("sig_pipe_handler() \n");
	exit(EXIT_FAILURE);
}

/*
 * This function makes fq_obj
 */
static int 
LINK_processing( fq_logger_t *l, FQ_protocol_t *fqp, int sid ) 
{ 
	int		rc;
	int 	found_flag = 0;
	int		empty_index = -1;
	fqueue_obj_t	*obj=NULL;
	
	FQ_TRACE_ENTER(l);

	obj = find_a_qobj( l, fqp->path, fqp->qname, ll_qobj);
	if( obj ) { 
		FQ_TRACE_EXIT(l);
		return(TRUE);
	}
	else {
		FQ_TRACE_EXIT(l);
		return(FALSE);
	}
}

#if 0
static int
UNLINK_processing( fq_logger_t *l, int sid )
{
    int     rc;
   
	FQ_TRACE_ENTER(l);

    rc = CloseFileQueue(l, &SS.GlbSm[sid].sess->obj);

    if(rc != TRUE)  { // false
        fq_log(l, FQ_LOG_ERROR, "CloseFileQueue() error sid=[%d].", sid);
		FQ_TRACE_EXIT(l);
        return(-1);
    }
	FQ_TRACE_EXIT(l);
    return(rc);
}
#endif

static int 
ENQU_processing( fq_logger_t *l,  int sid, unsigned char *data, int datalen) 
{ 
	int		rc=-1;
	long    seq;
	long 	run_time=0L;
	int	fq_obj_index = -1;
	fqueue_obj_t	*obj=NULL;

	FQ_TRACE_ENTER(l);


	obj = find_a_qobj(l, SS.GlbSm[sid].sess->qpath, SS.GlbSm[sid].sess->qname, ll_qobj);
	if( obj == NULL ) {
		FQ_TRACE_EXIT(l);
		return -1;
	}

	fq_log(l, FQ_LOG_DEBUG, "We will put msg to [%s]/[%s].", 
		obj->h_obj->h->path, obj->h_obj->h->qname);

	if( data[0] == 0x00) {
		fq_log(l, FQ_LOG_ERROR, "Received Data is null. We do not put it. and throw it.");
		FQ_TRACE_EXIT(l);
		return rc;
	}
	else {
		while(1) {
			rc =  obj->on_en(l, obj, EN_NORMAL_MODE, data, datalen+1, datalen, &seq, &run_time );
			if( rc < 0 ) {
				fq_log(l, FQ_LOG_ERROR,  "ENQU error: on_en() error. [%s][%s]", 
					obj->h_obj->h->path, obj->h_obj->h->qname);
				FQ_TRACE_EXIT(l);
				return rc;
			}
			else if( rc == EQ_FULL_RETURN_VALUE  ) {
				fq_log(l, FQ_LOG_EMERG, "ENQU :  queue is full. I'll retry to put to the queue.[%s][%s]", obj->h_obj->h->path, obj->h_obj->h->qname);
				FQ_TRACE_EXIT(l);
				return(EQ_FULL_RETURN_VALUE);
				// usleep(100000);
				// continue;
			}
			else {
				fq_log(l, FQ_LOG_DEBUG,  "ENQU OK: Data is saved to the queue successfully. seq=[%ld]\n", seq);
				break;
			}
		}
		rc = datalen;
	}
	FQ_TRACE_EXIT(l);
	return(rc);	
}

static int 
DEQU_processing( fq_logger_t *l, int sid, unsigned char **data) 
{ 
	int		len;
	size_t	buffer_size;
	long	seq=0L;
	long 	run_time=0L;
	fqueue_obj_t *obj=NULL;
	

	FQ_TRACE_ENTER(l);

	fq_log(l, FQ_LOG_DEBUG, "qpath=[%s], qname=[%s].", 
			SS.GlbSm[sid].sess->qpath, SS.GlbSm[sid].sess->qname);


	obj = find_a_qobj(l, SS.GlbSm[sid].sess->qpath, SS.GlbSm[sid].sess->qname, ll_qobj);
	if( obj == NULL ) {
		FQ_TRACE_EXIT(l);
		fq_log(l, FQ_LOG_ERROR, "Failed to get fqueue object.");
		return -1;
	}

	// buffer_size = SS.GlbSm[sid].sess->obj->h_obj->h->msglen - SOCK_CLIENT_HEADER_SIZE + 1;
	buffer_size = 65536;

	*data = malloc(buffer_size);
	memset(*data, 0x00, buffer_size);

	len=obj->on_de(l, obj, *data, buffer_size, &seq, &run_time);
	if( len < 0 ) {
		if( len == DQ_FATAL_SKIP_RETURN_VALUE ) { /* -88 */
			printf("This msg was crashed. I'll continue.\n");
		}
		else if( len == MANAGER_STOP_RETURN_VALUE ) { /* -99 */
			printf("Manager asked to stop a processing.\n");
		}
		fq_log(l, FQ_LOG_ERROR, "DEQU error: on_de() error.");
		SAFE_FREE(*data);
		goto L0;
	}
	else if( len == DQ_EMPTY_RETURN_VALUE ) {
		fq_log(l, FQ_LOG_INFO, "Queue is empty.");
		SAFE_FREE(*data);
		goto L0;
	}
	else {
		fq_log(l, FQ_LOG_DEBUG, "deQ OK len=[%d] seq=[%ld]", len, seq);
	}
L0:
	FQ_TRACE_EXIT(l);
	return(len);	
}
static int 
HCHK_processing( fq_logger_t *l, int sid) 
{ 
	int		len;
	size_t	buffer_size;
	long	seq=0L;
	long 	run_time=0L;
	fqueue_obj_t *obj=NULL;
	
	FQ_TRACE_ENTER(l);

	fq_log(l, FQ_LOG_DEBUG, "health checking OK.");
L0:
	FQ_TRACE_EXIT(l);
	return(1);	
}

int json_request_parsing( fq_logger_t *l, JSON_Value *jsonValue, JSON_Object *jsonObject, FQ_protocol_t *fqp, unsigned char **ptr_reqmsg, int *msglen, int *req )
{
	FQ_TRACE_ENTER(l);

	sprintf(fqp->version, "%s", json_object_get_string(jsonObject, "FQP_VERSION"));
	sprintf(fqp->skey, "%s", json_object_get_string(jsonObject, "SESSION_ID"));
	sprintf(fqp->path, "%s", json_object_get_string(jsonObject, "QUEUE_PATH"));
	sprintf(fqp->qname, "%s", json_object_get_string(jsonObject, "QUEUE_NAME"));

	char ack_mode[2];
	sprintf(ack_mode, "%s", json_object_get_string(jsonObject, "ACK_MODE"));

	fqp->ack_mode = ack_mode[0];
	sprintf(fqp->action_code, "%s", json_object_get_string(jsonObject, "ACTION"));

	int message_len = json_object_get_number(jsonObject, "MSG_LENGTH");

	char *msg = NULL;
	if( message_len > 0 ) {
		msg = calloc(message_len+1, sizeof(unsigned char));
		sprintf(msg, "%s", json_object_get_string(jsonObject, "MESSAGE"));
	}
	else if( strcmp(fqp->action_code, "ENQU") == 0 ) {
		SAFE_FREE(msg);
		fq_log(l, FQ_LOG_ERROR, "illegal request message.[%s], in ENQU, MSG_LENGTH is 0", fqp->action_code);
		FQ_TRACE_EXIT(l);
		return(-1);
	}

	fq_log(l, FQ_LOG_DEBUG, "\t- fqpVersion=[%s]", fqp->version);
	fq_log(l, FQ_LOG_DEBUG, "\t- sessionID=[%s]", fqp->skey);
	fq_log(l, FQ_LOG_DEBUG, "\t- qPath=[%s]", fqp->path);
	fq_log(l, FQ_LOG_DEBUG, "\t- qName=[%s]", fqp->qname);
	fq_log(l, FQ_LOG_DEBUG, "\t- ackMode=[%c]", fqp->ack_mode);
	fq_log(l, FQ_LOG_DEBUG, "\t- action=[%s]", fqp->action_code);
	fq_log(l, FQ_LOG_DEBUG, "\t- message_len=[%d]", message_len);
	if( msg ) {
		fq_log(l, FQ_LOG_DEBUG, "\t- message=[%s]", msg);
	}

	if(strncmp(fqp->action_code, FQP_LINK_MSG, FQP_ACTION_CODE_LEN) == 0) { // link
		*req = FQP_LINK_REQ;
	} else if( strncmp(fqp->action_code, FQP_ENQU_MSG, FQP_ACTION_CODE_LEN) == 0) { // enQ 
		*req = FQP_ENQU_REQ;
	} else if( strncmp(fqp->action_code, FQP_DEQU_MSG, FQP_ACTION_CODE_LEN) == 0) { // deQ
		*req = FQP_DEQU_REQ;
	} else if( strncmp(fqp->action_code, FQP_EXIT_MSG, FQP_ACTION_CODE_LEN) == 0) {
		*req = FQP_EXIT_REQ;
	} else if( strncmp(fqp->action_code, FQP_HCHK_MSG, FQP_ACTION_CODE_LEN) == 0) { // health check
		*req = FQP_HCHK_REQ;
	} else {
		SAFE_FREE(msg);
		fq_log(l, FQ_LOG_ERROR, "illegal request message.[%s]", fqp->action_code);
		FQ_TRACE_EXIT(l);
		return(-1);
	}
	*msglen = message_len;
	if( msg ) {
		*ptr_reqmsg = strdup(msg);
	}

	SAFE_FREE(msg);

	FQ_TRACE_EXIT(l);
	return(0);
}

#if 0
#define CHECK_RUN_TIME 1
#endif

int CB_function( unsigned char *data, int len, unsigned char *resp, int *resp_len)
{
	int	req;
	unsigned char header[FQ_PROTOCOL_LEN+1];
	FQ_protocol_t fqp;
	int		sid=-1; /* session ID */
	int DEQU_rc = 0;
	unsigned char *resp_msg_buffer=NULL; /* free for deQ */
	unsigned char *req_msg=NULL; /* for enQ */
	int			   req_msglen; /* for enQ */
	int		rc = -1;

#ifdef CHECK_RUN_TIME
	stopwatch_micro_t p;
	long sec=0L, usec=0L;

	on_stopwatch_micro(&p);
#endif

	fq_log(l, FQ_LOG_DEBUG, "CB called.");

	/*
	** Warning : Variable data has value of header.so If you print data to sceeen, you can't see real data.
	** so you have to add 4bytes(header length) for saving and printing.
	*/
	fq_log(l, FQ_LOG_DEBUG, "len=[%d] JSON=[%s]\n", len, data+4);

	char *json_request_msg = calloc( len-4+1, sizeof(unsigned char));
	json_request_msg = strdup( data+4 );

	JSON_Value *jsonValue = json_parse_string(json_request_msg);
	if( !jsonValue ) {
		fq_log(l, FQ_LOG_ERROR, "illegal json format. We throw it.\n");
		error_code = 1; // json format error
		goto L0;
	}
	JSON_Object *jsonObject = json_value_get_object(jsonValue);
	if( !jsonObject ) {
		fq_log(l, FQ_LOG_ERROR, "illegal json format. We throw it.\n");
		error_code = 1; // json format error
		goto L0;
	}
	debug_json(l, jsonValue);

	/*
	** json request parsing
	*/
	rc = json_request_parsing( l, jsonValue, jsonObject, &fqp, &req_msg, &req_msglen, &req );
	if( rc < 0 ) {
		error_code = 2; // json fq protocol error
		fq_log(l, FQ_LOG_ERROR, "request_parsing() error.");
		goto L0;
	}

	/*
	** job processing
	*/
	JSON_Value *r_jsonValue = json_value_init_object();
	JSON_Object *r_jsonObject = json_value_get_object(r_jsonValue);
	char *json_buf=NULL;
	size_t json_buf_size;

	switch(req) {
		int rc;

		case FQP_LINK_REQ:
			rc = sess_register(l, NULL, fqp.path, fqp.qname);
			fq_log(l, FQ_LOG_DEBUG, "LINK (queue file open) sess_register() rc=[%d]", rc);

			if( rc < 0 ) {
				fq_log(l, FQ_LOG_ERROR, "sess_register() error.");
				error_code = 1000; // system error, session regist error
				goto L0;
			}

			sid = rc;
			rc = LINK_processing(l, &fqp, sid);
			if( rc < 0 ) {
				fq_log(l, FQ_LOG_ERROR, "LINK_processing() error.");
				error_code = 3; // link error
				goto L0;
			}
			handler_eventlog(l, g_eventlog_filename, "LINK", error_code, rc);
			fq_log(l, FQ_LOG_DEBUG, "LINK(queue open) processing OK.");
			break;

		case FQP_ENQU_REQ:
			fq_log(l, FQ_LOG_DEBUG, "ENQU request.");
			sid = sess_findskey(l, fqp.skey);
			if(sid < 0 ) {
				fq_log(l, FQ_LOG_ERROR, "sess_findsky() error.");
				error_code = 4; // session not found error
				goto L0;
			}
			fq_log(l, FQ_LOG_DEBUG, "ENQU (on_en)");
			
			/* It increase memory using size, but it is not bug (mmap). */
			rc = ENQU_processing(l, sid, req_msg, req_msglen);
			if ( rc < 0 ) {
				fq_log(l, FQ_LOG_ERROR, "ENQU_processing() error.");
				error_code = 1001; // enqueue error
				goto L0;
			}
			else if( rc == EQ_FULL_RETURN_VALUE ) {
				fq_log(l, FQ_LOG_ERROR, "ENQU_processing() is full.");
				error_code = 1002; // session full
				goto L1;
			}
			else { // enQ success.
				handler_eventlog(l, g_eventlog_filename, "ENQU", error_code, rc);
			}
			break;

		case FQP_DEQU_REQ:
			fq_log(l, FQ_LOG_DEBUG, "ENQU request.");
			sid = sess_findskey(l, fqp.skey);
			if(sid < 0 ) {
				fq_log(l, FQ_LOG_ERROR, "sess_findsky() error.");
				error_code = 4; // session not found error
				goto L0;
			}
			fq_log(l, FQ_LOG_DEBUG, "DEQU (on_de)");
			DEQU_rc = DEQU_processing(l, sid, &resp_msg_buffer);
			if ( DEQU_rc < 0 ) {
				fq_log(l, FQ_LOG_ERROR, "DEQU_processing() error.");
				error_code = 1002; // dequeue error
				goto L0;
			}
			else if( DEQU_rc == DQ_EMPTY_RETURN_VALUE ) {
				fq_log(l, FQ_LOG_INFO, "DEQU_processing() is empty.");
				goto L2;
			}
			else {
				handler_eventlog(l, g_eventlog_filename, "DEQU", error_code, DEQU_rc);
				fq_log(l, FQ_LOG_INFO, "deQ success. rc=[%d]", DEQU_rc);
			}
			break;
		case FQP_HCHK_REQ:
			fq_log(l, FQ_LOG_DEBUG, "HCHK request.(health check->session extension.");
			sid = sess_findskey(l, fqp.skey);
			if(sid < 0 ) {
				fq_log(l, FQ_LOG_ERROR, "sess_findsky() error.");
				error_code = 4; // session not found error
				goto L0;
			}
			fq_log(l, FQ_LOG_DEBUG, "HCHK (health check) end.");
			rc = HCHK_processing(l, sid);
			if( rc != 1 ) { // is not normal : error
				fq_log(l, FQ_LOG_ERROR, "HCHK_processing() error.");
                error_code = 1003; // health checking error
                goto L0;
			}
			handler_eventlog(l, g_eventlog_filename, "HCHK", error_code, rc);
			break;
		case FQP_EXIT_REQ:
			fq_log(l, FQ_LOG_DEBUG, "EXIT (on_de)");
			goto L0;
		default:
			fq_log(l, FQ_LOG_ERROR, "undefined action code.");
			error_code = 5;
			fq_log(l, FQ_LOG_ERROR, "illegal requesting.");
			goto L0;
	}


	// success return
	int response_len;

	make_success_response_msg( l, req, DEQU_rc, sid, r_jsonValue, r_jsonObject, resp, resp_msg_buffer, &response_len);
	*resp_len = response_len;

	error_code = 0; // success


	// fq_log(l, FQ_LOG_INFO, "response =[%s] resp_len=[%d]", resp, *resp_len);

	if( r_jsonValue) json_value_free(r_jsonValue);

	SAFE_FREE( req_msg );
	SAFE_FREE( resp_msg_buffer );
	SAFE_FREE( json_request_msg );
#ifdef CHECK_RUN_TIME
	off_stopwatch_micro(&p);
	get_stopwatch_micro(&p, &sec, &usec);
	fq_log(l, FQ_LOG_DEBUG, "CB_function delay time: %ldsec:%ldusec", sec, usec);
#endif
	if( jsonValue ) json_value_free(jsonValue);
	return(*resp_len);

L0: /* error return */
	make_fail_response_msg(l, error_code , SS.GlbSm[sid].sess->skey, r_jsonValue, r_jsonObject, resp, &response_len );
	*resp_len = response_len;

	SAFE_FREE( resp_msg_buffer );
	SAFE_FREE( req_msg );
	SAFE_FREE( json_request_msg );

#ifdef CHECK_RUN_TIME
	off_stopwatch_micro(&p);
	get_stopwatch_micro(&p, &sec, &usec);
	fq_log(l, FQ_LOG_DEBUG, "CB_function delay time: %ldsec:%ldusec", sec, usec);
#endif

	if( jsonValue ) json_value_free(jsonValue);
	if( r_jsonValue) json_value_free(r_jsonValue);

	return(*resp_len);

L1: /* full return */
	make_full_response_msg(l, error_code , SS.GlbSm[sid].sess->skey, r_jsonValue, r_jsonObject, resp, &response_len );
	*resp_len = response_len;

	SAFE_FREE( resp_msg_buffer );
	SAFE_FREE( req_msg );
	SAFE_FREE( json_request_msg );

#ifdef CHECK_RUN_TIME
	off_stopwatch_micro(&p);
	get_stopwatch_micro(&p, &sec, &usec);
	fq_log(l, FQ_LOG_DEBUG, "CB_function delay time: %ldsec:%ldusec", sec, usec);
#endif
	if( r_jsonValue) json_value_free(r_jsonValue);
	if( jsonValue ) json_value_free(jsonValue);

	handler_eventlog(l, g_eventlog_filename, "ENQU", error_code, response_len);
	SAFE_FREE( json_request_msg );
	return(*resp_len);

L2: /* empty return */
	make_empty_response_msg(l, error_code , SS.GlbSm[sid].sess->skey, r_jsonValue, r_jsonObject, resp, &response_len );
	*resp_len = response_len;

	SAFE_FREE( resp_msg_buffer );
	SAFE_FREE( req_msg );
	SAFE_FREE( json_request_msg );


#ifdef CHECK_RUN_TIME
	off_stopwatch_micro(&p);
	get_stopwatch_micro(&p, &sec, &usec);
	fq_log(l, FQ_LOG_DEBUG, "CB_function delay time: %ldsec:%ldusec", sec, usec);
#endif
	if( r_jsonValue) json_value_free(r_jsonValue);
	if( jsonValue ) json_value_free(jsonValue);

	SAFE_FREE( json_request_msg );
	return(*resp_len);
}

void
restart(void)
{
	sleep(3);
	fq_log(l, FQ_LOG_INFO, "server will restart.");

	if( strlen(restart_cmd) > 5) {
		fq_log(l, FQ_LOG_ERROR, "Process is received a EXIT. I send a message requisting re-start to svc_auto_forker.");
		request_restart_me(l, "9999", (unsigned char *)restart_cmd);
	}
	else {
		fq_log(l, FQ_LOG_ERROR, "Good bye!!! forever!\n");
	}

	exit(0);
}

typedef struct _admin_phone_map {
	char dir_name[256];
	unsigned long long size ;
} check_filesystem_t;


static bool MakeLinkedList_check_filesystem_mapfile(char *mapfile, char delimiter, linkedlist_t	*ll)
{
	file_list_obj_t *filelist_obj=NULL;
	FILELIST *this_entry=NULL;
	int rc;
	int t_no;

	printf("mapfile='%s'\n" , mapfile);

	rc = open_file_list(0, &filelist_obj, mapfile);
	CHECK( rc == TRUE );

	printf("count = [%d].\n", filelist_obj->count);

	// filelist_obj->on_print(filelist_obj);

	this_entry = filelist_obj->head;
	for( t_no=0;  (this_entry->next && this_entry->value); t_no++ ) {
		char *errmsg = NULL;
		printf("this_entry->value=[%s]\n", this_entry->value);

		char	dir_name[256]; 
		char	str_size[32]; 

		int cnt;

		cnt = fq_sscanf(delimiter, this_entry->value, "%s%s", dir_name, str_size);

		CHECK(cnt == 2);

		fprintf(stdout, "dir_name='%s', size='%s'.", dir_name, str_size);

		check_filesystem_t *tmp = NULL;
		tmp = (check_filesystem_t *)calloc(1, sizeof(check_filesystem_t));

		char	key[11];
		memset(key, 0x00, sizeof(key));
		sprintf(key, "%010d", t_no);

		sprintf(tmp->dir_name, "%s", dir_name);
        tmp->size = atoll(str_size);

		ll_node_t *ll_node=NULL;
		ll_node = linkedlist_put(ll, key, (void *)tmp, sizeof(char)+sizeof(check_filesystem_t));
		CHECK(ll_node);

		this_entry = this_entry->next;
	}

	if( filelist_obj ) close_file_list(l, &filelist_obj);
	
	fprintf( stdout, "ll->length is [%d]\n", ll->length);

	return true;
}

bool CheckFileSystem( fq_handler_conf_t *my_conf, linkedlist_t *check_filesystem_ll)
{
    bool tf;
    ll_node_t *node_p = NULL;
    int t_no;

    for( node_p=check_filesystem_ll->head, t_no=0; (node_p != NULL); (node_p=node_p->next    , t_no++) ) {
        check_filesystem_t *tmp;
        tmp = (check_filesystem_t *) node_p->value;

        printf("dir_name='%s', size='%lld'", tmp->dir_name, tmp->size);
        tf =  checkFileSystemSpace(NULL, tmp->dir_name, tmp->size);
        CHECK(tf);
    }
    return true;
}
int main(int ac, char **av)
{
	TCP_server_t x;

	/* server environment */
	int	ack_mode = 1; /* ACK 모드: FQ_Protocol_svr 는 항상 1로 설정한다. */

	int	rc = -1, i;
	int	ch;
	char	*errmsg=NULL;

	printf("Compiled on %s %s source program version=[%s]\n\n\n", __TIME__, __DATE__, FQ_PROTOCOL_SVR_JSON__C_VERSION);
	fq_handler_conf_t   *my_conf=NULL;

	if( ac != 2 ) {
		fprintf(stderr, "Usage: $ %s [your config file] <enter>\n", av[0]);
		return 0;
	}

	if(LoadConf(av[1], &my_conf, &errmsg) == false) {
		fprintf(stderr, "LoadMyConf('%s') error. reason='%s'\n", av[1], errmsg);
		return(-1);
	}

    linkedlist_t *check_filesystem_ll = NULL;
    bool tf;

    check_filesystem_ll = linkedlist_new("check_filesytem");
    tf = MakeLinkedList_check_filesystem_mapfile(my_conf->use_filesystem_list_file, '|', check_filesystem_ll);
    CHECK(tf);

    tf = CheckFileSystem(my_conf, check_filesystem_ll);
    CHECK(tf);
    printf("CheckFileSytem() OK\n");

	rc = fq_open_file_logger(&l, my_conf->log_filename, my_conf->i_log_level);
	CHECK(rc==TRUE);
	fprintf(stdout, "log file: '%s'\n", my_conf->log_filename);

	if( FQ_IS_ERROR_LEVEL(l) ) {
		fclose(stdout);
	}

	fq_log(l, FQ_LOG_INFO, "fq_handler my conf loading ok.");

	/*
	** make background daemon process.
	*/
	// daemon_init();

	signal(SIGPIPE, SIG_IGN);

	/************************************************************************************************/

	if( my_conf->bin_dir ) {
		/**********************************************************************************************
		** 이하에서 발생하는 오류 또는 signal 에 의해 프로그램이 종료되면 자동으로 재구동 되도록 셋팅한다.
		*/
		memset(restart_cmd, 0x00, sizeof(restart_cmd));
		sprintf(restart_cmd, "%s/%s %s", my_conf->bin_dir, av[0], av[1]);
		i = atexit(restart);
		if (i != 0) {
			fq_log(l, FQ_LOG_ERROR, "cannot set exit function.");
			exit(EXIT_FAILURE);
		}
		signal(SIGPIPE, sig_exit_handler); /* retry 시에는 반드시 헨들러를 재 구동하여야 한다 */
		signal(SIGTERM, sig_exit_handler); /* retry 시에는 반드시 헨들러를 재 구동하여야 한다 */
		signal(SIGQUIT, sig_exit_handler); /* retry 시에는 반드시 헨들러를 재 구동하여야 한다 */
		/************************************************************************************************/
	}


	g_session_timeout_sec = my_conf->session_timeout_sec;

	tf = Make_all_fq_objects_linkedlist(l, &ll_qobj);
	CHECK(tf);

	pthread_t thread_id;
	thread_param_t tp;

	tp.l = l;

	if( pthread_create( &thread_id, NULL, &session_view_thread, &tp) ) {
		fq_log(l, FQ_LOG_ERROR, "pthread_create() error.");
	}

	g_eventlog_filename = strdup(my_conf->eventlog_filename);

	make_thread_sync_CB_server( l, &x, my_conf->server_ip, my_conf->i_server_port, BINARY_HEADER_TYPE, 4, ack_mode, CB_function);

	fq_log(l, FQ_LOG_EMERG, "fq_handler server (CLANG Co.version %s) start!!!.(listening ip=[%s] port=[%s]", 
		FQ_PROTOCOL_SVR_JSON__C_VERSION, my_conf->server_ip, my_conf->server_port);

	pause();

    fq_close_file_logger(&l);
	exit(EXIT_FAILURE);
}

void *session_view_thread( void *arg )
{
	thread_param_t *tp = arg;

	fq_logger_t *l = tp->l;

	while(1) {
		int i;
		for (i=1; i <= MAX_SESSION_LIMIT; i++) {
			if ( SS.GlbSm[i].sess ) {
				fq_log(l, FQ_LOG_DEBUG, "[%d]-th : live session.", i);
				// SS.GlbSm[i].sess->last_access_time  = time(0);
				// SS.GlbSm[i].set_time = time(0);
				// FQ_TRACE_EXIT(l);
				// return(i);

				char *ptime;
				ptime = asc_time(SS.GlbSm[i].sess->last_access_time);
				fq_log(l, FQ_LOG_DEBUG, "last_access_time=[%s], bin_time=[%ld]\n",
					ptime, SS.GlbSm[i].sess->last_access_time);
				
				SAFE_FREE(ptime);

				if( (SS.GlbSm[i].sess->last_access_time+ g_session_timeout_sec) < time(0)) {
					fq_log(l, FQ_LOG_EMERG, "This is a expired session.");


					pthread_mutex_lock(&SS.GlbSm[i].lock);

					sess_free(l, &SS.GlbSm[i].sess);

					pthread_mutex_unlock(&SS.GlbSm[i].lock);

					pthread_mutex_lock( &lock_nusers );
					n_users--;
					SS.current_users--;
					pthread_mutex_unlock( &lock_nusers );

					fq_log(l, FQ_LOG_EMERG, "session free OK, n_users=[%d].", n_users);
				}
				else {
					fq_log(l, FQ_LOG_INFO, "This is a session which using really, n_users=[%d].", n_users);
				}
			}
			else {
				// fq_log(l, FQ_LOG_DEBUG, "[%d]-th : empty session.", i);
			}
		}
		// fq_log(l, FQ_LOG_DEBUG, "now, We are seeing a session table.\n");
		sleep(1);
	}
}

void make_success_response_msg( fq_logger_t *l, int req, int DEQU_rc, int sid, JSON_Value *r_jsonValue, JSON_Object *r_jsonObject, unsigned char *resp, char *msg, int *resp_len)
{
	FQ_TRACE_ENTER(l);

	json_object_set_string(r_jsonObject, "RESULT", "OK");
	json_object_set_string(r_jsonObject, "SESSION_ID", SS.GlbSm[sid].sess->skey);
	switch(req) {
		case FQP_LINK_REQ:
		case FQP_ENQU_REQ:
			break;
		case FQP_DEQU_REQ:
            json_object_set_string(r_jsonObject, "MESSAGE", msg);
            json_object_set_number(r_jsonObject, "MESSAGE_LEN", DEQU_rc);
			break;
		case FQP_HCHK_REQ:
			break;
		default:
			break;
	}
	int json_buf_size;
	char *json_buf = NULL;

	json_buf_size = json_serialization_size(r_jsonValue) + 1;
	json_buf = calloc(sizeof(char), json_buf_size);
	json_serialize_to_buffer(r_jsonValue, json_buf, json_buf_size);

	debug_json(l, r_jsonValue);

	memcpy(resp, json_buf, strlen(json_buf));
	*resp_len = strlen(json_buf);	
	SAFE_FREE(json_buf);

	return;
}
void	make_fail_response_msg(fq_logger_t *l, int error_code , char *skey, JSON_Value *r_jsonValue, JSON_Object *r_jsonObject, unsigned char *resp, int *resp_len )
{
	json_object_set_string(r_jsonObject, "RESULT", "FAIL");
	json_object_set_string(r_jsonObject, "SESSION_ID", skey);
	json_object_set_number(r_jsonObject, "ERROR_CODE", error_code);

	int json_buf_size;
	char *json_buf = NULL;
	json_buf_size = json_serialization_size(r_jsonValue) + 1;
	json_buf = calloc(sizeof(char), json_buf_size);
	json_serialize_to_buffer(r_jsonValue, json_buf, json_buf_size);

	debug_json(l, r_jsonValue);

	memcpy(resp, json_buf, strlen(json_buf));
	*resp_len = strlen(json_buf);	

	return;
}
void	make_full_response_msg(fq_logger_t *l, int error_code , char *skey, JSON_Value *r_jsonValue, JSON_Object *r_jsonObject, unsigned char *resp, int *resp_len )
{
	json_object_set_string(r_jsonObject, "RESULT", "FULL");
	json_object_set_string(r_jsonObject, "SESSION_ID", skey);
	json_object_set_number(r_jsonObject, "ERROR_CODE", error_code);

	int json_buf_size;
	char *json_buf = NULL;
	json_buf_size = json_serialization_size(r_jsonValue) + 1;
	json_buf = calloc(sizeof(char), json_buf_size);
	json_serialize_to_buffer(r_jsonValue, json_buf, json_buf_size);

	debug_json(l, r_jsonValue);

	memcpy(resp, json_buf, strlen(json_buf));
	*resp_len = strlen(json_buf);	

	return;
}
void	make_empty_response_msg(fq_logger_t *l, int error_code , char *skey, JSON_Value *r_jsonValue, JSON_Object *r_jsonObject, unsigned char *resp, int *resp_len )
{
	json_object_set_string(r_jsonObject, "RESULT", "EMPTY");
	json_object_set_string(r_jsonObject, "SESSION_ID", skey);
	json_object_set_number(r_jsonObject, "ERROR_CODE", error_code);

	int json_buf_size;
	char *json_buf = NULL;
	json_buf_size = json_serialization_size(r_jsonValue) + 1;
	json_buf = calloc(sizeof(char), json_buf_size);
	json_serialize_to_buffer(r_jsonValue, json_buf, json_buf_size);

	debug_json(l, r_jsonValue);

	memcpy(resp, json_buf, strlen(json_buf));
	*resp_len = strlen(json_buf);	

	return;
}

static int handler_eventlog(fq_logger_t *l, char *eventlog_filename, ...)
{
	char fn[256];
	char datestr[40], timestr[40];
	FILE* fp=NULL;
	va_list ap;

	get_time(datestr, timestr);
	snprintf(fn, sizeof(fn), "%s.%s", eventlog_filename, datestr);

	fp = fopen(fn, "a");
	if(!fp) {
		fq_log(l, FQ_LOG_ERROR, "failed to open eventlog file. [%s]", fn);
		goto return_FALSE;
	}

	va_start(ap, eventlog_filename);
	fprintf(fp, "%s|%s|%s|", "fq_handler", datestr, timestr);

	vfprintf(fp, "%s|%d|%d|\n", ap); 

	if(fp) fclose(fp);
	va_end(ap);

	return(TRUE);

return_FALSE:
	if(fp) fclose(fp);
	return(FALSE);
}
