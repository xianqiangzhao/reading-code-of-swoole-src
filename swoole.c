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
#include "php_swoole.h"
#include "zend_variables.h"

#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <sys/ioctl.h>

#ifdef SW_COROUTINE
#include "swoole_coroutine.h"
#endif
//声明全局变量
ZEND_DECLARE_MODULE_GLOBALS(swoole)

//引入 sapi ，用来判断当前运行的model cli or fpm eg...
extern sapi_module_struct sapi_module;

// arginfo server
// *_oo : for object style

//server class 的参数
//宏展开后是
static const zend_internal_arg_info arginfo_swoole_void[] = { \
        { (const char*)(zend_uintptr_t)(required_num_args), 0, return_reference, 0 },
            { #name, 0, pass_by_ref, 0},

               }

ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_void, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_server__construct, 0, 0, 1) //最后一个参数是最小要求参数个数
    ZEND_ARG_INFO(0, host)   //第一个参数是否为引用 1是 0 不是， host 是参数名
    ZEND_ARG_INFO(0, port)
    ZEND_ARG_INFO(0, mode)
    ZEND_ARG_INFO(0, sock_type)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_server_set_oo, 0, 0, 1)
    ZEND_ARG_ARRAY_INFO(0, settings, 0) //array 类型
ZEND_END_ARG_INFO()

//for object style
ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_server_send_oo, 0, 0, 2)
    ZEND_ARG_INFO(0, fd)
    ZEND_ARG_INFO(0, send_data)
    ZEND_ARG_INFO(0, reactor_id)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_server_sendwait, 0, 0, 2)
    ZEND_ARG_INFO(0, conn_fd)
    ZEND_ARG_INFO(0, send_data)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_server_exist, 0, 0, 1)
    ZEND_ARG_INFO(0, fd)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_server_protect, 0, 0, 1)
    ZEND_ARG_INFO(0, fd)
    ZEND_ARG_INFO(0, is_protected)
ZEND_END_ARG_INFO()

//for object style
ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_server_sendto, 0, 0, 3)
    ZEND_ARG_INFO(0, ip)
    ZEND_ARG_INFO(0, port)
    ZEND_ARG_INFO(0, send_data)
    ZEND_ARG_INFO(0, server_socket)
ZEND_END_ARG_INFO()

//for object style
ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_server_sendfile, 0, 0, 2)
    ZEND_ARG_INFO(0, conn_fd)
    ZEND_ARG_INFO(0, filename)
    ZEND_ARG_INFO(0, offset)
    ZEND_ARG_INFO(0, length)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_server_close, 0, 0, 1)
    ZEND_ARG_INFO(0, fd)
    ZEND_ARG_INFO(0, reset)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_server_pause, 0, 0, 1)
    ZEND_ARG_INFO(0, fd)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_server_resume, 0, 0, 1)
    ZEND_ARG_INFO(0, fd)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_server_confirm, 0, 0, 1)
    ZEND_ARG_INFO(0, fd)
ZEND_END_ARG_INFO()

#ifdef SWOOLE_SOCKETS_SUPPORT
ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_server_getSocket, 0, 0, 0)
    ZEND_ARG_INFO(0, port)
ZEND_END_ARG_INFO()
#endif

ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_server_on, 0, 0, 2)
    ZEND_ARG_INFO(0, event_name)
    ZEND_ARG_INFO(0, callback)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_server_listen, 0, 0, 3)
    ZEND_ARG_INFO(0, host)
    ZEND_ARG_INFO(0, port)
    ZEND_ARG_INFO(0, sock_type)
ZEND_END_ARG_INFO()

//object style
ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_server_task, 0, 0, 1)
    ZEND_ARG_INFO(0, data)
    ZEND_ARG_INFO(0, worker_id)
    ZEND_ARG_INFO(0, finish_callback)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_server_taskwait, 0, 0, 1)
    ZEND_ARG_INFO(0, data)
    ZEND_ARG_INFO(0, timeout)
    ZEND_ARG_INFO(0, worker_id)
ZEND_END_ARG_INFO()

#ifdef SW_COROUTINE
ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_server_taskCo, 0, 0, 1)
    ZEND_ARG_ARRAY_INFO(0, tasks, 0)
    ZEND_ARG_INFO(0, timeout)
ZEND_END_ARG_INFO()
#endif

ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_server_taskWaitMulti_oo, 0, 0, 1)
    ZEND_ARG_ARRAY_INFO(0, tasks, 0)
    ZEND_ARG_INFO(0, timeout)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_server_finish_oo, 0, 0, 1)
    ZEND_ARG_INFO(0, data)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_server_reload_oo, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_server_heartbeat_oo, 0, 0, 1)
    ZEND_ARG_INFO(0, reactor_id)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_server_stop, 0, 0, 0)
    ZEND_ARG_INFO(0, worker_id)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_server_bind, 0, 0, 2)
    ZEND_ARG_INFO(0, fd)
    ZEND_ARG_INFO(0, uid)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_server_sendMessage, 0, 0, 2)
    ZEND_ARG_INFO(0, message)
    ZEND_ARG_INFO(0, dst_worker_id)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_server_addProcess, 0, 0, 1)
    ZEND_ARG_OBJ_INFO(0, process, swoole_process, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_connection_info, 0, 0, 1)
    ZEND_ARG_INFO(0, fd)
    ZEND_ARG_INFO(0, reactor_id)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_connection_list, 0, 0, 1)
    ZEND_ARG_INFO(0, start_fd)
    ZEND_ARG_INFO(0, find_count)
ZEND_END_ARG_INFO()

//arginfo event
ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_event_add, 0, 0, 2)
    ZEND_ARG_INFO(0, fd)
    ZEND_ARG_INFO(0, read_callback)
    ZEND_ARG_INFO(0, write_callback)
    ZEND_ARG_INFO(0, events)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_event_set, 0, 0, 1)
    ZEND_ARG_INFO(0, fd)
    ZEND_ARG_INFO(0, read_callback)
    ZEND_ARG_INFO(0, write_callback)
    ZEND_ARG_INFO(0, events)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_event_write, 0, 0, 2)
    ZEND_ARG_INFO(0, fd)
    ZEND_ARG_INFO(0, data)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_event_defer, 0, 0, 1)
    ZEND_ARG_INFO(0, callback)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_event_cycle, 0, 0, 1)
    ZEND_ARG_INFO(0, callback)
    ZEND_ARG_INFO(0, before)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_event_del, 0, 0, 1)
    ZEND_ARG_INFO(0, fd)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_event_isset, 0, 0, 1)
    ZEND_ARG_INFO(0, fd)
    ZEND_ARG_INFO(0, events)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_timer_tick, 0, 0, 2)
    ZEND_ARG_INFO(0, ms)
    ZEND_ARG_INFO(0, callback)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_timer_after, 0, 0, 2)
    ZEND_ARG_INFO(0, ms)
    ZEND_ARG_INFO(0, callback)
    ZEND_ARG_INFO(0, param)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_timer_exists, 0, 0, 1)
    ZEND_ARG_INFO(0, timer_id)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_timer_clear, 0, 0, 1)
    ZEND_ARG_INFO(0, timer_id)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_async_set, 0, 0, 1)
    ZEND_ARG_ARRAY_INFO(0, settings, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_async_readfile, 0, 0, 2)
    ZEND_ARG_INFO(0, filename)
    ZEND_ARG_INFO(0, callback)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_async_writefile, 0, 0, 2)
    ZEND_ARG_INFO(0, filename)
    ZEND_ARG_INFO(0, content)
    ZEND_ARG_INFO(0, callback)
    ZEND_ARG_INFO(0, flags)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_async_read, 0, 0, 2)
    ZEND_ARG_INFO(0, filename)
    ZEND_ARG_INFO(0, callback)
    ZEND_ARG_INFO(0, chunk_size)
    ZEND_ARG_INFO(0, offset)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_async_write, 0, 0, 2)
    ZEND_ARG_INFO(0, filename)
    ZEND_ARG_INFO(0, content)
    ZEND_ARG_INFO(0, offset)
    ZEND_ARG_INFO(0, callback)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_async_dns_lookup, 0, 0, 2)
    ZEND_ARG_INFO(0, hostname)
    ZEND_ARG_INFO(0, callback)
ZEND_END_ARG_INFO()

#ifdef SW_COROUTINE
ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_async_dns_lookup_coro, 0, 0, 1)
    ZEND_ARG_INFO(0, domain_name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_coroutine_create, 0, 0, 1)
    ZEND_ARG_INFO(0, func)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_coroutine_exec, 0, 0, 1)
    ZEND_ARG_INFO(0, command)
ZEND_END_ARG_INFO()
#endif

ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_async_exec, 0, 0, 2)
    ZEND_ARG_INFO(0, command)
    ZEND_ARG_INFO(0, callback)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_client_select, 0, 0, 3)
    ZEND_ARG_INFO(1, read_array)
    ZEND_ARG_INFO(1, write_array)
    ZEND_ARG_INFO(1, error_array)
    ZEND_ARG_INFO(0, timeout)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_set_process_name, 0, 0, 1)
    ZEND_ARG_INFO(0, process_name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_strerror, 0, 0, 1)
    ZEND_ARG_INFO(0, errno)
    ZEND_ARG_INFO(0, error_type)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_hashcode, 0, 0, 1)
    ZEND_ARG_INFO(0, data)
    ZEND_ARG_INFO(0, type)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_connection_iterator_offsetExists, 0, 0, 1)
    ZEND_ARG_INFO(0, fd)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_connection_iterator_offsetGet, 0, 0, 1)
    ZEND_ARG_INFO(0, fd)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_connection_iterator_offsetUnset, 0, 0, 1)
    ZEND_ARG_INFO(0, fd)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_connection_iterator_offsetSet, 0, 0, 2)
    ZEND_ARG_INFO(0, fd)
    ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

//arginfo end

#include "zend_exceptions.h"

