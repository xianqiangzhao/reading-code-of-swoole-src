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

static int swReactorTimer_init(long msec);
static int swReactorTimer_set(swTimer *timer, long exec_msec);
static swTimer_node* swTimer_add(swTimer *timer, int _msec, int interval, void *data, swTimerCallback callback);

//把当前时间放到time中
int swTimer_now(struct timeval *time)
{
#if defined(SW_USE_MONOTONIC_TIME) && defined(CLOCK_MONOTONIC)
    struct timespec _now;
    if (clock_gettime(CLOCK_MONOTONIC, &_now) < 0)
    {
        swSysError("clock_gettime(CLOCK_MONOTONIC) failed.");
        return SW_ERR;
    }
    time->tv_sec = _now.tv_sec;
    time->tv_usec = _now.tv_nsec / 1000;
#else
    if (gettimeofday(time, NULL) < 0)
    {
        swSysError("gettimeofday() failed.");
        return SW_ERR;
    }
#endif
    return SW_OK;
}

//现在时间 - basetime时间  (swTimer_init 函数)
//定时器时间矫正
static sw_inline int64_t swTimer_get_relative_msec()
{
    struct timeval now;
    if (swTimer_now(&now) < 0)
    {
        return SW_ERR;
    }
    //1,000,000 微秒 = 1秒  = 1000ms
    //转为毫秒
    int64_t msec1 = (now.tv_sec - SwooleG.timer.basetime.tv_sec) * 1000;
    int64_t msec2 = (now.tv_usec - SwooleG.timer.basetime.tv_usec) / 1000;
    return msec1 + msec2;
}

//time 初期化
/*

struct _swTimer
{
     swHeap *heap;
    swHashMap *map;
    int num;
    int use_pipe;
    int lasttime;
    int fd;
    long _next_id;
    long _current_id;
    long _next_msec;
    swPipe pipe;
    struct timeval basetime; //当前时间
    int (*set)(swTimer *timer, long exec_msec);
    swTimer_node* (*add)(swTimer *timer, int _msec, int persistent, void *data, swTimerCallback callback);
};
typedef struct _swHeap
{
    uint32_t num;
    uint32_t size;
    uint8_t type;
    swHeap_node **nodes;
} swHeap;

typedef struct swHeap_node
{
    uint64_t priority;
    uint32_t position;
    void *data;
} swHeap_node;


typedef struct
{
    struct swHashMap_node *root;
    struct swHashMap_node *iterator;
    swHashMap_dtor dtor;
} swHashMap;


typedef struct swHashMap_node
{
    uint64_t key_int;
    char *key_str;
    void *data;
    UT_hash_handle hh;
} swHashMap_node;

*/
int swTimer_init(long msec)
{
    if (swTimer_now(&SwooleG.timer.basetime) < 0)
    {
        return SW_ERR;
    }

    //最小堆
    SwooleG.timer.heap = swHeap_new(1024, SW_MIN_HEAP);
    if (!SwooleG.timer.heap)
    {
        return SW_ERR;
    }
    //hashmap  SW_HASHMAP_INIT_BUCKET_N =32
    /*
    typedef struct
    {
        struct swHashMap_node *root;
        struct swHashMap_node *iterator;
        swHashMap_dtor dtor;
    } swHashMap;
    */
    SwooleG.timer.map = swHashMap_new(SW_HASHMAP_INIT_BUCKET_N, NULL);
    if (!SwooleG.timer.map)
    {
        swHeap_free(SwooleG.timer.heap);
        SwooleG.timer.heap = NULL;
        return SW_ERR;
    }

    SwooleG.timer._current_id = -1;
    //定时时间，第一次会设定，第二次的swoole_tick 定时器会在swTimer_add 中有个比较，小于第一次设定的话，就用最小的那个值，这个值用在epoll_wait 时间
    SwooleG.timer._next_msec = msec;

    SwooleG.timer._next_id = 1;
    SwooleG.timer.add = swTimer_add;//追加定时器回调函数

    if (swIsTaskWorker())//task 进程
    {   //创建eventfd 
        swSystemTimer_init(msec, SwooleG.use_timer_pipe);
    }
    else //非task 进程 
    {   //设定timer epoll wait 时间就是定时器执行时间
        swReactorTimer_init(msec);
    }

    return SW_OK;
}

void swTimer_free(swTimer *timer)
{
    if (timer->heap)
    {
        swHeap_free(timer->heap);
    }
}

//非task 进程  定时器初始化
static int swReactorTimer_init(long exec_msec)
{
    SwooleG.main_reactor->check_timer = SW_TRUE;
    SwooleG.main_reactor->timeout_msec = exec_msec;
    SwooleG.timer.set = swReactorTimer_set;
    SwooleG.timer.fd = -1;
    return SW_OK;
}

static int swReactorTimer_set(swTimer *timer, long exec_msec)
{
    SwooleG.main_reactor->timeout_msec = exec_msec;
    return SW_OK;
}

