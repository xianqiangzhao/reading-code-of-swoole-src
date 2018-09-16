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

//信号处理
#include "swoole.h"

#ifdef HAVE_SIGNALFD
#include <sys/signalfd.h>
static void swSignalfd_set(int signo, swSignalHander callback);
static void swSignalfd_clear();
static int swSignalfd_onSignal(swReactor *reactor, swEvent *event);

static sigset_t signalfd_mask;
static int signal_fd = 0;
#endif

#ifdef HAVE_KQUEUE
#include <sys/event.h>
static void swKqueueSignal_set(int signo, swSignalHander callback);
#endif

typedef struct
{
    swSignalHander callback;//信号回调函数
    uint16_t signo; //信号值
    uint16_t active;//是否active
} swSignal;

static swSignal signals[SW_SIGNO_MAX];//定义 SW_SIGNO_MAX 128个 swsignal结构
static int _lock = 0;

static void swSignal_async_handler(int signo);

/**
 * clear all singal
 */
//清除所有信号
void swSignal_none(void)
{
    sigset_t mask;
    sigfillset(&mask);//填充mask
    int ret = pthread_sigmask(SIG_BLOCK, &mask, NULL);//设置默认信号处理
    if (ret < 0)
    {
        swWarn("pthread_sigmask() failed. Error: %s[%d]", strerror(ret), ret);
    }
}

/**
 * setup signal
 */
swSignalHander swSignal_set(int sig, swSignalHander func, int restart, int mask)
{
    //ignore
    if (func == NULL)
    {
        func = SIG_IGN;
    }
    //clear
    else if ((long) func == -1)
    {
        func = SIG_DFL;
    }

    struct sigaction act, oact;//信号按照系统函数
    act.sa_handler = func;//设置信号处理函数
    if (mask)
    {   //specifies a mask of signals which should be blocked
        //也就是信号发生时，进入信号处理程序中，这时要阻塞的信号
        sigfillset(&act.sa_mask);
    }
    else
    {
        sigemptyset(&act.sa_mask);
    }
    act.sa_flags = 0;
    if (sigaction(sig, &act, &oact) < 0)//安装信号
    {
        return NULL;
    }
    return oact.sa_handler;//返回老的信号处理程序
}
//信号处理追加
void swSignal_add(int signo, swSignalHander func)
{
#ifdef HAVE_SIGNALFD
    if (SwooleG.use_signalfd)//支持信号signalfd的话，默认时支持的 http://man7.org/linux/man-pages/man2/signalfd.2.html
    {
        swSignalfd_set(signo, func);
    }
    else
#endif
    {
#ifdef HAVE_KQUEUE
        // SIGCHLD can not be monitored by kqueue, if blocked by SIG_IGN
        // see https://www.freebsd.org/cgi/man.cgi?kqueue
        // if there's no main reactor, signals cannot be monitored either
        if (signo != SIGCHLD && SwooleG.main_reactor)
        {
            swKqueueSignal_set(signo, func);
        }
        else
#endif
        {
            signals[signo].callback = func;
            signals[signo].active = 1;
            signals[signo].signo = signo;
            swSignal_set(signo, swSignal_async_handler, 1, 0);
        }
    }
}

static void swSignal_async_handler(int signo)
{
    if (SwooleG.main_reactor)
    {
        SwooleG.main_reactor->singal_no = signo;
    }
    else
    {
        //discard signal
        if (_lock)
        {
            return;
        }
        _lock = 1;
        swSignal_callback(signo);
        _lock = 0;
    }
}

void swSignal_callback(int signo)
{
    if (signo >= SW_SIGNO_MAX)
    {
        swWarn("signal[%d] numberis invalid.", signo);
        return;
    }
    swSignalHander callback = signals[signo].callback;
    if (!callback)
    {
        swWarn("signal[%d] callback is null.", signo);
        return;
    }
    callback(signo);
}

void swSignal_clear(void)
{
#ifdef HAVE_SIGNALFD
    if (SwooleG.use_signalfd)
    {
        swSignalfd_clear();
    }
    else
#endif
    {
        int i;
        for (i = 0; i < SW_SIGNO_MAX; i++)
        {
            if (signals[i].active)
            {
#ifdef HAVE_KQUEUE
                if (signals[i].signo != SIGCHLD && SwooleG.main_reactor)
                {
                    swKqueueSignal_set(signals[i].signo, NULL);
                }
                else
#endif
                {
                    swSignal_set(signals[i].signo, (swSignalHander) -1, 1, 0);
                }
            }
        }
    }
    bzero(&signals, sizeof(signals));
}