static PHP_FUNCTION(swoole_last_error);
static PHP_FUNCTION(swoole_hashcode);
// 定义的函数结构体
//PHP_FE 展开后是
//#define ZEND_FE(name, arg_info)                     ZEND_FENTRY(name, ZEND_FN(name), arg_info, 0)
//ZEND_FENTRY(name, ZEND_FN(name), arg_info, 0)   { #zend_name, name, arg_info, (uint32_t) (sizeof(arg_info)/sizeof(struct _zend_internal_arg_info)-1), flags },
//#define ZEND_FN(name) zif_##name
//最终是  = { "zend_name", zif_name, arg_info, (uint32_t) (sizeof(arg_info)/sizeof(struct _zend_internal_arg_info)-1), flags },


//PHP_FUNCTION 展开后是
//#define ZEND_FUNCTION(name)               ZEND_NAMED_FUNCTION(ZEND_FN(name))
//#define ZEND_NAMED_FUNCTION(name)       void name(INTERNAL_FUNCTION_PARAMETERS)
//#define INTERNAL_FUNCTION_PARAMETERS zend_execute_data *execute_data, zval *return_value
// 最终是  = void zif_name(zend_execute_data *execute_data, zval *return_value)

//从以上分析 
//   PHP_FE(swoole_version, arginfo_swoole_void)
//会被展开为
// { "swoole_version", zif_swoole_version, arginfo_swoole_void, (uint32_t) (sizeof(arginfo_swoole_void)/sizeof(struct _zend_internal_arg_info)-1), 0 },

//意思是 swoole_version 函数指针是 zif_swoole_version ，参数是 arginfo_swoole_void

const zend_function_entry swoole_functions[] =
{
    PHP_FE(swoole_version, arginfo_swoole_void)
    PHP_FE(swoole_cpu_num, arginfo_swoole_void)
    PHP_FE(swoole_last_error, arginfo_swoole_void)
    /*------swoole_event-----*/
    PHP_FE(swoole_event_add, arginfo_swoole_event_add)
    PHP_FE(swoole_event_set, arginfo_swoole_event_set)
    PHP_FE(swoole_event_del, arginfo_swoole_event_del)
    PHP_FE(swoole_event_exit, arginfo_swoole_void)
    PHP_FE(swoole_event_wait, arginfo_swoole_void)
    PHP_FE(swoole_event_write, arginfo_swoole_event_write)
    PHP_FE(swoole_event_defer, arginfo_swoole_event_defer)
    PHP_FE(swoole_event_cycle, arginfo_swoole_event_cycle)
    PHP_FE(swoole_event_dispatch, arginfo_swoole_void)
    PHP_FE(swoole_event_isset, arginfo_swoole_event_isset)
    /*------swoole_timer-----*/
    PHP_FE(swoole_timer_after, arginfo_swoole_timer_after)
    PHP_FE(swoole_timer_tick, arginfo_swoole_timer_tick)
    PHP_FE(swoole_timer_exists, arginfo_swoole_timer_exists)
    PHP_FE(swoole_timer_clear, arginfo_swoole_timer_clear)
    /*------swoole_async_io------*/
    PHP_FE(swoole_async_set, arginfo_swoole_async_set)
    PHP_FE(swoole_async_read, arginfo_swoole_async_read)
    PHP_FE(swoole_async_write, arginfo_swoole_async_write)
    PHP_FE(swoole_async_readfile, arginfo_swoole_async_readfile)
    PHP_FE(swoole_async_writefile, arginfo_swoole_async_writefile)
    PHP_FE(swoole_async_dns_lookup, arginfo_swoole_async_dns_lookup)
#ifdef SW_COROUTINE
    PHP_FE(swoole_async_dns_lookup_coro, arginfo_swoole_async_dns_lookup_coro)
    PHP_FE(swoole_coroutine_create, arginfo_swoole_coroutine_create)
    PHP_FE(swoole_coroutine_exec, arginfo_swoole_coroutine_exec)

    //定义别名 go 就代表 swoole_coroutine_create
    //go()  相当于 swoole_coroutine_create()
    PHP_FALIAS(go, swoole_coroutine_create, arginfo_swoole_coroutine_create) 
#endif
    /*------other-----*/
    PHP_FE(swoole_client_select, arginfo_swoole_client_select)
    PHP_FALIAS(swoole_select, swoole_client_select, arginfo_swoole_client_select)
    PHP_FE(swoole_set_process_name, arginfo_swoole_set_process_name)
    PHP_FE(swoole_get_local_ip, arginfo_swoole_void)
    PHP_FE(swoole_get_local_mac, arginfo_swoole_void)
    PHP_FE(swoole_strerror, arginfo_swoole_strerror)
    PHP_FE(swoole_errno, arginfo_swoole_void)
    PHP_FE(swoole_hashcode, arginfo_swoole_hashcode)
    PHP_FE(swoole_call_user_shutdown_begin, arginfo_swoole_void)
    PHP_FE_END /* Must be the last line in swoole_functions[] */
};


