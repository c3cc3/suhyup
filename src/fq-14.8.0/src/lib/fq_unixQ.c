/* vi: set sw=4 ts=4: */
/*
 * fq_unixQ.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>
#include <errno.h>

#include <sys/ipc.h>
#include <sys/msg.h>

#include "fq_common.h"
#include "fq_unixQ.h"
#include "fq_logger.h"

#define FQ_UNIXQ_C_VERSION "1.0.0"

/*
** 1.0.1: Update: open_unixQ() 
*/

/* #define TRUE  1 */
/* #define FALSE 0 */

static int on_send_unixQ( unixQ_obj_t *obj, const void *msgp, size_t msgsz, int flag);
static int on_recv_unixQ( unixQ_obj_t *obj, void *msgp, size_t msgsz, long msgtype );
static int on_getstat_unixQ( unixQ_obj_t *obj, struct msqid_ds *buf);
static int on_en( unixQ_obj_t *obj, unsigned char *msg, size_t msglen, long to, long returnID,  int wait_flag);
static int on_de( unixQ_obj_t *obj, unsigned char *dst, size_t dstlen, long msgtype, long *Sender, long *returnID);

#if 0
int msgctl(int msqid, int cmd, struct msqid_ds *buf);

msgctl() performs the control operation specified by cmd on the message queue with identifier msqid.

The msqid_ds data structure is defined in <sys/msg.h> as follows:

   struct msqid_ds {
	   struct ipc_perm msg_perm;     /* Ownership and permissions */
	   time_t          msg_stime;    /* Time of last msgsnd(2) */
	   time_t          msg_rtime;    /* Time of last msgrcv(2) */
	   time_t          msg_ctime;    /* Time of last change */
	   unsigned long   __msg_cbytes; /* Current number of bytes in
										queue (nonstandard) */
	   msgqnum_t       msg_qnum;     /* Current number of messages
										in queue */
	   msglen_t        msg_qbytes;   /* Maximum number of bytes
										allowed in queue */
	   pid_t           msg_lspid;    /* PID of last msgsnd(2) */
	   pid_t           msg_lrpid;    /* PID of last msgrcv(2) */
   };

The ipc_perm structure is defined in <sys/ipc.h> as follows (the highlighted fields are settable using IPC_SET):

   struct ipc_perm {
	   key_t          __key;       /* Key supplied to msgget(2) */
	   uid_t          uid;         /* Effective UID of owner */
	   gid_t          gid;         /* Effective GID of owner */
	   uid_t          cuid;        /* Effective UID of creator */
	   gid_t          cgid;        /* Effective GID of creator */
	   unsigned short mode;        /* Permissions */
	   unsigned short __seq;       /* Sequence number */
   };
Valid values for cmd are:

       IPC_STAT
              Copy information from the kernel data structure associated with msqid into the msqid_ds structure pointed to by buf.  The caller must have read permission on the message queue.


sg@c3cc3 ~/v10/src/etc/UNIX_Queue/UNIX_Queue_call [18:40]$ ipcs -l

------ Shared Memory Limits --------
max number of segments = 4096
max seg size (kbytes) = 32768
max total shared memory (kbytes) = 8388608
min seg size (bytes) = 1

------ Semaphore Limits --------
max number of arrays = 128
max semaphores per array = 250
max semaphores system wide = 32000
max ops per semop call = 32
semaphore max value = 32767

------ Messages Limits --------
max queues system wide = 3980
max size of message (bytes) = 8192
default max size of queue (bytes) = 16384

   # ipcs -l

   ------ ���� �޸� �Ѱ� --------
   �ִ� ���׸�Ʈ �� = 4096                       /* SHMMNI	*/
   �ִ� ���׸�Ʈ ũ��(ų�ι���Ʈ) = 32768        /* SHMMAX*/
   �ִ� �� ���� �޸�(ų�ι���Ʈ) = 8388608     /* SHMALL*/
   �ּ� ���׸�Ʈ ũ��(����Ʈ) = 1

   ------ �������� �Ѱ� --------
   �ִ� �迭 �� = 1024                           /* SEMMNI*/
   �迭�� �ִ� �������� = 250                    /* SEMMSL*/
   �ִ� �������� �ý��� �ʺ� = 256000            /* SEMMNS*/
   �������� ȣ��� �ִ� �۵� �� = 32             /* SEMOPM*/
   �������� �ִ밪 = 32767

   ------ �޽���: �Ѱ� --------
   �ִ� ť �ý��� �ʺ� = 1024               /* MSGMNI*/
   �ִ� �޽��� ũ��(����Ʈ) = 65536         /* MSGMAX*/
   ����Ʈ �ִ� ť ũ��(����Ʈ) = 65536    /* MSGMNB*/


