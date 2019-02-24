
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>


#ifndef _NGX_QUEUE_H_INCLUDED_
#define _NGX_QUEUE_H_INCLUDED_


typedef struct ngx_queue_s  ngx_queue_t;

struct ngx_queue_s {
    ngx_queue_t  *prev;
    ngx_queue_t  *next;
};


#define ngx_queue_init(q)                                                     \
    (q)->prev = q;                                                            \
    (q)->next = q


#define ngx_queue_empty(h)                                                    \
    (h == (h)->prev)


// 在 h 结点之后插入 x 结点
#define ngx_queue_insert_head(h, x)                                           \
    (x)->next = (h)->next;                                                    \
    (x)->next->prev = x;                                                      \
    (x)->prev = h;                                                            \
    (h)->next = x


#define ngx_queue_insert_after   ngx_queue_insert_head


// 在 h 结点之前插入 x 结点
#define ngx_queue_insert_tail(h, x)                                           \
    (x)->prev = (h)->prev;                                                    \
    (x)->prev->next = x;                                                      \
    (x)->next = h;                                                            \
    (h)->prev = x


// h 结点不存储数据
#define ngx_queue_head(h)                                                     \
    (h)->next


#define ngx_queue_last(h)                                                     \
    (h)->prev


#define ngx_queue_sentinel(h)                                                 \
    (h)


#define ngx_queue_next(q)                                                     \
    (q)->next


#define ngx_queue_prev(q)                                                     \
    (q)->prev


#if (NGX_DEBUG)

#define ngx_queue_remove(x)                                                   \
    (x)->next->prev = (x)->prev;                                              \
    (x)->prev->next = (x)->next;                                              \
    (x)->prev = NULL;                                                         \
    (x)->next = NULL

#else

#define ngx_queue_remove(x)                                                   \
    (x)->next->prev = (x)->prev;                                              \
    (x)->prev->next = (x)->next

#endif


/* 队列分割
 *
 * 将 h 队列从 q 结点分割成队列 h 与队列 n
 * +---------------------------------------------------------------------+
 * | +-----------------------------------------------------------------+ |
 * | |   +------+             +-----+     +------+            +-----+  | |
 * | +-> | h[m] | ----------> |  h  | --> | h[1] | ---------> |  q  | -+ |
 * +---- |      | <---------- |     | <-- |      | <--------- |     | <--+
 *       +------+             +-----+     +------+            +-----+
 *                            +-----+
 *                            |  n  |
 *                            |     |
 *                            +-----+
 *
 * +---------------------------------------------------------------------+
 * | +-----------------------------------------------------------------+ |
 * | |   +------+             +-----+     +------+            +-----+  | |
 * | +-> | h[m] | ----+ +---> |  h  | --> | h[1] | ----+ +--> |  q  | -+ |
 * +---- |      | <-+ | | +-- |     | <-- |      | <-+ | | +- |     | <--+
 *       +------+   | | | |   +-----+     +------+   | | | |  +-----+
 *                  | | | +--------------------------+ | | |
 *                  | | +------------------------------+ | |
 *                  | |       +-----+                    | |
 *                  | +-----> |  n  | -------------------+ |
 *                  +-------- |     | <--------------------+
 *                            +-----+
 */
#define ngx_queue_split(h, q, n)                                              \
    (n)->prev = (h)->prev;                                                    \
    (n)->prev->next = n;                                                      \
    (n)->next = q;                                                            \
    (h)->prev = (q)->prev;                                                    \
    (h)->prev->next = h;                                                      \
    (q)->prev = n;


/* 队列合并
 *
 * 将 h 队列与 n 队列合并
 * +-------------------------------------------------------+
 * | +---------------------------------------------------+ |
 * | |   +------+             +-----+         +------+   | |
 * | +-> | h[m] | ----------> |  h  | ------> | h[1] | --+ |
 * +---- |      | <---------- |     | <------ |      | <---+
 *       +------+             +-----+         +------+
 *       +------+             +-----+         +------+
 * +---> | n[m] | ----------> |  n  | ------> | n[1] | ----+
 * | +-- |      | <---------- |     | <------ |      | <-+ |
 * | |   +------+             +-----+         +------+   | |
 * | +---------------------------------------------------+ |
 * +-------------------------------------------------------+
 *
 * +-------------------------------------------------------+
 * | +---------------------------------------------------+ |
 * | |   +------+             +-----+         +------+   | |
 * | +-> | h[m] | ----+ +---> |  h  | ------> | h[1] | --+ |
 * +---- |      | <-+ | | +-- |     | <------ |      | <---+
 *       +------+   | | | |   +-----+         +------+
 *                  | +-|-|---------------+
 *                  +---|-|-------------+ |
 *       +------+       | |   +-----+   | |   +------+
 * +---> | n[m] | ------+ |   |  n  |   | +-> | n[1] | ----+
 * | +-- |      | <-------+   |     |   +---- |      | <-+ |
 * | |   +------+             +-----+         +------+   | |
 * | +---------------------------------------------------+ |
 * +-------------------------------------------------------+
 */
#define ngx_queue_add(h, n)                                                   \
    (h)->prev->next = (n)->next;                                              \
    (n)->next->prev = (h)->prev;                                              \
    (h)->prev = (n)->prev;                                                    \
    (h)->prev->next = h;


// 通过指针偏移获取结点中的数据
#define ngx_queue_data(q, type, link)                                         \
    (type *) ((u_char *) q - offsetof(type, link))


ngx_queue_t *ngx_queue_middle(ngx_queue_t *queue);
void ngx_queue_sort(ngx_queue_t *queue,
    ngx_int_t (*cmp)(const ngx_queue_t *, const ngx_queue_t *));


#endif /* _NGX_QUEUE_H_INCLUDED_ */
