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
//线程进程有时会竞争一个资源，这时就要一个锁，加锁方可以对资源进程操作
//完成后释放，同一时刻只能有一方进行加锁，另一方等待。
//锁有文件锁、互斥锁、信号量、读写锁、条件变量、自旋锁、原子锁

#include "php_swoole.h"

static PHP_METHOD(swoole_lock, __construct);
static PHP_METHOD(swoole_lock, __destruct);
static PHP_METHOD(swoole_lock, lock);
static PHP_METHOD(swoole_lock, lockwait);
static PHP_METHOD(swoole_lock, trylock);
static PHP_METHOD(swoole_lock, lock_read);
static PHP_METHOD(swoole_lock, trylock_read);
static PHP_METHOD(swoole_lock, unlock);
static PHP_METHOD(swoole_lock, destroy);

static zend_class_entry swoole_lock_ce;
static zend_class_entry *swoole_lock_class_entry_ptr;

ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_void, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_lock_construct, 0, 0, 0)
    ZEND_ARG_INFO(0, type)
    ZEND_ARG_INFO(0, filename)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_lock_lockwait, 0, 0, 0)
    ZEND_ARG_INFO(0, timeout)
ZEND_END_ARG_INFO()

static const zend_function_entry swoole_lock_methods[] =
{
    PHP_ME(swoole_lock, __construct, arginfo_swoole_lock_construct, ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
    PHP_ME(swoole_lock, __destruct, arginfo_swoole_void, ZEND_ACC_PUBLIC | ZEND_ACC_DTOR)
    PHP_ME(swoole_lock, lock, arginfo_swoole_void, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_lock, lockwait, arginfo_swoole_lock_lockwait, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_lock, trylock, arginfo_swoole_void, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_lock, lock_read, arginfo_swoole_void, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_lock, trylock_read, arginfo_swoole_void, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_lock, unlock, arginfo_swoole_void, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_lock, destroy, arginfo_swoole_void, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

//初始化
void swoole_lock_init(int module_number TSRMLS_DC)
{
    SWOOLE_INIT_CLASS_ENTRY(swoole_lock_ce, "swoole_lock", "Swoole\\Lock", swoole_lock_methods);
    swoole_lock_class_entry_ptr = zend_register_internal_class(&swoole_lock_ce TSRMLS_CC);
    SWOOLE_CLASS_ALIAS(swoole_lock, "Swoole\\Lock");

    zend_declare_class_constant_long(swoole_lock_class_entry_ptr, SW_STRL("FILELOCK")-1, SW_FILELOCK TSRMLS_CC);
    zend_declare_class_constant_long(swoole_lock_class_entry_ptr, SW_STRL("MUTEX")-1, SW_MUTEX TSRMLS_CC);
    zend_declare_class_constant_long(swoole_lock_class_entry_ptr, SW_STRL("SEM")-1, SW_SEM TSRMLS_CC);
#ifdef HAVE_RWLOCK
    zend_declare_class_constant_long(swoole_lock_class_entry_ptr, SW_STRL("RWLOCK")-1, SW_RWLOCK TSRMLS_CC);
#endif
#ifdef HAVE_SPINLOCK
    zend_declare_class_constant_long(swoole_lock_class_entry_ptr, SW_STRL("SPINLOCK")-1, SW_SPINLOCK TSRMLS_CC);
#endif

    zend_declare_property_long(swoole_lock_class_entry_ptr, SW_STRL("errCode")-1, 0, ZEND_ACC_PUBLIC TSRMLS_CC);

    REGISTER_LONG_CONSTANT("SWOOLE_FILELOCK", SW_FILELOCK, CONST_CS | CONST_PERSISTENT);//文件互斥锁
    REGISTER_LONG_CONSTANT("SWOOLE_MUTEX", SW_MUTEX, CONST_CS | CONST_PERSISTENT);//互斥锁
    REGISTER_LONG_CONSTANT("SWOOLE_SEM", SW_SEM, CONST_CS | CONST_PERSISTENT);//信号量
#ifdef HAVE_RWLOCK
    REGISTER_LONG_CONSTANT("SWOOLE_RWLOCK", SW_RWLOCK, CONST_CS | CONST_PERSISTENT);//读写锁
#endif
#ifdef HAVE_SPINLOCK
    REGISTER_LONG_CONSTANT("SWOOLE_SPINLOCK", SW_SPINLOCK, CONST_CS | CONST_PERSISTENT);//自旋锁
#endif
}

//实例化
static PHP_METHOD(swoole_lock, __construct)
{
    long type = SW_MUTEX;
    char *filelock;
    zend_size_t filelock_len = 0;
    int ret;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|ls", &type, &filelock, &filelock_len) == FAILURE)
    {
        RETURN_FALSE;
    }

 /* swLock 结构体 

 typedef struct _swLock
{
    int type;  类型
    union    //union 
    {
        swMutex mutex; //互斥锁
#ifdef HAVE_RWLOCK
        swRWLock rwlock; //读写锁
#endif
#ifdef HAVE_SPINLOCK
        swSpinLock spinlock; //自旋锁
#endif
        swFileLock filelock; //文件锁
        swSem sem;           //信号量
        swAtomicLock atomlock; //原子锁
    } object;

    int (*lock_rd)(struct _swLock *);
    int (*lock)(struct _swLock *);
    int (*unlock)(struct _swLock *);
    int (*trylock_rd)(struct _swLock *);
    int (*trylock)(struct _swLock *);
    int (*free)(struct _swLock *);
} swLock;

 */
    swLock *lock = SwooleG.memory_pool->alloc(SwooleG.memory_pool, sizeof(swLock));
    if (lock == NULL)
    {
        zend_throw_exception(swoole_exception_class_entry_ptr, "global memory allocation failure.", SW_ERROR_MALLOC_FAIL TSRMLS_CC);
        RETURN_FALSE;
    }

     //根据参数类型创建锁
    switch(type)
    {
#ifdef HAVE_RWLOCK
    case SW_RWLOCK:
        ret = swRWLock_create(lock, 1);
        break;
#endif
    case SW_FILELOCK:
        if (filelock_len <= 0)
        {
            zend_throw_exception(swoole_exception_class_entry_ptr, "filelock requires file name of the lock.", SW_ERROR_INVALID_PARAMS TSRMLS_CC);
            RETURN_FALSE;
        }
        int fd; //文件描述符
        if ((fd = open(filelock, O_RDWR | O_CREAT, 0666)) < 0) //建立文件，文件名时filelock
        {
            zend_throw_exception_ex(swoole_exception_class_entry_ptr, errno TSRMLS_CC, "open file[%s] failed. Error: %s [%d]", filelock, strerror(errno), errno);
            RETURN_FALSE;
        }
        ret = swFileLock_create(lock, fd);
        break;
    case SW_SEM:
        ret = swSem_create(lock, IPC_PRIVATE);
        break;
#ifdef HAVE_SPINLOCK
    case SW_SPINLOCK:
        ret = swSpinLock_create(lock, 1);
        break;
#endif
    case SW_MUTEX:
    default:
        ret = swMutex_create(lock, 1);
        break;
    }
    if (ret < 0)
    {
        zend_throw_exception(swoole_exception_class_entry_ptr, "failed to create lock.", errno TSRMLS_CC);
        RETURN_FALSE;
    }
    swoole_set_object(getThis(), lock);//把当前锁保存
    RETURN_TRUE;
}

//释放
static PHP_METHOD(swoole_lock, __destruct)
{
    SW_PREVENT_USER_DESTRUCT;

    swLock *lock = swoole_get_object(getThis());
    if (lock)
    {
        swoole_set_object(getThis(), NULL);
    }
}
//加锁，成功返回，不成功等待
static PHP_METHOD(swoole_lock, lock)
{
    swLock *lock = swoole_get_object(getThis());
    SW_LOCK_CHECK_RETURN(lock->lock(lock));
}

//锁等待
static PHP_METHOD(swoole_lock, lockwait)
{
    double timeout = 1.0;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "d", &timeout) == FAILURE)
    {
        RETURN_FALSE;
    }
    swLock *lock = swoole_get_object(getThis());
    if (lock->type != SW_MUTEX)
    {
        zend_throw_exception(swoole_exception_class_entry_ptr, "only mutex supports lockwait.", -2 TSRMLS_CC);
        RETURN_FALSE;
    }
    SW_LOCK_CHECK_RETURN(swMutex_lockwait(lock, (int)timeout * 1000));
}

//释放锁
static PHP_METHOD(swoole_lock, unlock)
{
    swLock *lock = swoole_get_object(getThis());
    SW_LOCK_CHECK_RETURN(lock->unlock(lock));
}

//尝试加锁
static PHP_METHOD(swoole_lock, trylock)
{
    swLock *lock = swoole_get_object(getThis());
    if (lock->trylock == NULL)
    {
        swoole_php_error(E_WARNING, "lock[type=%d] can't use trylock", lock->type);
        RETURN_FALSE;
    }
    SW_LOCK_CHECK_RETURN(lock->trylock(lock));
}

//尝试取得读锁
static PHP_METHOD(swoole_lock, trylock_read)
{
    swLock *lock = swoole_get_object(getThis());
    if (lock->trylock_rd == NULL)
    {
        swoole_php_error(E_WARNING, "lock[type=%d] can't use trylock_read", lock->type);
        RETURN_FALSE;
    }
    SW_LOCK_CHECK_RETURN(lock->trylock_rd(lock));
}

//取得读锁
static PHP_METHOD(swoole_lock, lock_read)
{
    swLock *lock = swoole_get_object(getThis());
    if (lock->lock_rd == NULL)
    {
        swoole_php_error(E_WARNING, "lock[type=%d] can't use lock_read", lock->type);
        RETURN_FALSE;
    }
    SW_LOCK_CHECK_RETURN(lock->lock_rd(lock));
}

//释放锁对象
static PHP_METHOD(swoole_lock, destroy)
{
    swLock *lock = swoole_get_object(getThis());
    lock->free(lock);
}
