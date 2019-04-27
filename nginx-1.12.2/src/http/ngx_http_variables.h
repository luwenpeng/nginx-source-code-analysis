
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_HTTP_VARIABLES_H_INCLUDED_
#define _NGX_HTTP_VARIABLES_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


/*
 * 存储变量值的结构体
 *
 * typedef struct {
 *     unsigned    len:28;
 *
 *     unsigned    valid:1;        // 有效
 *     unsigned    no_cacheable:1; // 不可缓存
 *     unsigned    not_found:1;    // 未找到
 *     unsigned    escape:1;       // 逃脱
 *
 *     u_char     *data;           // 变量的值
 * } ngx_variable_value_t;
 */
typedef ngx_variable_value_t  ngx_http_variable_value_t;

#define ngx_http_variable(v)     { sizeof(v) - 1, 1, 0, 0, 0, (u_char *) v }

// 存储变量的结构体
typedef struct ngx_http_variable_s  ngx_http_variable_t;

// ngx_http_variable_s 的 set_handler 回调函数的格式
typedef void (*ngx_http_set_variable_pt) (ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);

// ngx_http_variable_s 的 get_handler 回调函数的格式
typedef ngx_int_t (*ngx_http_get_variable_pt) (ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);


// ngx_http_variable_s 的 flags 属性
#define NGX_HTTP_VAR_CHANGEABLE   1  // 表示该变量若重名可覆盖
#define NGX_HTTP_VAR_NOCACHEABLE  2  // 表示该变量不可被缓存
#define NGX_HTTP_VAR_INDEXED      4  // 表示该变量是用索引读取的
#define NGX_HTTP_VAR_NOHASH       8  // 表示该变量不需要被哈希
#define NGX_HTTP_VAR_WEAK         16
#define NGX_HTTP_VAR_PREFIX       32 // 表示该变量为前缀变量


// 每一个变量都是一个 ngx_http_variable_s 结构体
struct ngx_http_variable_s {
    ngx_str_t                     name;   /* must be first to build the hash */ //变量的名称
    ngx_http_set_variable_pt      set_handler; // 设置变量值的回调
    ngx_http_get_variable_pt      get_handler; // 获取变量值的回调
    uintptr_t                     data;        // set_handler/get_handler 的参数
    ngx_uint_t                    flags;       // 变量的属性
    ngx_uint_t                    index;       // 变量在 cmcf->variables 数组中的索引
};


// 添加变量
ngx_http_variable_t *ngx_http_add_variable(ngx_conf_t *cf, ngx_str_t *name,
    ngx_uint_t flags);

// 通过变量名获取变量对应的索引
ngx_int_t ngx_http_get_variable_index(ngx_conf_t *cf, ngx_str_t *name);

// 通过变量索引获取变量的值
ngx_http_variable_value_t *ngx_http_get_indexed_variable(ngx_http_request_t *r,
    ngx_uint_t index);

// 通过变量索引获取变量的值
ngx_http_variable_value_t *ngx_http_get_flushed_variable(ngx_http_request_t *r,
    ngx_uint_t index);

// 通过变量名获取变量的值
ngx_http_variable_value_t *ngx_http_get_variable(ngx_http_request_t *r,
    ngx_str_t *name, ngx_uint_t key);

ngx_int_t ngx_http_variable_unknown_header(ngx_http_variable_value_t *v,
    ngx_str_t *var, ngx_list_part_t *part, size_t prefix);


#if (NGX_PCRE)

typedef struct {
    ngx_uint_t                    capture;
    ngx_int_t                     index;
} ngx_http_regex_variable_t;


typedef struct {
    ngx_regex_t                  *regex;
    ngx_uint_t                    ncaptures;
    ngx_http_regex_variable_t    *variables;
    ngx_uint_t                    nvariables;
    ngx_str_t                     name;
} ngx_http_regex_t;


typedef struct {
    ngx_http_regex_t             *regex;
    void                         *value;
} ngx_http_map_regex_t;


ngx_http_regex_t *ngx_http_regex_compile(ngx_conf_t *cf,
    ngx_regex_compile_t *rc);
ngx_int_t ngx_http_regex_exec(ngx_http_request_t *r, ngx_http_regex_t *re,
    ngx_str_t *s);

#endif


typedef struct {
    ngx_hash_combined_t           hash; // 哈希表
#if (NGX_PCRE)
    ngx_http_map_regex_t         *regex; // 存储正则变量的数组
    ngx_uint_t                    nregex; // 正则变量的个数
#endif
} ngx_http_map_t;


void *ngx_http_map_find(ngx_http_request_t *r, ngx_http_map_t *map,
    ngx_str_t *match);


// 配置解析阶段添加 HTTP 核心模块提供的内置变量
ngx_int_t ngx_http_variables_add_core_vars(ngx_conf_t *cf);
// 配置解析阶段初始化变量
ngx_int_t ngx_http_variables_init_vars(ngx_conf_t *cf);


extern ngx_http_variable_value_t  ngx_http_variable_null_value;
extern ngx_http_variable_value_t  ngx_http_variable_true_value;


#endif /* _NGX_HTTP_VARIABLES_H_INCLUDED_ */
