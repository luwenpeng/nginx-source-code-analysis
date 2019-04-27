
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


/*
***************************************************
* 定义 event/ngx_event.c                          *
***************************************************

    #if (NGX_STAT_STUB)

    static ngx_atomic_t   ngx_stat_accepted0;
    ngx_atomic_t         *ngx_stat_accepted = &ngx_stat_accepted0;
    static ngx_atomic_t   ngx_stat_handled0;
    ngx_atomic_t         *ngx_stat_handled  = &ngx_stat_handled0;
    static ngx_atomic_t   ngx_stat_requests0;
    ngx_atomic_t         *ngx_stat_requests = &ngx_stat_requests0;
    static ngx_atomic_t   ngx_stat_active0;
    ngx_atomic_t         *ngx_stat_active   = &ngx_stat_active0;
    static ngx_atomic_t   ngx_stat_reading0;
    ngx_atomic_t         *ngx_stat_reading  = &ngx_stat_reading0;
    static ngx_atomic_t   ngx_stat_writing0;
    ngx_atomic_t         *ngx_stat_writing  = &ngx_stat_writing0;
    static ngx_atomic_t   ngx_stat_waiting0;
    ngx_atomic_t         *ngx_stat_waiting  = &ngx_stat_waiting0;

    #endif

***************************************************
* 初始化 ngx_event_module_init()                  *
***************************************************

ngx_stat_accepted, ngx_stat_handled, ngx_stat_requests,
ngx_stat_active,   ngx_stat_reading, ngx_stat_writing,
ngx_stat_waiting 等指针指向共享内存中创建的 ngx_atomic_t 变量。

    #if (NGX_STAT_STUB)

    ngx_stat_accepted = (ngx_atomic_t *) (shared + 3 * cl);
    ngx_stat_handled  = (ngx_atomic_t *) (shared + 4 * cl);
    ngx_stat_requests = (ngx_atomic_t *) (shared + 5 * cl);
    ngx_stat_active   = (ngx_atomic_t *) (shared + 6 * cl);
    ngx_stat_reading  = (ngx_atomic_t *) (shared + 7 * cl);
    ngx_stat_writing  = (ngx_atomic_t *) (shared + 8 * cl);
    ngx_stat_waiting  = (ngx_atomic_t *) (shared + 9 * cl);

    #endif

***************************************************
* 赋值                                            *
***************************************************

+--------------------+
| ngx_event_accept() |
+--------------------+

    accept()
    (void) ngx_atomic_fetch_add(ngx_stat_accepted, 1);
    ngx_get_connection()
    (void) ngx_atomic_fetch_add(ngx_stat_active, 1);
    创建基于请求的内存池，设置非阻塞，设置回调。
    (void) ngx_atomic_fetch_add(ngx_stat_handled, 1);

+---------------------------+
| ngx_http_create_request() |
+---------------------------+

    (void) ngx_atomic_fetch_add(ngx_stat_reading, 1);
    (void) ngx_atomic_fetch_add(ngx_stat_requests, 1);

+----------------------------+
| ngx_http_process_request() |
+----------------------------+

    (void) ngx_atomic_fetch_add(ngx_stat_reading, -1);
    (void) ngx_atomic_fetch_add(ngx_stat_writing, 1);

+-------------------------+
| ngx_http_free_request() |
+-------------------------+

    (void) ngx_atomic_fetch_add(ngx_stat_reading, -1);
    (void) ngx_atomic_fetch_add(ngx_stat_writing, -1);

+---------------------------+
| ngx_reusable_connection() |
+---------------------------+

    (void) ngx_atomic_fetch_add(ngx_stat_waiting, -1);
    (void) ngx_atomic_fetch_add(ngx_stat_waiting, 1);

+---------------------------------+
| ngx_close_accepted_connection() |
+---------------------------------+

    (void) ngx_atomic_fetch_add(ngx_stat_active, -1);
 */
static ngx_int_t ngx_http_stub_status_handler(ngx_http_request_t *r);
static ngx_int_t ngx_http_stub_status_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_stub_status_add_variables(ngx_conf_t *cf);
static char *ngx_http_set_stub_status(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);


// 定义配置项
static ngx_command_t  ngx_http_status_commands[] = {

    { ngx_string("stub_status"),
      NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_NOARGS|NGX_CONF_TAKE1,
      ngx_http_set_stub_status, // 配置项回调函数
      0,
      0,
      NULL },

      ngx_null_command
};


/*
 * 定义模块上下文
 * preconfiguration() 阶段将当前模块自定义的变量加入全局存储结构
 */
static ngx_http_module_t  ngx_http_stub_status_module_ctx = {
    ngx_http_stub_status_add_variables,    /* preconfiguration */
    NULL,                                  /* postconfiguration */

    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */

    NULL,                                  /* create location configuration */
    NULL                                   /* merge location configuration */
};


