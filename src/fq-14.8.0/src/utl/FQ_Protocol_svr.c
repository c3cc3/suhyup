/*
** FQ_Protocol_svr.c
*/
#define FQ_PROTOCOL_SVR_C_VERSION	"1.0.1"

#include <stdio.h>
#include <getopt.h>
#include "fq_param.h"
#include "fqueue.h"
#include "fq_socket.h"
#include "fq_logger.h"
#include "fq_common.h"
#include "fq_monitor.h"
#include "fq_protocol.h"
#include "fq_config.h"

/*
** Session Control
*/
#define SESSION_TIMEOUT_SEC 	600 /* 최종사용시간이 600초가 넘은 세션은 재사용의 대상이 된다 */
#define MAX_SESSION_LIMIT       250 /* concurent users, If you decide this value according to available memory size. */
#define SKEY_SIZE				32

typedef struct {
	char    skey[SKEY_SIZE+1];
	long	tot_req;
	char*   client_addr;
	time_t  last_access_time;
	fqueue_obj_t	*obj;
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

sess_t* sess_new(fq_logger_t *l, char *client_addr)
{
	sess_t *s=NULL;

	FQ_TRACE_ENTER(l);
	s = (sess_t*)malloc(sizeof(sess_t));

	if( !s ) {
		fq_log(l, FQ_LOG_ERROR, "sess_new: Can't allocate memory for new sess, %s.", strerror(errno));
		FQ_TRACE_EXIT(l);
		return( (sess_t*)NULL );
	}

	s->client_addr = (char *)strdup(client_addr);

	bzero((void*)s->skey, SKEY_SIZE+1);
	s->tot_req     = 0;
	s->last_access_time  = time(0);
	s->obj = NULL;

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

	pthread_cond_destroy(&((*s_ptr)->cond));

	SAFE_FREE( *s_ptr );

	(*s_ptr) = NULL;
	FQ_TRACE_EXIT(l);

	return;
}

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

int sess_add_unlocked(fq_logger_t *l, char* skey, char *client_addr)
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
				pthread_mutex_lock(&SS.GlbSm[new_sid].lock);

				s = sess_new(l, client_addr);
				if( !s ) {
						fq_log(l, FQ_LOG_ERROR, "sess_new() error.");
						goto L1;
				}

				pthread_mutex_lock( &lock_nusers );
				n_users++;
				pthread_mutex_unlock( &lock_nusers );

				SS.GlbSm[new_sid].sess = s;
				SS.GlbSm[new_sid].set_time = time(0);

				pthread_mutex_unlock(&SS.GlbSm[new_sid].lock);
				last_sid = new_sid;
				rc = new_sid;
				fq_log(l, FQ_LOG_DEBUG, "New sesion OK.[%d]\n", rc);

				goto L1;
		} else {
				fq_log(l, FQ_LOG_DEBUG, "120초 이상된 세션을 잡아쓰기(%d-th)", i);
				/* 120초 이상된 세션을 잡아쓰기 */
				if( SS.GlbSm[new_sid].sess->last_access_time+SESSION_TIMEOUT_SEC < time(0)) {

						if( SS.GlbSm[i].sess->obj ) {
							CloseFileQueue(l, &SS.GlbSm[i].sess->obj);
						}

						sess_free(l, &SS.GlbSm[new_sid].sess);

						pthread_mutex_lock(&SS.GlbSm[new_sid].lock);

						s = sess_new(l, client_addr);
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
		}
	}
	fq_log(l, FQ_LOG_ERROR, "%s. Table Scan count=[%d].", "Too many clients", i);
L1:
	FQ_TRACE_EXIT(l);

	return(rc);
}

