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

#ifdef HAVE_EPOLL
#include <sys/epoll.h>
#ifndef EPOLLRDHUP
#error "require linux kernel version 2.6.32 or later."
#endif

#ifndef EPOLLONESHOT
#error "require linux kernel version 2.6.32 or later."
#endif

typedef struct swReactorEpoll_s swReactorEpoll;

typedef struct _swFd
{
    uint32_t fd;
    uint32_t fdtype;
} swFd;

static int swReactorEpoll_add(swReactor *reactor, int fd, int fdtype);
static int swReactorEpoll_set(swReactor *reactor, int fd, int fdtype);
static int swReactorEpoll_del(swReactor *reactor, int fd);
static int swReactorEpoll_wait(swReactor *reactor, struct timeval *timeo);
static void swReactorEpoll_free(swReactor *reactor);

//根据fdtype，设定flag（epoll 监听读，写），并返回
static sw_inline int swReactorEpoll_event_set(int fdtype)
{
    uint32_t flag = 0;
    if (swReactor_event_read(fdtype))
    {
        flag |= EPOLLIN; //读
    }
    if (swReactor_event_write(fdtype))
    {
        flag |= EPOLLOUT;//写
    }
    if (fdtype & SW_EVENT_ONCE) //只监听一次事件 如果还需要继续监听这个socket的话，需要再次把这个socket加入到EPOLL队列里 
    {
        flag |= EPOLLONESHOT;
    }
    if (swReactor_event_error(fdtype))//event error https://blog.csdn.net/q576709166/article/details/8649911
    {
        //flag |= (EPOLLRDHUP);
        flag |= (EPOLLRDHUP | EPOLLHUP | EPOLLERR);
    }
    return flag;
}

struct swReactorEpoll_s
{
    int epfd; //fd
    struct epoll_event *events; //event
};

//参数max_event_num 没有什么意义
int swReactorEpoll_create(swReactor *reactor, int max_event_num)
{
    //create reactor object
    swReactorEpoll *reactor_object = sw_malloc(sizeof(swReactorEpoll));
    if (reactor_object == NULL)
    {
        swWarn("malloc[0] failed.");
        return SW_ERR;
    }
    bzero(reactor_object, sizeof(swReactorEpoll));
    reactor->object = reactor_object;// void *object 类型
    reactor->max_event_num = max_event_num;

    reactor_object->events = sw_calloc(max_event_num, sizeof(struct epoll_event));//分配 max_event_num* epoll_event 内存

    if (reactor_object->events == NULL)
    {
        swWarn("malloc[1] failed.");
        sw_free(reactor_object);
        return SW_ERR;
    }
    //epoll create
    //http://www.man7.org/linux/man-pages/man2/epoll_create.2.html
    reactor_object->epfd = epoll_create(512);//调用 epoll系统函数 epoll_create ，返回描述符
    if (reactor_object->epfd < 0)
    {
        swWarn("epoll_create failed. Error: %s[%d]", strerror(errno), errno);
        sw_free(reactor_object);
        return SW_ERR;
    }
    //binding method
    reactor->add = swReactorEpoll_add;
    reactor->set = swReactorEpoll_set;
    reactor->del = swReactorEpoll_del;
    reactor->wait = swReactorEpoll_wait;
    reactor->free = swReactorEpoll_free;

    return SW_OK;
}

//释放epoll 
static void swReactorEpoll_free(swReactor *reactor)
{
    swReactorEpoll *object = reactor->object;
    close(object->epfd);
    sw_free(object->events);
    sw_free(object);
}

//监听事件追加
static int swReactorEpoll_add(swReactor *reactor, int fd, int fdtype)
{
    swReactorEpoll *object = reactor->object;
    struct epoll_event e;
    swFd fd_;
    bzero(&e, sizeof(struct epoll_event));

    fd_.fd = fd;
    fd_.fdtype = swReactor_fdtype(fdtype);//取得监听的事件类型 读，写
    e.events = swReactorEpoll_event_set(fdtype);//event flag 设置

    swReactor_add(reactor, fd, fdtype);//fd 相关联的socket 绑定事件类型

    /*
        typedef union epoll_data { 
        void *ptr; 
        int fd; 
        uint32_t u32; 
        uint64_t u64; 
        } epoll_data_t;

        struct epoll_event { 
        uint32_t events; /* Epoll events  
        epoll_data_t data; /* User data variable  
        }; 
     

    typedef struct _swFd
    {
        uint32_t fd;
        uint32_t fdtype;
    } swFd;
    */
    memcpy(&(e.data.u64), &fd_, sizeof(fd_));//fd_ copy to e.data.u64
    if (epoll_ctl(object->epfd, EPOLL_CTL_ADD, fd, &e) < 0) //增加监听
    {
        swSysError("add events[fd=%d#%d, type=%d, events=%d] failed.", fd, reactor->id, fd_.fdtype, e.events);
        swReactor_del(reactor, fd);
        return SW_ERR;
    }

    swTraceLog(SW_TRACE_EVENT, "add event[reactor_id=%d, fd=%d, events=%d]", reactor->id, fd, swReactor_events(fdtype));
    reactor->event_num++;

    return SW_OK;
}

