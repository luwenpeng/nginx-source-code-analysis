
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_event.h>


ngx_queue_t  ngx_posted_accept_events;
ngx_queue_t  ngx_posted_events;


/*
 * brief  : 处理 posted 队列中的所有事件, 优先处理队首的事件.
 */
void
ngx_event_process_posted(ngx_cycle_t *cycle, ngx_queue_t *posted)
{
    ngx_queue_t  *q;
    ngx_event_t  *ev;

    while (!ngx_queue_empty(posted)) {

        // 取第一个结点
        q = ngx_queue_head(posted);

        // 获取结点 q 中的数据
        ev = ngx_queue_data(q, ngx_event_t, queue);

        ngx_log_debug1(NGX_LOG_DEBUG_EVENT, cycle->log, 0,
                      "posted event %p", ev);

        // 将 ev 从其所在的事件队列中删除
        ngx_delete_posted_event(ev);

        // 使用 ev 注册的 handler 处理 ev 事件
        ev->handler(ev);
    }
}
