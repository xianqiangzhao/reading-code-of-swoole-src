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

//内存映射 mmap可以减少读写磁盘操作的IO消耗、减少内存拷贝。在实现高性能的磁盘操作程序中，可以使用mmap来提升性能。

#include "php_swoole.h"

typedef struct
{
    size_t size;
    off_t offset;
    char *filename;
    void *memory;
    void *ptr;
} swMmapFile;

static size_t mmap_stream_write(php_stream * stream, const char *buffer, size_t length TSRMLS_DC);
static size_t mmap_stream_read(php_stream *stream, char *buffer, size_t length TSRMLS_DC);
static int mmap_stream_flush(php_stream *stream TSRMLS_DC);
static int mmap_stream_seek(php_stream *stream, off_t offset, int whence, off_t *newoffset TSRMLS_DC);
static int mmap_stream_close(php_stream *stream, int close_handle TSRMLS_DC);
static PHP_METHOD(swoole_mmap, open);

static zend_class_entry swoole_mmap_ce;
zend_class_entry *swoole_mmap_class_entry_ptr;

ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_mmap_open, 0, 0, 1)
    ZEND_ARG_INFO(0, filename)
    ZEND_ARG_INFO(0, size)
    ZEND_ARG_INFO(0, offset)
ZEND_END_ARG_INFO()

static const zend_function_entry swoole_mmap_methods[] =
{
    PHP_ME(swoole_mmap, open, arginfo_swoole_mmap_open, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_FE_END
};

//定义 stream 结构体回调函数
php_stream_ops mmap_ops =
{
    mmap_stream_write, //写
    mmap_stream_read,  //读
    mmap_stream_close, //关闭
    mmap_stream_flush, //刷新
    "swoole_mmap",//名称
    mmap_stream_seek,//find
    NULL,
    NULL,
    NULL
};

//写
static size_t mmap_stream_write(php_stream * stream, const char *buffer, size_t length TSRMLS_DC)
{
    swMmapFile *res = stream->abstract;//取得内存映射对应的结构体，在open 方法时定义的。

    int n_write = MIN(res->memory + res->size - res->ptr, length);//写入的最小size ，防止写入过大
    if (n_write == 0)
    {
        return 0;
    }
    memcpy(res->ptr, buffer, n_write); //写入到res->ptr ，buffer 里面的 n_write 字节。
    res->ptr += n_write;//写入地址增大
    return n_write;//返回写入大小
}

//读
static size_t mmap_stream_read(php_stream *stream, char *buffer, size_t length TSRMLS_DC)
{
    swMmapFile *res = stream->abstract;

    int n_read = MIN(res->memory + res->size - res->ptr, length);
    if (n_read == 0)
    {
        return 0;
    }
    memcpy(buffer, res->ptr, n_read); //读到buffer 中
    res->ptr += n_read;
    return n_read;
}

//刷新同步到文件中
static int mmap_stream_flush(php_stream *stream TSRMLS_DC)
{
    swMmapFile *res = stream->abstract;
    return msync(res->memory, res->size, MS_SYNC | MS_INVALIDATE);
}

static int mmap_stream_seek(php_stream *stream, off_t offset, int whence, off_t *newoffset TSRMLS_DC)
{
    swMmapFile *res = stream->abstract;

    switch (whence)
    {
    case SEEK_SET:
        if (offset < 0 || offset > res->size)
        {
            *newoffset = (off_t) -1;
            return -1;
        }
        res->ptr = res->memory + offset;
        *newoffset = offset;
        return 0;
    case SEEK_CUR:
        if (res->ptr + offset < res->memory || res->ptr + offset > res->memory + res->size)
        {
            *newoffset = (off_t) -1;
            return -1;
        }
        res->ptr += offset;
        *newoffset = res->ptr - res->memory;
        return 0;
    case SEEK_END:
        if (offset > 0 || -1 * offset > res->size)
        {
            *newoffset = (off_t) -1;
            return -1;
        }
        res->ptr += offset;
        *newoffset = res->ptr - res->memory;
        return 0;
    default:
        *newoffset = (off_t) -1;
        return -1;
    }
}

//关闭
static int mmap_stream_close(php_stream *stream, int close_handle TSRMLS_DC)
{
    swMmapFile *res = stream->abstract;
    if (close_handle)
    {
        munmap(res->memory, res->size);//内存映射解除
    }
    efree(res);
    return 0;
}

//初始化
void swoole_mmap_init(int module_number TSRMLS_DC)
{
    SWOOLE_INIT_CLASS_ENTRY(swoole_mmap_ce, "swoole_mmap", "Swoole\\Mmap", swoole_mmap_methods);
    swoole_mmap_class_entry_ptr = zend_register_internal_class(&swoole_mmap_ce TSRMLS_CC);
    SWOOLE_CLASS_ALIAS(swoole_mmap, "Swoole\\Mmap");
}

//open 方法 参数文件名

static PHP_METHOD(swoole_mmap, open)
{
    char *filename;
    zend_size_t l_filename;
    long offset = 0;
    long size = -1;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|ll", &filename, &l_filename, &size, &offset) == FAILURE)
    {
        RETURN_FALSE;
    }

    if (l_filename <= 0)
    {
        swoole_php_fatal_error(E_WARNING, "file name is required.");
        RETURN_FALSE;
    }

    int fd;
    if ((fd = open(filename, O_RDWR)) < 0)
    {
        swoole_php_sys_error(E_WARNING, "open(%s, O_RDWR) failed.", filename);
        RETURN_FALSE;
    }

    if (size <= 0)
    {
        struct stat _stat;
        if (fstat(fd, &_stat) < 0)
        {
            swoole_php_sys_error(E_WARNING, "fstat(%s) failed.", filename);
            close(fd);
            RETURN_FALSE;
        }
        if (_stat.st_size == 0)
        {
            swoole_php_sys_error(E_WARNING, "file[%s] is empty.", filename);
            close(fd);
            RETURN_FALSE;
        }
        if (offset > 0)
        {
            size = _stat.st_size - offset;
        }
        else
        {
            size = _stat.st_size;
        }
    }
    //把传进来的文件名打开后，调用内存映射函数 mmap
    //size 打开的size 
    // PROT_WRITE | PROT_READ 以可读可写方式打开
    // MAP_SHARED Share this mapping
    //fd 打开的文件描述符
    //offset  偏移量
    void *addr = mmap(NULL, size, PROT_WRITE | PROT_READ, MAP_SHARED, fd, offset);
    if (addr == NULL)
    {
        swoole_php_sys_error(E_WARNING, "mmap(%ld) failed.", size);
        RETURN_FALSE;
    }
    //把打开的信息情报放到内存映射结构体中
    swMmapFile *res = emalloc(sizeof(swMmapFile));
    res->filename = filename;//文件名
    res->size = size;  //size 
    res->offset = offset;//偏移量
    res->memory = addr;//内存映射地址 对应打开的文件
    res->ptr = addr;//同上

    close(fd);//关闭描述符

    //以 php_stream 方式读，写，刷新操作这个内存映射空间地址。
    php_stream *stream = php_stream_alloc(&mmap_ops, res, NULL, "r+");
    php_stream_to_zval(stream, return_value);
}
