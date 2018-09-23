/*
 +----------------------------------------------------------------------+
 | Swoole                                                               |
 +----------------------------------------------------------------------+
 | Copyright (c) 2012-2015 The Swoole Group                             |
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

//EventLoop
//swoole_event class 的add,del 等方法的实体就是本文件定义的函数
/* swoole.c
static const zend_function_entry swoole_event_methods[] =
{
    ZEND_FENTRY(add, ZEND_FN(swoole_event_add), arginfo_swoole_event_add, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    ZEND_FENTRY(del, ZEND_FN(swoole_event_del), arginfo_swoole_event_del, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    ZEND_FENTRY(set, ZEND_FN(swoole_event_set), arginfo_swoole_event_set, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    ZEND_FENTRY(exit, ZEND_FN(swoole_event_exit), arginfo_swoole_void, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    ZEND_FENTRY(write, ZEND_FN(swoole_event_write), arginfo_swoole_event_write, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    ZEND_FENTRY(wait, ZEND_FN(swoole_event_wait), arginfo_swoole_void, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    ZEND_FENTRY(defer, ZEND_FN(swoole_event_defer), arginfo_swoole_event_defer, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    ZEND_FENTRY(cycle, ZEND_FN(swoole_event_cycle), arginfo_swoole_event_cycle, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_FE_END
};*/

#include "php_swoole.h"
#ifdef SW_COROUTINE
#include "swoole_coroutine.h"
#endif

typedef struct
{
    struct
    {
        zval cb_read;
        zval cb_write;
        zval socket;
    } stack;
    zval *cb_read;
    zval *cb_write;
    zval *socket;
} php_reactor_fd;

typedef struct
{
    zval _callback;
    zval *callback;
} php_defer_callback;

static int php_swoole_event_onRead(swReactor *reactor, swEvent *event);
static int php_swoole_event_onWrite(swReactor *reactor, swEvent *event);
static int php_swoole_event_onError(swReactor *reactor, swEvent *event);
static void php_swoole_event_onDefer(void *_cb);
static void php_swoole_event_onEndCallback(void *_cb);

//释放事件 callback
static void free_event_callback(void* data)
{
    php_reactor_fd *ev_set = (php_reactor_fd*) data;
    if (ev_set->cb_read)
    {
        sw_zval_ptr_dtor(&(ev_set->cb_read));
        ev_set->cb_read = NULL;
    }
    if (ev_set->cb_write)
    {
        sw_zval_ptr_dtor(&(ev_set->cb_write));
        ev_set->cb_write = NULL;
    }
    if (ev_set->socket)
    {
        sw_zval_ptr_dtor(&(ev_set->socket));
        ev_set->socket = NULL;
    }
    efree(ev_set);
}

static void free_callback(void* data)
{
    php_defer_callback *cb = (php_defer_callback *) data;
    sw_zval_ptr_dtor(&cb->callback);
    efree(cb);
}

//swoole_event_add 函数的回调函数
//定义在  SwooleG.main_reactor->setHandle(SwooleG.main_reactor, SW_FD_USER | SW_EVENT_READ, php_swoole_event_onRead);
//也就是当epoll 有可读事件时就调会调用该函数
static int php_swoole_event_onRead(swReactor *reactor, swEvent *event)
{
    zval *retval;
    zval **args[1];
    php_reactor_fd *fd = event->socket->object;


    args[0] = &fd->socket;//取得读的回调函数
    //执行
    if (sw_call_user_function_ex(EG(function_table), NULL, fd->cb_read, &retval, 1, args, 0, NULL TSRMLS_CC) == FAILURE)
    {
        swoole_php_fatal_error(E_WARNING, "swoole_event: onRead handler error.");
        SwooleG.main_reactor->del(SwooleG.main_reactor, event->fd);
        return SW_ERR;
    }
    if (EG(exception))
    {
        zend_exception_error(EG(exception), E_ERROR TSRMLS_CC);
    }
    if (retval != NULL)
    {
        sw_zval_ptr_dtor(&retval);
    }
    return SW_OK;
}
//同上，描述符可写时的回调函数
static int php_swoole_event_onWrite(swReactor *reactor, swEvent *event)
{
    zval *retval;
    zval **args[1];
    php_reactor_fd *fd = event->socket->object;


    if (!fd->cb_write)
    {
        return swReactor_onWrite(reactor, event);
    }

    args[0] = &fd->socket;

    if (sw_call_user_function_ex(EG(function_table), NULL, fd->cb_write, &retval, 1, args, 0, NULL TSRMLS_CC) == FAILURE)
    {
        swoole_php_fatal_error(E_WARNING, "swoole_event: onWrite handler error");
        SwooleG.main_reactor->del(SwooleG.main_reactor, event->fd);
        return SW_ERR;
    }
    if (EG(exception))
    {
        zend_exception_error(EG(exception), E_ERROR TSRMLS_CC);
    }
    if (retval != NULL)
    {
        sw_zval_ptr_dtor(&retval);
    }
    return SW_OK;
}

