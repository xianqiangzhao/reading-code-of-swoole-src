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
//php7 包裹函数，目的是php版本的变化的话，直接修改这个包裹函数就可以了。
//当然也可以实现php5 ,php 7 不同版本不同接口。对于调用方而言，不用做任何修改。

#ifndef EXT_SWOOLE_PHP7_WRAPPER_H_
#define EXT_SWOOLE_PHP7_WRAPPER_H_

#include "ext/standard/php_http.h"

#define sw_php_var_serialize                php_var_serialize
typedef size_t zend_size_t;

//增加arr 的引用，并增加或更新放到ht 数组里 key 为str
#define ZEND_SET_SYMBOL(ht,str,arr)         zval_add_ref(arr); zend_hash_str_update(ht, str, sizeof(str)-1, arr);

//判断参数传过来的是是否为bool 类型。
static sw_inline zend_bool Z_BVAL_P(zval *v)
{
    return Z_TYPE_P(v) == IS_TRUE;
}

//----------------------------------Array API------------------------------------
//  数组 key value 增加
#define sw_add_assoc_stringl(__arg, __key, __str, __length, __duplicate)   add_assoc_stringl_ex(__arg, __key, strlen(__key), __str, __length)
static sw_inline int sw_add_assoc_stringl_ex(zval *arg, const char *key, size_t key_len, char *str, size_t length, int __duplicate)
{
    return add_assoc_stringl_ex(arg, key, key_len - 1, str, length);
}
//  数组 下表自动增加 value 增加
#define sw_add_next_index_stringl(arr, str, len, dup)    add_next_index_stringl(arr, str, len)

// 数组 long 型增加
static sw_inline int sw_add_assoc_long_ex(zval *arg, const char *key, size_t key_len, long value)
{
    return add_assoc_long_ex(arg, key, key_len - 1, value);
}

//数组double 型增加
static sw_inline int sw_add_assoc_double_ex(zval *arg, const char *key, size_t key_len, double value)
{
    return add_assoc_double_ex(arg, key, key_len - 1, value);
}

#define SW_Z_ARRVAL_P(z)                          Z_ARRVAL_P(z)->ht

