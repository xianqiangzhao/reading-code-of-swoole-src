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

#include "swoole.h"

#define SW_CHANNEL_MIN_MEM (1024*64)

//channel 中存放的item
typedef struct _swChannel_item
{
    int length;
    char data[0];
} swChannel_item;

//申请channel 内存
swChannel* swChannel_new(size_t size, int maxlen, int flags)
{
    assert(size >= maxlen);
    int ret;
    void *mem;

    //use shared memory
    if (flags & SW_CHAN_SHM) //用共享内存 可以多进程共享
    {
        mem = sw_shm_malloc(size + sizeof(swChannel));
    }
    else
    {
        mem = sw_malloc(size + sizeof(swChannel));//不使用共享内存
    }

    if (mem == NULL)
    {
        swWarn("swChannel_create: malloc(%ld) failed.", size);
        return NULL;
    }
    swChannel *object = mem;
    mem += sizeof(swChannel);

    bzero(object, sizeof(swChannel));

    //overflow space
    object->size = size;//8192
    object->mem = mem;  //申请到的内存地址是头存放swShareMemory接着是swChannel，后面才是可用的内存，也就是会都申请出来两个他们两个的结构体的内存。
    object->maxlen = maxlen;//申请到的内存size 
    object->flag = flags;

    //use lock
    if (flags & SW_CHAN_LOCK) //判断是否使用锁
    {
        //init lock
        if (swMutex_create(&object->lock, 1) < 0) //建立锁
        {
            swWarn("mutex init failed.");
            return NULL;
        }
    }
    //use notify
    if (flags & SW_CHAN_NOTIFY) //chanel 没有用到
    {
        ret = swPipeNotify_auto(&object->notify_fd, 1, 1);//通過eventfd 的是否可读可写来实现进程间通信。
        if (ret < 0)
        {
            swWarn("notify_fd init failed.");
            return NULL;
        }
    }
    return object;
}

/**
 * push data(no lock)  //数据放入channel
 */
int swChannel_in(swChannel *object, void *in, int data_length)
{
    assert(data_length <= object->maxlen);
    if (swChannel_full(object))
    {
        return SW_ERR;
    }
    swChannel_item *item;
    int msize = sizeof(item->length) + data_length;

    if (object->tail < object->head) //空间不足就报错？？？？
    {
        //no enough memory space
        if ((object->head - object->tail) < msize)
        {
            return SW_ERR;
        }
        item = object->mem + object->tail;
        object->tail += msize;
    }
    else
    {
        item = object->mem + object->tail; //item 执行内存地址，第一次时object->tail = 0
        object->tail += msize;
        if (object->tail >= object->size)
        {
            object->tail = 0;
            object->tail_tag = 1 - object->tail_tag;
        }
    }
    object->num++;
    object->bytes += data_length;
    item->length = data_length;
    memcpy(item->data, in, data_length); //把in 的数据放到item->data 中
    return SW_OK;
}

/**
 * pop data(no lock) //数据弹出
 */
int swChannel_out(swChannel *object, void *out, int buffer_length)
{
    if (swChannel_empty(object))
    {
        return SW_ERR;
    }

    swChannel_item *item = object->mem + object->head;
    assert(buffer_length >= item->length);
    memcpy(out, item->data, item->length);
    object->head += (item->length + sizeof(item->length));
    if (object->head >= object->size)
    {
        object->head = 0;
        object->head_tag = 1 - object->head_tag;
    }
    object->num--;
    object->bytes -= item->length;
    return item->length;
}

/**
 * peek data
 */
int swChannel_peek(swChannel *object, void *out, int buffer_length)
{
    if (swChannel_empty(object))
    {
        return SW_ERR;
    }

    int length;
    object->lock.lock(&object->lock);
    swChannel_item *item = object->mem + object->head;
    assert(buffer_length >= item->length);
    memcpy(out, item->data, item->length);
    length = item->length;
    object->lock.unlock(&object->lock);

    return length;
}

/**
 * wait notify
 */ //没用用到
int swChannel_wait(swChannel *object)
{
    assert(object->flag & SW_CHAN_NOTIFY);
    uint64_t flag;
    return object->notify_fd.read(&object->notify_fd, &flag, sizeof(flag));
}

/**
 * new data coming, notify to customer
 *///没用用到
int swChannel_notify(swChannel *object)
{
    assert(object->flag & SW_CHAN_NOTIFY);
    uint64_t flag = 1;
    return object->notify_fd.write(&object->notify_fd, &flag, sizeof(flag));
}

/**
 * push data (lock)
 */
int swChannel_push(swChannel *object, void *in, int data_length)
{
    assert(object->flag & SW_CHAN_LOCK);
    object->lock.lock(&object->lock);
    int ret = swChannel_in(object, in, data_length);
    object->lock.unlock(&object->lock);
    return ret;
}

/**
 * free channel
 */
void swChannel_free(swChannel *object)
{
    if (object->flag & SW_CHAN_LOCK)
    {
        object->lock.free(&object->lock);
    }
    if (object->flag & SW_CHAN_NOTIFY)
    {
        object->notify_fd.close(&object->notify_fd);
    }
    if (object->flag & SW_CHAN_SHM)
    {
        sw_shm_free(object);
    }
    else
    {
        sw_free(object);
    }
}

/**
 * pop data (lock)
 */
int swChannel_pop(swChannel *object, void *out, int buffer_length)
{
    assert(object->flag & SW_CHAN_LOCK);
    object->lock.lock(&object->lock);
    int n = swChannel_out(object, out, buffer_length);
    object->lock.unlock(&object->lock);
    return n;
}

void swChannel_print(swChannel *chan)
{
    printf("swChannel\n{\n"
            "    off_t head = %ld;\n"
            "    off_t tail = %ld;\n"
            "    size_t size = %ld;\n"
            "    char head_tag = %d;\n"
            "    char tail_tag = %d;\n"
            "    int num = %d;\n"
            "    size_t bytes = %ld;\n"
            "    int flag = %d;\n"
            "    int maxlen = %d;\n"
            "\n}\n", (long)chan->head, (long)chan->tail, chan->size, chan->tail_tag, chan->head_tag, chan->num, chan->bytes,
            chan->flag, chan->maxlen);
}

