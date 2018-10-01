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
//用数组结构存储二叉最小堆

#include "swoole.h"
#include "heap.h"

#define left(i)   ((i) << 1) //取左节点
#define right(i)  (((i) << 1) + 1) //右节点
#define parent(i) ((i) >> 1)  //父节点

static void swHeap_bubble_up(swHeap *heap, uint32_t i);
static uint32_t swHeap_maxchild(swHeap *heap, uint32_t i);
static void swHeap_percolate_down(swHeap *heap, uint32_t i);
//创建最小堆 time 定时器设置时用
swHeap *swHeap_new(size_t n, uint8_t type)
{
    //申请heap
    swHeap *heap = sw_malloc(sizeof(swHeap));
    if (!heap)
    {
        return NULL;
    }
    //1024 + 1 个node void * 指针
    if (!(heap->nodes = sw_malloc((n + 1) * sizeof(void *))))
    {
        sw_free(heap);
        return NULL;
    }
    heap->num = 1;//个数
    heap->size = (n + 1);//size 1025
    heap->type = type;//类型
    return heap;
}

//heap 释放
void swHeap_free(swHeap *heap)
{
    sw_free(heap->nodes);
    sw_free(heap);
}

static sw_inline int swHeap_compare(uint8_t type, uint64_t a, uint64_t b)
{
    if (type == SW_MIN_HEAP)
    {
        return a > b;
    }
    else
    {
        return a < b;
    }
}

//heap size
uint32_t swHeap_size(swHeap *q)
{
    return (q->num - 1);
}

//找到i节点下的最小子节点（priority最小）
static uint32_t swHeap_maxchild(swHeap *heap, uint32_t i)
{
    uint32_t child_i = left(i); //左节点
    if (child_i >= heap->num) //左节点不存在
    {
        return 0;
    }
    swHeap_node * child_node = heap->nodes[child_i];//取得左节点
    //比较左右节点，左节点 > 右节点 ，返回右节点下标
    if ((child_i + 1) < heap->num && swHeap_compare(heap->type, child_node->priority, heap->nodes[child_i + 1]->priority))
    {
        child_i++;
    }
    return child_i;
}
// 下标i的heap_node 插入到堆中，执行的结果是执行时间最小者排第一个的最小堆结构                   
static void swHeap_bubble_up(swHeap *heap, uint32_t i)
{
    swHeap_node *moving_node = heap->nodes[i];实际上就是                                                                                                                  
    uint32_t parent_i;
    //判断插入节点与父节点的执行时间大小进行替换。
    for (parent_i = parent(i); //parent(i)  展开是 ((i) >> 1) 即 i/2 的商  1/2 = 0 5/2 = 2 6/2 = 3
            (i > 1) && swHeap_compare(heap->type, heap->nodes[parent_i]->priority, moving_node->priority);//父节点>该节点
            i = parent_i, parent_i = parent(i))
    {   
        //交换位置
        heap->nodes[i] = heap->nodes[parent_i];
        heap->nodes[i]->position = i;
    }
    //把加入的节点放到i中,经过上面的调整，i是放入加入节点的最合适的位置。
    heap->nodes[i] = moving_node;
    moving_node->position = i;
}

// i节点向下移到合适位置
static void swHeap_percolate_down(swHeap *heap, uint32_t i)
{
    uint32_t child_i;
    swHeap_node *moving_node = heap->nodes[i];

    while ((child_i = swHeap_maxchild(heap, i)) //i 节点下面的最小priority的节点下标
            && swHeap_compare(heap->type, moving_node->priority, heap->nodes[child_i]->priority)) //如果该节点大于子节点，则字节的向上调整
    {
        heap->nodes[i] = heap->nodes[child_i];
        heap->nodes[i]->position = i;
        i = child_i;//i  =  子节点（最小priority）
    }

    heap->nodes[i] = moving_node;
    moving_node->position = i;
}

//新建一个swHeap_node 把 swTimer_node 作为data 参数挂载到 heap->nodes
swHeap_node* swHeap_push(swHeap *heap, uint64_t priority, void *data)
{
    void *tmp;
    uint32_t i;
    uint32_t newsize;

    if (heap->num >= heap->size)
    {
        newsize = heap->size * 2;
        if (!(tmp = sw_realloc(heap->nodes, sizeof(void *) * newsize)))
        {
            return NULL;
        }
        heap->nodes = tmp;
        heap->size = newsize;
    }

    swHeap_node *node = sw_malloc(sizeof(swHeap_node));//新建一个swHeap_node 把 swTimer_node 作为data 参数挂载到 heap->nodes
    if (!node)
    {
        return NULL;
    }
    /*typedef struct swHeap_node
    {
        uint64_t priority;
        uint32_t position;
        void *data;
    } swHeap_node;

    typedef struct _swHeap
    {
        uint32_t num;
        uint32_t size;
        uint8_t type;
        swHeap_node **nodes;
    } swHeap;
    */
    node->priority = priority;//定时器
    node->data = data;
    i = heap->num++;
    heap->nodes[i] = node;//
    swHeap_bubble_up(heap, i);
    return node;
}
//swHeap_change_priority(timer->heap, tnode->exec_msec, tmp)
void swHeap_change_priority(swHeap *heap, uint64_t new_priority, void* ptr)
{
    swHeap_node *node = ptr;
    uint32_t pos = node->position;
    uint64_t old_pri = node->priority;

    node->priority = new_priority;
    //old_pri 是旧的权重   new_priority 新的权重
    if (swHeap_compare(heap->type, old_pri, new_priority))
    {
        swHeap_bubble_up(heap, pos);
    }
    else//正常会进入这个逻辑  旧的 < 新的  把该节点向推下调整
    {
        swHeap_percolate_down(heap, pos);//pos 是heap->node 中的下标
    }
}

//移除节点
int swHeap_remove(swHeap *heap, swHeap_node *node)
{
    uint32_t pos = node->position;
    heap->nodes[pos] = heap->nodes[--heap->num];//最后一个节点放到要移除的节点

    if (swHeap_compare(heap->type, node->priority, heap->nodes[pos]->priority))//要移除的节点大于最后节点的priority的话，最后一个节点上浮，否则下浮
    {
        swHeap_bubble_up(heap, pos);
    }
    else
    {
        swHeap_percolate_down(heap, pos);
    }
    return SW_OK;
}

//将尾部元素和堆顶元素进行交换，然后再对堆顶元素进行下浮，找到最小priority的节点放到top 位置。
void *swHeap_pop(swHeap *heap)
{
    swHeap_node *head;
    if (!heap || heap->num == 1) 
    {
        return NULL;
    }

    head = heap->nodes[1];
    heap->nodes[1] = heap->nodes[--heap->num];
    swHeap_percolate_down(heap, 1);

    void *data = head->data;
    sw_free(head);
    return data; //返回pop节点的data
}
//返回top 节点中的值
void *swHeap_peek(swHeap *heap)
{
    if (heap->num == 1)
    {
        return NULL;
    }
    swHeap_node *node = heap->nodes[1];
    if (!node)
    {
        return NULL;
    }
    return node->data;
}

void swHeap_print(swHeap *heap)
{
    int i;
    for(i = 1; i < heap->num; i++)
    {
        printf("#%d\tpriority=%ld, data=%p\n", i, (long)heap->nodes[i]->priority, heap->nodes[i]->data);
    }
}
