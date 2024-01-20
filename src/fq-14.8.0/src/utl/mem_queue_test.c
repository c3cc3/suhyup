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
			/* ���۰� full�� �ƴϱ⸦(���ڸ��� �ֱ⸦) ��ٸ��� */
            pthread_cond_wait(&p->obj->Buffer_Not_Full,&p->obj->mtx);
			/* cond_wait ���� ���� lock�� �ڵ����� �ȴ� */
			/* wait ���� ���´ٴ� ���� ������� �����. �� ���۰� �����. �� ���۰� ����� �ִٴ� �ñ׳���  �޾Ҵ� ��� �ǹ�  */
        }

        printf("Produce(put) : %d \n", value);
		value++;

		/* ���ۿ� �����͸� �־����� ��������� �ñ׳��� �۽��Ѵ� */
        pthread_cond_signal(&p->obj->Buffer_Not_Empty);
		/* producer�� sleep�ϸ� ���۰� 1���� ���� consumer�� �������� index = 1 �� ��� �����Ѵ�. */ 

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
			/* ���۰� empty �� �ƴϱ⸦(ä�����⸦)  ��ٸ��� */
            pthread_cond_wait(&p->obj->Buffer_Not_Empty,&p->obj->mtx);
			/* cond_wait ���� ���� lock�� �ڵ����� �ȴ� */
			/* wait ���� ���´ٴ� ���� ���� �����Ͱ� �ִ�. �� ������ �����Ͱ� �ִٴ� �ñ׳��� �������̴� */
        }
		p->obj->on_getItem(p->obj, &d);
        printf("Consume(%d) : %ld, %s  \n", p->id_num,  d.len, d.msg);
		/* ���� �����ϳ��� ������� ���ڸ��� �ִٴ� �ñ׳��� �۽��Ѵ� */
        pthread_cond_signal(&p->obj->Buffer_Not_Full);
		/* consumer�� sleep �ϸ�(������) ���۰� ���ä����ä�� (10) �� �����ϸ� �����Ѵ�. */
		// usleep(1000);
		
        pthread_mutex_unlock(&p->obj->mtx);
    }
}
