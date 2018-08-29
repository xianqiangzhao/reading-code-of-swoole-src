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
/*
 该类是原子操作类
 原子是操作结果不会被打扰，造成不一致
 比如 a = b + 1;
  这个操作不是原子的，从取出b后，b有可能被改写。造成结果不一致。

*/
//声明为静态类型
//PHP_METHOD 展开后是  zim_XXX = zim_类名_方法名
//static PHP_METHOD(swoole_atomic, add);
// 展开后是 static zim_swoole_atomic_add
static PHP_METHOD(swoole_atomic, __construct); //实例化
static PHP_METHOD(swoole_atomic, add);  //原子增加
static PHP_METHOD(swoole_atomic, sub);
static PHP_METHOD(swoole_atomic, get);
static PHP_METHOD(swoole_atomic, set);
static PHP_METHOD(swoole_atomic, cmpset);
static PHP_METHOD(swoole_atomic, wait);
static PHP_METHOD(swoole_atomic, wakeup);

static PHP_METHOD(swoole_atomic_long, __construct);
static PHP_METHOD(swoole_atomic_long, add);
static PHP_METHOD(swoole_atomic_long, sub);
static PHP_METHOD(swoole_atomic_long, get);
static PHP_METHOD(swoole_atomic_long, set);
static PHP_METHOD(swoole_atomic_long, cmpset);

#ifdef HAVE_FUTEX
#include <linux/futex.h>
#include <syscall.h>

//系统调用 当atomic = 0 时进入等待
static sw_inline int swoole_futex_wait(sw_atomic_t *atomic, double timeout)
{    //atomic ==1 时返回
    if (sw_atomic_cmp_set(atomic, 1, 0))
    {
        return SW_OK;
    }

    int ret;
    struct timespec _timeout;

    if (timeout > 0)
    {

        _timeout.tv_sec = (long) timeout;//秒
        _timeout.tv_nsec = (timeout - _timeout.tv_sec) * 1000 * 1000 * 1000;//纳秒
        //syscall 参见https://blog.csdn.net/nellson/article/details/5400360
        ret = syscall(SYS_futex, atomic, FUTEX_WAIT, 0, &_timeout, NULL, 0);//wait
    }
    else
    {
        ret = syscall(SYS_futex, atomic, FUTEX_WAIT, 0, NULL, NULL, 0);
    }
    if (ret == SW_OK)
    {
        sw_atomic_cmp_set(atomic, 1, 0);
    }
    return ret;
}
//atomic 为0时进入唤醒
static sw_inline int swoole_futex_wakeup(sw_atomic_t *atomic, int n)
{
    if (sw_atomic_cmp_set(atomic, 0, 1)) //为0 时改为 并唤醒并改写atomic =1
    {
        return syscall(SYS_futex, atomic, FUTEX_WAKE, n, NULL, NULL, 0); //唤醒 wait的进程，n 为唤醒进程数量。
    }
    else
    {
        return SW_OK;
    }
}
#endif

ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_atomic_construct, 0, 0, 0)
    ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_atomic_add, 0, 0, 0)
    ZEND_ARG_INFO(0, add_value)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_atomic_sub, 0, 0, 0)
    ZEND_ARG_INFO(0, sub_value)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_atomic_get, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_atomic_set, 0, 0, 1)
    ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_atomic_cmpset, 0, 0, 2)
    ZEND_ARG_INFO(0, cmp_value)
    ZEND_ARG_INFO(0, new_value)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_atomic_wait, 0, 0, 0)
    ZEND_ARG_INFO(0, timeout)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_atomic_waitup, 0, 0, 0)
    ZEND_ARG_INFO(0, count)
ZEND_END_ARG_INFO()

static zend_class_entry swoole_atomic_ce;
zend_class_entry *swoole_atomic_class_entry_ptr;

static zend_class_entry swoole_atomic_long_ce;
zend_class_entry *swoole_atomic_long_class_entry_ptr;

