/* 
 * mem_queue.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h> /* for memcpy() */

#include "mem_queue.h"
#include "fq_common.h"

static int on_isEmpty(mem_queue_obj_t *obj);
static int on_isFull( mem_queue_obj_t *obj);
static int on_putItem(mem_queue_obj_t *obj, queue_message_t *qm);
static int on_getItem( mem_queue_obj_t *obj, queue_message_t *qm);
void on_printQueue( mem_queue_obj_t *obj);

int open_mem_queue_obj( mem_queue_obj_t **obj)
{
	mem_queue_obj_t *rc=NULL;

	rc = (mem_queue_obj_t *)calloc(1, sizeof( mem_queue_obj_t ));

    if(rc) {
		int i;

        /* initialize members  */
		rc->Queue.validItems = 0;
		rc->Queue.first      = 0;
		rc->Queue.last       = 0;
		for(i=0; i<MAX_ITEMS; i++) {
			rc->Queue.qm[i].len = 0;
			rc->Queue.qm[i].msg[0] = 0;
		}

		pthread_condattr_init(&rc->cond_attr);
		pthread_cond_init(&rc->Buffer_Not_Full, &rc->cond_attr);
		pthread_cond_init(&rc->Buffer_Not_Empty, &rc->cond_attr);

		pthread_mutexattr_init( &rc->mtx_attr);
		pthread_mutex_init(&rc->mtx, &rc->mtx_attr); 

        /* assign on_function pointers */
        rc->on_isEmpty    = on_isEmpty;
        rc->on_isFull     = on_isFull;
		rc->on_putItem    = on_putItem;
		rc->on_getItem    = on_getItem;
		rc->on_printQueue = on_printQueue;

        *obj = rc;

        return(TRUE);
    }
    SAFE_FREE( (*obj) );
    return(FALSE);
}

int close_mem_queue_obj( mem_queue_obj_t **obj)
{

    SAFE_FREE( (*obj) );

    return(TRUE);
}

static int 
on_isEmpty(mem_queue_obj_t *obj)
{
    if(obj->Queue.validItems==0)
        return(1);
    else
        return(0);
}

static int 
on_isFull( mem_queue_obj_t *obj)
{
    if(obj->Queue.validItems>=MAX_ITEMS)
        return(1);
    else
        return(0);
}

static int 
on_putItem(mem_queue_obj_t *obj, queue_message_t *theItemValue)
{
    if(obj->Queue.validItems>=MAX_ITEMS)
    {
        //printf("The queue is full\n");
        //printf("You cannot add items\n");
        return(MEM_QUEUE_FULL);
    }
    else
    {
        obj->Queue.validItems++;

        obj->Queue.qm[obj->Queue.last].len = theItemValue->len;
		memcpy( (void *)obj->Queue.qm[obj->Queue.last].msg, theItemValue->msg, theItemValue->len); 
        obj->Queue.last = (obj->Queue.last+1)%MAX_ITEMS;
		return(theItemValue->len);
    }
}

static int 
on_getItem( mem_queue_obj_t *obj, queue_message_t *theItemValue)
{
    if(on_isEmpty(obj))
    {
        printf("isempty\n");
        return(MEM_QUEUE_EMPTY);
    }
    else
    {
		theItemValue->len = obj->Queue.qm[obj->Queue.first].len;
		memcpy(theItemValue->msg, obj->Queue.qm[obj->Queue.first].msg, obj->Queue.qm[obj->Queue.first].len);
        obj->Queue.first=(obj->Queue.first+1)%MAX_ITEMS;
        obj->Queue.validItems--;
        return(obj->Queue.qm[obj->Queue.first].len);
    }
}

void on_printQueue( mem_queue_obj_t *obj)
{
    int aux, aux1;

    aux  = obj->Queue.first;
    aux1 = obj->Queue.validItems;
    while(aux1>0)
    {
        printf("Element #%d = %ld:%s\n", aux, (long int)obj->Queue.qm[aux].len, obj->Queue.qm[aux].msg);
        aux=(aux+1)%MAX_ITEMS;
        aux1--;
    }
    return;
}
#if 0 /* library usage */

