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
#include <sys/shm.h>

//申请共享内存
void* sw_shm_malloc(size_t size)
{
    /*
    typedef struct _swShareMemory_mmap
    {
        size_t size;
        char mapfile[SW_SHM_MMAP_FILE_LEN];
        int tmpfd;
        int key;
        int shmid;
        void *mem;
    } swShareMemory;
    */
    swShareMemory object;
    void *mem;
    size += sizeof(swShareMemory);//size = 申请的size + sizeof(swShareMemory)
    mem = swShareMemory_mmap_create(&object, size, NULL);
    if (mem == NULL)
    {
        return NULL;
    }
    else
    {
        memcpy(mem, &object, sizeof(swShareMemory)); //把swShareMemory copy 到申请到内存的头
        return mem + sizeof(swShareMemory); //返回可用的内存地址
    }
}

//申请共享内存 size = num * _size
void* sw_shm_calloc(size_t num, size_t _size)
{
    swShareMemory object;
    void *mem;
    void *ret_mem;
    int size = sizeof(swShareMemory) + (num * _size);
    mem = swShareMemory_mmap_create(&object, size, NULL);
    if (mem == NULL)
    {
        return NULL;
    }
    else
    {
        memcpy(mem, &object, sizeof(swShareMemory));
        ret_mem = mem + sizeof(swShareMemory);
        bzero(ret_mem, size - sizeof(swShareMemory)); //除去头以外的内存初始化为\0
        return ret_mem;
    }
}

//在内存区域设置保护
int sw_shm_protect(void *addr, int flags)
{
    swShareMemory *object = (swShareMemory *) (addr - sizeof(swShareMemory));//把内存地址头的swShareMemory减去，就得到swShareMemory结果体，里面保存这块内存的信息。
    return mprotect(object, object->size, flags);//object->size 就是这块内存的大小。
}

//释放内存
void sw_shm_free(void *ptr)
{
    swShareMemory *object = ptr - sizeof(swShareMemory);
    swShareMemory_mmap_free(object);
}

//重新申请内存
void* sw_shm_realloc(void *ptr, size_t new_size)
{
    swShareMemory *object = ptr - sizeof(swShareMemory);//取得原内存swShareMemory对象，目的是取得原内存的size
    void *new_ptr;
    new_ptr = sw_shm_malloc(new_size);//新内存地址申请
    if (new_ptr == NULL)
    {
        return NULL;
    }
    else
    {
        memcpy(new_ptr, ptr, object->size);//把原内存上的数据 copy 到新内存上
        sw_shm_free(ptr);//释放老内存
        return new_ptr;//返回新内存地址
    }
}

//内存映射的内存申请
//主要是用到了mmap http://man7.org/linux/man-pages/man2/mmap.2.html

void *swShareMemory_mmap_create(swShareMemory *object, size_t size, char *mapfile)
{
    void *mem;
    int tmpfd = -1;
    int flag = MAP_SHARED;
    bzero(object, sizeof(swShareMemory));

#ifdef MAP_ANONYMOUS
    flag |= MAP_ANONYMOUS;//这个逻辑会进入，这个参数的意思是不映射任何文件，初始空间为0，fd = -1
#else
    if (mapfile == NULL)
    {
        mapfile = "/dev/zero";
    }
    if ((tmpfd = open(mapfile, O_RDWR)) < 0)
    {
        return NULL;
    }
    strncpy(object->mapfile, mapfile, SW_SHM_MMAP_FILE_LEN);
    object->tmpfd = tmpfd;
#endif

#if defined(SW_USE_HUGEPAGE) && defined(MAP_HUGETLB) //大页面内存映射  swoole configure 时加入  --enable-hugepage  参数的话就开启了支持HUGEPAGE
    if (size > 2 * 1024 * 1024)
    {
        flag |= MAP_HUGETLB;
    }
#endif

    mem = mmap(NULL, size, PROT_READ | PROT_WRITE, flag, tmpfd, 0);//调用mmap系统函数映射size 大小可读可写，tmpfd= -1 ,flag = MAP_SHARED ,offset = 0的内存空间。
#ifdef MAP_FAILED
    if (mem == MAP_FAILED) //返回时 = -1 的话映射失败
#else
    if (!mem)
#endif
    {
        swWarn("mmap(%ld) failed. Error: %s[%d]", size, strerror(errno), errno);
        return NULL;
    }
    else
    {//成功的话，把申请信息给到 object(swShareMemory) 中
        object->size = size;
        object->mem = mem;
        return mem;
    }
}

//内存释放
int swShareMemory_mmap_free(swShareMemory *object)
{
    return munmap(object->mem, object->size);
}

//没有地方用到，shmget也是进程间共享内存的一种方式
void *swShareMemory_sysv_create(swShareMemory *object, size_t size, int key)
{
    int shmid;
    void *mem;
    bzero(object, sizeof(swShareMemory));

    if (key == 0)
    {
        key = IPC_PRIVATE;
    }
    //SHM_R | SHM_W
    if ((shmid = shmget(key, size, IPC_CREAT)) < 0)
    {
        swSysError("shmget(%d, %ld) failed.", key, size);
        return NULL;
    }
    if ((mem = shmat(shmid, NULL, 0)) == (void *) -1)
    {
        swWarn("shmat() failed. Error: %s[%d]", strerror(errno), errno);
        return NULL;
    }
    else
    {
        object->key = key;
        object->shmid = shmid;
        object->size = size;
        object->mem = mem;
        return mem;
    }
}

int swShareMemory_sysv_free(swShareMemory *object, int rm)
{
    int shmid = object->shmid;
    int ret = shmdt(object->mem);
    if (rm == 1)
    {
        shmctl(shmid, IPC_RMID, NULL);
    }
    return ret;
}