//删除监听
static int swReactorEpoll_del(swReactor *reactor, int fd)
{
    swReactorEpoll *object = reactor->object;
    if (epoll_ctl(object->epfd, EPOLL_CTL_DEL, fd, NULL) < 0)//删除监听fd
    {
        swSysError("epoll remove fd[%d#%d] failed.", fd, reactor->id);
        return SW_ERR;
    }

    swTraceLog(SW_TRACE_REACTOR, "remove event[reactor_id=%d|fd=%d]", reactor->id, fd);
    reactor->event_num = reactor->event_num <= 0 ? 0 : reactor->event_num - 1;
    swReactor_del(reactor, fd);

    return SW_OK;
}

//修改监听设置
static int swReactorEpoll_set(swReactor *reactor, int fd, int fdtype)
{
    swReactorEpoll *object = reactor->object;
    swFd fd_;
    struct epoll_event e;
    int ret;

    bzero(&e, sizeof(struct epoll_event));
    e.events = swReactorEpoll_event_set(fdtype);

    if (e.events & EPOLLOUT)//写事件时 fd >2 
    {
        assert(fd > 2);
    }

    fd_.fd = fd;
    fd_.fdtype = swReactor_fdtype(fdtype);
    memcpy(&(e.data.u64), &fd_, sizeof(fd_));

    ret = epoll_ctl(object->epfd, EPOLL_CTL_MOD, fd, &e);
    if (ret < 0)
    {
        swSysError("reactor#%d->set(fd=%d|type=%d|events=%d) failed.", reactor->id, fd, fd_.fdtype, e.events);
        return SW_ERR;
    }
    swTraceLog(SW_TRACE_EVENT, "set event[reactor_id=%d, fd=%d, events=%d]", reactor->id, fd, swReactor_events(fdtype));
    //execute parent method
    swReactor_set(reactor, fd, fdtype);
    return SW_OK;
}

//事件等待
//第二个参数时时间
static int swReactorEpoll_wait(swReactor *reactor, struct timeval *timeo)
{
    swEvent event;
    swReactorEpoll *object = reactor->object;
    swReactor_handle handle;
    int i, n, ret, msec;

    int reactor_id = reactor->id;
    int epoll_fd = object->epfd;
    int max_event_num = reactor->max_event_num;
    struct epoll_event *events = object->events;

    if (reactor->timeout_msec == 0)
    {
        if (timeo == NULL)
        {
            reactor->timeout_msec = -1;
        }
        else
        {
            reactor->timeout_msec = timeo->tv_sec * 1000 + timeo->tv_usec / 1000;
        }
    }

    reactor->start = 1;

    while (reactor->running > 0) //事件模型启动flag
    {
        if (reactor->onBegin != NULL)
        {
            reactor->onBegin(reactor);
        }
        msec = reactor->timeout_msec;
        n = epoll_wait(epoll_fd, events, max_event_num, msec);//wait 事件发生
        if (n < 0) //有错误发生
        {
            if (swReactor_error(reactor) < 0) //error 错误不是中断引起的话，就调用错误处理函数
            {
                swWarn("[Reactor#%d] epoll_wait failed. Error: %s[%d]", reactor_id, strerror(errno), errno);
                return SW_ERR;
            }
            else
            {
                continue;
            }
        }
        else if (n == 0)
        {
            if (reactor->onTimeout != NULL)
            {
                reactor->onTimeout(reactor);//执行定时处理
            }
            continue;
        }
        for (i = 0; i < n; i++)//有事件发生
        {
            event.fd = events[i].data.u64;//事件发生的描述符
            event.from_id = reactor_id;//来自于哪个reactor
            //事件类型  events[i].data.u64 中存放  uint32_t fd ，uint32_t fdtype;
            //所以左移动32位就是fdtype
            event.type = events[i].data.u64 >> 32;
            event.socket = swReactor_get(reactor, event.fd);

            //read
            if ((events[i].events & EPOLLIN) && !event.socket->removed)
            {   //取得handle
                handle = swReactor_getHandle(reactor, SW_EVENT_READ, event.type);
                ret = handle(reactor, &event);
                if (ret < 0)
                {
                    swSysError("EPOLLIN handle failed. fd=%d.", event.fd);
                }
            }
            //write
            if ((events[i].events & EPOLLOUT) && !event.socket->removed)
            {   //取得写handle
                handle = swReactor_getHandle(reactor, SW_EVENT_WRITE, event.type);
                ret = handle(reactor, &event);
                if (ret < 0)
                {
                    swSysError("EPOLLOUT handle failed. fd=%d.", event.fd);
                }
            }
            //error
            if ((events[i].events & (EPOLLRDHUP | EPOLLERR | EPOLLHUP)) && !event.socket->removed)
            {
                //ignore ERR and HUP, because event is already processed at IN and OUT handler.
                if ((events[i].events & EPOLLIN) || (events[i].events & EPOLLOUT))
                {
                    continue;
                }
                handle = swReactor_getHandle(reactor, SW_EVENT_ERROR, event.type);
                ret = handle(reactor, &event);
                if (ret < 0)
                {
                    swSysError("EPOLLERR handle failed. fd=%d.", event.fd);
                }
            }
            if (!event.socket->removed && (event.socket->events & SW_EVENT_ONCE))
            {
                reactor->event_num = reactor->event_num <= 0 ? 0 : reactor->event_num - 1;
                swReactor_del(reactor, event.fd);
            }
        }

        if (reactor->onFinish != NULL)
        {
            reactor->onFinish(reactor);
        }
        if (reactor->once)
        {
            break;
        }
    }
    return 0;
}

#endif
