# nginx-source-code-analysis

nginx 源码解析, 持续更新中...

### 文章列表

[nginx-doc](http://www.luwenpeng.cn/categories/nginx/)

### 提交格式

mygit [add] (core) nginx.c
mygit [add] (core) nginx.h

第一个字段表示操作

    [add] [del] [mod]

第二个字段表示模块

    (auto) (core) (event) (http) (os)

第三个字段表示文件

    先提交 .c 文件，再提交 .h 文件

### 提交记录

**auto**

    [add] have
    [add] init
    [add] options
    [add] sources

**core**

    [add] nginx.c
    [add] nginx.h

[nginx数组](http://www.luwenpeng.cn/2018/11/12/nginx%E6%95%B0%E7%BB%84/)

    [add] ngx_array.c
    [add] ngx_array.h

[nginx链表](http://www.luwenpeng.cn/2018/11/06/nginx%E9%93%BE%E8%A1%A8/)

    [add] ngx_list.c
    [add] ngx_list.h

[nginx队列](http://www.luwenpeng.cn/2018/11/12/nginx%E9%98%9F%E5%88%97/)

    [add] ngx_queue.c
    [add] ngx_queue.h

[nginx模块设计](http://www.luwenpeng.cn/2018/12/15/nginx%E6%A8%A1%E5%9D%97%E8%AE%BE%E8%AE%A1/)
[nginx模块执行顺序](http://www.luwenpeng.cn/2018/12/22/nginx%E6%A8%A1%E5%9D%97%E6%89%A7%E8%A1%8C%E9%A1%BA%E5%BA%8F/)
[nginx模块加载过程](http://www.luwenpeng.cn/2018/12/30/nginx%E6%A8%A1%E5%9D%97%E5%8A%A0%E8%BD%BD%E8%BF%87%E7%A8%8B/)

    [add] ngx_module.c
    [add] ngx_module.h

[nginx红黑树]

    [add] ngx_rbtree.c
    [add] ngx_rbtree.h

[nginx基数树]

    [add] ngx_radix_tree.c
    [add] ngx_radix_tree.h

[nginx时间缓存](http://www.luwenpeng.cn/2019/02/19/nginx%E6%97%B6%E9%97%B4%E7%BC%93%E5%AD%98/)

    [add] ngx_times.c
    [add] ngx_times.h

[nginx共享内存锁]

    [add] ngx_shmtx.c
    [add] ngx_shmtx.h

**event**

[nginx事件队列]

    [add] ngx_event_posted.c
    [add] ngx_event_posted.h

[nginx定时器API](http://www.luwenpeng.cn/2018/11/17/nginx%E5%AE%9A%E6%97%B6%E5%99%A8API/)
[nginx定时器原理](http://www.luwenpeng.cn/2018/11/18/nginx%E5%AE%9A%E6%97%B6%E5%99%A8%E5%8E%9F%E7%90%86/)

    [add] ngx_event_timer.c
    [add] ngx_event_timer.h

**os**

    [add] ngx_atomic.h

    [add] ngx_daemon.c

    [add] ngx_errno.c
    [add] ngx_errno.h

    [add] ngx_thread_cond.c
    [add] ngx_thread_mutex.c
