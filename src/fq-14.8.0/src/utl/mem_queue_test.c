/*
** mem_queue_test.c
*/

# include <stdio.h>
# include <string.h> /* memset() */
# include <stdlib.h>
# include <unistd.h>
# include <pthread.h>

#include "mem_queue.h"

#define THREAD_NUMS 3

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
	mem_queue_obj_t *obj=NULL;
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

	close_mem_queue_obj(&obj);

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

    for(;;)
    {
		int value;
		queue_message_t	d;

		value = get_data();
		sprintf((char *)d.msg, "%08d", value);
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
        printf("Consume(%d) : %ld, %s  \n", p->id_num,  d.len, d.msg);
		/* 내가 버퍼하나를 비웠으니 빈자리가 있다는 시그널을 송신한다 */
        pthread_cond_signal(&p->obj->Buffer_Not_Full);
		/* consumer가 sleep 하면(느리면) 버퍼가 계속채워진채로 (10) 을 유지하며 동작한다. */
		// usleep(1000);
		
        pthread_mutex_unlock(&p->obj->mtx);
    }
}
