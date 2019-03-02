
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_RADIX_TREE_H_INCLUDED_
#define _NGX_RADIX_TREE_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


// 节点未使用的标志
#define NGX_RADIX_NO_VALUE   (uintptr_t) -1

typedef struct ngx_radix_node_s  ngx_radix_node_t;

struct ngx_radix_node_s {
    ngx_radix_node_t  *right;  // 右孩子
    ngx_radix_node_t  *left;   // 左孩子
    ngx_radix_node_t  *parent; // 父节点
    uintptr_t          value;  // 存储的是指针的值, 指向用户定义的数据结构
};


typedef struct {
    ngx_radix_node_t  *root;  // 根节点
    ngx_pool_t        *pool;  // 内存池
    ngx_radix_node_t  *free;  // 空闲节点通过 right 指针组成的链表
    char              *start; // 已分配内存中还未使用内存的首地址
    size_t             size;  // 已分配内存中还未使用的内存大小
} ngx_radix_tree_t;

/*
 +---------------------+----------------------+
 | ngx_radix_tree      | ngx_rbtree           |
 +---------------------+----------------------+
 | 提供节点内存的分配  | 不提供节点内存的分配 |
 |     ||              |    ||                |
 |     \/              |    \/                |
 | 节点不灵活          | 节点灵活             |
 |     ||              |    ||                |
 |     \/              |    \/                |
 | key 必须为整型      | key 可以为任意类型   |
 |     ||              |    ||                |
 |     \/              |    \/                |
 | 应用范围小          | 应用范围广           |
 |     ||              |    ||                |
 |     \/              |    \/                |
 | 地址定位            | 定时器/session       |
 +---------------------+----------------------+
 | 插入/删除不需要旋转 | 插入/删除需要旋转    |
 |     ||              |    ||                |
 |     \/              |    \/                |
 | 代码较简单          | 代码较复杂           |
 |     ||              |    ||                |
 |     \/              |    \/                |
 | 执行效率较高        | 执行效率较低         |
 +---------------------+----------------------+
 */

ngx_radix_tree_t *ngx_radix_tree_create(ngx_pool_t *pool,
    ngx_int_t preallocate);

ngx_int_t ngx_radix32tree_insert(ngx_radix_tree_t *tree,
    uint32_t key, uint32_t mask, uintptr_t value);
ngx_int_t ngx_radix32tree_delete(ngx_radix_tree_t *tree,
    uint32_t key, uint32_t mask);
uintptr_t ngx_radix32tree_find(ngx_radix_tree_t *tree, uint32_t key);

#if (NGX_HAVE_INET6)
ngx_int_t ngx_radix128tree_insert(ngx_radix_tree_t *tree,
    u_char *key, u_char *mask, uintptr_t value);
ngx_int_t ngx_radix128tree_delete(ngx_radix_tree_t *tree,
    u_char *key, u_char *mask);
uintptr_t ngx_radix128tree_find(ngx_radix_tree_t *tree, u_char *key);
#endif


#endif /* _NGX_RADIX_TREE_H_INCLUDED_ */
