/*
  +----------------------------------------------------------------------+
  | Swoole                                                               |
  +----------------------------------------------------------------------+
  | This source file is subject to version 2.0 of the Apache license,    |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.apache.org/licenses/LICENSE-2.0.html                      |
  | If you did not receive a copy of the Apache2.0 license and are unable|
  | to obtain it through the world-wide-web, please send a note to       |
  | license@swoole.com so we can mail you a copy immediately.            |
  +----------------------------------------------------------------------+
  | Author: Tianfeng Han  <mikan.tenny@gmail.com>                        |
  +----------------------------------------------------------------------+
*/
#include <stdio.h>
#include "swoole.h"
#include "ring_queue.h"

#ifdef SW_USE_RINGQUEUE_TS


// typedef struct _swRingQueue
// {
//     int head; /* 头部，出队列方向*/
//     int tail; /* 尾部，入队列方向*/
//     int tag; /* 为空还是为满的标志位*/
//     int size; /* 队列总尺寸 */
//     void **data; /* 队列空间 */
//} swRingQueue;

int swRingQueue_init(swRingQueue *queue, int buffer_size)
{
    queue->size = buffer_size;
    queue->flags = (char *)sw_malloc(queue->size);
    if (queue->flags == NULL)
    {
        return -1;
    }
    queue->data = (void **)sw_calloc(queue->size, sizeof(void*));
    if (queue->data == NULL)
    {
        sw_free(queue->flags);
        return -1;
    }
    queue->head = 0;
    queue->tail = 0;
    memset(queue->flags, 0, queue->size);
    memset(queue->data, 0, queue->size * sizeof(void*));
    return 0;
}

int swRingQueue_push(swRingQueue *queue, void * ele)
{
    if (!(queue->num < queue->size))
    {
        return -1;
    }
    int cur_tail_index = queue->tail;
    char * cur_tail_flag_index = queue->flags + cur_tail_index;
    //TODO Scheld
    while (!sw_atomic_cmp_set(cur_tail_flag_index, 0, 1))
    {
        cur_tail_index = queue->tail;
        cur_tail_flag_index = queue->flags + cur_tail_index;
    }

    // 两个入队线程之间的同步
    //TODO 取模操作可以优化
    int update_tail_index = (cur_tail_index + 1) % queue->size;

    // 如果已经被其他的线程更新过，则不需要更新；
    // 否则，更新为 (cur_tail_index+1) % size;
    sw_atomic_cmp_set(&queue->tail, cur_tail_index, update_tail_index);

    // 申请到可用的存储空间
    *(queue->data + cur_tail_index) = ele;

    sw_atomic_fetch_add(cur_tail_flag_index, 1);
    sw_atomic_fetch_add(&queue->num, 1);
    return 0;
}

int swRingQueue_pop(swRingQueue *queue, void **ele)
{
    if (!(queue->num > 0))
        return -1;
    int cur_head_index = queue->head;
    char * cur_head_flag_index = queue->flags + cur_head_index;

    while (!sw_atomic_cmp_set(cur_head_flag_index, 2, 3))
    {
        cur_head_index = queue->head;
        cur_head_flag_index = queue->flags + cur_head_index;
    }
    //TODO 取模操作可以优化
    int update_head_index = (cur_head_index + 1) % queue->size;
    sw_atomic_cmp_set(&queue->head, cur_head_index, update_head_index);
    *ele = *(queue->data + cur_head_index);

    sw_atomic_fetch_sub(cur_head_flag_index, 3);
    sw_atomic_fetch_sub(&queue->num, 1);
    return 0;
}
#else
//默认走到这里

int swRingQueue_init(swRingQueue *queue, int buffer_size)
{   
    //分配存储空间用来存储指针类型
    queue->data = sw_calloc(buffer_size, sizeof(void*));
    if (queue->data == NULL)
    {
        swWarn("malloc failed.");
        return -1;
    }
    queue->size = buffer_size;
    queue->head = 0;
    queue->tail = 0;
    queue->tag = 0;
    return 0;
}
//释放
void swRingQueue_free(swRingQueue *queue)
{
    sw_free(queue->data);
}

//推数据进来
int swRingQueue_push(swRingQueue *queue, void *push_data)
{   // ( (q->head == q->tail) && (q->tag == 1))
    //判断是否没有空间
    if (swRingQueue_full(queue))
    {
        return SW_ERR;
    }
    //数据放到队尾
    queue->data[queue->tail] = push_data;
    //队尾移动
    queue->tail = (queue->tail + 1) % queue->size;
    //队尾和对首相等时，设置queue->tag 为1
    if (queue->tail == queue->head)
    {
        queue->tag = 1;
    }
    return SW_OK;
}

//弹出数据
int swRingQueue_pop(swRingQueue *queue, void **pop_data)
{   //( (q->head == q->tail) && (q->tag == 0))
    //判断是否为空
    if (swRingQueue_empty(queue))
    {
        return SW_ERR;
    }
    //弹出队首数据
    *pop_data = queue->data[queue->head];
    //队首移动
    queue->head = (queue->head + 1) % queue->size;
    //队尾和队首相等时，队列为空
    if (queue->tail == queue->head)
    {
        queue->tag = 0;
    }
    return SW_OK;
}

#endif
