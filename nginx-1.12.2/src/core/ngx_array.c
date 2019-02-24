
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>


/*
 * brief  : 初始化由 n 个大小为 size 的基本存储单元组成的数组
 * param  : [in] p 该数组用于分配内存的内存池指针
 * param  : [in] n 该数组中基本存储单元的个数
 * param  : [in] size 该数组中基本存储单元的大小
 * return : NULL/ngx_array_t *
 */
ngx_array_t *
ngx_array_create(ngx_pool_t *p, ngx_uint_t n, size_t size)
{
    ngx_array_t *a;

    a = ngx_palloc(p, sizeof(ngx_array_t));
    if (a == NULL) {
        return NULL;
    }

    if (ngx_array_init(a, p, n, size) != NGX_OK) {
        return NULL;
    }

    return a;
}


/*
 * brief  : 释放当前数组所占的内存到内存池中
 * note   : 若分配内存不在 p->d 中则不会释放, 而 ngx_array_push()/
 *          ngx_array_push_n() 中 allocate a new array 时, 新分配的数组肯定不在 p->d 中
 */
void
ngx_array_destroy(ngx_array_t *a)
{
    ngx_pool_t  *p;

    // 获取当前数组的内存池结构体指针
    p = a->pool;

    /* 若当前数组不是太大, 分配的内存都位于 p->d 中, 不会位于 p->d->next 中
     * 则释放内存到内存池, 否则则不释放.
     */
    if ((u_char *) a->elts + a->size * a->nalloc == p->d.last) {
        p->d.last -= a->size * a->nalloc;
    }

    if ((u_char *) a + sizeof(ngx_array_t) == p->d.last) {
        p->d.last = (u_char *) a;
    }
}


/*
 * brief  : 从数组中返回一个未用的基本存储单元的地址. 如果已分配的基本存储单元
 *          已经被用完, 则再分配新的内存. 若当前数组所占的内存在 p->d 中, 且当前
 *          内存块还有充足内存, 则使用该内存块继续分配. 否则分配一个长度为原来
 *          2 倍数的新数组, 老数组中的数组拷贝到新数组中, 老数组继续保留.
 * return : NULL/void *
 */
void *
ngx_array_push(ngx_array_t *a)
{
    void        *elt, *new;
    size_t       size;
    ngx_pool_t  *p;

    // 如果已分配的基本存储单元已经被用完, 则再分配新的内存
    if (a->nelts == a->nalloc) {

        /* the array is full */

        size = a->size * a->nalloc;

        p = a->pool;

        if ((u_char *) a->elts + size == p->d.last
            && p->d.last + a->size <= p->d.end)
        {
            /*
             * the array allocation is the last in the pool
             * and there is space for new allocation
             */

            p->d.last += a->size;
            a->nalloc++;

        } else {
            /* allocate a new array */

            new = ngx_palloc(p, 2 * size);
            if (new == NULL) {
                return NULL;
            }

            ngx_memcpy(new, a->elts, size);
            a->elts = new;
            a->nalloc *= 2;

            // TODO 未释放先前分配的内存
            // 先前分配的内存可能被某些程序占用
        }
    }

    elt = (u_char *) a->elts + a->size * a->nelts;
    a->nelts++;

    return elt;
}


/*
 * brief  : 从数组中返回 n 个未用的基本存储单元的地址. 如果已分配的基本存储单元
 *          已经被用完, 则再分配新的内存. 若当前数组所占的内存在 p->d 中, 且当
 *          前内存块还有充足内存, 则使用该内存块继续分配. 否则分配一个长度为 2
 *          * min(n + nalloc)的新数组, 老数组中的数组拷贝到新数组中, 老数组继续
 *          保留.
 * reutrn : NULL/void *
 */
void *
ngx_array_push_n(ngx_array_t *a, ngx_uint_t n)
{
    void        *elt, *new;
    size_t       size;
    ngx_uint_t   nalloc;
    ngx_pool_t  *p;

    // n 个 size 大小的基本存储空间所占的内存
    size = n * a->size;

    // 数组中剩余的基本存储空间的个数不足
    if (a->nelts + n > a->nalloc) {

        /* the array is full */

        p = a->pool;

        if ((u_char *) a->elts + a->size * a->nalloc == p->d.last
            && p->d.last + size <= p->d.end)
        {
            /*
             * the array allocation is the last in the pool
             * and there is space for new allocation
             */

            p->d.last += size;
            a->nalloc += n;

        } else {
            /* allocate a new array */

            nalloc = 2 * ((n >= a->nalloc) ? n : a->nalloc);

            new = ngx_palloc(p, nalloc * a->size);
            if (new == NULL) {
                return NULL;
            }

            ngx_memcpy(new, a->elts, a->nelts * a->size);
            a->elts = new;
            a->nalloc = nalloc;
        }
    }

    elt = (u_char *) a->elts + a->size * a->nelts;
    a->nelts += n;

    return elt;
}
