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
#include "connection.h"
#include "async.h"
#include "server.h"

#ifdef SW_USE_MALLOC_TRIM
#ifdef __APPLE__
#include <sys/malloc.h>
#else
#include <malloc.h>
#endif
#endif

#ifdef SW_COROUTINE
#include "coroutine.h"
#endif

static void swReactor_onTimeout_and_Finish(swReactor *reactor);
static void swReactor_onTimeout(swReactor *reactor);
static void swReactor_onFinish(swReactor *reactor);
static void swReactor_onBegin(swReactor *reactor);
static int swReactor_defer(swReactor *reactor, swCallback callback, void *data);

//创建reactor
//reactor是一种事件驱动体系结构 
//https://www.jianshu.com/p/eef7ebe28673
int swReactor_create(swReactor *reactor, int max_event)
{
    int ret;
    bzero(reactor, sizeof(swReactor));

#ifdef HAVE_EPOLL
    ret = swReactorEpoll_create(reactor, max_event); //create epoll事件模型
#elif defined(HAVE_KQUEUE)
    ret = swReactorKqueue_create(reactor, max_event);
#elif defined(HAVE_POLL)
    ret = swReactorPoll_create(reactor, max_event);
#else
    ret = swReactorSelect_create(reactor);
#endif
sizeof(swConnection)
    reactor->running = 1; // 为1是事件处理启动，为0的话事件处理停止 epoll wait 循环的判断条件
    //注册回调函数
    reactor->setHandle = swReactor_setHandle;//设定事件回调函数方法

    reactor->onFinish = swReactor_onFinish;
    reactor->onTimeout = swReactor_onTimeout;

    reactor->write = swReactor_write;
    reactor->defer = swReactor_defer;
    reactor->close = swReactor_close;

    reactor->socket_array = swArray_new(1024, sizeof(swConnection));//分配 1024 * sizeof(swConnection) 内存空间
    if (!reactor->socket_array)
    {
        swWarn("create socket array failed.");
        return SW_ERR;
    }

    return ret;
}

//设置事件类型与相应的回调函数
int swReactor_setHandle(swReactor *reactor, int _fdtype, swReactor_handle handle)
{
    int fdtype = swReactor_fdtype(_fdtype);//取得fdtype

    if (fdtype >= SW_MAX_FDTYPE) // SW_MAX_FDTYPE= 32
    {
        swWarn("fdtype > SW_MAX_FDTYPE[%d]", SW_MAX_FDTYPE);
        return SW_ERR;
    }

    if (swReactor_event_read(_fdtype))//_fdtype  < 256 || fdtype & SW_EVENT_READ 时为read 事件
    {
        reactor->handle[fdtype] = handle;//保存为读handle
    }
    else if (swReactor_event_write(_fdtype))//fdtype & SW_EVENT_WRITE （100000000000）
    {
        reactor->write_handle[fdtype] = handle;//保存为写handle
    }
    else if (swReactor_event_error(_fdtype))
    {
        reactor->error_handle[fdtype] = handle;//保存为错误handle
    }
    else
    {
        swWarn("unknow fdtype");
        return SW_ERR;
    }

    return SW_OK;
}

static void swReactor_defer_timer_callback(swTimer *timer, swTimer_node *tnode)
{
    swDefer_callback *cb = (swDefer_callback *) tnode->data;
    cb->callback(cb->data);
    sw_free(cb);
}


static int swReactor_defer(swReactor *reactor, swCallback callback, void *data)
{
    swDefer_callback *cb = sw_malloc(sizeof(swDefer_callback));
    if (!cb)
    {
        swWarn("malloc(%ld) failed.", sizeof(swDefer_callback));
        return SW_ERR;
    }
    cb->callback = callback;
    cb->data = data;
    if (unlikely(reactor->start == 0))
    {
        if (unlikely(SwooleG.timer.fd == 0))
        {
            swTimer_init(1);
        }
        SwooleG.timer.add(&SwooleG.timer, 1, 0, cb, swReactor_defer_timer_callback);
    }
    else
    {
        LL_APPEND(reactor->defer_tasks, cb);
    }
    return SW_OK;
}

int swReactor_empty(swReactor *reactor)
{
    //timer
    if (SwooleG.timer.num > 0)
    {
        return SW_FALSE;
    }

    int empty = SW_FALSE;
    //thread pool
    if (SwooleAIO.init && reactor->event_num == 1 && SwooleAIO.task_num == 0)
    {
        empty = SW_TRUE;
    }
    //no event
    else if (reactor->event_num == 0)
    {
        empty = SW_TRUE;
    }
    //coroutine
    if (reactor->can_exit && !reactor->can_exit(reactor))
    {
        empty = SW_FALSE;
    }
    return empty;
}

/**
 * execute when reactor timeout and reactor finish
 */