static int php_swoole_event_onError(swReactor *reactor, swEvent *event)
{

    int error;
    socklen_t len = sizeof(error);

    if (getsockopt(event->fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0)
    {
        swoole_php_fatal_error(E_WARNING, "swoole_event->onError[1]: getsockopt[sock=%d] failed. Error: %s[%d]", event->fd, strerror(errno), errno);
    }

    if (error != 0)
    {
        swoole_php_fatal_error(E_WARNING, "swoole_event->onError[1]: socket error. Error: %s [%d]", strerror(error), error);
    }

    efree(event->socket->object);
    event->socket->active = 0;

    SwooleG.main_reactor->del(SwooleG.main_reactor, event->fd);

    return SW_OK;
}

//延期执行的回调函数swReactor_defer
static void php_swoole_event_onDefer(void *_cb)
{
    php_defer_callback *defer = _cb;

    zval *retval;
    if (sw_call_user_function_ex(EG(function_table), NULL, defer->callback, &retval, 0, NULL, 0, NULL TSRMLS_CC) == FAILURE)
    {
        swoole_php_fatal_error(E_WARNING, "swoole_event: defer handler error");
        return;
    }
    if (EG(exception))
    {
        zend_exception_error(EG(exception), E_ERROR TSRMLS_CC);
    }
    if (retval != NULL)
    {
        sw_zval_ptr_dtor(&retval);
    }
    sw_zval_ptr_dtor(&defer->callback);
    efree(defer);
}

static void php_swoole_event_onEndCallback(void *_cb)
{
    php_defer_callback *defer = _cb;

    zval *retval;
    if (sw_call_user_function_ex(EG(function_table), NULL, defer->callback, &retval, 0, NULL, 0, NULL TSRMLS_CC) == FAILURE)
    {
        swoole_php_fatal_error(E_WARNING, "swoole_event: defer handler error");
        return;
    }
    if (EG(exception))
    {
        zend_exception_error(EG(exception), E_ERROR TSRMLS_CC);
    }
    if (retval != NULL)
    {
        sw_zval_ptr_dtor(&retval);
    }
}

//建立reactor
void php_swoole_check_reactor()
{
    if (likely(SwooleWG.reactor_init)) //第二次进来的话就 return 出去
    {
        return;
    }

    if (!SWOOLE_G(cli))
    {
        swoole_php_fatal_error(E_ERROR, "async-io must be used in PHP CLI mode.");
        return;
    }

    if (swIsTaskWorker())
    {
        swoole_php_fatal_error(E_ERROR, "can't use async-io in task process.");
        return;
    }

    if (SwooleG.main_reactor == NULL)
    {
        swTraceLog(SW_TRACE_PHP, "init reactor");

        SwooleG.main_reactor = (swReactor *) sw_malloc(sizeof(swReactor));
        if (SwooleG.main_reactor == NULL)
        {
            swoole_php_fatal_error(E_ERROR, "malloc failed.");
            return;
        }//创建 reactor 
        if (swReactor_create(SwooleG.main_reactor, SW_REACTOR_MAXEVENTS) < 0)
        {
            swoole_php_fatal_error(E_ERROR, "failed to create reactor.");
            return;
        }

#ifdef SW_COROUTINE
        SwooleG.main_reactor->can_exit = php_coroutine_reactor_can_exit;
#endif

        //client, swoole_event_exit will set swoole_running = 0
        SwooleWG.in_client = 1;
        SwooleWG.reactor_wait_onexit = 1;
        SwooleWG.reactor_ready = 0;
        //only client side
        //注册请求shutdown 时执行 swoole_event_wait 函数进入事件等待
        php_swoole_at_shutdown("swoole_event_wait");
    }
    //设定读事件处理函数
    SwooleG.main_reactor->setHandle(SwooleG.main_reactor, SW_FD_USER | SW_EVENT_READ, php_swoole_event_onRead);
    //设定写事件处理函数
    SwooleG.main_reactor->setHandle(SwooleG.main_reactor, SW_FD_USER | SW_EVENT_WRITE, php_swoole_event_onWrite);
    //设定错误处理函数
    SwooleG.main_reactor->setHandle(SwooleG.main_reactor, SW_FD_USER | SW_EVENT_ERROR, php_swoole_event_onError);
    //写处理函数
    SwooleG.main_reactor->setHandle(SwooleG.main_reactor, SW_FD_WRITE, swReactor_onWrite);

    SwooleWG.reactor_init = 1;
}

//事件等待函数
void php_swoole_event_wait()
{
    if (SwooleWG.in_client == 1 && SwooleWG.reactor_ready == 0 && SwooleG.running)
    {
        if (PG(last_error_message))
        {
            switch (PG(last_error_type))
            {
            case E_ERROR:
            case E_CORE_ERROR:
            case E_USER_ERROR:
            case E_COMPILE_ERROR:
                return;
            default:
                break;
            }
        }
        SwooleWG.reactor_ready = 1;//设置ready 参数

#ifdef HAVE_SIGNALFD
        if (SwooleG.main_reactor->check_signalfd) //1
        {
            swSignalfd_setup(SwooleG.main_reactor);//信号初始化
        }
#endif

#ifdef SW_COROUTINE
        if (COROG.active == 0)
        {
            coro_init(TSRMLS_C);
        }
#endif
        if (!swReactor_empty(SwooleG.main_reactor))
        {
            int ret = SwooleG.main_reactor->wait(SwooleG.main_reactor, NULL);//进入事件等待 第二个参数 epoll等待时间
            if (ret < 0)
            {
                swoole_php_fatal_error(E_ERROR, "reactor wait failed. Error: %s [%d]", strerror(errno), errno);
            }
        }
        if (SwooleG.timer.map)
        {
            php_swoole_clear_all_timer(); //清除定时器
        }
        SwooleWG.reactor_exit = 1;
    }
}

//从资源类型,对象（process ，client）取得描述符
int swoole_convert_to_fd(zval *zfd TSRMLS_DC)
{
    php_stream *stream;
    int socket_fd;

#ifdef SWOOLE_SOCKETS_SUPPORT
    php_socket *php_sock;
#endif
    if (SW_Z_TYPE_P(zfd) == IS_RESOURCE)
    {
        if (SW_ZEND_FETCH_RESOURCE_NO_RETURN(stream, php_stream *, &zfd, -1, NULL, php_file_le_stream()))
        {
            if (php_stream_cast(stream, PHP_STREAM_AS_FD_FOR_SELECT | PHP_STREAM_CAST_INTERNAL, (void* )&socket_fd, 1) != SUCCESS || socket_fd < 0)
            {
                return SW_ERR;
            }
        }
        else
        {
#ifdef SWOOLE_SOCKETS_SUPPORT
            if (SW_ZEND_FETCH_RESOURCE_NO_RETURN(php_sock, php_socket *, &zfd, -1, NULL, php_sockets_le_socket()))
            {
                socket_fd = php_sock->bsd_socket;

            }
            else
            {
                swoole_php_fatal_error(E_WARNING, "fd argument must be either valid PHP stream or valid PHP socket resource");
                return SW_ERR;
            }
#else
            swoole_php_fatal_error(E_WARNING, "fd argument must be valid PHP stream resource");
            return SW_ERR;
#endif
        }
    }
    else if (SW_Z_TYPE_P(zfd) == IS_LONG)
    {
        socket_fd = Z_LVAL_P(zfd);
        if (socket_fd < 0)
        {
            swoole_php_fatal_error(E_WARNING, "invalid file descriptor passed");
            return SW_ERR;
        }
    }
    else if (SW_Z_TYPE_P(zfd) == IS_OBJECT)
    {
        zval *zsock = NULL;
        if (instanceof_function(Z_OBJCE_P(zfd), swoole_client_class_entry_ptr TSRMLS_CC))
        {
            zsock = sw_zend_read_property(Z_OBJCE_P(zfd), zfd, SW_STRL("sock")-1, 0 TSRMLS_CC);//client 打开的描述符
        }
        else if (instanceof_function(Z_OBJCE_P(zfd), swoole_process_class_entry_ptr TSRMLS_CC)) //process
        {
            zsock = sw_zend_read_property(Z_OBJCE_P(zfd), zfd, SW_STRL("pipe")-1, 0 TSRMLS_CC); //取得管道描述符
        }
        if (zsock == NULL || ZVAL_IS_NULL(zsock))
        {
            swoole_php_fatal_error(E_WARNING, "object is not instanceof swoole_client or swoole_process.");
            return -1;
        }
        socket_fd = Z_LVAL_P(zsock);
    }
    else
    {
        return SW_ERR;
    }
    return socket_fd;
}

int swoole_convert_to_fd_ex(zval *zfd, int *async TSRMLS_DC)
{
    php_stream *stream;
    int fd;

#ifdef SWOOLE_SOCKETS_SUPPORT
    php_socket *php_sock;
#endif

    if (SW_Z_TYPE_P(zfd) != IS_RESOURCE)
    {
        swoole_php_fatal_error(E_WARNING, "fd argument must be either valid PHP stream or valid PHP socket resource");
        return SW_ERR;
    }
    if (SW_ZEND_FETCH_RESOURCE_NO_RETURN(stream, php_stream *, &zfd, -1, NULL, php_file_le_stream()))
    {
        if (php_stream_cast(stream, PHP_STREAM_AS_FD_FOR_SELECT | PHP_STREAM_CAST_INTERNAL, (void* )&fd, 1) == SUCCESS && fd >= 0)
        {
            *async = stream->wrapper->wops == php_plain_files_wrapper.wops ? 0 : 1;
            return fd;
        }
    }
#ifdef SWOOLE_SOCKETS_SUPPORT
    else if (SW_ZEND_FETCH_RESOURCE_NO_RETURN(php_sock, php_socket *, &zfd, -1, NULL, php_sockets_le_socket()))
    {
        fd = php_sock->bsd_socket;
        *async = 1;
    }
#endif
    swoole_php_fatal_error(E_WARNING, "invalid file descriptor passed");
    return SW_ERR;
}

#ifdef SWOOLE_SOCKETS_SUPPORT
php_socket* swoole_convert_to_socket(int sock)
{
    php_socket *socket_object = emalloc(sizeof *socket_object);
    bzero(socket_object, sizeof(php_socket));
    socket_object->bsd_socket = sock;
    socket_object->blocking = 1;

    struct sockaddr_storage addr;
    socklen_t addr_len = sizeof(addr);

    if (getsockname(sock, (struct sockaddr*) &addr, &addr_len) == 0)
    {
        socket_object->type = addr.ss_family;
    }
    else
    {
        swoole_php_sys_error(E_WARNING, "unable to obtain socket family");
        error:
        efree(socket_object);
        return NULL;
    }

    int t = fcntl(sock, F_GETFL);
    if (t == -1)
    {
        swoole_php_sys_error(E_WARNING, "unable to obtain blocking state");
        goto error;
    }
    else
    {
        socket_object->blocking = !(t & O_NONBLOCK);
    }
    return socket_object;
}
#endif

//将一个socket加入到底层的reactor事件监听中。此函数可以用在Server或Client模式下。
//使用举例
/*
<?php
swoole_event_add(STDIN, function($fp) { //当终端可读时 执行回调函数，不可读时进入epoll_wait
    echo "STDIN: ".fread($fp, 8192);
});

*/

PHP_FUNCTION(swoole_event_add)
{
    zval *cb_read = NULL;
    zval *cb_write = NULL;
    zval *zfd;
    char *func_name = NULL;
    long event_flag = 0;
    //zfd 文件描述符或资源 ，cb_read可读事件回调函数 cb_write可写事件回调函数
    //event_flag 为事件类型的掩码 SWOOLE_EVENT_READ | SWOOLE_EVENT_WRITE
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|zzl", &zfd, &cb_read, &cb_write, &event_flag) == FAILURE)
    { 
        return;
    }

    if ((cb_read == NULL && cb_write == NULL) || (ZVAL_IS_NULL(cb_read) && ZVAL_IS_NULL(cb_write)))
    {
        swoole_php_fatal_error(E_WARNING, "no read or write event callback.");
        RETURN_FALSE;
    }
    //取得描述符
    int socket_fd = swoole_convert_to_fd(zfd TSRMLS_CC);
    if (socket_fd < 0)
    {
        swoole_php_fatal_error(E_WARNING, "unknow type.");
        RETURN_FALSE;
    }
    if (socket_fd == 0 && (event_flag & SW_EVENT_WRITE))
    {
        swoole_php_fatal_error(E_WARNING, "invalid socket fd [%d].", socket_fd);
        RETURN_FALSE;
    }

/*

typedef struct
{
    struct
    {
        zval cb_read;
        zval cb_write;
        zval socket;
    } stack;
    zval *cb_read;
    zval *cb_write;
    zval *socket;//描述符
} php_reactor_fd;
*/
    php_reactor_fd *reactor_fd = emalloc(sizeof(php_reactor_fd));
    reactor_fd->socket = zfd;
    sw_copy_to_stack(reactor_fd->socket, reactor_fd->stack.socket);//a copy to b 
    sw_zval_add_ref(&reactor_fd->socket);

    if (cb_read!= NULL && !ZVAL_IS_NULL(cb_read))
    {
        //判断读参数是否可以回调
        if (!sw_zend_is_callable(cb_read, 0, &func_name TSRMLS_CC))
        {
            swoole_php_fatal_error(E_ERROR, "Function '%s' is not callable", func_name);
            efree(func_name);
            RETURN_FALSE;
        }
        efree(func_name);
        reactor_fd->cb_read = cb_read;
        sw_zval_add_ref(&cb_read);//ref 增加引用，因下面有copy ，要不函数结束php mm 会回收该变量了
        sw_copy_to_stack(reactor_fd->cb_read, reactor_fd->stack.cb_read);//copy 第一个参数指针指向的内容 to 第二个参数中
    }
    else
    {
        reactor_fd->cb_read = NULL;
    }

    if (cb_write!= NULL && !ZVAL_IS_NULL(cb_write))
    {   //判断写参数是否可以回调
        if (!sw_zend_is_callable(cb_write, 0, &func_name TSRMLS_CC))
        {
            swoole_php_fatal_error(E_ERROR, "Function '%s' is not callable", func_name);
            efree(func_name);
            RETURN_FALSE;
        }
        efree(func_name);
        reactor_fd->cb_write = cb_write;
        sw_zval_add_ref(&cb_write);
        sw_copy_to_stack(reactor_fd->cb_write, reactor_fd->stack.cb_write);
    }
    else
    {
        reactor_fd->cb_write = NULL;
    }
    //建立epoll ,设定读事件处理函数,注册 shutdown 时事件等待函数。
    php_swoole_check_reactor();
    //设置监听的描述符为非阻塞
    swSetNonBlock(socket_fd); //must be nonblock
    //向epoll 中增加感兴趣的事件
    //实体是 swReactorEpoll_add 函数
    if (SwooleG.main_reactor->add(SwooleG.main_reactor, socket_fd, SW_FD_USER | event_flag) < 0)
    {
        swoole_php_fatal_error(E_WARNING, "swoole_event_add failed.");
        RETURN_FALSE;
    }
    // socket 是  reactor->socket_array 中分配的，初期化时分配 reactor->socket_array = swArray_new(1024, sizeof(swConnection));
    //这里的意思是返回 reactor->socket_array 应有的位置，并把回调函数等相关数据放进去，event_set ,del 时会用到
    swConnection *socket = swReactor_get(SwooleG.main_reactor, socket_fd);
    socket->object = reactor_fd; //回调函数结构体
    socket->active = 1;//是否有效
    socket->nonblock = 1;//非阻塞

    RETURN_LONG(socket_fd);
}

//向指定描述符写数据
PHP_FUNCTION(swoole_event_write)
{
    zval *zfd;
    char *data;
    zend_size_t len;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zs", &zfd, &data, &len) == FAILURE)
    {
        return;
    }

    if (len <= 0)//data 为空报错
    {
        swoole_php_fatal_error(E_WARNING, "data empty.");
        RETURN_FALSE;
    }

    int socket_fd = swoole_convert_to_fd(zfd TSRMLS_CC);
    if (socket_fd < 0)
    {
        swoole_php_fatal_error(E_WARNING, "unknow type.");
        RETURN_FALSE;
    }
    //建立事件绑定
    php_swoole_check_reactor();
    // 调用  swReactor_write 发送数据 
    // php_swoole_check_reactor->swReactor_create 中建立的绑定
    /*
    reactor->setHandle = swReactor_setHandle; 
    reactor->onFinish = swReactor_onFinish;
    reactor->onTimeout = swReactor_onTimeout;
    reactor->write = swReactor_write;
    reactor->defer = swReactor_defer;
    reactor->close = swReactor_close;
    
    php_swoole_check_reactor 函数中建立的set 事件回调函数
    增加事件 add 中有对应的事件类型，这样在 epoll_wait 中有事件触发时，swReactor_getHandle 取得对应的回调函数。
    回调函数定义在Reactor结构体中
    swReactor_handle handle[SW_MAX_FDTYPE];        //默认事件  SW_MAX_FDTYPE = 32
    swReactor_handle write_handle[SW_MAX_FDTYPE];  //扩展事件1(一般为写事件)
    swReactor_handle error_handle[SW_MAX_FDTYPE];  //扩展事件2(一般为错误事件,如socket关闭)

    //设定读事件处理函数
    SwooleG.main_reactor->setHandle(SwooleG.main_reactor, SW_FD_USER | SW_EVENT_READ, php_swoole_event_onRead);
    //设定写事件处理函数
    SwooleG.main_reactor->setHandle(SwooleG.main_reactor, SW_FD_USER | SW_EVENT_WRITE, php_swoole_event_onWrite);
    //设定错误处理函数
    SwooleG.main_reactor->setHandle(SwooleG.main_reactor, SW_FD_USER | SW_EVENT_ERROR, php_swoole_event_onError);
    //写处理函数
    SwooleG.main_reactor->setHandle(SwooleG.main_reactor, SW_FD_WRITE, swReactor_onWrite);

    事件epoll绑定函数
    reactor->add = swReactorEpoll_add;
    reactor->set = swReactorEpoll_set;
    reactor->del = swReactorEpoll_del;
    reactor->wait = swReactorEpoll_wait;
    reactor->free = swReactorEpoll_free;
    */
    if (SwooleG.main_reactor->write(SwooleG.main_reactor, socket_fd, data, len) < 0)
    {
        RETURN_FALSE;
    }
    else
    {
        RETURN_TRUE;
    }
}

//修改事件监听的回调函数和掩码
//swoole_event_set只能替换回调函数，但并不能释放事件回调函数
PHP_FUNCTION(swoole_event_set)
{
    zval *cb_read = NULL;
    zval *cb_write = NULL;
    zval *zfd;

    char *func_name = NULL;
    long event_flag = 0;
    if (!SwooleG.main_reactor)
    {
        swoole_php_fatal_error(E_WARNING, "reactor no ready, cannot swoole_event_set.");
        RETURN_FALSE;
    }

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|zzl", &zfd, &cb_read, &cb_write, &event_flag) == FAILURE)
    {
        return;
    }

    int socket_fd = swoole_convert_to_fd(zfd TSRMLS_CC);
    if (socket_fd < 0)
    {
        swoole_php_fatal_error(E_WARNING, "unknow type.");
        RETURN_FALSE;
    }

    swConnection *socket = swReactor_get(SwooleG.main_reactor, socket_fd);
    if (!socket->active) // 有效状态在swoole_event_add 中改变的
    {
        swoole_php_fatal_error(E_WARNING, "socket[%d] is not found in the reactor.", socket_fd);
        efree(func_name);
        RETURN_FALSE;
    }

    php_reactor_fd *ev_set = socket->object;//取得回到函数设置对象
    if (cb_read != NULL && !ZVAL_IS_NULL(cb_read))
    {
        if (!sw_zend_is_callable(cb_read, 0, &func_name TSRMLS_CC))
        {
            swoole_php_fatal_error(E_ERROR, "Function '%s' is not callable", func_name);
            efree(func_name);
            RETURN_FALSE;
        }
        else
        {
            if (ev_set->cb_read)
            {
                sw_zval_ptr_dtor(&ev_set->cb_read);//old 的回调函数释放
            }
            ev_set->cb_read = cb_read;
            sw_zval_add_ref(&cb_read);
            sw_copy_to_stack(ev_set->cb_read, ev_set->stack.cb_read);
            efree(func_name);
        }
    }

    if (cb_write != NULL && !ZVAL_IS_NULL(cb_write))
    {
        if (socket_fd == 0 && (event_flag & SW_EVENT_WRITE))
        {
            swoole_php_fatal_error(E_WARNING, "invalid socket fd [%d].", socket_fd);
            RETURN_FALSE;
        }
        if (!sw_zend_is_callable(cb_write, 0, &func_name TSRMLS_CC))
        {
            swoole_php_fatal_error(E_ERROR, "Function '%s' is not callable", func_name);
            efree(func_name);
            RETURN_FALSE;
        }
        else
        {
            if (ev_set->cb_write)
            {
                sw_zval_ptr_dtor(&ev_set->cb_write);
            }
            ev_set->cb_write = cb_write;
            sw_zval_add_ref(&cb_write);
            sw_copy_to_stack(ev_set->cb_write, ev_set->stack.cb_write);
            efree(func_name);
        }
    }

    if ((event_flag & SW_EVENT_READ) && ev_set->cb_read == NULL)
    {
        swoole_php_fatal_error(E_WARNING, "swoole_event: no read callback.");
        RETURN_FALSE;
    }

    if ((event_flag & SW_EVENT_WRITE) && ev_set->cb_write == NULL)
    {
        swoole_php_fatal_error(E_WARNING, "swoole_event: no write callback.");
        RETURN_FALSE;
    }
    //重新设定事件属性
    if (SwooleG.main_reactor->set(SwooleG.main_reactor, socket_fd, SW_FD_USER | event_flag) < 0)
    {
        swoole_php_fatal_error(E_WARNING, "swoole_event_set failed.");
        RETURN_FALSE;
    }

    RETURN_TRUE;
}

//swoole_event_del函数用于从reactor中移除监听的socket
PHP_FUNCTION(swoole_event_del)
{
    zval *zfd;

    if (!SwooleG.main_reactor)
    {
        swoole_php_fatal_error(E_WARNING, "reactor no ready, cannot swoole_event_del.");
        RETURN_FALSE;
    }

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &zfd) == FAILURE)
    {
        return;
    }

    int socket_fd = swoole_convert_to_fd(zfd TSRMLS_CC);
    if (socket_fd < 0)
    {
        swoole_php_fatal_error(E_WARNING, "unknow type.");
        RETURN_FALSE;
    }

    swConnection *socket = swReactor_get(SwooleG.main_reactor, socket_fd);
    if (socket->object)//取得回到函数设置对象
    {
        //调用 swReactor_defer
        SwooleG.main_reactor->defer(SwooleG.main_reactor, free_event_callback, socket->object);
        socket->object = NULL;
    }
    //删除事件描述符
    int ret = SwooleG.main_reactor->del(SwooleG.main_reactor, socket_fd);
    socket->active = 0;
    SW_CHECK_RETURN(ret);
}