//定时器增加  SwooleG.timer.add 回调函数
//调用原型  SwooleG.timer.add(&SwooleG.timer, ms, persistent, cb, timer_func)

static swTimer_node* swTimer_add(swTimer *timer, int _msec, int interval, void *data, swTimerCallback callback)
{
    /*
    typedef struct _swHeap
    {
        uint32_t num;
        uint32_t size;
        uint8_t type;
        swHeap_node **nodes;
    } swHeap;
    typedef struct swHeap_node
    {
        uint64_t priority;
        uint32_t position;
        void *data;
    } swHeap_node;

    struct _swTimer_node
    {
        swHeap_node *heap_node;
        void *data;
        swTimerCallback callback;
        int64_t exec_msec;
        uint32_t interval;
        long id;
        int type;                 //0 normal node 1 node for client_coro
        uint8_t remove;
    };
    */
    swTimer_node *tnode = sw_malloc(sizeof(swTimer_node));
    if (!tnode)
    {
        swSysError("malloc(%ld) failed.", sizeof(swTimer_node));
        return NULL;
    }
    //取得现在时间 - basetime 毫秒单位
    int64_t now_msec = swTimer_get_relative_msec();
    if (now_msec < 0)
    {
        sw_free(tnode);
        return NULL;
    }

    tnode->data = data;//data 中是一个 swTimer_callback 结构体，里面有php 回调函数
    tnode->type = SW_TIMER_TYPE_KERNEL;
    tnode->exec_msec = now_msec + _msec;//_msec 是定时器时间  now_msec 是 now - basetime
    tnode->interval = interval ? _msec : 0;//定时器触发间隔时间
    tnode->remove = 0;
    tnode->callback = callback;
    //当前epoll 时间值 > 要追加的时间值的话，重新设定epoll wait 时间
    if (timer->_next_msec < 0 || timer->_next_msec > _msec)
    {   //回调swReactorTimer_set，进行epoll 等待时间设定
        timer->set(timer, _msec);
        timer->_next_msec = _msec;
    }

    tnode->id = timer->_next_id++; //timer id
    if (unlikely(tnode->id < 0))
    {
        tnode->id = 1;
        timer->_next_id = 2;
    }
    timer->num++;//定时器个数+1
    //tnode 放到 timer->heap中
    tnode->heap_node = swHeap_push(timer->heap, tnode->exec_msec, tnode);
    if (tnode->heap_node == NULL)
    {
        sw_free(tnode);
        return NULL;
    }
    swHashMap_add_int(timer->map, tnode->id, tnode);
    return tnode;
}

int swTimer_del(swTimer *timer, swTimer_node *tnode)
{
    if (tnode->remove)
    {
        return SW_FALSE;
    }
    if (SwooleG.timer._current_id > 0 && tnode->id == SwooleG.timer._current_id)
    {
        tnode->remove = 1;
        return SW_TRUE;
    }
    if (swHashMap_del_int(timer->map, tnode->id) < 0)
    {
        return SW_ERR;
    }
    if (tnode->heap_node)
    {
        //remove from min-heap
        swHeap_remove(timer->heap, tnode->heap_node);
        sw_free(tnode->heap_node);
    }
    sw_free(tnode);
    timer->num --;
    return SW_TRUE;
}

//定时器到期执行回调函数
int swTimer_select(swTimer *timer)
{   
    //now - basetime
    int64_t now_msec = swTimer_get_relative_msec();
    if (now_msec < 0)
    {
        return SW_ERR;
    }

    swTimer_node *tnode = NULL;
    swHeap_node *tmp;
    long timer_id;

    while ((tmp = swHeap_top(timer->heap)))//循环定时器堆
    {
        tnode = tmp->data;//取出timer_node ，这个结构体中有定时器设置
        if (tnode->exec_msec > now_msec)//检查时间 
        {
            break;
        }

        timer_id = timer->_current_id = tnode->id;
        if (!tnode->remove)
        {
            //回调执行 
            // swoole_timer_tick 时调用  php_swoole_onInterval   swoole_timer_after 时调用 php_swoole_onTimeout
            tnode->callback(timer, tnode);
        }
        timer->_current_id = -1;

        //persistent timer
        if (tnode->interval > 0 && !tnode->remove)
        {
            while (tnode->exec_msec <= now_msec)
            {
                tnode->exec_msec += tnode->interval;
            }
            swHeap_change_priority(timer->heap, tnode->exec_msec, tmp);
            continue;
        }

        timer->num--;//定时器个数减少
        swHeap_pop(timer->heap);
        swHashMap_del_int(timer->map, timer_id);
        sw_free(tnode);
    }

    if (!tnode || !tmp)
    {
        timer->_next_msec = -1;
        timer->set(timer, -1);
    }
    else
    {
        timer->set(timer, tnode->exec_msec - now_msec);
    }
    return SW_OK;
}