//定时器到期执行
static void swReactor_onTimeout_and_Finish(swReactor *reactor)
{
    //check timer
    //swReactorTimer_init 时設置为true
    if (reactor->check_timer)
    {
        swTimer_select(&SwooleG.timer);
    }
    //defer tasks
    do
    {
        //下一个事件循环开始时执行函数回调
        swDefer_callback *defer_tasks = reactor->defer_tasks;
        swDefer_callback *cb, *tmp;
        reactor->defer_tasks = NULL;
        LL_FOREACH(defer_tasks, cb)
        {
            cb->callback(cb->data);
        }
        LL_FOREACH_SAFE(defer_tasks, cb, tmp)
        {
            sw_free(cb);
        }
    } while (reactor->defer_tasks);

    //callback at the end
    //每一轮事件循环结束时调用
    if (reactor->idle_task.callback)
    {
        reactor->idle_task.callback(reactor->idle_task.data);
    }
    //server worker
    swWorker *worker = SwooleWG.worker;
    if (worker != NULL)
    {
        if (SwooleWG.wait_exit == 1) //swWorker_stop 函數会设置为1 ，意味着worker进程退出
        {
            swWorker_try_to_exit();
        }
    }
    //not server, the event loop is empty
    if (SwooleG.serv == NULL && swReactor_empty(reactor))
    {
        reactor->running = 0;
    }

#ifdef SW_USE_MALLOC_TRIM
    if (SwooleG.serv && reactor->last_malloc_trim_time < SwooleG.serv->gs->now - SW_MALLOC_TRIM_INTERVAL)
    {
        malloc_trim(SW_MALLOC_TRIM_PAD);
        reactor->last_malloc_trim_time = SwooleG.serv->gs->now;
    }
#endif
}
//swoole_tick 定时器到期回调函数
static void swReactor_onTimeout(swReactor *reactor)
{   //定时器回调函数执行等
    swReactor_onTimeout_and_Finish(reactor);

    if (reactor->disable_accept)//假如当前状态是 不能接收请求则设定为运行接收请求
    {
        reactor->enable_accept(reactor);//swServer_enable_accept
        reactor->disable_accept = 0;
    }
}

//事件循环最后执行该函数
static void swReactor_onFinish(swReactor *reactor)
{
    //check signal
    if (reactor->singal_no)
    {
        swSignal_callback(reactor->singal_no);
        reactor->singal_no = 0;
    }
    swReactor_onTimeout_and_Finish(reactor);
}

void swReactor_activate_future_task(swReactor *reactor)
{
    reactor->onBegin = swReactor_onBegin;
}

static void swReactor_onBegin(swReactor *reactor)
{
    if (reactor->future_task.callback)
    {
        reactor->future_task.callback(reactor->future_task.data);
    }
}

int swReactor_close(swReactor *reactor, int fd)
{
    swConnection *socket = swReactor_get(reactor, fd);
    if (socket->out_buffer)
    {
        swBuffer_free(socket->out_buffer);
    }
    if (socket->in_buffer)
    {
        swBuffer_free(socket->in_buffer);
    }
    if (socket->websocket_buffer)
    {
        swString_free(socket->websocket_buffer);
    }
    bzero(socket, sizeof(swConnection));
    socket->removed = 1;
    swTraceLog(SW_TRACE_CLOSE, "fd=%d.", fd);
    return close(fd);
}

