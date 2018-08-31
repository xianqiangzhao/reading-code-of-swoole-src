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
//互斥锁操作函数
#include "swoole.h"

#ifdef HAVE_RWLOCK

static int swRWLock_lock_rd(swLock *lock);
static int swRWLock_lock_rw(swLock *lock);
static int swRWLock_unlock(swLock *lock);
static int swRWLock_trylock_rw(swLock *lock);
static int swRWLock_trylock_rd(swLock *lock);
static int swRWLock_free(swLock *lock);

//初始化
int swRWLock_create(swLock *lock, int use_in_process)
{
    int ret;
    bzero(lock, sizeof(swLock));
    lock->type = SW_RWLOCK;
    pthread_rwlockattr_init(&lock->object.rwlock.attr);//初始化锁属性
    if (use_in_process == 1)
    {
        pthread_rwlockattr_setpshared(&lock->object.rwlock.attr, PTHREAD_PROCESS_SHARED);//是否可以进程，线程间使用该锁
    }
    if ((ret = pthread_rwlock_init(&lock->object.rwlock._lock, &lock->object.rwlock.attr)) < 0) //初始化锁
    {
        return SW_ERR;
    }
    lock->lock_rd = swRWLock_lock_rd; //读锁
    lock->lock = swRWLock_lock_rw;   //写锁
    lock->unlock = swRWLock_unlock;   //释放锁
    lock->trylock = swRWLock_trylock_rw; //尝试读写锁
    lock->trylock_rd = swRWLock_trylock_rd; //尝试读锁
    lock->free = swRWLock_free;//释放锁
    return SW_OK;
}

static int swRWLock_lock_rd(swLock *lock)
{
    return pthread_rwlock_rdlock(&lock->object.rwlock._lock);
}

static int swRWLock_lock_rw(swLock *lock)
{
    return pthread_rwlock_wrlock(&lock->object.rwlock._lock);
}

static int swRWLock_unlock(swLock *lock)
{
    return pthread_rwlock_unlock(&lock->object.rwlock._lock);
}

static int swRWLock_trylock_rd(swLock *lock)
{
    return pthread_rwlock_tryrdlock(&lock->object.rwlock._lock);
}

static int swRWLock_trylock_rw(swLock *lock)
{
    return pthread_rwlock_trywrlock(&lock->object.rwlock._lock);
}

static int swRWLock_free(swLock *lock)
{
    return pthread_rwlock_destroy(&lock->object.rwlock._lock);
}

#endif