//原子类中的方法
static const zend_function_entry swoole_atomic_methods[] =
{
    PHP_ME(swoole_atomic, __construct, arginfo_swoole_atomic_construct, ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
    PHP_ME(swoole_atomic, add, arginfo_swoole_atomic_add, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_atomic, sub, arginfo_swoole_atomic_sub, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_atomic, get, arginfo_swoole_atomic_get, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_atomic, set, arginfo_swoole_atomic_set, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_atomic, wait, arginfo_swoole_atomic_wait, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_atomic, wakeup, arginfo_swoole_atomic_waitup, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_atomic, cmpset, arginfo_swoole_atomic_cmpset, ZEND_ACC_PUBLIC)
    PHP_FE_END
};
//long 型原子类
static const zend_function_entry swoole_atomic_long_methods[] =
{
    PHP_ME(swoole_atomic_long, __construct, arginfo_swoole_atomic_construct, ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
    PHP_ME(swoole_atomic_long, add, arginfo_swoole_atomic_add, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_atomic_long, sub, arginfo_swoole_atomic_sub, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_atomic_long, get, arginfo_swoole_atomic_get, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_atomic_long, set, arginfo_swoole_atomic_set, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_atomic_long, cmpset, arginfo_swoole_atomic_cmpset, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

//初始化注册，在swoole.c 中被调用
void swoole_atomic_init(int module_number TSRMLS_DC)
{
    SWOOLE_INIT_CLASS_ENTRY(swoole_atomic_ce, "swoole_atomic", "Swoole\\Atomic", swoole_atomic_methods);
    swoole_atomic_class_entry_ptr = zend_register_internal_class(&swoole_atomic_ce TSRMLS_CC);
    swoole_atomic_class_entry_ptr->serialize = zend_class_serialize_deny;
    swoole_atomic_class_entry_ptr->unserialize = zend_class_unserialize_deny;
    SWOOLE_CLASS_ALIAS(swoole_atomic, "Swoole\\Atomic");

    SWOOLE_INIT_CLASS_ENTRY(swoole_atomic_long_ce, "swoole_atomic_long", "Swoole\\Atomic\\Long", swoole_atomic_long_methods);
    swoole_atomic_long_class_entry_ptr = zend_register_internal_class(&swoole_atomic_long_ce TSRMLS_CC);
    swoole_atomic_long_class_entry_ptr->serialize = zend_class_serialize_deny;
    swoole_atomic_long_class_entry_ptr->unserialize = zend_class_unserialize_deny;
    SWOOLE_CLASS_ALIAS(swoole_atomic_long, "Swoole\\Atomic\\Long");
}

//实例化swoole_atomic 时被执行
PHP_METHOD(swoole_atomic, __construct)
{
    zend_long value = 0;

#ifdef FAST_ZPP
    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(value)  //可选参数long 型放到value 中
    ZEND_PARSE_PARAMETERS_END();
#else
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|l", &value) == FAILURE)
    {
        RETURN_FALSE;
    }
#endif
    //从共享内存中分配内存
    //atomic 时 volatile uint32_t 类型，也就是不做优化读取
    sw_atomic_t *atomic = SwooleG.memory_pool->alloc(SwooleG.memory_pool, sizeof(sw_atomic_t));
    if (atomic == NULL)
    {
        zend_throw_exception(swoole_exception_class_entry_ptr, "global memory allocation failure.", SW_ERROR_MALLOC_FAIL TSRMLS_CC);
        RETURN_FALSE;
    }
    *atomic = (sw_atomic_t) value;//把传进来的value 的地址给atomic
    swoole_set_object(getThis(), (void*) atomic); //把atomic保存到swoole_object 中，其它方法用时从这个object 中取出。

    RETURN_TRUE;
}
//原子增加操作
PHP_METHOD(swoole_atomic, add)
{
    zend_long add_value = 1;
    sw_atomic_t *atomic = swoole_get_object(getThis());//根据对象handle 取出保存的值，从而实现变量在不同方法中传递。

#ifdef FAST_ZPP
    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(add_value)
    ZEND_PARSE_PARAMETERS_END();
#else
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|l", &add_value) == FAILURE)
    {
        RETURN_FALSE;
    }
#endif
    //用sw_atomic_add_fetch 执行原子增加
    //sw_atomic_add_fetch宏展开后是 __sync_add_and_fetch 多线程对全局变量进行自加，不用加线程锁。
    //https://www.jianshu.com/p/da1d69f0a6ad
    RETURN_LONG(sw_atomic_add_fetch(atomic, (uint32_t ) add_value));//第一个参数是指针，第二个参数是具体值。
    
}
//原子减法
PHP_METHOD(swoole_atomic, sub)
{
    zend_long sub_value = 1;
    sw_atomic_t *atomic = swoole_get_object(getThis());

#ifdef FAST_ZPP
    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(sub_value)
    ZEND_PARSE_PARAMETERS_END();
#else
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|l", &sub_value) == FAILURE)
    {
        RETURN_FALSE;
    }
#endif
    RETURN_LONG(sw_atomic_sub_fetch(atomic, (uint32_t ) sub_value));
}

//取得当前atomic
PHP_METHOD(swoole_atomic, get)
{
    sw_atomic_t *atomic = swoole_get_object(getThis());
    RETURN_LONG(*atomic);
}

//set atomic
PHP_METHOD(swoole_atomic, set)
{
    sw_atomic_t *atomic = swoole_get_object(getThis());
    zend_long set_value;

#ifdef FAST_ZPP
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(set_value)
    ZEND_PARSE_PARAMETERS_END();
#else
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &set_value) == FAILURE)
    {
        RETURN_FALSE;
    }
#endif
    *atomic = (uint32_t) set_value;
}

//atomic == cmp_value时 atomic = set_value
PHP_METHOD(swoole_atomic, cmpset)
{
    zend_long cmp_value, set_value;
    sw_atomic_t *atomic = swoole_get_object(getThis());

#ifdef FAST_ZPP
    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_LONG(cmp_value)
        Z_PARAM_LONG(set_value)
    ZEND_PARSE_PARAMETERS_END();
#else
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ll", &cmp_value, &set_value) == FAILURE)
    {
        RETURN_FALSE;
    }
#endif

    RETURN_BOOL(sw_atomic_cmp_set(atomic, (sw_atomic_t) cmp_value, (sw_atomic_t) set_value));
}

//当原子计数的值为0时程序进入等待状态。另外一个进程调用wakeup可以再次唤醒程序。
//https://wiki.swoole.com/wiki/page/764.html
PHP_METHOD(swoole_atomic, wait)
{
    double timeout = 1.0;
    sw_atomic_t *atomic = swoole_get_object(getThis());

#ifdef FAST_ZPP
    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_DOUBLE(timeout)
    ZEND_PARSE_PARAMETERS_END();
#else
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|d", &timeout) == FAILURE)
    {
        RETURN_FALSE;
    }
#endif

#ifdef HAVE_FUTEX
    SW_CHECK_RETURN(swoole_futex_wait(atomic, timeout));
#else
    timeout = timeout <= 0 ? SW_MAX_INT : timeout;
    while (timeout > 0)
    {
        if (sw_atomic_cmp_set(atomic, 1, 0))
        {
            RETURN_TRUE;
        }
        else
        {
            usleep(1000);
            timeout -= 0.001;
        }
    }
#endif
}

PHP_METHOD(swoole_atomic, wakeup)
{
    zend_long n = 1;
    sw_atomic_t *atomic = swoole_get_object(getThis());

#ifdef FAST_ZPP
    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(n)
    ZEND_PARSE_PARAMETERS_END();
#else
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|l", &n) == FAILURE)
    {
        RETURN_FALSE;
    }
#endif

#ifdef HAVE_FUTEX
    SW_CHECK_RETURN(swoole_futex_wakeup(atomic, (int ) n));
#else
    *atomic = 1;
#endif
}

PHP_METHOD(swoole_atomic_long, __construct)
{
    zend_long value = 0;

#ifdef FAST_ZPP
    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(value)
    ZEND_PARSE_PARAMETERS_END();
#else
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|l", &value) == FAILURE)
    {
        RETURN_FALSE;
    }
#endif

    sw_atomic_long_t *atomic = SwooleG.memory_pool->alloc(SwooleG.memory_pool, sizeof(sw_atomic_long_t));
    if (atomic == NULL)
    {
        zend_throw_exception(swoole_exception_class_entry_ptr, "global memory allocation failure.", SW_ERROR_MALLOC_FAIL TSRMLS_CC);
        RETURN_FALSE;
    }
    *atomic = (sw_atomic_long_t) value;
    swoole_set_object(getThis(), (void*) atomic);

    RETURN_TRUE;
}

PHP_METHOD(swoole_atomic_long, add)
{
    zend_long add_value = 1;
    sw_atomic_long_t *atomic = swoole_get_object(getThis());

#ifdef FAST_ZPP
    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(add_value)
    ZEND_PARSE_PARAMETERS_END();
#else
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|l", &add_value) == FAILURE)
    {
        RETURN_FALSE;
    }
#endif

    RETURN_LONG(sw_atomic_add_fetch(atomic, (sw_atomic_long_t ) add_value));
}

PHP_METHOD(swoole_atomic_long, sub)
{
    zend_long sub_value = 1;
    sw_atomic_long_t *atomic = swoole_get_object(getThis());

#ifdef FAST_ZPP
    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(sub_value)
    ZEND_PARSE_PARAMETERS_END();
#else
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|l", &sub_value) == FAILURE)
    {
        RETURN_FALSE;
    }
#endif

    RETURN_LONG(sw_atomic_sub_fetch(atomic, (sw_atomic_long_t ) sub_value));
}

PHP_METHOD(swoole_atomic_long, get)
{
    sw_atomic_long_t *atomic = swoole_get_object(getThis());
    RETURN_LONG(*atomic);
}

PHP_METHOD(swoole_atomic_long, set)
{
    sw_atomic_long_t *atomic = swoole_get_object(getThis());
    zend_long set_value;

#ifdef FAST_ZPP
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(set_value)
    ZEND_PARSE_PARAMETERS_END();
#else
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &set_value) == FAILURE)
    {
        RETURN_FALSE;
    }
#endif
    *atomic = (sw_atomic_long_t) set_value;
}

PHP_METHOD(swoole_atomic_long, cmpset)
{
    zend_long cmp_value, set_value;
    sw_atomic_long_t *atomic = swoole_get_object(getThis());

#ifdef FAST_ZPP
    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_LONG(cmp_value)
        Z_PARAM_LONG(set_value)
    ZEND_PARSE_PARAMETERS_END();
#else
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ll", &cmp_value, &set_value) == FAILURE)
    {
        RETURN_FALSE;
    }
#endif

    RETURN_BOOL(sw_atomic_cmp_set(atomic, (sw_atomic_long_t) cmp_value, (sw_atomic_long_t) set_value));
}
