
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_ARRAY_H_INCLUDED_
#define _NGX_ARRAY_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


typedef struct {
    void        *elts;   // 该数组中基本存储单元数组的起始地址
    ngx_uint_t   nelts;  // 该数组中已经使用的基本存储单元的个数
    size_t       size;   // 该数组中基本存储单元的大小
    ngx_uint_t   nalloc; // 该数组中基本存储单元的个数
    ngx_pool_t  *pool;   // 该数组用于分配内存的内存池指针
} ngx_array_t;


ngx_array_t *ngx_array_create(ngx_pool_t *p, ngx_uint_t n, size_t size);
void ngx_array_destroy(ngx_array_t *a);
void *ngx_array_push(ngx_array_t *a);
void *ngx_array_push_n(ngx_array_t *a, ngx_uint_t n);


/*
 * brief  : 初始化由 n 个大小为 size 的基本存储单元组成的数组.
 * param  : [in] arry 结构体数组的管理结构指针
 * param  : [in] pool 该数组用于分配内存的内存池指针
 * param  : [in] n 该数组中基本存储单元的个数
 * param  : [in] size 该数组中基本存储单元的大小
 * return : NGX_ERROR/NGX_OK
 */
static ngx_inline ngx_int_t
ngx_array_init(ngx_array_t *array, ngx_pool_t *pool, ngx_uint_t n, size_t size)
{
    /*
     * set "array->nelts" before "array->elts", otherwise MSVC thinks
     * that "array->nelts" may be used without having been initialized
     */

    array->nelts = 0;
    array->size = size;
    array->nalloc = n;
    array->pool = pool;

    array->elts = ngx_palloc(pool, n * size);
    if (array->elts == NULL) {
        return NGX_ERROR;
    }

    return NGX_OK;
}


#endif /* _NGX_ARRAY_H_INCLUDED_ */
