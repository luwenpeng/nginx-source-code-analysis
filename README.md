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

[nginx-arry 数据结构](http://www.luwenpeng.cn/2018/11/12/nginx-arry/)

    [add] ngx_array.c
    [add] ngx_array.h

[nginx-list 数据结构](http://www.luwenpeng.cn/2018/11/06/nginx-list/)

    [add] ngx_list.c
    [add] ngx_list.h

[nginx-queus 数据结构](http://www.luwenpeng.cn/2018/11/12/nginx-queue/)

    [add] ngx_queue.c
    [add] ngx_queue.h

[nginx-module 设计](http://www.luwenpeng.cn/2018/12/15/nginx-module-design/)
[nginx-module 执行顺序](http://www.luwenpeng.cn/2018/12/22/nginx-module-exec-order/)
[nginx-module 加载过程](http://www.luwenpeng.cn/2018/12/30/nginx-module-load-process/)

    [add] ngx_module.c
    [add] ngx_module.h

    [add] ngx_rbtree.c
    [add] ngx_rbtree.h

[nginx-times 时间缓存](http://www.luwenpeng.cn/2019/02/19/nginx-times/)

    [add] ngx_times.c
    [add] ngx_times.h

    [add] ngx_shmtx.c
    [add] ngx_shmtx.h

**event**

    [add] ngx_event_posted.c
    [add] ngx_event_posted.h

[nginx-event-timer 定时器接口](http://www.luwenpeng.cn/2018/11/17/nginx-timer-API)
[nginx-event-timer 定时器原理](http://www.luwenpeng.cn/2018/11/18/nginx-timer-%E5%8E%9F%E7%90%86/)

    [add] ngx_event_timer.c
    [add] ngx_event_timer.h

**os**

    [add] ngx_atomic.h

    [add] ngx_daemon.c

    [add] ngx_errno.c
    [add] ngx_errno.h

    [add] ngx_thread_cond.c
    [add] ngx_thread_mutex.c
