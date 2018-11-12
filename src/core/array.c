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

//动态数组

#include "swoole.h"
#include "array.h"

/**
 * Create new array
 */
swArray *swArray_new(int page_size, size_t item_size)
{
    /*
    typedef struct _swArray
    {
        void **pages;

        
         // 页的数量
         
        uint16_t page_num;

        
        // 每页的数据元素个数
        
        uint16_t page_size;

        
        // 数据元素的尺寸
        
        uint32_t item_size;

        
        // 数据个数
        
        uint32_t item_num;
        uint32_t offset;
    } swArray;
    */
    swArray *array = sw_malloc(sizeof(swArray));
    if (array == NULL)
    {
        swoole_error_log(SW_LOG_ERROR, SW_ERROR_MALLOC_FAIL, "malloc[0] failed.");
        return NULL;
    }
    bzero(array, sizeof(swArray));

    array->pages = sw_malloc(sizeof(void*) * SW_ARRAY_PAGE_MAX);//SW_ARRAY_PAGE_MAX = 1024 个页面
    if (array->pages == NULL)
    {
        sw_free(array);
        swoole_error_log(SW_LOG_ERROR, SW_ERROR_MALLOC_FAIL, "malloc[1] failed.");
        return NULL;
    }

    array->item_size = item_size;//数据元素的尺寸
    array->page_size = page_size;//每页的数据元素个数 1024

    swArray_extend(array);//分配一页内存

    return array;
}

/**
 * Destory the array
 */
void swArray_free(swArray *array)
{
    int i;
    for (i = 0; i < array->page_num; i++)
    {
        sw_free(array->pages[i]);
    }
    sw_free(array->pages);
    sw_free(array);
}

/**
 * Extend the memory pages of the array
 */
//扩展内存page 也就是增加一页
int swArray_extend(swArray *array)
{
    if (array->page_num == SW_ARRAY_PAGE_MAX) // SW_ARRAY_PAGE_MAX = 1024
    {
        swWarn("max page_num is %d", array->page_num);
        return SW_ERR;
    }
    array->pages[array->page_num] = sw_calloc(array->page_size, array->item_size);//分配一个页面的内存，内存大小是 每页的数据元素个数*数据元素的尺寸
    if (array->pages[array->page_num] == NULL)
    {
        swWarn("malloc[1] failed.");
        return SW_ERR;
    }
    array->page_num++;//页数++
    return SW_OK;
}

/**
 * Fetch data by index of the array
 */
//取得存储元素的其实地址
void *swArray_fetch(swArray *array, uint32_t n)
{   //取得在第几页
    int page = swArray_page(array, n); //swArray_page(array, n) = ((n) / (array)->page_size)
    if (page >= array->page_num)
    {
        return NULL;
    }
    //取得页面上的位置
    // swArray_offset(array, n) = ((n) % (array)->page_size)
    return array->pages[page] + (swArray_offset(array, n) * array->item_size);
}

/**
 * Append to the array
 */
//这个方法没有地方用
int swArray_append(swArray *array, void *data)
{
    int n = array->offset++;
    int page = swArray_page(array, n);

    if (page >= array->page_num && swArray_extend(array) < 0)
    {
        return SW_ERR;
    }
    array->item_num++;
    memcpy(array->pages[page] + (swArray_offset(array, n) * array->item_size), data, array->item_size);
    return n;
}

//data 放到array 中 这个方法没有地方用
int swArray_store(swArray *array, uint32_t n, void *data)
{
    int page = swArray_page(array, n);
    if (page >= array->page_num)//超页
    {
        swWarn("fetch index[%d] out of array", n);
        return SW_ERR;
    }
    memcpy(array->pages[page] + (swArray_offset(array, n) * array->item_size), data, array->item_size);
    return SW_OK;
}

//返回n 的存储位置 
void *swArray_alloc(swArray *array, uint32_t n)
{
    //n >= 当前页数*每页数据size
    while (n >= array->page_num * array->page_size)
    {
        if (swArray_extend(array) < 0)//增加一页
        {
            return NULL;
        }
    }

    int page = swArray_page(array, n);
    if (page >= array->page_num)
    {
        swWarn("fetch index[%d] out of array", n);
        return NULL;
    }
    return array->pages[page] + (swArray_offset(array, n) * array->item_size);
}

void swArray_clear(swArray *array)
{
    array->offset = 0;
    array->item_num = 0;
}