// 定义 http 模块
ngx_module_t  ngx_http_stub_status_module = {
    NGX_MODULE_V1,
    &ngx_http_stub_status_module_ctx,      /* module context */
    ngx_http_status_commands,              /* module directives */
    NGX_HTTP_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};


/*
 * 定义变量数组
 *
 * <0>.此处未显式的设置 set_handler，实际上 set 操作分散到了 nginx 代码的很多地方。
 *
 *     (void) ngx_atomic_fetch_add(ngx_stat_reading, -1);
 *     (void) ngx_atomic_fetch_add(ngx_stat_writing, 1);
 *
 * <1>.设置的 get_handler 均为 ngx_http_stub_status_variable。
 *
 *
 * <2>.通常回调函数的 data 参数为存储变量值的地址。但是此处的 data 参数代表的是
 *     变量的 id，在 get_handler 中通过判断 data 的值决定获取哪个参数。
 *
 *     Q:哪变量的值存储在哪里呢？
 *     A:在 ngx_event_module_init() 函数中将全局指针指向共享内存的存储空间的操作就相当于 init 操作。
 *
 *       ngx_stat_accepted = (ngx_atomic_t *) (shared + 3 * cl);
 *       ngx_stat_handled  = (ngx_atomic_t *) (shared + 4 * cl);
 *       ngx_stat_requests = (ngx_atomic_t *) (shared + 5 * cl);
 *       ngx_stat_active   = (ngx_atomic_t *) (shared + 6 * cl);
 *       ngx_stat_reading  = (ngx_atomic_t *) (shared + 7 * cl);
 *       ngx_stat_writing  = (ngx_atomic_t *) (shared + 8 * cl);
 *       ngx_stat_waiting  = (ngx_atomic_t *) (shared + 9 * cl);
 *       由此可见，变量的值是存储在共享内存中的。
 *
 *     Q:为何要存储到共享内存中呢？
 *     A:因为 nginx 为多进程架构，多个 worker 进程同时运行时，只有把连接状态参数
 *       都存储到共享内存中，该模块才能统计 nginx 整体的连接状态参数。
 *
 * <3>.变量的属性都是 NGX_HTTP_VAR_NOCACHEABLE 的。
 *
 *     NGX_HTTP_VAR_NOCACHEABLE 标示变量是频繁更新的，不可被缓存。并且该模块定
 *     义的变量是要被多个 worker 进程共享，为了防止读写冲突，都是原子操作，更不
 *     可能缓存变量的值了。
 *
 * 注意：在本模块中只是定义了变量，并未使用到变量。此模块定义变量，应该是为了让其他模块使用的而暴露出来的接口机制。
 * 比如在定义日志格式时使用该模块中定义的变量。
 */
static ngx_http_variable_t  ngx_http_stub_status_vars[] = {

    { ngx_string("connections_active"), NULL, ngx_http_stub_status_variable,
      0, NGX_HTTP_VAR_NOCACHEABLE, 0 },

    { ngx_string("connections_reading"), NULL, ngx_http_stub_status_variable,
      1, NGX_HTTP_VAR_NOCACHEABLE, 0 },

    { ngx_string("connections_writing"), NULL, ngx_http_stub_status_variable,
      2, NGX_HTTP_VAR_NOCACHEABLE, 0 },

    { ngx_string("connections_waiting"), NULL, ngx_http_stub_status_variable,
      3, NGX_HTTP_VAR_NOCACHEABLE, 0 },

    { ngx_null_string, NULL, NULL, 0, 0, 0 }
};


/*
 * brief  : 真正处理 stub_status 配置项的 handler
 * param  : [in] r
 */