#ifdef HAVE_SIGNALFD
void swSignalfd_init()
{
    sigemptyset(&signalfd_mask);
    bzero(&signals, sizeof(signals));
}
//信号挂载函数
static void swSignalfd_set(int signo, swSignalHander callback)
{
    if (callback == NULL && signals[signo].active)
    {
        sigdelset(&signalfd_mask, signo);
        bzero(&signals[signo], sizeof(swSignal));
    }
    else
    {
        sigaddset(&signalfd_mask, signo);
        signals[signo].callback = callback;
        signals[signo].signo = signo;
        signals[signo].active = 1;
    }
    if (signal_fd > 0)
    {
        sigprocmask(SIG_BLOCK, &signalfd_mask, NULL);
        signalfd(signal_fd, &signalfd_mask, SFD_NONBLOCK | SFD_CLOEXEC);
    }
}

//建立信号描述符
int swSignalfd_setup(swReactor *reactor)
{
    if (signal_fd == 0)
    {   
        //http://www.man7.org/linux/man-pages/man2/signalfd.2.html
        // create a file descriptor for accepting signals
        signal_fd = signalfd(-1, &signalfd_mask, SFD_NONBLOCK | SFD_CLOEXEC);
        if (signal_fd < 0)
        {
            swWarn("signalfd() failed. Error: %s[%d]", strerror(errno), errno);
            return SW_ERR;
        }
        SwooleG.signal_fd = signal_fd;
        if (sigprocmask(SIG_BLOCK, &signalfd_mask, NULL) == -1)
        {
            swWarn("sigprocmask() failed. Error: %s[%d]", strerror(errno), errno);
            return SW_ERR;
        }
        //设置事件回调函数 也就是SW_FD_SIGNAL （11）可读事件发生时就回调 swSignalfd_onSignal 函数
        reactor->setHandle(reactor, SW_FD_SIGNAL, swSignalfd_onSignal);//
        //设置监控的描述符 signal_fd ，监听SW_FD_SIGNAL事件
        reactor->add(reactor, signal_fd, SW_FD_SIGNAL);
        return SW_OK;
    }
    else
    {
        swWarn("signalfd has been created");
        return SW_ERR;
    }
}

static void swSignalfd_clear()
{
    if (signal_fd)
    {
        if (sigprocmask(SIG_UNBLOCK, &signalfd_mask, NULL) < 0)//清除信号阻塞
        {
            swSysError("sigprocmask(SIG_UNBLOCK) failed.");
        }
        close(signal_fd);
        bzero(&signalfd_mask, sizeof(signalfd_mask));
    }
    signal_fd = 0;
}

//信号处理函数
static int swSignalfd_onSignal(swReactor *reactor, swEvent *event)
{
    int n;
    struct signalfd_siginfo siginfo;
    n = read(event->fd, &siginfo, sizeof(siginfo));//从信号描述符中读取siginfo
    if (n < 0)
    {
        swWarn("read from signalfd failed. Error: %s[%d]", strerror(errno), errno);
        return SW_OK;
    }
    if (siginfo.ssi_signo >=  SW_SIGNO_MAX)
    {
        swWarn("unknown signal[%d].", siginfo.ssi_signo);
        return SW_OK;
    }
    if (signals[siginfo.ssi_signo].active)//判断信号是否有效，并回调信号处理函数
    {
        if (signals[siginfo.ssi_signo].callback)
        {
            signals[siginfo.ssi_signo].callback(siginfo.ssi_signo);//回调信号处理函数 参数是信号值
        }
        else
        {
            swWarn("signal[%d] callback is null.", siginfo.ssi_signo);
        }
    }

    return SW_OK;
}

#endif

#ifdef HAVE_KQUEUE
static void swKqueueSignal_set(int signo, swSignalHander callback)
{
    struct kevent ev;
    swReactor *reactor = SwooleG.main_reactor;
    struct
    {
        int fd;
    } *reactor_obj = reactor->object;
    int new_event_num;
    // clear signal
    if (callback == NULL)
    {
        signal(signo, SIG_DFL);
        bzero(&signals[signo], sizeof(swSignal));
        EV_SET(&ev, signo, EVFILT_SIGNAL, EV_DELETE, 0, 0, NULL);
        new_event_num = reactor->event_num <= 0 ? 0 : reactor->event_num - 1;
    }
    // add/update signal
    else
    {
        signal(signo, SIG_IGN);
        signals[signo].callback = callback;
        signals[signo].signo = signo;
        if (signals[signo].active)
        {
            // the event already exists, do not change event_num
            new_event_num = reactor->event_num;
        }
        else
        {
            signals[signo].active = 1;
            // otherwise increment event_num
            new_event_num = reactor->event_num + 1;
        }
        // save swSignal* as udata
        EV_SET(&ev, signo, EVFILT_SIGNAL, EV_ADD, 0, 0, &signals[signo]);
    }
    int n = kevent(reactor_obj->fd, &ev, 1, NULL, 0, NULL);
    if (n < 0)
    {
        if (unlikely(callback))
        {
            swWarn("kevent set signal[%d] error", signo);
        }
        return;
    }
    // change event_num only when kevent() succeeded
    reactor->event_num = new_event_num;
}

#endif
