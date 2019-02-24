
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_event.h>


ngx_rbtree_t              ngx_event_timer_rbtree;
static ngx_rbtree_node_t  ngx_event_timer_sentinel;

/*
 * the event timer rbtree may contain the duplicate keys, however,
 * it should not be a problem, because we use the rbtree to find
 * a minimum timer value only
 */

/*
 * brief  : 初始化红黑树
 * return : NGX_OK
 */
ngx_int_t
ngx_event_timer_init(ngx_log_t *log)
{
    ngx_rbtree_init(&ngx_event_timer_rbtree, &ngx_event_timer_sentinel,
                    ngx_rbtree_insert_timer_value);

    return NGX_OK;
}


/*
 * brief  : 查找红黑树中已注册的最小定时器的值.
 * return : NGX_TIMER_INFINITE (-1) : 红黑树为空, 未注册定时器
 *           >0                     : 最小定时器还有多久会超时
 *            0                     : 最小定时器已经超时
 */
ngx_msec_t
ngx_event_find_timer(void)
{
    ngx_msec_int_t      timer;
    ngx_rbtree_node_t  *node, *root, *sentinel;

    // 首先判断红黑树是否为空
    if (ngx_event_timer_rbtree.root == &ngx_event_timer_sentinel) {
        return NGX_TIMER_INFINITE;
    }

    root = ngx_event_timer_rbtree.root;
    sentinel = ngx_event_timer_rbtree.sentinel;

    node = ngx_rbtree_min(root, sentinel);

    timer = (ngx_msec_int_t) (node->key - ngx_current_msec);

    return (ngx_msec_t) (timer > 0 ? timer : 0);
}


/*
 * brief  : 处理红黑树中所有超时的事件, 然后把超时的定时器从红黑树中删除.
 *          ev->timer_set 设置为 0, ev->timedout  设置为 1.
 * note   : 一次性的定时器
 */
void
ngx_event_expire_timers(void)
{
    ngx_event_t        *ev;
    ngx_rbtree_node_t  *node, *root, *sentinel;

    sentinel = ngx_event_timer_rbtree.sentinel;

    for ( ;; ) {
        root = ngx_event_timer_rbtree.root;

        if (root == sentinel) {
            return;
        }

        node = ngx_rbtree_min(root, sentinel);

        /* node->key > ngx_current_msec */

        // 最小的定时器未超时
        if ((ngx_msec_int_t) (node->key - ngx_current_msec) > 0) {
            return;
        }

        // 通过对 timer 进行 offsetof 偏移, 找到 ev
        ev = (ngx_event_t *) ((char *) node - offsetof(ngx_event_t, timer));

        ngx_log_debug2(NGX_LOG_DEBUG_EVENT, ev->log, 0,
                       "event timer del: %d: %M",
                       ngx_event_ident(ev->data), ev->timer.key);

        // 删除该定时器
        ngx_rbtree_delete(&ngx_event_timer_rbtree, &ev->timer);

#if (NGX_DEBUG)
        ev->timer.left = NULL;
        ev->timer.right = NULL;
        ev->timer.parent = NULL;
#endif

        ev->timer_set = 0;

        ev->timedout = 1;

        ev->handler(ev);
    }
}


/*
 * brief  : 检查红黑树中还有没有定时器
 * return : NGX_OK    : 代表红黑树中没有定时器了
 *          NGX_AGAIN : 代表红黑树中还有定时器
 */
ngx_int_t
ngx_event_no_timers_left(void)
{
    ngx_event_t        *ev;
    ngx_rbtree_node_t  *node, *root, *sentinel;

    sentinel = ngx_event_timer_rbtree.sentinel;
    root = ngx_event_timer_rbtree.root;

    if (root == sentinel) {
        return NGX_OK;
    }

    for (node = ngx_rbtree_min(root, sentinel);
         node;
         node = ngx_rbtree_next(&ngx_event_timer_rbtree, node))
    {
        ev = (ngx_event_t *) ((char *) node - offsetof(ngx_event_t, timer));

        /*
         * cancelable — 可取消标记.
         * 表示当 nginx 工作进程退出时, 即使该事件没过期也能被立即调用.
         * 它提供了一种方式用来完成特定动作, 比如清空日志文件.
         */
        if (!ev->cancelable) {
            return NGX_AGAIN;
        }
    }

    /* only cancelable timers left */

    return NGX_OK;
}
