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
#include "buffer.h"

/**
 * create new buffer
 */


/*

typedef struct _swBuffer_chunk
{
    uint32_t type;
    uint32_t length;
    uint32_t offset;
    union
    {
        void *ptr;
        struct
        {
            uint32_t val1;
            uint32_t val2;
        } data;
    } store;
    uint32_t size;
    void (*destroy)(struct _swBuffer_chunk *chunk);
    struct _swBuffer_chunk *next;
} swBuffer_chunk;

typedef struct _swBuffer
{
    int fd;
    uint8_t chunk_num; //chunk数量
    uint16_t chunk_size;
    uint32_t length;
    swBuffer_chunk *head;
    swBuffer_chunk *tail;
} swBuffer;
*/
swBuffer* swBuffer_new(int chunk_size)
{
    swBuffer *buffer = sw_malloc(sizeof(swBuffer));
    if (buffer == NULL)
    {
        swWarn("malloc for buffer failed. Error: %s[%d]", strerror(errno), errno);
        return NULL;
    }

    bzero(buffer, sizeof(swBuffer));
    buffer->chunk_size = chunk_size;

    return buffer;
}

/**
 * create new chunk
 */
//建立chunk 
swBuffer_chunk *swBuffer_new_chunk(swBuffer *buffer, uint32_t type, uint32_t size)
{   
    //申请一个buffer 结构
    swBuffer_chunk *chunk = sw_malloc(sizeof(swBuffer_chunk));
    if (chunk == NULL)
    {
        swWarn("malloc for chunk failed. Error: %s[%d]", strerror(errno), errno);
        return NULL;
    }
    //置0
    bzero(chunk, sizeof(swBuffer_chunk));

    //require alloc memory
    //分配 size 的内存给chunk->store.ptr
    if (type == SW_CHUNK_DATA && size > 0)
    {   
        void *buf = sw_malloc(size);
        if (buf == NULL)
        {
            swWarn("malloc(%d) for data failed. Error: %s[%d]", size, strerror(errno), errno);
            sw_free(chunk);
            return NULL;
        }
        chunk->size = size;
        chunk->store.ptr = buf;
    }

    chunk->type = type;
    buffer->chunk_num ++; //buffer 中的chunk 个数增加

    //把 新申请的chunk 挂载到buffer 链表中
    if (buffer->head == NULL)
    {
        buffer->tail = buffer->head = chunk;
    }
    else
    {
        buffer->tail->next = chunk;
        buffer->tail = chunk;
    }

    return chunk;//返回新建立的chunk
}

/**
 * pop the head chunk
 */
void swBuffer_pop_chunk(swBuffer *buffer, swBuffer_chunk *chunk)
{
    if (chunk->next == NULL)
    {
        buffer->head = NULL;
        buffer->tail = NULL;
        buffer->length = 0;
        buffer->chunk_num = 0;
    }
    else
    {
        buffer->head = chunk->next;
        buffer->length -= chunk->length;
        buffer->chunk_num--;
    }
    if (chunk->type == SW_CHUNK_DATA)
    {
        sw_free(chunk->store.ptr);
    }
    if (chunk->destroy)
    {
        chunk->destroy(chunk);
    }
    sw_free(chunk);
}

/**
 * free buffer
 */
int swBuffer_free(swBuffer *buffer)
{
    volatile swBuffer_chunk *chunk = buffer->head;
    void * *will_free_chunk;  //free the point
    while (chunk != NULL)
    {
        if (chunk->type == SW_CHUNK_DATA)
        {
            sw_free(chunk->store.ptr);
        }
        will_free_chunk = (void *) chunk;
        chunk = chunk->next;
        sw_free(will_free_chunk);
    }
    sw_free(buffer);
    return SW_OK;
}

/**
 * append to buffer queue
 */
//向 buffer 缓存中增加数据
int swBuffer_append(swBuffer *buffer, void *data, uint32_t size)
{   

    /*
    typedef struct _swBuffer_chunk
    {
        uint32_t type;
        uint32_t length;
        uint32_t offset;
        union
        {
            void *ptr;
            struct
            {
                uint32_t val1;
                uint32_t val2;
            } data;
        } store;
        uint32_t size;
        void (*destroy)(struct _swBuffer_chunk *chunk);
        struct _swBuffer_chunk *next;
    } swBuffer_chunk;
    */
    swBuffer_chunk *chunk = swBuffer_new_chunk(buffer, SW_CHUNK_DATA, size);
    if (chunk == NULL)
    {
        return SW_ERR;
    }

    buffer->length += size;//buffer 总size 增加
    chunk->length = size;

    memcpy(chunk->store.ptr, data, size);//把数据放到 chunk->store.ptr

    swTraceLog(SW_TRACE_BUFFER, "chunk_n=%d|size=%d|chunk_len=%d|chunk=%p", buffer->chunk_num, size,
            chunk->length, chunk);

    return SW_OK;
}

/**
 * print buffer
 */
void swBuffer_debug(swBuffer *buffer, int print_data)
{
    int i = 0;
    volatile swBuffer_chunk *chunk = buffer->head;
    printf("%s\n%s\n", SW_START_LINE, __func__);
    while (chunk != NULL)
    {
        i++;
        printf("%d.\tlen=%d", i, chunk->length);
        if (print_data)
        {
            printf("\tdata=%s", (char *) chunk->store.ptr);
        }
        printf("\n");
        chunk = chunk->next;
    }
    printf("%s\n%s\n", SW_END_LINE, __func__);
}