//数组循环
#define SW_HASHTABLE_FOREACH_START(ht, _val) ZEND_HASH_FOREACH_VAL(ht, _val);  {
#define SW_HASHTABLE_FOREACH_START2(ht, k, klen, ktype, _val) zend_string *_foreach_key;\
    ZEND_HASH_FOREACH_STR_KEY_VAL(ht, _foreach_key, _val);\
    if (!_foreach_key) {k = NULL; klen = 0; ktype = 0;}\
    else {k = _foreach_key->val, klen=_foreach_key->len; ktype = 1;} {

#define SW_HASHTABLE_FOREACH_END()                 } ZEND_HASH_FOREACH_END();
//这几个没用 
#define Z_ARRVAL_PP(s)                             Z_ARRVAL_P(*s)
#define SW_Z_TYPE_P                                Z_TYPE_P        
#define SW_Z_TYPE_PP(s)                            SW_Z_TYPE_P(*s)
#define Z_STRVAL_PP(s)                             Z_STRVAL_P(*s)
#define Z_STRLEN_PP(s)                             Z_STRLEN_P(*s)
#define Z_LVAL_PP(v)                               Z_LVAL_P(*v)

//日期format ,返回string
static inline char* sw_php_format_date(char *format, size_t format_len, time_t ts, int localtime)
{
    zend_string *time = php_format_date(format, format_len, ts, localtime);
    char *return_str = estrndup(time->val, time->len);
    zend_string_release(time);
    return return_str;
}

//url encode
static sw_inline char* sw_php_url_encode(char *value, size_t value_len, int* exten)
{
    zend_string *str = php_url_encode(value, value_len);
    *exten = str->len;
    char *return_str = estrndup(str->val, str->len);
    zend_string_release(str);
    return return_str;
}
//给zval 增加ref
#define sw_zval_add_ref(p)   Z_TRY_ADDREF_P(*p)
//销毁对象，ref-1 等于0 的话，就销毁，大于0 就放到GC中，被怀疑是垃圾。
#define sw_zval_ptr_dtor(p)  zval_ptr_dtor(*p)

//定义最大参数个数
#define SW_PHP_MAX_PARAMS_NUM     20

//函数调用,一般用在回调注册的函数上
//server->on("connect", function(){})  function 就会注册到一个zval上,
static sw_inline int sw_call_user_function_ex(HashTable *function_table, zval** object_pp, zval *function_name, zval **retval_ptr_ptr, uint32_t param_count, zval ***params, int no_separation, HashTable* ymbol_table)
{
    zval real_params[SW_PHP_MAX_PARAMS_NUM];
    uint32_t i = 0;
    for (; i < param_count; i++)
    {
        real_params[i] = **params[i];
    }
    zval phpng_retval;
    *retval_ptr_ptr = &phpng_retval;
    zval *object_p = (object_pp == NULL) ? NULL : *object_pp;
    return call_user_function_ex(function_table, object_p, function_name, &phpng_retval, param_count, real_params, no_separation, NULL);
}

//下面是另一种调用方法，利用zend_fcall_info 传参，直接调用zend_call_function
//call_user_function_ex 是一个宏，最后也是调用 zend_call_function
static sw_inline int sw_call_user_function_fast(zval *function_name, zend_fcall_info_cache *fci_cache, zval **retval_ptr_ptr, uint32_t param_count, zval ***params)
{
    zval real_params[SW_PHP_MAX_PARAMS_NUM];
    uint32_t i = 0;
    for (; i < param_count; i++)
    {
        real_params[i] = **params[i];
    }

    zval phpng_retval;
    *retval_ptr_ptr = &phpng_retval;

    zend_fcall_info fci;
    fci.size = sizeof(fci);
#if PHP_MAJOR_VERSION == 7 && PHP_MINOR_VERSION == 0
    fci.function_table = EG(function_table);
    fci.symbol_table = NULL;
#endif
    fci.object = NULL;
    ZVAL_COPY_VALUE(&fci.function_name, function_name);
    fci.retval = &phpng_retval;
    fci.param_count = param_count;
    fci.params = real_params;
    fci.no_separation = 0;

    return zend_call_function(&fci, fci_cache);
}
//unserialize
#define sw_php_var_unserialize(rval, p, max, var_hash)  php_var_unserialize(*rval, p, max, var_hash)
//定义一个变量名p的zval 结构
#define SW_MAKE_STD_ZVAL(p)             zval _stack_zval_##p; p = &(_stack_zval_##p)
//从php 内存管理器中申请一个zval,并初始化赋值0
#define SW_ALLOC_INIT_ZVAL(p)           do{p = (zval *)emalloc(sizeof(zval)); bzero(p, sizeof(zval));}while(0)
//zval 分离
#define SW_SEPARATE_ZVAL(p)             zval _##p;\
    memcpy(&_##p, p, sizeof(_##p));\
    p = &_##p
//返回string
#define SW_RETURN_STRINGL(s, l, dup)    do{RETVAL_STRINGL(s, l); if (dup == 0) efree(s);}while(0);return
#define SW_RETVAL_STRINGL(s, l, dup)    do{RETVAL_STRINGL(s, l); if (dup == 0) efree(s);}while(0)
#define SW_RETVAL_STRING(s, dup)        do{RETVAL_STRING(s); if (dup == 0) efree(s);}while(0)
//fetch resource
#define SW_ZEND_FETCH_RESOURCE_NO_RETURN(rsrc, rsrc_type, passed_id, default_id, resource_type_name, resource_type)        \
        (rsrc = (rsrc_type) zend_fetch_resource(Z_RES_P(*passed_id), resource_type_name, resource_type))

//注册资源
#define SW_ZEND_REGISTER_RESOURCE(return_value, result, le_result)  ZVAL_RES(return_value,zend_register_resource(result, le_result))

#define SW_RETURN_STRING(val, duplicate)     RETURN_STRING(val)
#define sw_add_assoc_string(array, key, value, duplicate)   add_assoc_string(array, key, value)
//hash copy 代码中没有用到 
#define sw_zend_hash_copy(target,source,pCopyConstructor,tmp,size) zend_hash_copy(target,source,pCopyConstructor)
#define sw_php_array_merge                                          php_array_merge

//类注册
#define sw_zend_register_internal_class_ex(entry,parent_ptr,str)    zend_register_internal_class_ex(entry,parent_ptr)

//取得执行文件名
#define sw_zend_get_executed_filename()                             zend_get_executed_filename()

//根据有无参数及个数来调用函数，一般用在自定义函数。
#define sw_zend_call_method_with_0_params(obj, ptr, what, method, retval) \
    zval __retval;\
    zend_call_method_with_0_params(*obj, ptr, what, method, &__retval);\
    if (ZVAL_IS_NULL(&__retval)) *(retval) = NULL;\
    else *(retval) = &__retval;

#define sw_zend_call_method_with_1_params(obj, ptr, what, method, retval, v1)           \
    zval __retval;\
    zend_call_method_with_1_params(*obj, ptr, what, method, &__retval, v1);\
    if (ZVAL_IS_NULL(&__retval)) *(retval) = NULL;\
    else *(retval) = &__retval;

#define sw_zend_call_method_with_2_params(obj, ptr, what, method, retval, v1, v2)    \
    zval __retval;\
    zend_call_method_with_2_params(*obj, ptr, what, method, &__retval, v1, v2);\
    if (ZVAL_IS_NULL(&__retval)) *(retval) = NULL;\
    else *(retval) = &__retval;
//char 赋值到zval
#define SW_ZVAL_STRINGL(z, s, l, dup)         ZVAL_STRINGL(z, s, l)
#define SW_ZVAL_STRING(z,s,dup)               ZVAL_STRING(z,s)
//智能string 可以自动扩容
#define sw_smart_str                          smart_string
#define zend_get_class_entry                  Z_OBJCE_P

//copy  b = a
#define sw_copy_to_stack(a, b)                {zval *__tmp = (zval *) a;\
    a = &b;\
    memcpy(a, __tmp, sizeof(zval));}
//新分配一个zval ，值为val
static sw_inline zval* sw_zval_dup(zval *val)
{
    zval *dup;
    SW_ALLOC_INIT_ZVAL(dup);
    memcpy(dup, val, sizeof(zval));
    return dup;
}

//释放val,先是dtor val 中的值，然后efree val
static sw_inline void sw_zval_free(zval *val)
{
    sw_zval_ptr_dtor(&val);
    efree(val);
}

//读class 中的属性
static sw_inline zval* sw_zend_read_property(zend_class_entry *class_ptr, zval *obj, const char *s, int len, int silent)
{
    zval rv;
    return zend_read_property(class_ptr, obj, s, len, silent, &rv);
}
//读属性，不是array ，就放一个空array.
static sw_inline zval* sw_zend_read_property_array(zend_class_entry *class_ptr, zval *obj, const char *s, int len, int silent)
{
    zval rv, *property = zend_read_property(class_ptr, obj, s, len, silent, &rv);
    zend_uchar ztype = Z_TYPE_P(property);
    if (ztype != IS_ARRAY)
    {
        zval temp_array;
        array_init(&temp_array);
        zend_update_property(class_ptr, obj, s, len, &temp_array TSRMLS_CC);
        zval_ptr_dtor(&temp_array);
        // NOTICE: if user unset the property, this pointer will be changed
        // some objects such as `swoole_http2_request` always be writable
        if (ztype == IS_UNDEF)
        {
            property = zend_read_property(class_ptr, obj, s, len, silent, &rv);
        }
    }

    return property;
}

//是否可以回调，回调函数名字放到 name 中。
//内核判断是否回调的原理是判断 cb 的类型，如是string ，那么根据这个string 去 全局EG(function_table) 查找是否存在这个函数。
//如果cb 是array ,那么这个array中是对象名，函数名，根据这两个参数去查找是否是注册过的函数。
static sw_inline int sw_zend_is_callable(zval *cb, int a, char **name)
{
    zend_string *key = NULL;
    int ret = zend_is_callable(cb, a, &key);
    char *tmp = estrndup(key->val, key->len);
    zend_string_release(key);
    *name = tmp;
    return ret;
}
//zend_is_callable 是包裹 zend_is_callable_ex 的宏
static inline int sw_zend_is_callable_ex(zval *callable, zval *object, uint check_flags, char **callable_name, int *callable_name_len, zend_fcall_info_cache *fcc, char **error TSRMLS_DC)
{
    zend_string *key = NULL;
    int ret = zend_is_callable_ex(callable, NULL, check_flags, &key, fcc, error);
    char *tmp = estrndup(key->val, key->len);
    zend_string_release(key);
    *callable_name = tmp;
    return ret;
}

//hash 中删除元素
static inline int sw_zend_hash_del(HashTable *ht, char *k, int len)
{
    return zend_hash_str_del(ht, k, len - 1);
}
//hash 中增加元素，存在的话就走更新
static inline int sw_zend_hash_add(HashTable *ht, char *k, int len, void *pData, int datasize, void **pDest)
{
    zval **real_p = (zval **)pData;
    return zend_hash_str_add(ht, k, len - 1, *real_p) ? SUCCESS : FAILURE;
}

//hash 更新
static inline int sw_zend_hash_index_update(HashTable *ht, int key, void *pData, int datasize, void **pDest)
{
    zval **real_p = (zval **)pData;
    return zend_hash_index_update(ht, key, *real_p) ? SUCCESS : FAILURE;
}

static inline int sw_zend_hash_update(HashTable *ht, char *k, int len, zval *val, int size, void *ptr)
{
    return zend_hash_str_update(ht, (const char*)k, len -1, val) ? SUCCESS : FAILURE;
}

//hash find
static inline int sw_zend_hash_find(HashTable *ht, const char *k, int len, void **v)
{
    zval *value = zend_hash_str_find(ht, k, len - 1);
    if (value == NULL)
    {
        return FAILURE;
    }
    else
    {
        *v = (void *) value;
        return SUCCESS;
    }
}

//给calss 注册别名
static inline int sw_zend_register_class_alias(const char *name, zend_class_entry *ce)
{
    int name_len = strlen(name);
    zend_string *_name;
    if (name[0] == '\\')
    {
        _name = zend_string_init(name, name_len, 1);
        zend_str_tolower_copy(ZSTR_VAL(_name), name + 1, name_len - 1);
    }
    else
    {
        _name = zend_string_init(name, strlen(name), 1);
        zend_str_tolower_copy(ZSTR_VAL(_name), name, name_len);
    }

    zend_string *_interned_name = zend_new_interned_string(_name);

#if PHP_VERSION_ID >= 70300
    return zend_register_class_alias_ex(_interned_name->val, _interned_name->len, ce, 1);
#else
    return zend_register_class_alias_ex(_interned_name->val, _interned_name->len, ce);
#endif
}

//根据data 中的数据key ,value 生产 key=value&&key1=value 结构的char
static sw_inline char* sw_http_build_query(zval *data, zend_size_t *length, smart_str *formstr TSRMLS_DC)
{
    if (php_url_encode_hash_ex(HASH_OF(data), formstr, NULL, 0, NULL, 0, NULL, 0, NULL, NULL, (int) PHP_QUERY_RFC1738) == FAILURE)
    {
        if (formstr->s)
        {
            smart_str_free(formstr);
        }
        return NULL;
    }
    if (!formstr->s)
    {
        return NULL;
    }
    smart_str_0(formstr);
    *length = formstr->s->len;
    return formstr->s->val;
}
//取得对象的handle 值，每个对象都有不同的值
#define sw_get_object_handle(object)    Z_OBJ_HANDLE(*object)
//这个宏用在class 的__destruct时，来判断是否时销毁对象。
#define SW_PREVENT_USER_DESTRUCT if(unlikely(!(GC_FLAGS(Z_OBJ_P(getThis())) & IS_OBJ_DESTRUCTOR_CALLED))){RETURN_NULL()}

#endif /* EXT_SWOOLE_PHP7_WRAPPER_H_ */
