
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */

#include <ngx_config.h>
#include <ngx_core.h>

/***********************************************************
 * @Func   : ngx_list_create()
 * @Data   : 2018/05/15 15:36:23
 * @Author : lwp
 * @Brief  : 创建链表，为链表管理结构体分配内存；
 *           并初始化链表，设置每个链表结点提供 n 个 size 大小的基本存储单元
 * @Param  : [in] pool 当前链表用于分配内存的内存池指针
 * @Param  : [in] n    当前链表中每个链表结点中基本存储单元的个数
 * @Param  : [in] size 当前链表中每个链表结点中基本存储单元的大小
 * @Return : 成功 ：返回新创建并初始化的链表指针
 *           失败 ：返回 NULL
 * @Note   : 
 ***********************************************************/
ngx_list_t *
ngx_list_create(ngx_pool_t *pool, ngx_uint_t n, size_t size)
{
    ngx_list_t *list;

    list = ngx_palloc(pool, sizeof(ngx_list_t));
    if (list == NULL)
    {
        return NULL;
    }

    if (ngx_list_init(list, pool, n, size) != NGX_OK)
    {
        return NULL;
    }

    return list;
}
// @ngx_list_create() ok

/***********************************************************
 * @Func   : ngx_list_push()
 * @Data   : 2018/05/15 15:54:42
 * @Author : lwp
 * @Brief  : 从链表 l 中返回一个基本存储单元；
 *           如链表中的基本存储单元已用完，则新分配一个。
 * @Param  : [in] l 链表管理结构体的指针
 * @Return : 成功 ：返回一个基本存储单元的地址
 *           失败 ：返回 NULL
 * @Note   : 
 ***********************************************************/
void *
ngx_list_push(ngx_list_t *l)
{
    void            *elt;
    ngx_list_part_t *last;

    // 当前链表中最后一个结点
    last = l->last;

    // 最后一个结点的基本存储单元已用完，重新分配一个
    if (last->nelts == l->nalloc)
    {
        // 首先分配一个链表结点
        last = ngx_palloc(l->pool, sizeof(ngx_list_part_t));
        if (last == NULL)
        {
            return NULL;
        }

        // 然后分配 l->nalloc 个 l->size 大小的基本存储单元
        last->elts = ngx_palloc(l->pool, l->nalloc * l->size);
        if (last->elts == NULL)
        {
            return NULL;
        }

        // 修改链表结点属性
        last->nelts = 0;
        last->next = NULL;

        // 修改链表指针，尾部插入新结点
        l->last->next = last;
        l->last = last;
    }

    // 返回一个基本存储单元(l->size bytes)
    elt = (char *) last->elts + l->size * last->nelts;
    last->nelts++;

    return elt;
}
// @ngx_list_push() ok