//在下一个事件循环开始时执行函数
PHP_FUNCTION(swoole_event_defer)
{
    zval *callback;
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &callback) == FAILURE)
    {
        return;
    }

    char *func_name;
    if (!sw_zend_is_callable(callback, 0, &func_name TSRMLS_CC))
    {
        swoole_php_fatal_error(E_ERROR, "Function '%s' is not callable", func_name);
        efree(func_name);
        RETURN_FALSE;
    }
    efree(func_name);

    php_swoole_check_reactor();

    php_defer_callback *defer = emalloc(sizeof(php_defer_callback));
    defer->callback = &defer->_callback;
    memcpy(defer->callback, callback, sizeof(zval));
    sw_zval_add_ref(&callback);
    //延期执行的回调函数swReactor_defer ，相关数据是defer
    SW_CHECK_RETURN(SwooleG.main_reactor->defer(SwooleG.main_reactor, php_swoole_event_onDefer, defer));
}

//定义事件循环周期执行函数。此函数会在每一轮事件循环结束时调用。
PHP_FUNCTION(swoole_event_cycle)
{
    if (!SwooleG.main_reactor)
    {
        swoole_php_fatal_error(E_WARNING, "reactor no ready, cannot swoole_event_defer.");
        RETURN_FALSE;
    }

    zval *callback;
    zend_bool before = 0;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|b", &callback, &before) == FAILURE)
    {
        return;
    }

    if (ZVAL_IS_NULL(callback))
    {
        if (SwooleG.main_reactor->idle_task.callback == NULL)
        {
            RETURN_FALSE;
        }
        else
        {
            SwooleG.main_reactor->defer(SwooleG.main_reactor, free_callback, SwooleG.main_reactor->idle_task.data);
            SwooleG.main_reactor->idle_task.callback = NULL;
            SwooleG.main_reactor->idle_task.data = NULL;
            RETURN_TRUE;
        }
    }

    char *func_name;
    if (!sw_zend_is_callable(callback, 0, &func_name TSRMLS_CC))
    {
        swoole_php_fatal_error(E_ERROR, "Function '%s' is not callable", func_name);
        efree(func_name);
        RETURN_FALSE;
    }
    efree(func_name);

    php_defer_callback *cb = emalloc(sizeof(php_defer_callback));

    cb->callback = &cb->_callback;
    memcpy(cb->callback, callback, sizeof(zval));
    sw_zval_add_ref(&callback);

    if (before == 0)
    {// EventLoop之后调用该函数
        if (SwooleG.main_reactor->idle_task.data != NULL)
        {
            SwooleG.main_reactor->defer(SwooleG.main_reactor, free_callback, SwooleG.main_reactor->idle_task.data);
        }

        SwooleG.main_reactor->idle_task.callback = php_swoole_event_onEndCallback;
        SwooleG.main_reactor->idle_task.data = cb;
    }
    else
    {   // EventLoop之前调用该函数
        if (SwooleG.main_reactor->future_task.data != NULL)
        {
            SwooleG.main_reactor->defer(SwooleG.main_reactor, free_callback, SwooleG.main_reactor->future_task.data);
        }

        SwooleG.main_reactor->future_task.callback = php_swoole_event_onEndCallback;
        SwooleG.main_reactor->future_task.data = cb;
        //Registration onBegin callback function
        swReactor_activate_future_task(SwooleG.main_reactor);
    }

    RETURN_TRUE;
}