# include <stdio.h>
# include <stdlib.h>
# include <unistd.h>
# include <pthread.h>

#include "mem_queue.h"

#define THREAD_NUMS 2

void *Producer();
void *Consumer();

int current_value = 0;

typedef struct _thread_param_t {
	mem_queue_obj_t	*obj;
	int			id_num;
	pthread_t	tid;
} thread_param_t;

int main()
{
	mem_queue_obj_t *obj;
	thread_param_t t[THREAD_NUMS];

	if( open_mem_queue_obj( &obj ) != TRUE ) {
		printf("open_mem_queue_obj() error.\n");
		exit(0);
	}

	t[0].obj = obj;
	t[0].id_num = 0;
    pthread_create(&t[0].tid,NULL,Producer, &t[0]);

	t[1].obj = obj;
	t[1].id_num = 1;
    pthread_create(&t[1].tid,NULL,Consumer, &t[1]);

	t[2].obj = obj;
	t[2].id_num = 2;
    pthread_create(&t[2].tid,NULL,Consumer, &t[2]);

    pthread_join(t[0].tid,NULL);
    pthread_join(t[1].tid,NULL);
    pthread_join(t[2].tid,NULL);

    return 0;
}

int get_data()
{
	usleep(1000);
	return(current_value++);
}
/* enQ threads */
void *Producer(void *t)
{
	thread_param_t *p = t;
	int	value=0;

    for(;;)
    {
		int value;
		queue_message_t	d;

		value = get_data();
		sprintf(d.msg, "%08d", value);
		d.len = 8;

        pthread_mutex_lock(&p->obj->mtx);
		if( p->obj->on_putItem(p->obj, &d) == MEM_QUEUE_FULL )  /* full */
        {
			/* 버퍼가 full이 아니기를(빈자리가 있기를) 기다린다 */
            pthread_cond_wait(&p->obj->Buffer_Not_Full,&p->obj->mtx);
			/* cond_wait 으로 들어가면 lock이 자동해제 된다 */
			/* wait 에서 나온다는 것은 빈공간이 생겼다. 즉 버퍼가 비었다. 즉 버퍼가 빈곳이 있다는 시그널을  받았다 라는 의미  */
        }

        printf("Produce(put) : %d \n", value);
		value++;

		/* 버퍼에 데이터를 넣었으니 가저가라는 시그널을 송신한다 */
        pthread_cond_signal(&p->obj->Buffer_Not_Empty);
		/* producer가 sleep하면 버퍼가 1개만 차도 consumer가 가저가서 index = 1 을 계속 유지한다. */ 

        pthread_mutex_unlock(&p->obj->mtx);
    }

}

/* deQ threads */
void *Consumer( void *t )
{
	thread_param_t *p = t;

    for(;;)
    {
		queue_message_t	d;

		d.len = 0;
		memset(d.msg, 0x00, sizeof(d.msg));

        pthread_mutex_lock(&p->obj->mtx);

        if( p->obj->on_isEmpty(p->obj) )
        {
			/* 버퍼가 empty 가 아니기를(채워지기를)  기다린다 */
            pthread_cond_wait(&p->obj->Buffer_Not_Empty,&p->obj->mtx);
			/* cond_wait 으로 들어가면 lock이 자동해제 된다 */
			/* wait 에서 나온다는 것은 들어온 데이터가 있다. 즉 가져갈 데이터가 있다는 시그널을 받은것이다 */
        }
		p->obj->on_getItem(p->obj, &d);
        printf("Consume(%d) : %d, %s  \n", p->id_num,  d.len, d.msg);
		/* 내가 버퍼하나를 비웠으니 빈자리가 있다는 시그널을 송신한다 */
        pthread_cond_signal(&p->obj->Buffer_Not_Full);
		/* consumer가 sleep 하면(느리면) 버퍼가 계속채워진채로 (10) 을 유지하며 동작한다. */
		// usleep(1000);
		
        pthread_mutex_unlock(&p->obj->mtx);
    }
}
#endif