MSGMNI�� ������ �� �ִ� ������Ʈ ��, MSGMAX�� ť���� ���۵� �� �ִ� �޽����� ũ��, MSGMNB�� ť�� ũ�⿡ ������ ��Ĩ�ϴ�.

���� �ý��ۿ����� MSGMAX�� 64 KB(�� 65535 ����Ʈ)�� �����ϰ� MSGMNB�� 65535�� �÷��� �մϴ�.

�� Ŀ�� �Ű������� �����Ϸ��� /etc/sysctl.conf ������ �����ؾ� �մϴ�. �� ������ �������� ������ ������ �ۼ��ؾ� �մϴ�. ���� ���� ���Ͽ� �����ؾ� �� �Ű������� ���� ���Դϴ�.

kernel.sem=250 256000 32 1024
#Example shmmax for a 64-bit system
kernel.shmmax=1073741824	
#Example shmall for 90 percent of 16 GB memory
kernel.shmall=3774873		
kernel.msgmax=65535
kernel.msgmnb=65535
-p �Ű������� �Բ� sysctl�� �����Ͽ� ����Ʈ ������ /etc/sysctl.conf�κ��� sysctl ������ �ε��Ͻʽÿ�.

     sysctl -p


SYNOPSIS
       #include <sys/types.h>
       #include <sys/ipc.h>

       key_t ftok(const char *pathname, int proj_id);


DESCRIPTION
       The ftok() function uses the identity of the file named by the given pathname (which must refer to an existing, accessible file) and 
	   the least significant 8 bits of proj_id (which must be nonzero) to generate a key_t type System V IPC key, suitable for use with
       msgget(2), semget(2), or shmget(2).

#endif

int open_unixQ_obj(  fq_logger_t *l, char *path, char key_char, unixQ_obj_t **obj)
{
	unixQ_obj_t *rc=NULL;

	FQ_TRACE_ENTER(l);
#if 0

	if( !HASVALUE(path) )  {
        fq_log(l, FQ_LOG_ERROR, "illgal function call.");
		FQ_TRACE_EXIT(l);
		return(FALSE);
    }

	rc = (unixQ_obj_t *)calloc(1, sizeof(unixQ_obj_t));
	if(rc) {
		rc->key = ftok(path, key_char);
		rc->path = strdup(path);
		rc->key_char = key_char;

		rc->l = l;
		rc->on_send_unixQ = on_send_unixQ;
		rc->on_recv_unixQ = on_recv_unixQ;
		rc->on_getstat_unixQ = on_getstat_unixQ;
		rc->on_en = on_en;
		rc->on_de = on_de;

		rc->msq_id = msgget(rc->key, IPC_CREAT|IPC_EXCL|0666 );
		if(rc->msq_id == (-1)) {
			if(errno == EEXIST) {
				fq_log(l, FQ_LOG_DEBUG, "Already exist. We will open already existing queue.");
				if ((rc->msq_id = msgget(rc->key, 0)) == -1) {
					fq_log(l, FQ_LOG_ERROR, "msgget() error. reason=[%s]", strerror(errno));
					goto return_FALSE;
				}
				*obj = rc;
				FQ_TRACE_EXIT(l);
				return(TRUE);
			}
			fq_log(l, FQ_LOG_ERROR, "msgget() error. errno=[%d] reason=[%s]", errno, strerror(errno));
			goto return_FALSE;
		}
		fq_log(l, FQ_LOG_DEBUG, "New Creation OK.");
		*obj = rc;
		FQ_TRACE_EXIT(l);
		return(TRUE);
	}
return_FALSE:
	SAFE_FREE(rc);
	SAFE_FREE(*obj);

	FQ_TRACE_EXIT(l);
	return(FALSE);
#else
	return(TRUE);
#endif
}

int close_unixQ_obj(fq_logger_t *l, unixQ_obj_t **obj)
{
	FQ_TRACE_ENTER(l);

	SAFE_FREE( (*obj)->path );
	SAFE_FREE( (*obj) );

	FQ_TRACE_EXIT(l);
	return(TRUE);
}

#if 0
/*
 * ���Լ��� ���� ������ �Ǿ��� �ʽ��ϴ�.
 * ������.
 */