//退出事件轮询，此函数仅在Client程序中有效
PHP_FUNCTION(swoole_event_exit)
{
    if (SwooleWG.in_client == 1) //php_swoole_check_reactor 中设置为1
    {
        if (SwooleG.main_reactor)
        {
            SwooleG.main_reactor->running = 0;//epoll_wait 中循环条件
        }
        SwooleG.running = 0;
    }
}

//php 脚本执行完毕后执行的回调函数
PHP_FUNCTION(swoole_event_wait)
{
    if (!SwooleG.main_reactor)
    {
        return;
    }
    //进入事件等待函数
    php_swoole_event_wait();
}

//仅执行一次reactor->wait操作
PHP_FUNCTION(swoole_event_dispatch)
{
    if (!SwooleG.main_reactor)
    {
        RETURN_FALSE;
    }
    SwooleG.main_reactor->once = 1;//执行1次 flag

#ifdef HAVE_SIGNALFD
    if (SwooleG.main_reactor->check_signalfd) //这里为false
    {
        swSignalfd_setup(SwooleG.main_reactor);
    }
#endif

#ifdef SW_COROUTINE
    if (COROG.active == 0)
    {
        coro_init(TSRMLS_C);
    }
#endif

    int ret = SwooleG.main_reactor->wait(SwooleG.main_reactor, NULL);
    if (ret < 0)
    {
        swoole_php_fatal_error(E_ERROR, "reactor wait failed. Error: %s [%d]", strerror(errno), errno);
    }

    SwooleG.main_reactor->once = 0;
    RETURN_TRUE;
}

//检测传入的$fd是否已加入了事件监听
PHP_FUNCTION(swoole_event_isset)
{
    if (!SwooleG.main_reactor)
    {
        RETURN_FALSE;
    }

    zval *zfd;
    long events = SW_EVENT_READ | SW_EVENT_WRITE;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|l", &zfd, &events) == FAILURE)
    {
        return;
    }

    int socket_fd = swoole_convert_to_fd(zfd TSRMLS_CC);
    if (socket_fd < 0)
    {
        swoole_php_fatal_error(E_WARNING, "unknow type.");
        RETURN_FALSE;
    }

    swConnection *_socket = swReactor_get(SwooleG.main_reactor, socket_fd);
    if (_socket == NULL || _socket->removed)
    {
        RETURN_FALSE;
    }
    if (_socket->events & events)//socket的事件类型在epoll add 的swReactor_add函数中增加了
    {
        RETURN_TRUE;
    }
    else
    {
        RETURN_FALSE;
    }
}