int sess_register_unlocked(fq_logger_t *l, char *skey)
{
	sess_t *s=NULL;
	int i, new_sid = -1;

	FQ_TRACE_ENTER(l);

	if ( !HASVALUE(skey) )
			goto new_session;

	for (i=1; i <= MAX_SESSION_LIMIT; i++) {
		s = SS.GlbSm[i].sess;
		if ( s && STRCMP(skey, s->skey) == 0 ) {
			new_sid = i;
			SS.GlbSm[new_sid].set_time = time(0);
			break;
		}
	}

new_session:
	if ( new_sid < 0 ) {
		SS.current_users++;
		new_sid = sess_add_unlocked( l, skey, "127.0.0.1");
		if ( new_sid > 0 ) {
			set_skey(l, SS.GlbSm[new_sid].sess);
		}
		else {
			fq_log(l, FQ_LOG_ERROR, "sess_add_unlocked() error.");
		}
	}
	else {
		SS.GlbSm[new_sid].set_time = time(0);
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

int sess_register(fq_logger_t *l, char* skey)
{
	int rc = -1;

	FQ_TRACE_ENTER(l);

	lock_sesstable();
	rc = sess_register_unlocked(l, skey);
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
	
	FQ_TRACE_ENTER(l);

	rc =  OpenFileQueue(l, 0, 0, 0, 0, fqp->path, fqp->qname, &SS.GlbSm[sid].sess->obj);

	if(rc != TRUE)  { // false
		fq_log(l, FQ_LOG_ERROR, "LINK error: OpenFileQueue(%s, %s) error. Check path or Queue file.", VALUE(fqp->path), VALUE(fqp->qname));
		FQ_TRACE_EXIT(l);
		return(-1);
	}
	FQ_TRACE_EXIT(l);
	return(rc);
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
	int		rc;
	long    seq;
	long 	run_time=0L;

	FQ_TRACE_ENTER(l);

	rc =  SS.GlbSm[sid].sess->obj->on_en(l, SS.GlbSm[sid].sess->obj, EN_NORMAL_MODE, data , datalen+1, datalen, &seq, &run_time );
	if( rc < 0 ) {
		fq_log(l, FQ_LOG_ERROR,  "ENQU error: on_en() error. [%s][%s]", 
			SS.GlbSm[sid].sess->obj->h_obj->h->path, SS.GlbSm[sid].sess->obj->h_obj->h->qname);
	}
	else if( rc == EQ_FULL_RETURN_VALUE  ) {
		fq_log(l, FQ_LOG_ERROR, "ENQU error:  queue is full. I'll retry to put to the queue.[%s][%s]",
			SS.GlbSm[sid].sess->obj->h_obj->h->path, SS.GlbSm[sid].sess->obj->h_obj->h->qname);
	}
	else {
		fq_log(l, FQ_LOG_DEBUG,  "ENQU OK: Data is saved to the queue successfully. seq=[%ld]\n", seq);
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
	

	FQ_TRACE_ENTER(l);

	// buffer_size = SS.GlbSm[sid].sess->obj->h_obj->h->msglen - SOCK_CLIENT_HEADER_SIZE + 1;
	buffer_size = 65536;

	*data = malloc(buffer_size);
	memset(*data, 0x00, buffer_size);

	len=SS.GlbSm[sid].sess->obj->on_de(l, SS.GlbSm[sid].sess->obj, *data, buffer_size, &seq, &run_time);
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

int request_parsing( fq_logger_t *l, unsigned char *data, unsigned char *header, FQ_protocol_t *fqp, int len, unsigned char **ptr_reqmsg, int *msglen, int *req )
{
	FQ_TRACE_ENTER(l);

	memcpy(fqp->version, header, FQP_VERSION_LEN);
	memcpy(fqp->skey, header+FQP_VERSION_LEN, FQP_SESSION_ID_LEN);
	memcpy(fqp->path, header+(FQP_VERSION_LEN+FQP_SESSION_ID_LEN), FQP_PATH_LEN);
	str_lrtrim(fqp->path);
	memcpy(fqp->qname, header+(FQP_VERSION_LEN+FQP_SESSION_ID_LEN+FQP_PATH_LEN), FQP_QNAME_LEN);
	str_lrtrim(fqp->qname);
	fqp->ack_mode = *(header+(FQP_VERSION_LEN+FQP_SESSION_ID_LEN+FQP_PATH_LEN+FQP_QNAME_LEN) );
	memcpy(fqp->action_code, header+(FQP_VERSION_LEN+FQP_SESSION_ID_LEN+FQP_PATH_LEN+FQP_QNAME_LEN+1), FQP_ACTION_CODE_LEN);

	fq_log(l, FQ_LOG_DEBUG, "\t- fqp->version=[%s]", fqp->version);
	fq_log(l, FQ_LOG_DEBUG, "\t- fqp->skey=[%s]", fqp->skey);
	fq_log(l, FQ_LOG_DEBUG, "\t- fqp->path=[%s]", fqp->path);
	fq_log(l, FQ_LOG_DEBUG, "\t- fqp->qname=[%s]", fqp->qname);
	fq_log(l, FQ_LOG_DEBUG, "\t- fqp->ack_mode=[%c]", fqp->ack_mode);
	fq_log(l, FQ_LOG_DEBUG, "\t- fqp->action_code=[%s]", fqp->action_code);

	if(strncmp(fqp->action_code, FQP_LINK_MSG, FQP_ACTION_CODE_LEN) == 0) {
		*req = FQP_LINK_REQ;
	} else if( strncmp(fqp->action_code, FQP_ENQU_MSG, FQP_ACTION_CODE_LEN) == 0) {
		*req = FQP_ENQU_REQ;
	} else if( strncmp(fqp->action_code, FQP_DEQU_MSG, FQP_ACTION_CODE_LEN) == 0) {
		*req = FQP_DEQU_REQ;
	} else if( strncmp(fqp->action_code, FQP_EXIT_MSG, FQP_ACTION_CODE_LEN) == 0) {
		*req = FQP_EXIT_REQ;
	} else {
		fq_log(l, FQ_LOG_ERROR, "illegal request message.[%s]", fqp->action_code);
		FQ_TRACE_EXIT(l);
		return(-1);
	}

	*msglen = len-FQ_PROTOCOL_LEN;
	*ptr_reqmsg = malloc(*msglen+1);

	memset(*ptr_reqmsg, 0x00, *msglen+1);
	memcpy(*ptr_reqmsg, data+SOCK_CLIENT_HEADER_SIZE+FQ_PROTOCOL_LEN, len-FQ_PROTOCOL_LEN);

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

	/* 
	** This has a bug for decrease.
	fq_log(l, FQ_LOG_EMERG, "current_users=[%d]", SS.current_users);
	*/

	fq_log(l, FQ_LOG_INFO, "CB called.");

	/*
	** Warning : Variable data has value of header.so If you print data to sceeen, you can't see real data.
	** so you have to add 4bytes(header length) for saving and printing.
	*/
	fq_log(l, FQ_LOG_DEBUG, "len=[%d] [%-512.512s]\n", len, data+SOCK_CLIENT_HEADER_SIZE);

	/*
	** get header (protocol) from 5-th charactor.
	*/
	memcpy(header, data+SOCK_CLIENT_HEADER_SIZE, FQ_PROTOCOL_LEN); /* 512 */
	
	/* set buffer */
	memset(&fqp, 0x00, sizeof(FQ_protocol_t));


	/*
	** request parsing
	*/
	rc = request_parsing( l, data, header, &fqp, len, &req_msg, &req_msglen, &req );
	if( rc < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "request_parsing() error.");
		goto L0;
	}

	/*
	** job processing
	*/
	switch(req) {
		int rc;

		case FQP_LINK_REQ:
			
			rc = sess_register(l, NULL);
			fq_log(l, FQ_LOG_DEBUG, "LINK (queue file open) sess_register() rc=[%d]", rc);

			if( rc < 0 ) {
				fq_log(l, FQ_LOG_ERROR, "sess_register() error.");
				goto L0;
			}

			sid = rc;
			rc = LINK_processing(l, &fqp, sid);
			if( rc < 0 ) {
				fq_log(l, FQ_LOG_ERROR, "LINK_processing() error.");
				goto L0;
			}
			fq_log(l, FQ_LOG_DEBUG, "LINK(queue open) processing OK.");
			break;
		case FQP_ENQU_REQ:
			sid = sess_findskey(l, fqp.skey);
			if(sid < 0 ) {
				fq_log(l, FQ_LOG_ERROR, "sess_findsky() error.");
				goto L0;
			}
			fq_log(l, FQ_LOG_DEBUG, "ENQU (on_en)");
			
			rc = ENQU_processing(l, sid, req_msg, req_msglen);
			if ( rc < 0 ) {
				fq_log(l, FQ_LOG_ERROR, "ENQU_processing() error.");
				goto L0;
			}
			else if( rc == EQ_FULL_RETURN_VALUE ) {
				fq_log(l, FQ_LOG_ERROR, "ENQU_processing() is full.");
				goto L1;
			}
			break;
		case FQP_DEQU_REQ:
			sid = sess_findskey(l, fqp.skey);
			if(sid < 0 ) {
				fq_log(l, FQ_LOG_ERROR, "sess_findsky() error.");
				goto L0;
			}
			fq_log(l, FQ_LOG_DEBUG, "DEQU (on_de)");

			DEQU_rc = DEQU_processing(l, sid, &resp_msg_buffer);
			if ( DEQU_rc < 0 ) {
				fq_log(l, FQ_LOG_ERROR, "ENQU_processing() error.");
				goto L0;
			}
			else if( DEQU_rc == DQ_EMPTY_RETURN_VALUE ) {
				fq_log(l, FQ_LOG_INFO, "ENQU_processing() is empty.");
				goto L2;
			}
			break;
		case FQP_EXIT_REQ:
			fq_log(l, FQ_LOG_DEBUG, "DEQU (on_de)");
			goto L0;
		default:
			fq_log(l, FQ_LOG_ERROR, "illegal requesting.");
			goto L0;
	}
	switch(req) {
		case FQP_LINK_REQ:
		case FQP_ENQU_REQ:
			memset(resp, 'Y', 1);
			memcpy(resp+1, SS.GlbSm[sid].sess->skey, SKEY_SIZE);
			*resp_len = 1+SKEY_SIZE;	
			break;
		case FQP_DEQU_REQ:
			memset(resp, 'Y', 1);
			memcpy(resp+1, SS.GlbSm[sid].sess->skey, SKEY_SIZE);
			memcpy(resp+1+SKEY_SIZE, resp_msg_buffer, DEQU_rc);
			*resp_len = 1+ SKEY_SIZE + DEQU_rc;
			break;
		default:
			break;
	}

	fq_log(l, FQ_LOG_DEBUG, "response =[%s] resp_len=[%d]", resp, *resp_len);

	SAFE_FREE( req_msg );
	SAFE_FREE( resp_msg_buffer );
#ifdef CHECK_RUN_TIME
	off_stopwatch_micro(&p);
	get_stopwatch_micro(&p, &sec, &usec);
	fq_log(l, FQ_LOG_DEBUG, "CB_function delay time: %ldsec:%ldusec", sec, usec);
#endif
	return(*resp_len);

L0: /* error return */
	memset(resp, 'N', 1);
	*resp_len = 1;	

	SAFE_FREE( resp_msg_buffer );
	SAFE_FREE( req_msg );
#ifdef CHECK_RUN_TIME
	off_stopwatch_micro(&p);
	get_stopwatch_micro(&p, &sec, &usec);
	fq_log(l, FQ_LOG_DEBUG, "CB_function delay time: %ldsec:%ldusec", sec, usec);
#endif
	return(*resp_len);

L1: /* full return */
	memset(resp, 'F', 1);
	*resp_len = 1;	
	SAFE_FREE( resp_msg_buffer );
	SAFE_FREE( req_msg );
#ifdef CHECK_RUN_TIME
	off_stopwatch_micro(&p);
	get_stopwatch_micro(&p, &sec, &usec);
	fq_log(l, FQ_LOG_DEBUG, "CB_function delay time: %ldsec:%ldusec", sec, usec);
#endif
	return(*resp_len);

L2: /* empty return */
	memset(resp, 'E', 1);
	*resp_len = 1;	
	SAFE_FREE( resp_msg_buffer );
	SAFE_FREE( req_msg );
#ifdef CHECK_RUN_TIME
	off_stopwatch_micro(&p);
	get_stopwatch_micro(&p, &sec, &usec);
	fq_log(l, FQ_LOG_DEBUG, "CB_function delay time: %ldsec:%ldusec", sec, usec);
#endif
	return(*resp_len);
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
	printf("example: $ %s -f FQ_Protocol_svr.conf <enter>\n", p);
	return;
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

void print_version(char *p)
{
	printf("\nversion: %s.\n\n", FQ_PROTOCOL_SVR_C_VERSION);
	return;
}


int main(int ac, char **av)
{
	TCP_server_t x;

	/* server environment */
	char *bin_dir = NULL;
	/* parameters */
	char *logname = NULL;
	char *ip = NULL;
	char *port = NULL;
	char *loglevel = NULL;
	char *conf=NULL;
	int	i_loglevel = 0;
	int	ack_mode = 1; /* ACK 모드: FQ_Protocol_svr 는 항상 1로 설정한다. */

	int	rc = -1, i;
	int	ch;

	printf("Compiled on %s %s source program version=[%s]\n\n\n", __TIME__, __DATE__, FQ_PROTOCOL_SVR_C_VERSION);

	getProgName(av[0], g_Progname);

	while(( ch = getopt(ac, av, "VvHh:p:Q:l:i:P:o:f:")) != -1) {
		switch(ch) {
			case 'H':
			case 'h':
				print_help(av[0]);
				return(0);
			case 'V':
			case 'v':
				print_version(av[0]);
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

		/* 사용자가 정한 항목을 읽을 경우 */
		get_config(t, "FQ_SERVER_IP", buffer);
		ip = strdup(buffer);

		get_config(t, "FQ_SERVER_PORT", buffer);
		port = strdup(buffer);

		get_config(t, "LOG_NAME", buffer);
		logname = strdup(buffer);

		get_config(t, "LOG_LEVEL", buffer);
		loglevel = strdup(buffer);

		free_config(&t);
	}

	/* Checking mandatory parameters */
	if( !ip || !port || !logname || !loglevel) {
		print_help(av[0]);
		printf("User input: ip=[%s] port=[%s] logname=[%s] loglevel=[%s]\n", 
			VALUE(ip), VALUE(port), VALUE(logname), VALUE(loglevel));
		return(0);
	}

	/*
	** make background daemon process.
	*/
	// daemon_init();

	signal(SIGPIPE, SIG_IGN);

	
	/********************************************************************************************
	** Open a file for logging
	*/
	i_loglevel = get_log_level(loglevel);
	rc = fq_open_file_logger(&l, logname, i_loglevel);
	CHECK( rc > 0 );
	/************************************************************************************************/

	if( bin_dir && conf ) {
		/**********************************************************************************************
		** 이하에서 발생하는 오류 또는 signal 에 의해 프로그램이 종료되면 자동으로 재구동 되도록 셋팅한다.
		*/
		memset(restart_cmd, 0x00, sizeof(restart_cmd));
		sprintf(restart_cmd, "%s/%s -f %s", bin_dir, g_Progname, conf);
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

	make_thread_sync_CB_server( l, &x, ip, atoi(port), BINARY_HEADER_TYPE, 4, ack_mode, CB_function);

	fq_log(l, FQ_LOG_EMERG, "FQ Protocol server (entrust Co.version %s) start!!!.(listening ip=[%s] port=[%s]", 
		FQ_PROTOCOL_SVR_C_VERSION, ip, port);

	pause();

    fq_close_file_logger(&l);
	exit(EXIT_FAILURE);
}
