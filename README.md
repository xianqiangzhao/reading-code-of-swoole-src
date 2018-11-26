# reading-code-of-swoole-src
swoole 源码阅读

# 8/25~11/26 update
 1.  php-wrapper.h 包裹宏定义分析。
 2.  php_swoole.h 函数定义等分析。
 3.  swoole.c  函数定义，这个文件中主要定义class ，执行初始化工作。
 4.  swoole_atomic.c  提供的原子计数操作类，可以方便整数的无锁原子增减，可以用于多进程线程中。
   gcc 原子操作，使用了Linux Futex 进行进程等待和唤醒。（使用共享内存）。
 6.  swoole_buffer.c 内存读写（向系统申请内存，非共享，每次扩容要向系统申请内存）。
 7.  swoole_channel.c 内存队列，读写时加锁。（不能扩容，只能在初始化时指定所需内存大小）。
 8.  swoole_lock.c 锁，用于解决进程或线程间对资源的竞争。
 9.  swoole_mmap.c 内存映射方式读写文件。
 10. src\memory\shared_memory.c 内存映射底层实现。
 11. swoole_process.c 进程管理。
 12. signal.c epoll.c 信号注册，epoll事件循环。
 13. swoole_process_pool.c 进程池管理。
 14. swoole_event.c 事件管理。
 15. swoole_time.c 定时器管理。
 16. swoole_server.c 服务器（核心），http server ,udp server ,socket server 都继承这个class。

# 阶段总结
 协程部分原来是zend 执行栈的切换，4.0以后用的是boost （http://www.boost.org/）来实现。
 这部分以后有空在看。

 最后看懂和会用是两回事，接下来我准备些个扩展练练了～