//class swoole_server　的函数
static zend_function_entry swoole_server_methods[] = {
    PHP_ME(swoole_server, __construct, arginfo_swoole_server__construct, ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
    PHP_ME(swoole_server, __destruct, arginfo_swoole_void, ZEND_ACC_PUBLIC | ZEND_ACC_DTOR)
    PHP_ME(swoole_server, listen, arginfo_swoole_server_listen, ZEND_ACC_PUBLIC)
    PHP_MALIAS(swoole_server, addlistener, listen, arginfo_swoole_server_listen, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_server, on, arginfo_swoole_server_on, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_server, set, arginfo_swoole_server_set_oo, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_server, start, arginfo_swoole_void, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_server, send, arginfo_swoole_server_send_oo, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_server, sendto, arginfo_swoole_server_sendto, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_server, sendwait, arginfo_swoole_server_sendwait, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_server, exist, arginfo_swoole_server_exist, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_server, protect, arginfo_swoole_server_protect, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_server, sendfile, arginfo_swoole_server_sendfile, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_server, close, arginfo_swoole_server_close, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_server, confirm, arginfo_swoole_server_confirm, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_server, pause, arginfo_swoole_server_pause, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_server, resume, arginfo_swoole_server_resume, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_server, task, arginfo_swoole_server_task, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_server, taskwait, arginfo_swoole_server_taskwait, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_server, taskWaitMulti, arginfo_swoole_server_taskWaitMulti_oo, ZEND_ACC_PUBLIC)
#ifdef SW_COROUTINE
    PHP_ME(swoole_server, taskCo, arginfo_swoole_server_taskCo, ZEND_ACC_PUBLIC)
#endif
    PHP_ME(swoole_server, finish, arginfo_swoole_server_finish_oo, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_server, reload, arginfo_swoole_server_reload_oo, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_server, shutdown, arginfo_swoole_void, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_server, stop, arginfo_swoole_server_stop, ZEND_ACC_PUBLIC)
    PHP_FALIAS(getLastError, swoole_last_error, arginfo_swoole_void)
    PHP_ME(swoole_server, heartbeat, arginfo_swoole_server_heartbeat_oo, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_server, connection_info, arginfo_swoole_connection_info, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_server, connection_list, arginfo_swoole_connection_list, ZEND_ACC_PUBLIC)
    //psr-0 style
    PHP_MALIAS(swoole_server, getClientInfo, connection_info, arginfo_swoole_connection_info, ZEND_ACC_PUBLIC)
    PHP_MALIAS(swoole_server, getClientList, connection_list, arginfo_swoole_connection_list, ZEND_ACC_PUBLIC)
    //timer
    // 函数的别名注册 ,第一个参数是别名，第二个是上面定义的函数
    PHP_FALIAS(after, swoole_timer_after, arginfo_swoole_timer_after)
    PHP_FALIAS(tick, swoole_timer_tick, arginfo_swoole_timer_tick)
    PHP_FALIAS(clearTimer, swoole_timer_clear, arginfo_swoole_timer_clear)
    PHP_FALIAS(defer, swoole_event_defer, arginfo_swoole_event_defer)
    //process
    PHP_ME(swoole_server, sendMessage, arginfo_swoole_server_sendMessage, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_server, addProcess, arginfo_swoole_server_addProcess, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_server, stats, arginfo_swoole_void, ZEND_ACC_PUBLIC)
#ifdef SWOOLE_SOCKETS_SUPPORT
    PHP_ME(swoole_server, getSocket, arginfo_swoole_server_getSocket, ZEND_ACC_PUBLIC)
#endif
#ifdef SW_BUFFER_RECV_TIME
    PHP_ME(swoole_server, getReceivedTime, arginfo_swoole_void, ZEND_ACC_PUBLIC)
#endif
    PHP_ME(swoole_server, bind, arginfo_swoole_server_bind, ZEND_ACC_PUBLIC)
    {NULL, NULL, NULL}
};

//connection_iterator class 函数
static const zend_function_entry swoole_connection_iterator_methods[] =
{
    PHP_ME(swoole_connection_iterator, rewind,      arginfo_swoole_void, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_connection_iterator, next,        arginfo_swoole_void, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_connection_iterator, current,     arginfo_swoole_void, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_connection_iterator, key,         arginfo_swoole_void, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_connection_iterator, valid,       arginfo_swoole_void, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_connection_iterator, count,       arginfo_swoole_void, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_connection_iterator, __destruct,  arginfo_swoole_void, ZEND_ACC_PUBLIC | ZEND_ACC_DTOR)
    PHP_ME(swoole_connection_iterator, offsetExists,    arginfo_swoole_connection_iterator_offsetExists, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_connection_iterator, offsetGet,       arginfo_swoole_connection_iterator_offsetGet, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_connection_iterator, offsetSet,       arginfo_swoole_connection_iterator_offsetSet, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_connection_iterator, offsetUnset,     arginfo_swoole_connection_iterator_offsetUnset, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

//swoole_timer 的函数，tick 是别名，真实调用的是函数zif_XXX（第二个参数）
static const zend_function_entry swoole_timer_methods[] =
{ 
    ZEND_FENTRY(tick, ZEND_FN(swoole_timer_tick), arginfo_swoole_timer_tick, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    ZEND_FENTRY(after, ZEND_FN(swoole_timer_after), arginfo_swoole_timer_after, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    ZEND_FENTRY(exists, ZEND_FN(swoole_timer_exists), arginfo_swoole_timer_exists, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    ZEND_FENTRY(clear, ZEND_FN(swoole_timer_clear), arginfo_swoole_timer_clear, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_FE_END
};

//swoole_event 事件函数
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
};

static const zend_function_entry swoole_async_methods[] =
{
    ZEND_FENTRY(read, ZEND_FN(swoole_async_read), arginfo_swoole_async_read, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    ZEND_FENTRY(write, ZEND_FN(swoole_async_write), arginfo_swoole_async_write, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    ZEND_FENTRY(readFile, ZEND_FN(swoole_async_readfile), arginfo_swoole_async_readfile, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    ZEND_FENTRY(writeFile, ZEND_FN(swoole_async_writefile), arginfo_swoole_async_writefile, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    ZEND_FENTRY(dnsLookup, ZEND_FN(swoole_async_dns_lookup), arginfo_swoole_async_dns_lookup, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
#ifdef SW_COROUTINE
    ZEND_FENTRY(dnsLookupCoro, ZEND_FN(swoole_async_dns_lookup_coro), arginfo_swoole_async_dns_lookup_coro, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
#endif
    ZEND_FENTRY(set, ZEND_FN(swoole_async_set), arginfo_swoole_async_set, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(swoole_async, exec, arginfo_swoole_async_exec, ZEND_ACC_PUBLIC| ZEND_ACC_STATIC)
    PHP_FE_END
};

#if PHP_MEMORY_DEBUG
php_vmstat_t php_vmstat;
#endif
//定义 class 类型
zend_class_entry swoole_server_ce;
zend_class_entry *swoole_server_class_entry_ptr;

zend_class_entry swoole_connection_iterator_ce;
zend_class_entry *swoole_connection_iterator_class_entry_ptr;

static zend_class_entry swoole_timer_ce;
static zend_class_entry *swoole_timer_class_entry_ptr;

static zend_class_entry swoole_event_ce;
static zend_class_entry *swoole_event_class_entry_ptr;

static zend_class_entry swoole_async_ce;
static zend_class_entry *swoole_async_class_entry_ptr;

zend_class_entry swoole_exception_ce;
zend_class_entry *swoole_exception_class_entry_ptr;


//module entry, php 调用模块时，会使用这个结构体
zend_module_entry swoole_module_entry =
{
#if ZEND_MODULE_API_NO >= 20050922
    STANDARD_MODULE_HEADER_EX,
    NULL,
    NULL,
#else
    STANDARD_MODULE_HEADER,
#endif
    "swoole",        //扩展模块名
    swoole_functions,//函数结构体
    PHP_MINIT(swoole), //模块初始化
    PHP_MSHUTDOWN(swoole),//模块关闭
    PHP_RINIT(swoole),     //RINIT   请求启动
    PHP_RSHUTDOWN(swoole), //RSHUTDOWN 请求结束
    PHP_MINFO(swoole),    //模块情报
    PHP_SWOOLE_VERSION,
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_SWOOLE
ZEND_GET_MODULE(swoole)
#endif

/* {{{ PHP_INI
 */
// php.ini 参数设定，赋值到 zend_swoole_globals
PHP_INI_BEGIN()  // 宏展开 =static const zend_ini_entry_def ini_entries[] = {


STD_PHP_INI_ENTRY("swoole.aio_thread_num", "2", PHP_INI_ALL, OnUpdateLong, aio_thread_num, zend_swoole_globals, swoole_globals)
//宏展开 = { name, on_modify, arg1, arg2, arg3, default_value, displayer, modifiable, sizeof(name)-1, sizeof(default_value)-1 },
//zend_swoole_globals 是全局结构体，php_swoole.h 中ZEND_BEGIN_MODULE_GLOBALS(swoole)  定义了 zend_swoole_globals 类型。
//php_swoole.h 中 extern ZEND_DECLARE_MODULE_GLOBALS(swoole);  定义了 zend_swoole_globals 类型的变量 swoole_globals。
// PHP_INI_ALL  是这个参数可以修改的范围。 OnUpdateXXX是当赋值给全局变量前，会调用的函数
// on off 都会转为zend_bool 型 on 为1, off 为0
STD_PHP_INI_ENTRY("swoole.display_errors", "On", PHP_INI_ALL, OnUpdateBool, display_errors, zend_swoole_globals, swoole_globals)
/**
 * namespace class style
 */
STD_PHP_INI_ENTRY("swoole.use_namespace", "On", PHP_INI_SYSTEM, OnUpdateBool, use_namespace, zend_swoole_globals, swoole_globals)
/**
 * use an short class name
 */
STD_PHP_INI_ENTRY("swoole.use_shortname", "On", PHP_INI_SYSTEM, OnUpdateBool, use_shortname, zend_swoole_globals, swoole_globals)
/**
 * enable swoole_serialize
 */
STD_PHP_INI_ENTRY("swoole.fast_serialize", "Off", PHP_INI_ALL, OnUpdateBool, fast_serialize, zend_swoole_globals, swoole_globals)
/**
 * Unix socket buffer size
 */
STD_PHP_INI_ENTRY("swoole.unixsock_buffer_size", "8388608", PHP_INI_ALL, OnUpdateLong, socket_buffer_size, zend_swoole_globals, swoole_globals)
PHP_INI_END()

//swoole_globals 全局参数设定（默认值设定）
//ZEND_INIT_MODULE_GLOBALS(swoole, php_swoole_init_globals, NULL);
static void php_swoole_init_globals(zend_swoole_globals *swoole_globals)
{
    swoole_globals->aio_thread_num = SW_AIO_THREAD_NUM_DEFAULT;
    swoole_globals->socket_buffer_size = SW_SOCKET_BUFFER_SIZE;
    swoole_globals->display_errors = 1;
    swoole_globals->use_namespace = 1;
    swoole_globals->use_shortname = 1;
    swoole_globals->fast_serialize = 0;
    swoole_globals->rshutdown_functions = NULL;
}

// 调用的地方 cli->protocol.get_package_length = php_swoole_length_func;
//去数据包长度
int php_swoole_length_func(swProtocol *protocol, swConnection *conn, char *data, uint32_t length)
{
    SwooleG.lock.lock(&SwooleG.lock);

    zval *zdata;
    zval *retval = NULL;

    SW_MAKE_STD_ZVAL(zdata);
    SW_ZVAL_STRINGL(zdata, data, length, 1);

    zval **args[1];
    args[0] = &zdata;
    //private_data 是php 函数，用于自定义获取包大小。
    //swoole_client.c的if (php_swoole_array_get_value(vht, "package_length_func", v))处
    zval *callback = protocol->private_data;
    //判断是否为可以调用的函数
    if (sw_call_user_function_ex(EG(function_table), NULL, callback, &retval, 1, args, 0, NULL TSRMLS_CC) == FAILURE)
    {
        swoole_php_fatal_error(E_WARNING, "length function handler error.");
        goto error;
    }
    if (EG(exception))
    {
        zend_exception_error(EG(exception), E_ERROR TSRMLS_CC);
        goto error;
    }
    //销毁zdata
    sw_zval_ptr_dtor(&zdata);
    if (retval != NULL) //返回结果不为NULL
    {
        convert_to_long(retval);//转为long
        int length = Z_LVAL_P(retval);
        sw_zval_ptr_dtor(&retval);//销毁retval
        SwooleG.lock.unlock(&SwooleG.lock); //释放锁，防止多个进程进入
        return length;
    }
    error:
    SwooleG.lock.unlock(&SwooleG.lock);
    return -1;
}

//serv->dispatch_func = func;
// 自定义连接分配函数 返回结果是worker_id ，即把这个连接请求分配给哪个work 进程
int php_swoole_dispatch_func(swServer *serv, swConnection *conn, swEventData *data)
{
    SwooleG.lock.lock(&SwooleG.lock);

    zval *zserv = (zval *) serv->ptr2;

    zval *zdata;
    zval *zfd;
    zval *ztype;
    zval *retval = NULL;

    SW_MAKE_STD_ZVAL(zdata);
    SW_ZVAL_STRINGL(zdata, data->data, data->info.len, 1);

    SW_MAKE_STD_ZVAL(zfd);
    ZVAL_LONG(zfd, (long ) conn->session_id);

    SW_MAKE_STD_ZVAL(ztype);
    ZVAL_LONG(ztype, (long ) data->info.type);

    zval **args[4];
    args[0] = &zserv;
    args[1] = &zfd;
    args[2] = &ztype;
    args[3] = &zdata;

    zval *callback = (zval*) serv->private_data_3;
    if (sw_call_user_function_ex(EG(function_table), NULL, callback, &retval, 4, args, 0, NULL TSRMLS_CC) == FAILURE)
    {
        swoole_php_fatal_error(E_WARNING, "dispatch function handler error.");
        goto error;
    }
    if (EG(exception))
    {
        zend_exception_error(EG(exception), E_ERROR TSRMLS_CC);
        goto error;
    }
    sw_zval_ptr_dtor(&zfd);
    sw_zval_ptr_dtor(&ztype);
    sw_zval_ptr_dtor(&zdata);
    if (retval != NULL)
    {
        convert_to_long(retval);
        int worker_id = (int) Z_LVAL_P(retval);
        if (worker_id >= serv->worker_num)
        {
            swoole_php_fatal_error(E_WARNING, "invalid target worker-id[%d].", worker_id);
            goto error;
        }
        sw_zval_ptr_dtor(&retval);
        SwooleG.lock.unlock(&SwooleG.lock);
        return worker_id;
    }
    error:
    SwooleG.lock.unlock(&SwooleG.lock);
    return -1;
}

//取得分配新内存大小
static sw_inline uint32_t swoole_get_new_size(uint32_t old_size, int handle TSRMLS_DC)
{
    uint32_t new_size = old_size * 2; //原来的2倍
    if (handle > SWOOLE_OBJECT_MAX) //大于10000000报错
    {
        swoole_php_fatal_error(E_ERROR, "handle %d exceed %d", handle, SWOOLE_OBJECT_MAX);
        return 0;
    }
    while (new_size <= handle) //new size < object 的handle 时，变成newsize 的2倍
    {
        new_size *= 2;
    }
    if (new_size > SWOOLE_OBJECT_MAX) //newsize > 10000000 ,newsize = 10000000
    {
        new_size = SWOOLE_OBJECT_MAX;
    }
    return new_size;
}

//给对象绑定值
//比如 php_swoole_server_add_port 监听端口增加函数中
//object_init_ex(port_object, swoole_server_port_class_entry_ptr);  //实例化一个swoole_server_port_class
//swoole_set_object(port_object, port);//把port信息绑定到port_object 中

//swoole_objects 结构体定义
//typedef struct
// {
//     void **array;
//     uint32_t size;
//     void **property[SWOOLE_PROPERTY_MAX];
//     uint32_t property_size[SWOOLE_PROPERTY_MAX];
// } swoole_object_array;

void swoole_set_object(zval *object, void *ptr)
{
    int handle = sw_get_object_handle(object);//取得对象的handle
    assert(handle < SWOOLE_OBJECT_MAX); //不能大于10000000
    // swoole_objects 是一个全局对象保存容器，模块初期时被设定成以下
    //swoole_objects.size = 65536;  //64k
    //swoole_objects.array = calloc(swoole_objects.size, sizeof(void*)); //向系统申请64K*8的内存来存放64K大小的指针。

    if (handle >= swoole_objects.size)// handle 的数量大于=65536(初期)，要向系统分配内存
    {
        uint32_t old_size = swoole_objects.size;
        uint32_t new_size = swoole_get_new_size(old_size, handle TSRMLS_CC);//新分配的内存大小

        void *old_ptr = swoole_objects.array;//old 指针
        void *new_ptr = NULL;

        new_ptr = realloc(old_ptr, sizeof(void*) * new_size); //重新分配
        if (!new_ptr)
        {
            swoole_php_fatal_error(E_ERROR, "malloc(%d) failed.", (int )(new_size * sizeof(void *)));
            return;
        }
        bzero(new_ptr + (old_size * sizeof(void*)), (new_size - old_size) * sizeof(void*));//新申请的内存初始化
        swoole_objects.array = new_ptr;
        swoole_objects.size = new_size;
    }
    swoole_objects.array[handle] = ptr; //把ptr 指针放到swoole_objects.array中
}

//属性设定到 swoole_objects
void swoole_set_property(zval *object, int property_id, void *ptr)
{
    int handle = sw_get_object_handle(object);
    assert(handle < SWOOLE_OBJECT_MAX);

    if (handle >= swoole_objects.property_size[property_id])
    {
        uint32_t old_size = swoole_objects.property_size[property_id];
        uint32_t new_size = 0;

        void **old_ptr = NULL;
        void **new_ptr = NULL;

        if (old_size == 0)
        {
            new_size = 65536;
            new_ptr = calloc(new_size, sizeof(void *));
        }
        else
        {
            new_size = swoole_get_new_size(old_size, handle TSRMLS_CC);
            old_ptr = swoole_objects.property[property_id];
            new_ptr = realloc(old_ptr, new_size * sizeof(void *));
        }
        if (new_ptr == NULL)
        {
            swoole_php_fatal_error(E_ERROR, "malloc(%d) failed.", (int )(new_size * sizeof(void *)));
            return;
        }
        if (old_size > 0)
        {
            bzero((void *) new_ptr + old_size * sizeof(void*), (new_size - old_size) * sizeof(void*));
        }
        swoole_objects.property_size[property_id] = new_size;
        swoole_objects.property[property_id] = new_ptr;
    }
    swoole_objects.property[property_id][handle] = ptr;
}

//没有地方用到 
// 追加请求关闭时调用的函数。可以放入多个函数
//SWOOLE_G(rshutdown_functions) = swoole_globals.rshutdown_functions  初期rshutdown_functions = NULL
int swoole_register_rshutdown_function(swCallback func, int push_back)
{
    if (SWOOLE_G(rshutdown_functions) == NULL)
    {
        SWOOLE_G(rshutdown_functions) = swLinkedList_new(0, NULL);
        if (SWOOLE_G(rshutdown_functions) == NULL)
        {
            return SW_ERR;
        }
    }
    if (push_back)
    {
        return swLinkedList_append(SWOOLE_G(rshutdown_functions), func);
    }
    else
    {
        return swLinkedList_prepend(SWOOLE_G(rshutdown_functions), func);
    }
}
//PHP_RSHUTDOWN_FUNCTION 请求关闭时调用
void swoole_call_rshutdown_function(void *arg)
{
    if (SWOOLE_G(rshutdown_functions)) //如果注册了就可以执行
    {
        swLinkedList *rshutdown_functions = SWOOLE_G(rshutdown_functions);
        swLinkedList_node *node = rshutdown_functions->head;
        swCallback func = NULL;//typedef void (*swCallback)(void *data);

        while (node)
        {
            func = node->data;
            func(arg);
            node = node->next;
        }
    }
}

#ifdef ZTS
__thread swoole_object_array swoole_objects;
void ***sw_thread_ctx;
#else
swoole_object_array swoole_objects;
#endif

/* {{{ PHP_MINIT_FUNCTION
 */
//模块初期化
PHP_MINIT_FUNCTION(swoole)
{
    //ini参数设定
    ZEND_INIT_MODULE_GLOBALS(swoole, php_swoole_init_globals, NULL);
    //调用 zend_register_ini_entries ，注册php.ini 参数
    REGISTER_INI_ENTRIES(); 

    /**
     * mode type
     */
    //常量注册
    //REGISTER_LONG_CONSTANT 展开后 = zend_register_long_constant((name), sizeof(name)-1, (lval), (flags), module_number)
    REGISTER_LONG_CONSTANT("SWOOLE_BASE", SW_MODE_SINGLE, CONST_CS | CONST_PERSISTENT); //基本模式
    REGISTER_LONG_CONSTANT("SWOOLE_THREAD", SW_MODE_THREAD, CONST_CS | CONST_PERSISTENT);//线程模式？？ 官方文档没有提及 目前代码里面也没有使用这个常量
    REGISTER_LONG_CONSTANT("SWOOLE_PROCESS", SW_MODE_PROCESS, CONST_CS | CONST_PERSISTENT);//多进程模式（默认）

    /**
     * task ipc mode
     */
    //task 进程通信模式
    REGISTER_LONG_CONSTANT("SWOOLE_IPC_UNSOCK", SW_TASK_IPC_UNIXSOCK, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SWOOLE_IPC_MSGQUEUE", SW_TASK_IPC_MSGQUEUE, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SWOOLE_IPC_PREEMPTIVE", SW_TASK_IPC_PREEMPTIVE, CONST_CS | CONST_PERSISTENT);

    /**
     * socket type
     */
    //socket 类型
    REGISTER_LONG_CONSTANT("SWOOLE_SOCK_TCP", SW_SOCK_TCP, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SWOOLE_SOCK_TCP6", SW_SOCK_TCP6, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SWOOLE_SOCK_UDP", SW_SOCK_UDP, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SWOOLE_SOCK_UDP6", SW_SOCK_UDP6, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SWOOLE_SOCK_UNIX_DGRAM", SW_SOCK_UNIX_DGRAM, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SWOOLE_SOCK_UNIX_STREAM", SW_SOCK_UNIX_STREAM, CONST_CS | CONST_PERSISTENT);

    /**
     * simple api
     */

    REGISTER_LONG_CONSTANT("SWOOLE_TCP", SW_SOCK_TCP, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SWOOLE_TCP6", SW_SOCK_TCP6, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SWOOLE_UDP", SW_SOCK_UDP, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SWOOLE_UDP6", SW_SOCK_UDP6, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SWOOLE_UNIX_DGRAM", SW_SOCK_UNIX_DGRAM, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SWOOLE_UNIX_STREAM", SW_SOCK_UNIX_STREAM, CONST_CS | CONST_PERSISTENT);

    /**
     * simple api
     */
    REGISTER_LONG_CONSTANT("SWOOLE_SOCK_SYNC", SW_SOCK_SYNC, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SWOOLE_SOCK_ASYNC", SW_SOCK_ASYNC, CONST_CS | CONST_PERSISTENT);

    REGISTER_LONG_CONSTANT("SWOOLE_SYNC", SW_FLAG_SYNC, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SWOOLE_ASYNC", SW_FLAG_ASYNC, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SWOOLE_KEEP", SW_FLAG_KEEP, CONST_CS | CONST_PERSISTENT);

#ifdef SW_USE_OPENSSL
    REGISTER_LONG_CONSTANT("SWOOLE_SSL", SW_SOCK_SSL, CONST_CS | CONST_PERSISTENT);

    /**
     * SSL method
     */
    REGISTER_LONG_CONSTANT("SWOOLE_SSLv3_METHOD", SW_SSLv3_METHOD, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SWOOLE_SSLv3_SERVER_METHOD", SW_SSLv3_SERVER_METHOD, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SWOOLE_SSLv3_CLIENT_METHOD", SW_SSLv3_CLIENT_METHOD, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SWOOLE_SSLv23_METHOD", SW_SSLv23_METHOD, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SWOOLE_SSLv23_SERVER_METHOD", SW_SSLv23_SERVER_METHOD, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SWOOLE_SSLv23_CLIENT_METHOD", SW_SSLv23_CLIENT_METHOD, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SWOOLE_TLSv1_METHOD", SW_TLSv1_METHOD, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SWOOLE_TLSv1_SERVER_METHOD", SW_TLSv1_SERVER_METHOD, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SWOOLE_TLSv1_CLIENT_METHOD", SW_TLSv1_CLIENT_METHOD, CONST_CS | CONST_PERSISTENT);
#ifdef TLS1_1_VERSION
    REGISTER_LONG_CONSTANT("SWOOLE_TLSv1_1_METHOD", SW_TLSv1_1_METHOD, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SWOOLE_TLSv1_1_SERVER_METHOD", SW_TLSv1_1_SERVER_METHOD, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SWOOLE_TLSv1_1_CLIENT_METHOD", SW_TLSv1_1_CLIENT_METHOD, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef TLS1_2_VERSION
    REGISTER_LONG_CONSTANT("SWOOLE_TLSv1_2_METHOD", SW_TLSv1_2_METHOD, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SWOOLE_TLSv1_2_SERVER_METHOD", SW_TLSv1_2_SERVER_METHOD, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SWOOLE_TLSv1_2_CLIENT_METHOD", SW_TLSv1_2_CLIENT_METHOD, CONST_CS | CONST_PERSISTENT);
#endif
    REGISTER_LONG_CONSTANT("SWOOLE_DTLSv1_METHOD", SW_DTLSv1_METHOD, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SWOOLE_DTLSv1_SERVER_METHOD", SW_DTLSv1_SERVER_METHOD, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SWOOLE_DTLSv1_CLIENT_METHOD", SW_DTLSv1_CLIENT_METHOD, CONST_CS | CONST_PERSISTENT);
#endif

    REGISTER_LONG_CONSTANT("SWOOLE_EVENT_READ", SW_EVENT_READ, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SWOOLE_EVENT_WRITE", SW_EVENT_WRITE, CONST_CS | CONST_PERSISTENT);

    REGISTER_STRINGL_CONSTANT("SWOOLE_VERSION", PHP_SWOOLE_VERSION, sizeof(PHP_SWOOLE_VERSION) - 1, CONST_CS | CONST_PERSISTENT);

    //ErrorCode
    //SWOOLE_DEFINE 宏展开=  REGISTER_LONG_CONSTANT("SWOOLE_"#constant, SW_##constant, CONST_CS | CONST_PERSISTENT)
    //# 意思是定义一个字符串 "SWOOLE_"#constant 变成 "SWOOLE_ERROR_MALLOC_FAIL"
    //## 是连接  SW_##constant 变成 SW_ERROR_MALLOC_FAIL
    SWOOLE_DEFINE(ERROR_MALLOC_FAIL);
    SWOOLE_DEFINE(ERROR_SYSTEM_CALL_FAIL);
    SWOOLE_DEFINE(ERROR_PHP_FATAL_ERROR);
    SWOOLE_DEFINE(ERROR_NAME_TOO_LONG);
    SWOOLE_DEFINE(ERROR_INVALID_PARAMS);
    SWOOLE_DEFINE(ERROR_FILE_NOT_EXIST);
    SWOOLE_DEFINE(ERROR_FILE_TOO_LARGE);
    SWOOLE_DEFINE(ERROR_FILE_EMPTY);
    SWOOLE_DEFINE(ERROR_DNSLOOKUP_DUPLICATE_REQUEST);
    SWOOLE_DEFINE(ERROR_DNSLOOKUP_RESOLVE_FAILED);
    SWOOLE_DEFINE(ERROR_SESSION_CLOSED_BY_SERVER);
    SWOOLE_DEFINE(ERROR_SESSION_CLOSED_BY_CLIENT);
    SWOOLE_DEFINE(ERROR_SESSION_CLOSING);
    SWOOLE_DEFINE(ERROR_SESSION_CLOSED);
    SWOOLE_DEFINE(ERROR_SESSION_NOT_EXIST);
    SWOOLE_DEFINE(ERROR_SESSION_INVALID_ID);
    SWOOLE_DEFINE(ERROR_SESSION_DISCARD_TIMEOUT_DATA);
    SWOOLE_DEFINE(ERROR_OUTPUT_BUFFER_OVERFLOW);
    SWOOLE_DEFINE(ERROR_SSL_NOT_READY);
    SWOOLE_DEFINE(ERROR_SSL_CANNOT_USE_SENFILE);
    SWOOLE_DEFINE(ERROR_SSL_EMPTY_PEER_CERTIFICATE);
    SWOOLE_DEFINE(ERROR_SSL_VEFIRY_FAILED);
    SWOOLE_DEFINE(ERROR_SSL_BAD_CLIENT);
    SWOOLE_DEFINE(ERROR_SSL_BAD_PROTOCOL);
    SWOOLE_DEFINE(ERROR_PACKAGE_LENGTH_TOO_LARGE);
    SWOOLE_DEFINE(ERROR_DATA_LENGTH_TOO_LARGE);
    SWOOLE_DEFINE(ERROR_TASK_PACKAGE_TOO_BIG);
    SWOOLE_DEFINE(ERROR_TASK_DISPATCH_FAIL);

    /**
     * AIO
     */
    SWOOLE_DEFINE(ERROR_AIO_BAD_REQUEST);

    /**
     * Client
     */
    SWOOLE_DEFINE(ERROR_CLIENT_NO_CONNECTION);

    SWOOLE_DEFINE(ERROR_HTTP2_STREAM_ID_TOO_BIG);
    SWOOLE_DEFINE(ERROR_HTTP2_STREAM_NO_HEADER);
    SWOOLE_DEFINE(ERROR_SOCKS5_UNSUPPORT_VERSION);
    SWOOLE_DEFINE(ERROR_SOCKS5_UNSUPPORT_METHOD);
    SWOOLE_DEFINE(ERROR_SOCKS5_AUTH_FAILED);
    SWOOLE_DEFINE(ERROR_SOCKS5_SERVER_ERROR);
    SWOOLE_DEFINE(ERROR_HTTP_PROXY_HANDSHAKE_ERROR);
    SWOOLE_DEFINE(ERROR_HTTP_INVALID_PROTOCOL);
    SWOOLE_DEFINE(ERROR_WEBSOCKET_BAD_CLIENT);
    SWOOLE_DEFINE(ERROR_WEBSOCKET_BAD_OPCODE);
    SWOOLE_DEFINE(ERROR_WEBSOCKET_UNCONNECTED);
    SWOOLE_DEFINE(ERROR_WEBSOCKET_HANDSHAKE_FAILED);
    SWOOLE_DEFINE(ERROR_SERVER_MUST_CREATED_BEFORE_CLIENT);
    SWOOLE_DEFINE(ERROR_SERVER_TOO_MANY_SOCKET);
    SWOOLE_DEFINE(ERROR_SERVER_WORKER_TERMINATED);
    SWOOLE_DEFINE(ERROR_SERVER_INVALID_LISTEN_PORT);
    SWOOLE_DEFINE(ERROR_SERVER_TOO_MANY_LISTEN_PORT);
    SWOOLE_DEFINE(ERROR_SERVER_PIPE_BUFFER_FULL);
    SWOOLE_DEFINE(ERROR_SERVER_NO_IDLE_WORKER);
    SWOOLE_DEFINE(ERROR_SERVER_ONLY_START_ONE);
    SWOOLE_DEFINE(ERROR_SERVER_WORKER_EXIT_TIMEOUT);

    /**
     * trace log
     */
    //TraceType
    SWOOLE_DEFINE(TRACE_SERVER);
    SWOOLE_DEFINE(TRACE_CLIENT);
    SWOOLE_DEFINE(TRACE_BUFFER);
    SWOOLE_DEFINE(TRACE_CONN);
    SWOOLE_DEFINE(TRACE_EVENT);
    SWOOLE_DEFINE(TRACE_WORKER);
    SWOOLE_DEFINE(TRACE_REACTOR);
    SWOOLE_DEFINE(TRACE_PHP);
    SWOOLE_DEFINE(TRACE_HTTP2);
    SWOOLE_DEFINE(TRACE_EOF_PROTOCOL);
    SWOOLE_DEFINE(TRACE_LENGTH_PROTOCOL);
    SWOOLE_DEFINE(TRACE_CLOSE);
    SWOOLE_DEFINE(TRACE_HTTP_CLIENT);
    SWOOLE_DEFINE(TRACE_COROUTINE);
    SWOOLE_DEFINE(TRACE_REDIS_CLIENT);
    SWOOLE_DEFINE(TRACE_MYSQL_CLIENT);
    SWOOLE_DEFINE(TRACE_AIO);
    REGISTER_LONG_CONSTANT("SWOOLE_TRACE_ALL", 0xffffffff, CONST_CS | CONST_PERSISTENT);

    /**
     * log level
     */
    SWOOLE_DEFINE(LOG_DEBUG);
    SWOOLE_DEFINE(LOG_TRACE);
    SWOOLE_DEFINE(LOG_INFO);
    SWOOLE_DEFINE(LOG_NOTICE);
    SWOOLE_DEFINE(LOG_WARNING);
    SWOOLE_DEFINE(LOG_ERROR);

    SWOOLE_DEFINE(IPC_NONE);
    SWOOLE_DEFINE(IPC_UNIXSOCK);
    SWOOLE_DEFINE(IPC_SOCKET);

    //swoole_server 定义
    //SWOOLE_INIT_CLASS_ENTRY 宏展开是
    //SWOOLE_INIT_CLASS_ENTRY(ce, name, name_ns, methods) \
     if (SWOOLE_G(use_namespace)) { \
        INIT_CLASS_ENTRY(ce, name_ns, methods); \
    } else { \
        INIT_CLASS_ENTRY(ce, name, methods); \
    }
    //swoole_server_ce 是 zend_class_entry 类型
    //swoole_server_class_entry_ptr 是 zend_class_entry类型的指针
    SWOOLE_INIT_CLASS_ENTRY(swoole_server_ce, "swoole_server", "Swoole\\Server", swoole_server_methods);
    swoole_server_class_entry_ptr = zend_register_internal_class(&swoole_server_ce TSRMLS_CC);
    swoole_server_class_entry_ptr->serialize = zend_class_serialize_deny;//不能被序列化
    swoole_server_class_entry_ptr->unserialize = zend_class_unserialize_deny;//不能被序列化
    SWOOLE_CLASS_ALIAS(swoole_server, "Swoole\\Server");// 注册类别名

    if (!SWOOLE_G(use_shortname))//是否可以用短类名
    {
        sw_zend_hash_del(CG(function_table), ZEND_STRS("go"));
    }
    else
    {
        sw_zend_register_class_alias("Co\\Server", swoole_server_class_entry_ptr);
    }
    //swoole_server 的属性定义
    zend_declare_property_null(swoole_server_class_entry_ptr, ZEND_STRL("onConnect"), ZEND_ACC_PUBLIC TSRMLS_CC);
    zend_declare_property_null(swoole_server_class_entry_ptr, ZEND_STRL("onReceive"), ZEND_ACC_PUBLIC TSRMLS_CC);
    zend_declare_property_null(swoole_server_class_entry_ptr, ZEND_STRL("onClose"), ZEND_ACC_PUBLIC TSRMLS_CC);
    zend_declare_property_null(swoole_server_class_entry_ptr, ZEND_STRL("onPacket"), ZEND_ACC_PUBLIC TSRMLS_CC);
    zend_declare_property_null(swoole_server_class_entry_ptr, ZEND_STRL("onBufferFull"), ZEND_ACC_PUBLIC TSRMLS_CC);
    zend_declare_property_null(swoole_server_class_entry_ptr, ZEND_STRL("onBufferEmpty"), ZEND_ACC_PUBLIC TSRMLS_CC);
    zend_declare_property_null(swoole_server_class_entry_ptr, ZEND_STRL("onStart"), ZEND_ACC_PUBLIC TSRMLS_CC);
    zend_declare_property_null(swoole_server_class_entry_ptr, ZEND_STRL("onShutdown"), ZEND_ACC_PUBLIC TSRMLS_CC);
    zend_declare_property_null(swoole_server_class_entry_ptr, ZEND_STRL("onWorkerStart"), ZEND_ACC_PUBLIC TSRMLS_CC);
    zend_declare_property_null(swoole_server_class_entry_ptr, ZEND_STRL("onWorkerStop"), ZEND_ACC_PUBLIC TSRMLS_CC);
    zend_declare_property_null(swoole_server_class_entry_ptr, ZEND_STRL("onWorkerExit"), ZEND_ACC_PUBLIC TSRMLS_CC);
    zend_declare_property_null(swoole_server_class_entry_ptr, ZEND_STRL("onWorkerError"), ZEND_ACC_PUBLIC TSRMLS_CC);
    zend_declare_property_null(swoole_server_class_entry_ptr, ZEND_STRL("onTask"), ZEND_ACC_PUBLIC TSRMLS_CC);
    zend_declare_property_null(swoole_server_class_entry_ptr, ZEND_STRL("onFinish"), ZEND_ACC_PUBLIC TSRMLS_CC);
    zend_declare_property_null(swoole_server_class_entry_ptr, ZEND_STRL("onManagerStart"), ZEND_ACC_PUBLIC TSRMLS_CC);
    zend_declare_property_null(swoole_server_class_entry_ptr, ZEND_STRL("onManagerStop"), ZEND_ACC_PUBLIC TSRMLS_CC);
    zend_declare_property_null(swoole_server_class_entry_ptr, ZEND_STRL("onPipeMessage"), ZEND_ACC_PUBLIC TSRMLS_CC);

    zend_declare_property_null(swoole_server_class_entry_ptr, ZEND_STRL("setting"), ZEND_ACC_PUBLIC TSRMLS_CC);
    zend_declare_property_null(swoole_server_class_entry_ptr, ZEND_STRL("connections"), ZEND_ACC_PUBLIC TSRMLS_CC);
    zend_declare_property_null(swoole_server_class_entry_ptr, ZEND_STRL("host"), ZEND_ACC_PUBLIC TSRMLS_CC);
    zend_declare_property_long(swoole_server_class_entry_ptr, ZEND_STRL("port"), 0, ZEND_ACC_PUBLIC TSRMLS_CC);
    zend_declare_property_long(swoole_server_class_entry_ptr, ZEND_STRL("type"), 0, ZEND_ACC_PUBLIC TSRMLS_CC);
    zend_declare_property_long(swoole_server_class_entry_ptr, ZEND_STRL("mode"), 0, ZEND_ACC_PUBLIC TSRMLS_CC);
    zend_declare_property_null(swoole_server_class_entry_ptr, ZEND_STRL("ports"), ZEND_ACC_PUBLIC TSRMLS_CC);
    zend_declare_property_long(swoole_server_class_entry_ptr, ZEND_STRL("master_pid"), 0, ZEND_ACC_PUBLIC TSRMLS_CC);
    zend_declare_property_long(swoole_server_class_entry_ptr, ZEND_STRL("manager_pid"), 0, ZEND_ACC_PUBLIC TSRMLS_CC);
    zend_declare_property_long(swoole_server_class_entry_ptr, ZEND_STRL("worker_id"), -1, ZEND_ACC_PUBLIC TSRMLS_CC);
    zend_declare_property_bool(swoole_server_class_entry_ptr, ZEND_STRL("taskworker"), 0, ZEND_ACC_PUBLIC TSRMLS_CC);
    zend_declare_property_long(swoole_server_class_entry_ptr, ZEND_STRL("worker_pid"), 0, ZEND_ACC_PUBLIC TSRMLS_CC);

    //swoole_timer 
    SWOOLE_INIT_CLASS_ENTRY(swoole_timer_ce, "swoole_timer", "Swoole\\Timer", swoole_timer_methods);
    swoole_timer_class_entry_ptr = zend_register_internal_class(&swoole_timer_ce TSRMLS_CC);
    SWOOLE_CLASS_ALIAS(swoole_timer, "Swoole\\Timer");
    //swoole_event
    SWOOLE_INIT_CLASS_ENTRY(swoole_event_ce, "swoole_event", "Swoole\\Event", swoole_event_methods);
    swoole_event_class_entry_ptr = zend_register_internal_class(&swoole_event_ce TSRMLS_CC);
    SWOOLE_CLASS_ALIAS(swoole_event, "Swoole\\Event");
    //swoole_async
    SWOOLE_INIT_CLASS_ENTRY(swoole_async_ce, "swoole_async", "Swoole\\Async", swoole_async_methods);
    swoole_async_class_entry_ptr = zend_register_internal_class(&swoole_async_ce TSRMLS_CC);
    SWOOLE_CLASS_ALIAS(swoole_async, "Swoole\\Async");
    //swoole_connection_iterator
    SWOOLE_INIT_CLASS_ENTRY(swoole_connection_iterator_ce, "swoole_connection_iterator", "Swoole\\Connection\\Iterator",  swoole_connection_iterator_methods);
    swoole_connection_iterator_class_entry_ptr = zend_register_internal_class(&swoole_connection_iterator_ce TSRMLS_CC);
    SWOOLE_CLASS_ALIAS(swoole_connection_iterator, "Swoole\\Connection\\Iterator");
    //接口继承
    zend_class_implements(swoole_connection_iterator_class_entry_ptr TSRMLS_CC, 2, zend_ce_iterator, zend_ce_arrayaccess);
#ifdef SW_HAVE_COUNTABLE
    zend_class_implements(swoole_connection_iterator_class_entry_ptr TSRMLS_CC, 1, zend_ce_countable);
#endif
    //swoole_exception
    SWOOLE_INIT_CLASS_ENTRY(swoole_exception_ce, "swoole_exception", "Swoole\\Exception", NULL);
    swoole_exception_class_entry_ptr = sw_zend_register_internal_class_ex(&swoole_exception_ce, zend_exception_get_default(TSRMLS_C), NULL TSRMLS_CC);
    SWOOLE_CLASS_ALIAS(swoole_exception, "Swoole\\Exception");

    //swoole init
    // 初期化处理
    //src\core\base.c
    /*
    1、SwooleG 初始化
        swoole.h 中定义的系统级别的全局结构体
    2、申请共享内存
        SwooleG.memory_pool
    3、全局锁分配&&init global lock
        SwooleGS_t *SwooleGS;
    4、create tmp dir
    5、signal 初期化
    */
    swoole_init();

    /*注册 swoole_server_port    
    */

    swoole_server_port_init(module_number TSRMLS_CC);
    /*注册 swoole_client
      php_sw_long_connections = 创建32个 swHashMap；
    */
    swoole_client_init(module_number TSRMLS_CC);
#ifdef SW_COROUTINE
    /*注册 Swoole\\Coroutine\\Socket
      注册 Swoole\\Coroutine\\Socket\\Exception
     */
    swoole_socket_coro_init(module_number TSRMLS_CC);
    /*注册 Swoole\\Coroutine\\Client  短名称 Co\\Client
    */
    swoole_client_coro_init(module_number TSRMLS_CC);
#ifdef SW_USE_REDIS
    swoole_redis_coro_init(module_number TSRMLS_CC);
#endif
#ifdef SW_USE_POSTGRESQL
    swoole_postgresql_coro_init(module_number TSRMLS_CC);
#endif

    swoole_mysql_coro_init(module_number TSRMLS_CC);
    swoole_http_client_coro_init(module_number TSRMLS_CC);
	swoole_coroutine_util_init(module_number TSRMLS_CC);
#endif
    /*注册 swoole_http_client
     创建http_client_buffer= swString_new(SW_HTTP_RESPONSE_INIT_SIZE); 65536
        swString_new 结构体 
        typedef struct _swString
        {
            size_t length;
            size_t size;
            off_t offset;
            char *str;
        } swString;

        这个结果是数据缓存用的
    */
    swoole_http_client_init(module_number TSRMLS_CC);

    swoole_async_init(module_number TSRMLS_CC);
    /*注册 swoole_process 用于进程管理     
    */
    swoole_process_init(module_number TSRMLS_CC);
    /*注册 swoole_process_pool 用于进程池，对进程的管理管理     
    */
    swoole_process_pool_init(module_number TSRMLS_CC);
    /*注册 swoole_table 内存表，就是像表一样定义表的定义，并存储数据
    */
    swoole_table_init(module_number TSRMLS_CC);
    /*注册 Swoole\\Runtime 貌似没用到
    */
    swoole_runtime_init(module_number TSRMLS_CC);
    /*注册 swoole_lock 用于进程之间锁定临界数据区，达到数据安全读写。
    */
    swoole_lock_init(module_number TSRMLS_CC);
    /*注册 swoole_atomic 用于进程之间锁
    */
    swoole_atomic_init(module_number TSRMLS_CC);
    /* 注册 swoole_http_server 
       注册 swoole_http_response 
       注册 swoole_http_request 
    */
    swoole_http_server_init(module_number TSRMLS_CC);
    /* 注册 swoole_buffer 直接读写内存
    */
    swoole_buffer_init(module_number TSRMLS_CC);
    /* 注册 swoole_websocket_server
    */
    swoole_websocket_init(module_number TSRMLS_CC);
    /* 注册 swoole_mysql
    */
    swoole_mysql_init(module_number TSRMLS_CC);
    /* 注册 swoole_mmap
    */
    swoole_mmap_init(module_number TSRMLS_CC);
    /* 注册 swoole_channel 高性能内存队列
    */
    swoole_channel_init(module_number TSRMLS_CC);

#ifdef SW_COROUTINE
    /* 注册 协程版 Swoole\\Coroutine\\Channel 高性能内存队列
       短名称 chan
    */
    swoole_channel_coro_init(module_number TSRMLS_CC);
#endif
    //RingQueue
    swoole_ringqueue_init(module_number TSRMLS_CC);
    //消息队列
    swoole_msgqueue_init(module_number TSRMLS_CC);
#ifdef SW_USE_HTTP2
    swoole_http2_client_init(module_number TSRMLS_CC);
#ifdef SW_COROUTINE
    //协程版   Swoole\\Coroutine\\Http2\\Client
    swoole_http2_client_coro_init(module_number TSRMLS_CC);
#endif
#endif
    //swoole_serialize 初始化
    swoole_serialize_init(module_number TSRMLS_DC);
    //Swoole\\Memory\\Pool 初始化
    //共享内存读写
    swoole_memory_pool_init(module_number TSRMLS_DC);

#ifdef SW_USE_REDIS
    //注册 swoole_redis
    swoole_redis_init(module_number TSRMLS_CC);
#endif
    //注册 swoole_redis_server
    swoole_redis_server_init(module_number TSRMLS_CC);

    if (SWOOLE_G(socket_buffer_size) > 0)//php.ini 的buffer_size 赋值到 全局结构体 SwooleG中
    {
        SwooleG.socket_buffer_size = SWOOLE_G(socket_buffer_size);
    }
    //Linux __linux__
    //FreeBSD __FreeBSD__
    //Unix __unix__
    //Windows _WIN32 32位和64位系统都有定义
      //_WIN64 仅64位系统有定义 
#ifdef __MACH__  //Mac OS X 
    SwooleG.socket_buffer_size = 256 * 1024;
#endif

    //default 60s
    SwooleG.dns_cache_refresh_time = 60;


    if (SWOOLE_G(aio_thread_num) > 0)
    {
        if (SWOOLE_G(aio_thread_num) > SW_AIO_THREAD_NUM_MAX)
        {
            SWOOLE_G(aio_thread_num) = SW_AIO_THREAD_NUM_MAX;
        }
        SwooleAIO.thread_num = SWOOLE_G(aio_thread_num);
    }

    if (strcasecmp("cli", sapi_module.name) == 0) //判断是否为cli 模式
    {
        SWOOLE_G(cli) = 1;
    }

    swoole_objects.size = 65536; //swoole_objects对象的大小
    swoole_objects.array = calloc(swoole_objects.size, sizeof(void*)); //向系统申请分配内存

    return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINIT_FUNCTION
 */
//模块关闭
PHP_MSHUTDOWN_FUNCTION(swoole)
{
    swoole_clean();

    return SUCCESS;
}
/* }}} */


/* {{{ PHP_MINFO_FUNCTION
 */
// phpinfo 输出信息
PHP_MINFO_FUNCTION(swoole)
{
    php_info_print_table_start();
    php_info_print_table_header(2, "swoole support", "enabled");
    php_info_print_table_row(2, "Version", PHP_SWOOLE_VERSION);
    php_info_print_table_row(2, "Author", "Swoole Group[email: team@swoole.com]");

#ifdef SW_COROUTINE
    php_info_print_table_row(2, "coroutine", "enabled");
#endif
#if USE_BOOST_CONTEXT
    php_info_print_table_row(2, "boost.context", "enabled");
#endif
#if USE_UCONTEXT
    php_info_print_table_row(2, "ucontext", "enabled");
#endif
#ifdef HAVE_EPOLL
    php_info_print_table_row(2, "epoll", "enabled");
#endif
#ifdef HAVE_EVENTFD
    php_info_print_table_row(2, "eventfd", "enabled");
#endif
#ifdef HAVE_KQUEUE
    php_info_print_table_row(2, "kqueue", "enabled");
#endif
#ifdef HAVE_SIGNALFD
    php_info_print_table_row(2, "signalfd", "enabled");
#endif
#ifdef SW_USE_ACCEPT4
    php_info_print_table_row(2, "accept4", "enabled");
#endif
#ifdef HAVE_CPU_AFFINITY
    php_info_print_table_row(2, "cpu affinity", "enabled");
#endif
#ifdef HAVE_SPINLOCK
    php_info_print_table_row(2, "spinlock", "enabled");
#endif
#ifdef HAVE_RWLOCK
    php_info_print_table_row(2, "rwlock", "enabled");
#endif
#ifdef SW_ASYNC_MYSQL
    php_info_print_table_row(2, "async mysql client", "enabled");
#endif
#ifdef SW_USE_POSTGRESQL
    php_info_print_table_row(2, "async postgresql", "enabled");
#endif
#ifdef SW_USE_REDIS
    php_info_print_table_row(2, "async redis client", "enabled");
#endif
    php_info_print_table_row(2, "async http/websocket client", "enabled");
#ifdef SW_SOCKETS
    php_info_print_table_row(2, "sockets", "enabled");
#endif
#ifdef SW_USE_OPENSSL
    php_info_print_table_row(2, "openssl", "enabled");
#endif
#ifdef SW_USE_HTTP2
    php_info_print_table_row(2, "http2", "enabled");
#endif
#ifdef SW_USE_RINGBUFFER
    php_info_print_table_row(2, "ringbuffer", "enabled");
#endif
#ifdef HAVE_GCC_AIO
    php_info_print_table_row(2, "GCC AIO", "enabled");
#endif
#ifdef HAVE_PCRE
    php_info_print_table_row(2, "pcre", "enabled");
#endif
#ifdef SW_HAVE_ZLIB
    php_info_print_table_row(2, "zlib", "enabled");
#endif
#ifdef HAVE_MUTEX_TIMEDLOCK
    php_info_print_table_row(2, "mutex_timedlock", "enabled");
#endif
#ifdef HAVE_PTHREAD_BARRIER
    php_info_print_table_row(2, "pthread_barrier", "enabled");
#endif
#ifdef HAVE_FUTEX
    php_info_print_table_row(2, "futex", "enabled");
#endif
#ifdef SW_USE_MYSQLND
    php_info_print_table_row(2, "mysqlnd", "enabled");
#endif
#ifdef SW_USE_JEMALLOC
    php_info_print_table_row(2, "jemalloc", "enabled");
#endif
#ifdef SW_USE_TCMALLOC
    php_info_print_table_row(2, "tcmalloc", "enabled");
#endif
#ifdef SW_USE_HUGEPAGE
    php_info_print_table_row(2, "hugepage", "enabled");
#endif
#ifdef SW_DEBUG
    php_info_print_table_row(2, "debug", "enabled");
#endif
    php_info_print_table_end();

    DISPLAY_INI_ENTRIES();
}
/* }}} */

//请求进来时执行
PHP_RINIT_FUNCTION(swoole)
{
    SWOOLE_G(req_status) = PHP_SWOOLE_RINIT_BEGIN;
    //注册请求关闭时的函数
    php_swoole_at_shutdown("swoole_call_user_shutdown_begin");
    //设定 swoole 服务启动标识
    //running
    SwooleG.running = 1;

#ifdef ZTS
    if (sw_thread_ctx == NULL)
    {
        TSRMLS_SET_CTX(sw_thread_ctx);
    }
#endif
   //client open  debug 目前貌似没有用到。
#ifdef SW_DEBUG_REMOTE_OPEN
    swoole_open_remote_debug();
#endif
    SWOOLE_G(req_status) = PHP_SWOOLE_RINIT_END;
    return SUCCESS;
}

//请求结束时
PHP_RSHUTDOWN_FUNCTION(swoole)
{
    SWOOLE_G(req_status) = PHP_SWOOLE_RSHUTDOWN_BEGIN;
    //请求shutdown 时，这里请求是一个php 处理的请求，在cli 模型下可以理解为脚本执行终了前，模块卸载前。
    swoole_call_rshutdown_function(NULL);
    //clear pipe buffer
    if (swIsWorker())//SwooleG.process_type==SW_PROCESS_WORKER 当前进程类型是worker 的话，执行 swWorker_clean
    {
        swWorker_clean();//主要是调用 swReactor_wait_write_buffer 把没有发出去的数据发送去。
    }

    if (SwooleG.serv && SwooleG.serv->gs->start > 0 && SwooleG.running > 0)
    {   //判断是否有错误，并输出
        if (PG(last_error_message))
        {
            switch(PG(last_error_type))
            {
            case E_ERROR:
            case E_CORE_ERROR:
            case E_USER_ERROR:
            case E_COMPILE_ERROR:
                swoole_error_log(SW_LOG_ERROR, SW_ERROR_PHP_FATAL_ERROR, "Fatal error: %s in %s on line %d.",
                        PG(last_error_message), PG(last_error_file)?PG(last_error_file):"-", PG(last_error_lineno));
                break;
            default:
                break;
            }
        }
        else
        {
            swoole_error_log(SW_LOG_NOTICE, SW_ERROR_SERVER_WORKER_TERMINATED, "worker process is terminated by exit()/die().");
        }
    }

    if (SwooleAIO.init)
    {
        swAio_free();
    }

    SwooleWG.reactor_wait_onexit = 0;

#ifdef SW_COROUTINE
    //有协程支持的话，释放
    coro_destroy(TSRMLS_C);
#endif
    //请求状态更新为shutdown
    SWOOLE_G(req_status) = PHP_SWOOLE_RSHUTDOWN_END;
    return SUCCESS;
}

//返回版本函数 
//使用例子 <?php  echo swoole_version();
PHP_FUNCTION(swoole_version)
{
    SW_RETURN_STRING(PHP_SWOOLE_VERSION, 1);
}

//hash 
static uint32_t hashkit_one_at_a_time(const char *key, size_t key_length)
{
    const char *ptr = key;
    uint32_t value = 0;

    while (key_length--)
    {
        uint32_t val = (uint32_t) *ptr++;
        value += val;
        value += (value << 10);
        value ^= (value >> 6);
    }
    value += (value << 3);
    value ^= (value >> 11);
    value += (value << 15);

    return value;
}

//hash code 取得
static PHP_FUNCTION(swoole_hashcode)
{
    char *data;
    zend_size_t l_data;
    zend_long type = 0;

#ifdef FAST_ZPP  //在PHP7中新提供的方式。是为了提高参数解析的性能。对应经常使用的方法，建议使用FAST ZPP方式。
    //ZEND_PARSE_PARAMETERS_START
    ZEND_PARSE_PARAMETERS_START(1, 2) //最少1个参数，最多2个参数
        Z_PARAM_STRING(data, l_data)  //接收string 类型的参数,放到data
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(type)
    ZEND_PARSE_PARAMETERS_END();
#else
    //在PHP7之前一直使用zend_parse_parameters函数获取参数
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|l", &data, &l_data, &type) == FAILURE)
    {
        return;
    }
#endif
    switch(type)
    {
    case 1:
        RETURN_LONG(hashkit_one_at_a_time(data, l_data));
    default:
        RETURN_LONG(zend_hash_func(data, l_data));
    }
}

static PHP_FUNCTION(swoole_last_error)
{
    RETURN_LONG(SwooleG.error);
}
//取得cpu 核心数
PHP_FUNCTION(swoole_cpu_num)
{
    long cpu_num = 1;
    cpu_num = sysconf(_SC_NPROCESSORS_CONF);
    if (cpu_num < 1)
    {
        cpu_num = 1;
    }
    RETURN_LONG(cpu_num);
}

//取得错误信息
PHP_FUNCTION(swoole_strerror)
{
    long swoole_errno = 0;
    char error_msg[256] = {0};
    long error_type = 0;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l|l", &swoole_errno, &error_type) == FAILURE)
    {
        return;
    }
    if (error_type == 1)
    {
        //gai_strerror 能取得 getaddrinfo() and getnameinfo()失败时的错误信息 
        //http://man7.org/linux/man-pages/man3/gai_strerror.3p.html
        snprintf(error_msg, sizeof(error_msg) - 1, "%s", gai_strerror(swoole_errno));
    }
    else if (error_type == 2)
    {   //取得 socket错误原因的信息
        snprintf(error_msg, sizeof(error_msg) - 1, "%s", hstrerror(swoole_errno));
    }
    else
    {   //错误信息
        //http://man7.org/linux/man-pages/man3/strerror.3.html
        snprintf(error_msg, sizeof(error_msg) - 1, "%s", strerror(swoole_errno));
    }
    SW_RETURN_STRING(error_msg, 1);
}

//取得错误号
PHP_FUNCTION(swoole_errno)
{
    RETURN_LONG(errno);
}


//设置进程title
//用法 ：swoole_set_process_name("php {$argv[0]}: worker");
PHP_FUNCTION(swoole_set_process_name)
{
    // MacOS doesn't support 'cli_set_process_title'
#ifdef __MACH__
    php_error_docref(NULL TSRMLS_CC, E_WARNING, "swoole_set_process_name is not supported on MacOS.");
    return;
#endif
    zval *name;
    long size = 128;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|l", &name, &size) == FAILURE)
    {
        return;
    }

    if (Z_STRLEN_P(name) == 0)
    {
        return;
    }
    else if (Z_STRLEN_P(name) > 127)
    {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "process name is too long, the max length is 127");
    }

    if (size > SwooleG.pagesize)
    {
        size = SwooleG.pagesize;
    }

    zval *retval;
    zval **args[1];
    args[0] = &name;

    zval *function;
    SW_MAKE_STD_ZVAL(function);
    SW_ZVAL_STRING(function, "cli_set_process_title", 1);//调用函数cli_set_process_title

    if (sw_call_user_function_ex(EG(function_table), NULL, function, &retval, 1, args, 0, NULL TSRMLS_CC) == FAILURE)
    {
        return;
    }
    if (EG(exception))
    {
        zend_exception_error(EG(exception), E_ERROR TSRMLS_CC);
    }
    sw_zval_ptr_dtor(&function);//销毁function
    if (retval)
    {
        sw_zval_ptr_dtor(&retval);
    }
}

//取得本地IP

PHP_FUNCTION(swoole_get_local_ip)
{
    struct sockaddr_in *s4;
    struct ifaddrs *ipaddrs, *ifa;
    void *in_addr;
    char ip[64];

    if (getifaddrs(&ipaddrs) != 0) //http://man7.org/linux/man-pages/man3/getifaddrs.3.html
    {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "getifaddrs() failed. Error: %s[%d]", strerror(errno), errno);
        RETURN_FALSE;
    }
    array_init(return_value);
    for (ifa = ipaddrs; ifa != NULL; ifa = ifa->ifa_next)//循环
    {
        if (ifa->ifa_addr == NULL || !(ifa->ifa_flags & IFF_UP))
        {
            continue;
        }

        switch (ifa->ifa_addr->sa_family)
        {
            case AF_INET:
                s4 = (struct sockaddr_in *)ifa->ifa_addr;
                in_addr = &s4->sin_addr;
                break;
            case AF_INET6:
                //struct sockaddr_in6 *s6 = (struct sockaddr_in6 *)ifa->ifa_addr;
                //in_addr = &s6->sin6_addr;
                continue;
            default:
                continue;
        }
        if (!inet_ntop(ifa->ifa_addr->sa_family, in_addr, ip, sizeof(ip)))//网络字节顺转为本地字节顺 http://man7.org/linux/man-pages/man3/inet_ntop.3.html
        {
            php_error_docref(NULL TSRMLS_CC, E_WARNING, "%s: inet_ntop failed.", ifa->ifa_name);
        }
        else
        {
            //if (ifa->ifa_addr->sa_family == AF_INET && ntohl(((struct in_addr *) in_addr)->s_addr) == INADDR_LOOPBACK)
            if (strcmp(ip, "127.0.0.1") == 0)
            {
                continue;
            }
            sw_add_assoc_string(return_value, ifa->ifa_name, ip, 1);
        }
    }
    freeifaddrs(ipaddrs);
}

//取得本机MAC地址
PHP_FUNCTION(swoole_get_local_mac)
{
#ifdef SIOCGIFHWADDR
    struct ifconf ifc;
    struct ifreq buf[16];
    char mac[32] = {0};

    int sock;
    int i = 0,num = 0;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "new socket failed. Error: %s[%d]", strerror(errno), errno);
        RETURN_FALSE;
    }
    array_init(return_value);

    ifc.ifc_len = sizeof (buf);
    ifc.ifc_buf = (caddr_t) buf;
    if (!ioctl(sock, SIOCGIFCONF, (char *) &ifc))
    {
        num = ifc.ifc_len / sizeof (struct ifreq);
        while (i < num)
        {
            if (!(ioctl(sock, SIOCGIFHWADDR, (char *) &buf[i])))
            {
                sprintf(mac, "%02X:%02X:%02X:%02X:%02X:%02X",
                        (unsigned char) buf[i].ifr_hwaddr.sa_data[0],
                        (unsigned char) buf[i].ifr_hwaddr.sa_data[1],
                        (unsigned char) buf[i].ifr_hwaddr.sa_data[2],
                        (unsigned char) buf[i].ifr_hwaddr.sa_data[3],
                        (unsigned char) buf[i].ifr_hwaddr.sa_data[4],
                        (unsigned char) buf[i].ifr_hwaddr.sa_data[5]);
                sw_add_assoc_string(return_value, buf[i].ifr_name, mac, 1);
            }
            i++;
        }
    }
    close(sock);
#else
    php_error_docref(NULL TSRMLS_CC, E_WARNING, "swoole_get_local_mac is not supported.");
    RETURN_FALSE;
#endif
}

//请求关闭时运行
PHP_FUNCTION(swoole_call_user_shutdown_begin)
{
    SWOOLE_G(req_status) = PHP_SWOOLE_CALL_USER_SHUTDOWNFUNC_BEGIN; //运行status 更新
    RETURN_TRUE;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
