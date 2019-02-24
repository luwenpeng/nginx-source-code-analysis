
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>


/*
 * brief  : 创建链表, 为链表管理结构体分配内存, 并初始化链表,
 *          设置每个链表结点提供 n 个 size 大小的基本存储单元
 * param  : [in] pool 当前链表用于分配内存的内存池指针
 * param  : [in] n 当前链表中每个链表结点中基本存储单元的个数
 * param  : [in] size 当前链表中每个链表结点中基本存储单元的大小
 * return : NULL/ngx_list_t *
 */
ngx_list_t *
ngx_list_create(ngx_pool_t *pool, ngx_uint_t n, size_t size)
{
    ngx_list_t  *list;

    list = ngx_palloc(pool, sizeof(ngx_list_t));
    if (list == NULL) {
        return NULL;
    }

    if (ngx_list_init(list, pool, n, size) != NGX_OK) {
        return NULL;
    }

    return list;
}


/*
 * brief  : 从链表 l 中返回一个基本存储单元,
 *          如链表中的基本存储单元已用完, 则新分配一个
 * return : NULL/void *
 */
void *
ngx_list_push(ngx_list_t *l)
{
    void             *elt;
    ngx_list_part_t  *last;

    // 当前链表中最后一个结点
    last = l->last;

    if (last->nelts == l->nalloc) {

        /* the last part is full, allocate a new list part */

        last = ngx_palloc(l->pool, sizeof(ngx_list_part_t));
        if (last == NULL) {
            return NULL;
        }

        // 然后分配 l->nalloc 个 l->size 大小的基本存储单元
        last->elts = ngx_palloc(l->pool, l->nalloc * l->size);
        if (last->elts == NULL) {
            return NULL;
        }

        // 修改链表结点属性
        last->nelts = 0;
        last->next = NULL;

        // 修改链表指针, 尾部插入新结点
        l->last->next = last;
        l->last = last;
    }

    // 返回一个基本存储单元(l->size bytes)
    elt = (char *) last->elts + l->size * last->nelts;
    last->nelts++;

    return elt;
}