//发送数据到 管道描述符 fd 
int swReactor_write(swReactor *reactor, int fd, void *buf, int n)
{
    int ret;
    swConnection *socket = swReactor_get(reactor, fd);
    swBuffer *buffer = socket->out_buffer;

    if (socket->fd == 0)
    {
        socket->fd = fd;
    }

    if (socket->buffer_size == 0)
    {
        // 默认 8M  buffer_output_size用于设置单次最大发送长度。socket_buffer_size用于设置客户端连接最大允许占用内存数量
        socket->buffer_size = SwooleG.socket_buffer_size;

    }

    if (socket->nonblock == 0) //如是非阻塞，设置为非阻塞
    {
        swoole_fcntl_set_option(fd, 1, -1);
        socket->nonblock = 1;
    }

    if (n > socket->buffer_size) //发送数据大小 大于缓存内存数量就报错
    {
        swoole_error_log(SW_LOG_WARNING, SW_ERROR_PACKAGE_LENGTH_TOO_LARGE, "data is too large, cannot exceed buffer size.");
        return SW_ERR;
    }

    if (swBuffer_empty(buffer)) //如果buffer 为空，第一次进来是空
    {
        if (socket->ssl_send)
        {
            goto do_buffer;
        }

        do_send:
        ret = swConnection_send(socket, buf, n, 0);//发送数据 最后一个参数0意味着非阻塞发送

        if (ret > 0)//发送数据大于0
        {
            if (n == ret) //一次发送完毕数据，就返回发送的size并退出
            {
                return ret;
            }
            else//一次发送没有完全把buf 中的数据发送完毕，buf 位置偏移， n 设置为剩余的数据size
            {
                buf += ret;
                n -= ret;
                goto do_buffer; //跳转到buffer 发送处理中
            }
        }
        else if (swConnection_error(errno) == SW_WAIT)//发送没有成功返回等待错误的话，把数据放到buffer中
        {
            do_buffer:
            if (!socket->out_buffer)//第一次out_buffer = null
            {
                buffer = swBuffer_new(sizeof(swEventData)); //申请buffer size = sizeof(swEventData)
                if (!buffer)
                {
                    swWarn("create worker buffer failed.");
                    return SW_ERR;
                }
                socket->out_buffer = buffer; //把新申请的buffer 赋值给socket->out_buffer 
            }

            socket->events |= SW_EVENT_WRITE;//增加 write 事件

            /*
                reactor->add = swReactorEpoll_add;
                reactor->set = swReactorEpoll_set;
                reactor->del = swReactorEpoll_del;
                reactor->wait = swReactorEpoll_wait;
                reactor->free = swReactorEpoll_free;
            */
            if (socket->events & SW_EVENT_READ) //原事件中有读事件的话，就重新set
            {
                if (reactor->set(reactor, fd, socket->fdtype | socket->events) < 0)
                {
                    swSysError("reactor->set(%d, SW_EVENT_WRITE) failed.", fd);
                }
            }
            else

            {   
                /*
                没有读事件就增加write事件，相应handle 就是swReactor_onWrite 函数
                定义在swoole_event.c 中
                SwooleG.main_reactor->setHandle(SwooleG.main_reactor, SW_FD_WRITE, swReactor_onWrite);
                */
                if (reactor->add(reactor, fd, socket->fdtype | SW_EVENT_WRITE) < 0)
                {
                    swSysError("reactor->add(%d, SW_EVENT_WRITE) failed.", fd);
                }
            }

            goto append_buffer;
        }
        else if (errno == EINTR)//返回中断信号，在重试的情况，重新发送
        {
            goto do_send;
        }
        else //其它的情况有错误
        {
            SwooleG.error = errno;
            return SW_ERR;
        }
    }
    else //有缓冲数据时
    {
        //buffer 中的数据size > socket 的buffer size（默认8M）的情况,就需要等待
        append_buffer: if (buffer->length > socket->buffer_size)
        {
            if (socket->dontwait) //不等待的话，报错
            {
                SwooleG.error = SW_ERROR_OUTPUT_BUFFER_OVERFLOW;
                return SW_ERR;
            }
            else
            {   //output buffer overflow
                swoole_error_log(SW_LOG_WARNING, SW_ERROR_OUTPUT_BUFFER_OVERFLOW, "socket#%d output buffer overflow.", fd);
                /* 让出cpu
                  http://man7.org/linux/man-pages/man2/sched_yield.2.html
                   ched_yield() causes the calling thread to relinquish the CPU.  The
                   thread is moved to the end of the queue for its static priority and a
                   new thread gets to run.
                */
                swYield();
                // SW_SOCKET_OVERFLOW_WAIT  100 
                swSocket_wait(fd, SW_SOCKET_OVERFLOW_WAIT, SW_EVENT_WRITE);
            }
        }

        if (swBuffer_append(buffer, buf, n) < 0)//第一次增加数据会进来，增加到buffer中
        {
            return SW_ERR;
        }
    }
    return SW_OK;
}

//一次没有发送成功，还有数据在buffer 中时,epoll 中发现该描述符可写时，调用本函数
int swReactor_onWrite(swReactor *reactor, swEvent *ev)
{
    int ret;
    int fd = ev->fd;

    swConnection *socket = swReactor_get(reactor, fd);
    swBuffer_chunk *chunk = NULL;
    swBuffer *buffer = socket->out_buffer;

    //send to socket
    while (!swBuffer_empty(buffer))
    {
        chunk = swBuffer_get_chunk(buffer);
        if (chunk->type == SW_CHUNK_CLOSE)
        {
            close_fd:
            reactor->close(reactor, ev->fd);
            return SW_OK;
        }
        else if (chunk->type == SW_CHUNK_SENDFILE)
        {
            ret = swConnection_onSendfile(socket, chunk);
        }
        else
        {
            ret = swConnection_buffer_send(socket);
        }

        if (ret < 0)
        {
            if (socket->close_wait)
            {
                goto close_fd;
            }
            else if (socket->send_wait)
            {
                return SW_OK;
            }
        }
    }

    //remove EPOLLOUT event
    if (swBuffer_empty(buffer))
    {
        if (socket->events & SW_EVENT_READ)
        {
            socket->events &= (~SW_EVENT_WRITE);
            if (reactor->set(reactor, fd, socket->fdtype | socket->events) < 0)
            {
                swSysError("reactor->set(%d, SW_EVENT_READ) failed.", fd);
            }
        }
        else
        {
            if (reactor->del(reactor, fd) < 0)
            {
                swSysError("reactor->del(%d) failed.", fd);
            }
        }
    }

    return SW_OK;
}

int swReactor_wait_write_buffer(swReactor *reactor, int fd)
{
    swConnection *conn = swReactor_get(reactor, fd);
    swEvent event;

    if (conn->out_buffer)
    {
        swSetBlock(fd);
        event.fd = fd;
        return swReactor_onWrite(reactor, &event);
    }
    return SW_OK;
}