int set_unixQ(int msq_id, struct msqid_ds *buf)
{
	int rc;

	/* ds->msg_qbytes = 1024*1024; */

    rc = msgctl( msq_id, IPC_SET, buf);
	if(rc < 0) {
      fprintf(stderr, "msgctl(msqid=%d, IPC_SET, ...) failed "
          "(msg_perm.uid=%u,"
          "msg_perm.cuid=%u):"
          "%s (errno=%d)\n",
          msq_id,
          buf->msg_perm.uid,
          buf->msg_perm.cuid,
          strerror(errno),
          errno);
		perror("msgctl(IPC_SET)");
    }
	return(rc);
}
#endif

int unlink_unixQ( fq_logger_t *l, char *path, char key_char )
{
	key_t msqkey;
	int	msq_id;

	FQ_TRACE_ENTER(l);

#if 0
	msqkey=ftok(path, key_char);

	if ((msq_id = msgget(msqkey, 0)) == -1) {
		fq_log(l, FQ_LOG_ERROR, "msgget() error. reason=[%s]", strerror(errno));
		FQ_TRACE_EXIT(l);
		return(FALSE);
	}

	if (msgctl(msq_id, IPC_RMID, NULL) == -1) {
		fq_log(l, FQ_LOG_ERROR, "msgctl( IPC_RMID ) error. reason=[%s]", strerror(errno));
		FQ_TRACE_EXIT(l);
		return(FALSE);
	}

#endif
	FQ_TRACE_EXIT(l);
	return(TRUE);
}

/*
 * If IPC_NOWAIT is specified in msgflg, then the call instead  fails  with  the  error
 *        EAGAIN.
 */
static int on_send_unixQ( unixQ_obj_t *obj, const void *msgp, size_t msgsz, int flag)
{
	int n;

	FQ_TRACE_ENTER(obj->l);

	n=msgsnd(obj->msq_id, msgp, msgsz, flag);
	if( n == 0 ) { /* succeed */
		FQ_TRACE_EXIT(obj->l);
		return(msgsz);
	}
	else { /* failed */
		if( errno == EAGAIN ) { /* Queue is full. errno == 11 */
			fq_log(obj->l, FQ_LOG_DEBUG, "Queue is full.");
			FQ_TRACE_EXIT(obj->l);
			return(0);
		}
		else {
			fq_log(obj->l, FQ_LOG_ERROR, "msgsnd() error. reason=[%s]", strerror(errno));
			FQ_TRACE_EXIT(obj->l);
			return(-1);
		}
	}
}
/*
 * If IPC_NOWAIT is specified in msgflg, then the call instead  fails  with  the  error
 *        EAGAIN.
 */

#if 0
/* UnixQueue(UQ) msgbuf */
struct UQ_msgbuf {
    long pid; /* getpid() */
    long tid; /* pthread_self() */
    char mtext[65532+1];
};
#endif

static int 
on_en( unixQ_obj_t *obj, unsigned char *msg, size_t msglen, long to, long my_listen_ID, int wait_flag)
{
	int n;
	struct UQ_msgbuf *msgp = NULL;
	size_t msgsz;
	size_t	header_size;

	FQ_TRACE_ENTER(obj->l);

	if( msglen > UQ_USER_MAX_MSGLEN) {
		fq_log(obj->l, FQ_LOG_DEBUG, "message length is too big. max=[%d]", UQ_USER_MAX_MSGLEN);
		FQ_TRACE_EXIT(obj->l);
		return(-1);
	}

	msgp = (struct UQ_msgbuf *)calloc(1, sizeof(struct UQ_msgbuf));
	msgp->pid = to;
	msgp->tid = my_listen_ID;
	memset(msgp->mtext, 0x00, sizeof(msgp->mtext));
	memcpy(msgp->mtext, msg, msglen);

	header_size = sizeof(msgp->pid) + sizeof(msgp->tid);
	msgsz = header_size + msglen;

	n=msgsnd(obj->msq_id, (void *)msgp, msgsz, wait_flag);
	if( n == 0 ) { /* succeed */
		SAFE_FREE(msgp);
		FQ_TRACE_EXIT(obj->l);
		return(msgsz-header_size);
	}
	else { /* failed */
		if( errno == EAGAIN ) { /* Queue is full. errno == 11 */
			fq_log(obj->l, FQ_LOG_DEBUG, "Queue is full.");
			SAFE_FREE(msgp);
			FQ_TRACE_EXIT(obj->l);
			return(UQ_FULL_RETURN_VALUE); /* UQ_FULL_RETURN_VALUE: 0 */
		}
		else {
			fq_log(obj->l, FQ_LOG_ERROR, "msgsnd() error. reason=[%s]", strerror(errno));
			SAFE_FREE(msgp);
			FQ_TRACE_EXIT(obj->l);
			return(-1);
		}
	}
}

