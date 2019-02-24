
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>


/*
 * find the middle queue element if the queue has odd number of elements
 * or the first element of the queue's second part otherwise
 */

/*
 * 除哨兵结点外, 如果队列具有奇数个元素, 则返回中间位置的元素.
 * 如果队列具有偶数个元素, 返回中间两个元素中位置靠后的那个元素.
 */
ngx_queue_t *
ngx_queue_middle(ngx_queue_t *queue)
{
    ngx_queue_t  *middle, *next;

    middle = ngx_queue_head(queue);

    /* 除哨兵结点外, 至多有一个结点
     *       +---+                +---+
     * +---- | h | <---+    +---- | h | <---+
     * | +-> |   | --+ |    | +-> |   | --+ |
     * | |   +---+   | | 或 | |   +---+   | |
     * | +-----------+ |    | |   +---+   | |
     * +---------------+    | +-- | p | <-+ |
     *                      +---> |   | ----+
     *                            +---+
     */
    if (middle == ngx_queue_last(queue)) {
        return middle;
    }

    // 除哨兵结点外, 至少有两个结点
    next = ngx_queue_head(queue);

    // 每次循环, middle 移动一个结点, next 移动两个结点
    for ( ;; ) {
        middle = ngx_queue_next(middle);

        next = ngx_queue_next(next);

        // 除哨兵结点外, 有偶数个结点, 返回中间位置靠后的那个结点
        if (next == ngx_queue_last(queue)) {
            return middle;
        }

        next = ngx_queue_next(next);

        // 除哨兵结点外, 有奇数个结点
        if (next == ngx_queue_last(queue)) {
            return middle;
        }
    }
}


/* the stable insertion sort */

/*
 * 使用自定义排序规则 (*cmp)() 对队列中元素排序
 */
void
ngx_queue_sort(ngx_queue_t *queue,
    ngx_int_t (*cmp)(const ngx_queue_t *, const ngx_queue_t *))
{
    ngx_queue_t  *q, *prev, *next;

    q = ngx_queue_head(queue);

    // 除哨兵结点外, 至多有一个结点
    if (q == ngx_queue_last(queue)) {
        return;
    }

    // 除哨兵结点外, 至少有两个结点
    for (q = ngx_queue_next(q); q != ngx_queue_sentinel(queue); q = next) {

        prev = ngx_queue_prev(q);
        next = ngx_queue_next(q);

        ngx_queue_remove(q);

        do {
            // q 应该排在 prev 之后, 若按照整形值从小到大排序(prev < q)则满足要求
            if (cmp(prev, q) <= 0) {
                break;
            }

            // 若 prev > q, 指针前移
            prev = ngx_queue_prev(prev);

        } while (prev != ngx_queue_sentinel(queue));

        // 在 prev 结点之后插入 q 结点
        ngx_queue_insert_after(prev, q);
    }
}