static ngx_int_t
ngx_http_stub_status_handler(ngx_http_request_t *r)
{
    size_t             size;
    ngx_int_t          rc;
    ngx_buf_t         *b;
    ngx_chain_t        out;
    ngx_atomic_int_t   ap, hn, ac, rq, rd, wr, wa;

    // 仅支持 GET/HEAD 请求
    if (!(r->method & (NGX_HTTP_GET|NGX_HTTP_HEAD))) {
        return NGX_HTTP_NOT_ALLOWED;
    }

    // 丢弃请求头
    rc = ngx_http_discard_request_body(r);

    if (rc != NGX_OK) {
        return rc;
    }

    // 构造响应头
    r->headers_out.content_type_len = sizeof("text/plain") - 1;
    ngx_str_set(&r->headers_out.content_type, "text/plain");
    r->headers_out.content_type_lowcase = NULL;

    if (r->method == NGX_HTTP_HEAD) {
        r->headers_out.status = NGX_HTTP_OK;

        rc = ngx_http_send_header(r);

        if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
            return rc;
        }
    }

    // 计算响应数据的长度，分配存储空间
    size = sizeof("Active connections:  \n") + NGX_ATOMIC_T_LEN
           + sizeof("server accepts handled requests\n") - 1
           + 6 + 3 * NGX_ATOMIC_T_LEN
           + sizeof("Reading:  Writing:  Waiting:  \n") + 3 * NGX_ATOMIC_T_LEN;

    b = ngx_create_temp_buf(r->pool, size);
    if (b == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    out.buf = b;
    out.next = NULL;

    // 获取共享内存中的数据
    ap = *ngx_stat_accepted;
    hn = *ngx_stat_handled;
    ac = *ngx_stat_active;
    rq = *ngx_stat_requests;
    rd = *ngx_stat_reading;
    wr = *ngx_stat_writing;
    wa = *ngx_stat_waiting;

    // 拼接数据
    b->last = ngx_sprintf(b->last, "Active connections: %uA \n", ac);

    b->last = ngx_cpymem(b->last, "server accepts handled requests\n",
                         sizeof("server accepts handled requests\n") - 1);

    b->last = ngx_sprintf(b->last, " %uA %uA %uA \n", ap, hn, rq);

    b->last = ngx_sprintf(b->last, "Reading: %uA Writing: %uA Waiting: %uA \n",
                          rd, wr, wa);

    // 设置响应头
    r->headers_out.status = NGX_HTTP_OK;
    r->headers_out.content_length_n = b->last - b->pos;

    b->last_buf = (r == r->main) ? 1 : 0;
    b->last_in_chain = 1;

    rc = ngx_http_send_header(r);

    if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
        return rc;
    }

    return ngx_http_output_filter(r, &out);
}


/*
 * brief  : 变量的 get_handler() 回调函数。
 * param  : [in] r : 指向请求的指针
 * param  : [in/out] v : 将当前变量的值写入到 v 中。
 * param  : [in] data : 定义变量时指定 set_handler/get_handler 的参数
 * return : NGX_ERROR/NGX_OK
 *
 * 通常变量的值是存储在 data 中的，get_handler 通常的操作是将 data 中的数据拷贝到 v->data 中。
 * 但是此模块有点特殊。此模块中变量的值是存储在共享内存中的，此处的 data 表示的是变量的 id，
 * 根据不同的 id 就从共享内存中获取不同变量的值，然后再存储到 v->data 中。
 */
static ngx_int_t
ngx_http_stub_status_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    u_char            *p;
    ngx_atomic_int_t   value;

    p = ngx_pnalloc(r->pool, NGX_ATOMIC_T_LEN);
    if (p == NULL) {
        return NGX_ERROR;
    }

    switch (data) {
    case 0:
        value = *ngx_stat_active;
        break;

    case 1:
        value = *ngx_stat_reading;
        break;

    case 2:
        value = *ngx_stat_writing;
        break;

    case 3:
        value = *ngx_stat_waiting;
        break;

    /* suppress warning */
    default:
        value = 0;
        break;
    }

    v->len = ngx_sprintf(p, "%uA", value) - p;
    v->valid = 1; // 设置为有效
    v->no_cacheable = 0;
    v->not_found = 0;
    v->data = p;

    return NGX_OK;
}


/*
 * brief  : 将自定义的变量加入全局存储结构中。
 * return : NGX_OK/NGX_ERROR
 */
static ngx_int_t
ngx_http_stub_status_add_variables(ngx_conf_t *cf)
{
    ngx_http_variable_t  *var, *v;

    for (v = ngx_http_stub_status_vars; v->name.len; v++) {
        // 返回的是当前变量的存储结构
        var = ngx_http_add_variable(cf, &v->name, v->flags);
        if (var == NULL) {
            return NGX_ERROR;
        }

        // 注意：要设置 get_handler/set_handler/data
        var->get_handler = v->get_handler;
        var->data = v->data;
    }

    return NGX_OK;
}


/* brief  : "stub_status" 配置项的回调。此模块是按需挂载，将当前模块注册到处理阶段中。
 *
 *           Q:什么时候使用按需挂载呢？
 *           A:一般情况下，某个模块对某个 location 进行了处理以后，发现符合自己
 *             处理的逻辑，而且也没有必要再调用 NGX_HTTP_CONTENT_PHASE 阶段的其
 *             它 handler 进行处理的时候，就动态挂载上这个 handler。
 * return : NGX_CONF_OK
 */
static char *
ngx_http_set_stub_status(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t  *clcf;

    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
    clcf->handler = ngx_http_stub_status_handler;

    return NGX_CONF_OK;
}
