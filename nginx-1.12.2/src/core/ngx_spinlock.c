
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>

/*
 +--------+-------------------------------------+------------------------------------+
 | 分类   | 自旋锁 (spinlock)                   | 互斥锁 (mutex)                     |
 +--------+-------------------------------------+------------------------------------+
 |        | 如果资源已经被占用，                | 如果资源已经被占用，               |
 | 不同点 | 不会引起调用者睡眠，                | 资源申请者只能进入睡眠状态。       |
 |        | 调用者一直循环查看自旋锁是否被释放。|                                    |
 |        | [忙等]                              | [睡眠]                             |
 +--------+-------------------------------------+------------------------------------+
 | 相同点 |  都是为了解决对某项资源的互斥使用，任何时刻最多只能有一个执行单元获得锁。|
 +--------+-------------------------------------+------------------------------------+

 nginx 原子锁底层 API 基于不同的平台采取不同的实现，此处仅介绍基于 gcc 4.1 原子操作的实现。

 #define ngx_atomic_cmp_set(lock, old, set) __sync_bool_compare_and_swap(lock, old, set)
 #define ngx_atomic_fetch_add(value, add)   __sync_fetch_and_add(value, add)
 #define ngx_memory_barrier()               __sync_synchronize()

 #if ( __i386__ || __i386 || __amd64__ || __amd64 )
     #define ngx_cpu_pause() __asm__ ("pause")
 #else
     #define ngx_cpu_pause()
 #endif

 #if (NGX_HAVE_SCHED_YIELD)
     #define ngx_sched_yield() sched_yield()
 #else
     #define ngx_sched_yield() usleep(1)
 #endif
 */

/*
 * brief  : nginx 自旋锁，返回时必定获得锁
 * param  : [in] lock : 原子锁对象
 * param  : [in] value : 将 lock 的值设为 value
 * param  : [in] spin
 * 使用例子：ngx_spinlock(&ngx_thread_pool_done_lock, 1, 2048);
 */
void
ngx_spinlock(ngx_atomic_t *lock, ngx_atomic_int_t value, ngx_uint_t spin)
{

#if (NGX_HAVE_ATOMIC_OPS)

    ngx_uint_t  i, n;

    for ( ;; ) {

        // 若 lock 为 0 才重新设置锁
        if (*lock == 0 && ngx_atomic_cmp_set(lock, 0, value)) {
            return;
        }

        /*
         * ngx_ncpu = sysconf(_SC_NPROCESSORS_ONLN);
         * ngx_ncpu 是 cpu 的个数，当它大于 1 时表示处于多处理器系统中
         */
        if (ngx_ncpu > 1) {

            // n: 1,2,4,8,16,32,64,128,512,1024, ... ,spin
            for (n = 1; n < spin; n <<= 1) {

                /*
                 * 每循环 n 次 ngx_cpu_pause()，尝试获取一次锁。
                 * 且 n 是以 2 的幂次递增，意味着尝试获取锁的时间间隔逐渐变大，
                 * 但 cpu 是不会被让出的，除非 n 大于等于 spin
                 */
                for (i = 0; i < n; i++) {

                    /*
                     *__asm__ ("pause")
                     * 在 x86 架构的 CPU 中，在循环用这个汇编指令，
                     * 可以保证循环不会被退出且可降低功耗。
                     */
                    ngx_cpu_pause();
                }

                // 若 lock 为 0 才重新设置锁
                if (*lock == 0 && ngx_atomic_cmp_set(lock, 0, value)) {
                    return;
                }
            }
        }

        /*
         * sched_yield() 使调用线程放弃 cpu，线程被移动到队列的末尾以获得其静态优先级，
         * 并且新线程可以运行。
         */
        ngx_sched_yield();
    }

#else

#if (NGX_THREADS)

#error ngx_spinlock() or ngx_atomic_cmp_set() are not defined !

#endif

#endif

}