static int on_recv_unixQ( unixQ_obj_t *obj, void *msgp, size_t msgsz, long msgtype )
{
	int n;

	FQ_TRACE_ENTER(obj->l);

#if 0
	// ssize_t msgrcv(int msqid, void *msgp, size_t msgsz, long msgtyp, int msgflg);
	// msgflag
	// MSG_NOERROR is specified, then the message text will be truncated (and the truncated part will be lost); if MSG_NOERROR is not specified, then the message isn't removed from the queue and the system call fails returning -1 with errno set to E2BIG.
	//
	// msgtype
	// If msgtyp is 0, then the first message in the queue is read.
	// If msgtyp is greater than 0, then the first message in the queue of type msgtyp is read, unless MSG_EXCEPT was specified in msgflg, in which case the first message in the queue of type not equal to msgtyp will be read.
	// If msgtyp is less than 0, then the first message in the queue with the lowest type less than or equal to the absolute value of msgtyp will be read.
	//
#endif

	if( (n=msgrcv(obj->msq_id, msgp, msgsz, msgtype, MSG_NOERROR))  == -1) {
		fq_log(obj->l, FQ_LOG_ERROR, "msgrcv() error. reason=[%s]", strerror(errno));
		FQ_TRACE_EXIT(obj->l);
	}

	FQ_TRACE_EXIT(obj->l);
	return(n);
}

static int 
on_de( unixQ_obj_t *obj, unsigned char *dst, size_t dstlen, long msgtype, long *Sender, long *returnID)
{
	int n;
	struct UQ_msgbuf *msgp = NULL;
	size_t msgsz;
	size_t	header_size;

	FQ_TRACE_ENTER(obj->l);

	msgp = (struct UQ_msgbuf *)calloc(1, sizeof(struct UQ_msgbuf));
	memset(msgp->mtext, 0x00, sizeof(msgp->mtext));
	msgsz = sizeof(struct UQ_msgbuf);
	header_size = sizeof(msgp->pid) + sizeof(msgp->tid);
	msgp->pid = -1;
	msgp->tid = -1;

#if 0
	// ssize_t msgrcv(int msqid, void *msgp, size_t msgsz, long msgtyp, int msgflg);
	// msgflag
	// MSG_NOERROR is specified, then the message text will be truncated (and the truncated part will be lost); if MSG_NOERROR is not specified, then the message isn't removed from the queue and the system call fails returning -1 with errno set to E2BIG.
	//
	// msgtype
	// If msgtyp is 0, then the first message in the queue is read.
	// If msgtyp is greater than 0, then the first message in the queue of type msgtyp is read, unless MSG_EXCEPT was specified in msgflg, in which case the first message in the queue of type not equal to msgtyp will be read.
	// If msgtyp is less than 0, then the first message in the queue with the lowest type less than or equal to the absolute value of msgtyp will be read.
	//
#endif
	if( (n=msgrcv(obj->msq_id, msgp, msgsz, msgtype, MSG_NOERROR))  == -1) {
	/* if( (n=msgrcv(obj->msq_id, msgp, msgsz, msgtype, IPC_NOWAIT))  == -1) { */
		fq_log(obj->l, FQ_LOG_ERROR, "msgrcv() error. reason=[%s]", strerror(errno));
		SAFE_FREE(msgp);
		FQ_TRACE_EXIT(obj->l);
		return(n);
	}

	*Sender = msgp->pid;
	*returnID = msgp->tid; /* ���������� enUQ�� ���μ��� ���̵� */
	memcpy(dst, msgp->mtext, sizeof(msgp->mtext));
	dstlen = n-header_size;
	SAFE_FREE(msgp);

	FQ_TRACE_EXIT(obj->l);
	return(dstlen);
}

static int on_getstat_unixQ( unixQ_obj_t *obj, struct msqid_ds *buf)
{
	int rc;

	FQ_TRACE_ENTER(obj->l);
	rc =  msgctl(obj->msq_id, IPC_STAT, buf);
	if( rc < 0 ) {
		fq_log(obj->l, FQ_LOG_ERROR, "msgctl(IPC_STAT) error. reason=[%s]", strerror(errno));
		perror("msgctl(IPC_STAT)");
	}

	FQ_TRACE_EXIT(obj->l);
	return(rc);
}
