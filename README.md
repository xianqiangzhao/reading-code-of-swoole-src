# reading-code-of-swoole-src
swoole 源码阅读

# 8/25 update
php-wrapper.h 包裹宏定义分析
# 8/26 update
 php_swoole.h 函数定义等分析

# 8/27~29 update
 1.  swoole.c  函数定义，这个文件中主要定义class ，执行初始化工作。
 2.  swoole_atomic.c  提供的原子计数操作类，可以方便整数的无锁原子增减，可以用于多进程线程中。
   gcc 原子操作，使用了Linux Futex 进行进程等待和唤醒。
