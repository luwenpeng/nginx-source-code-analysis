
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_CYCLE_H_INCLUDED_
#define _NGX_CYCLE_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


#ifndef NGX_CYCLE_POOL_SIZE
#define NGX_CYCLE_POOL_SIZE     NGX_DEFAULT_POOL_SIZE
#endif


#define NGX_DEBUG_POINTS_STOP   1
#define NGX_DEBUG_POINTS_ABORT  2


typedef struct ngx_shm_zone_s  ngx_shm_zone_t;

typedef ngx_int_t (*ngx_shm_zone_init_pt) (ngx_shm_zone_t *zone, void *data);

struct ngx_shm_zone_s {
    void                     *data;
    ngx_shm_t                 shm;
    ngx_shm_zone_init_pt      init;
    void                     *tag;
    ngx_uint_t                noreuse;  /* unsigned  noreuse:1; */
};


struct ngx_cycle_s {
    // 存储所有模块 create_conf() 返回值的指针数组
    void                  ****conf_ctx;
    // 当前 cycle 所使用的内存池
    ngx_pool_t               *pool;

    // 初始化时存储从 old_cycle->log 传递过来的日志对象，初始化后存储 cycle->new_log
    ngx_log_t                *log;
    // cycle 自己创建的日志对象
    ngx_log_t                 new_log;

    // 将错误输出到 stderr 的标示
    ngx_uint_t                log_use_stderr;  /* unsigned  log_use_stderr:1; */

    // 元素类型为 ngx_connection_t *，元素个数为 cycle->files_n 的指针数组
    ngx_connection_t        **files;
    // 依靠 next * 指针连接起来的空闲连接的链表
    ngx_connection_t         *free_connections;
    // 空闲连接的个数
    ngx_uint_t                free_connection_n;

    // 存储所有模块地址 (ngx_module_t *) 的指针数组
    ngx_module_t            **modules;
    // 模块的数量
    ngx_uint_t                modules_n;
    // 防止加载额外的模块的标示，在执行 ngx_count_modules() 后不可再加载动态模块
    ngx_uint_t                modules_used;    /* unsigned  modules_used:1; */

    // 可重用的连接的队列
    ngx_queue_t               reusable_connections_queue;
    // 可重用的连接的个数
    ngx_uint_t                reusable_connections_n;

    // 存储监听端口 (ngx_listening_t) 信息的数组
    ngx_array_t               listening;
    // 存储路径指针 (ngx_path_t *) 信息的数组
    ngx_array_t               paths;

    // 储存 (ngx_conf_dump_t) 信息的数组
    ngx_array_t               config_dump;
    // config_dump 红黑树的根节点
    ngx_rbtree_t              config_dump_rbtree;
    // config_dump 红黑树的哨兵结点
    ngx_rbtree_node_t         config_dump_sentinel;

    // 存储打开文件 (ngx_open_file_t) 信息的链表
    ngx_list_t                open_files;
    // 存储共享内存 (ngx_shm_zone_t) 信息的链表
    ngx_list_t                shared_memory;

    // 单个进程所支持的最大连接数，对应于 worker_connections 配置项
    ngx_uint_t                connection_n;
    // 进程最大打开文件数
    ngx_uint_t                files_n;

    // 元素类型为 ngx_connection_t，元素个数为 cycle->connection_n 的连接数组
    ngx_connection_t         *connections;
    // 元素类型为 ngx_event_t，元素个数为 cycle->connection_n 可读事件数组
    ngx_event_t              *read_events;
    // 元素类型为 ngx_event_t，元素个数为 cycle->connection_n 可写事件数组
    ngx_event_t              *write_events;

    // 老的 cycle
    ngx_cycle_t              *old_cycle;

    // 配置文件路径
    ngx_str_t                 conf_file;
    // 配置参数，例如 ./nginx -g xxx 中的 xxx
    ngx_str_t                 conf_param;
    // 配置项部署路径
    ngx_str_t                 conf_prefix;
    // nginx 安装路径
    ngx_str_t                 prefix;
    // 当系统不支持原子锁时，使用文件锁，此变量存储 lock_file 的路径
    ngx_str_t                 lock_file;
    // 以小写字母存储的主机名
    ngx_str_t                 hostname;
};


typedef struct {
    ngx_flag_t                daemon;
    ngx_flag_t                master;

    ngx_msec_t                timer_resolution;
    ngx_msec_t                shutdown_timeout;

    ngx_int_t                 worker_processes;
    ngx_int_t                 debug_points;

    ngx_int_t                 rlimit_nofile;
    off_t                     rlimit_core;

    int                       priority;

    ngx_uint_t                cpu_affinity_auto;
    ngx_uint_t                cpu_affinity_n;
    ngx_cpuset_t             *cpu_affinity;

    char                     *username;
    ngx_uid_t                 user;
    ngx_gid_t                 group;

    ngx_str_t                 working_directory;
    ngx_str_t                 lock_file;

    ngx_str_t                 pid;
    ngx_str_t                 oldpid;

    ngx_array_t               env;
    char                    **environment;
} ngx_core_conf_t;


#define ngx_is_init_cycle(cycle)  (cycle->conf_ctx == NULL)


ngx_cycle_t *ngx_init_cycle(ngx_cycle_t *old_cycle);
ngx_int_t ngx_create_pidfile(ngx_str_t *name, ngx_log_t *log);
void ngx_delete_pidfile(ngx_cycle_t *cycle);
ngx_int_t ngx_signal_process(ngx_cycle_t *cycle, char *sig);
void ngx_reopen_files(ngx_cycle_t *cycle, ngx_uid_t user);
char **ngx_set_environment(ngx_cycle_t *cycle, ngx_uint_t *last);
ngx_pid_t ngx_exec_new_binary(ngx_cycle_t *cycle, char *const *argv);
ngx_cpuset_t *ngx_get_cpu_affinity(ngx_uint_t n);
ngx_shm_zone_t *ngx_shared_memory_add(ngx_conf_t *cf, ngx_str_t *name,
    size_t size, void *tag);
void ngx_set_shutdown_timer(ngx_cycle_t *cycle);


extern volatile ngx_cycle_t  *ngx_cycle;
extern ngx_array_t            ngx_old_cycles;
extern ngx_module_t           ngx_core_module;
extern ngx_uint_t             ngx_test_config;
extern ngx_uint_t             ngx_dump_config;
extern ngx_uint_t             ngx_quiet_mode;


#endif /* _NGX_CYCLE_H_INCLUDED_ */
