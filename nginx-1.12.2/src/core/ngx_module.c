
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Maxim Dounin
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>


// nginx 允许的最大动态模块数量
#define NGX_MAX_DYNAMIC_MODULES  128


static ngx_uint_t ngx_module_index(ngx_cycle_t *cycle);
static ngx_uint_t ngx_module_ctx_index(ngx_cycle_t *cycle, ngx_uint_t type,
    ngx_uint_t index);


ngx_uint_t         ngx_max_module; // nginx 所允许的最大模块数量
static ngx_uint_t  ngx_modules_n; // ngx_modules.c 文件中 ngx_modules 数组中的模块总数


/*
 * brief  : 预初始模块, 按照 ngx_modules.c 文件中 ngx_modules 数组中模块的先后
 *          顺序为各个模块设置索引 index, 并设置模块名称.
 * return : NGX_OK
 */
ngx_int_t
ngx_preinit_modules(void)
{
    ngx_uint_t  i;

    for (i = 0; ngx_modules[i]; i++) {
        // 设置 index 索引编号
        ngx_modules[i]->index = i;

        // 设置模块名称
        ngx_modules[i]->name = ngx_module_names[i];
    }

    // 记录模块总数
    ngx_modules_n = i;

    // 模块的最大数量
    ngx_max_module = ngx_modules_n + NGX_MAX_DYNAMIC_MODULES;

    return NGX_OK;
}


/*
 * brief  : 将 ngx_modules 数组中的数据拷贝到 cycle->modules 中, 并记录静态模块
 *          数量到 cycle->modules_n 中.
 * return : NGX_ERROR/NGX_OK
 */
ngx_int_t
ngx_cycle_modules(ngx_cycle_t *cycle)
{
    /*
     * create a list of modules to be used for this cycle,
     * copy static modules to it
     */

    cycle->modules = ngx_pcalloc(cycle->pool, (ngx_max_module + 1)
                                              * sizeof(ngx_module_t *));
    if (cycle->modules == NULL) {
        return NGX_ERROR;
    }

    ngx_memcpy(cycle->modules, ngx_modules,
               ngx_modules_n * sizeof(ngx_module_t *));

    cycle->modules_n = ngx_modules_n;

    return NGX_OK;
}


/*
 * brief  : 依次执行所有静态模块的 init_module() 函数
 * return : NGX_ERROR/NGX_OK
 */
ngx_int_t
ngx_init_modules(ngx_cycle_t *cycle)
{
    ngx_uint_t  i;

    for (i = 0; cycle->modules[i]; i++) {
        if (cycle->modules[i]->init_module) {
            if (cycle->modules[i]->init_module(cycle) != NGX_OK) {
                return NGX_ERROR;
            }
        }
    }

    return NGX_OK;
}


/*
 * brief  : 计算 type 类型的模块一共有多少个, 并设置每个模块在同类模块中的索引.
 * param  : [in] type 指定的模块类型(NGX_MAIL_MODULE/NGX_EVENT_MODULE/NGX_STREAM_MODULE/NGX_HTTP_MODULE)
 * return : type 类型的模块个数
 */
ngx_int_t
ngx_count_modules(ngx_cycle_t *cycle, ngx_uint_t type)
{
    ngx_uint_t     i, next, max;
    ngx_module_t  *module;

    next = 0;
    max = 0;

    /* count appropriate modules, set up their indices */

    for (i = 0; cycle->modules[i]; i++) {
        module = cycle->modules[i];

        // 类型必须相同
        if (module->type != type) {
            continue;
        }

        // 如果分配了ctx_index, 则保留其索引, 并更新 max 与 next 的值
        if (module->ctx_index != NGX_MODULE_UNSET_INDEX) {

            /* if ctx_index was assigned, preserve it */

            if (module->ctx_index > max) {
                max = module->ctx_index;
            }

            if (module->ctx_index == next) {
                next++;
            }

            continue;
        }

        /* search for some free index */

        /*
         * 如果未分配 ctx_index, 则返回一个同类型模块中未使用的 ctx_index
         * 作为当前模块的 module->ctx_index
         */
        module->ctx_index = ngx_module_ctx_index(cycle, type, next);

        if (module->ctx_index > max) {
            max = module->ctx_index;
        }

        next = module->ctx_index + 1;
    }

    /*
     * make sure the number returned is big enough for previous
     * cycle as well, else there will be problems if the number
     * will be stored in a global variable (as it's used to be)
     * and we'll have to roll back to the previous cycle
     */

    if (cycle->old_cycle && cycle->old_cycle->modules) {

        for (i = 0; cycle->old_cycle->modules[i]; i++) {
            module = cycle->old_cycle->modules[i];

            // 类型必须相同
            if (module->type != type) {
                continue;
            }

            if (module->ctx_index > max) {
                max = module->ctx_index;
            }
        }
    }

    /* prevent loading of additional modules */

    // 防止加载额外的模块
    cycle->modules_used = 1;

    return max + 1;
}


/*
 * brief  : 向 cf->cycle->modules 中添加动态模块
 * param  : [in] cf
 * param  : [in] file 动态库文件路径
 * param  : [in] module 新增动态模块的地址
 * param  : [in] order 即动态库模块 ngx_module_order 参数, 为动态模块指定执行的顺序
 * return : NGX_ERROR/NGX_OK
 * link   : https://www.nginx.com/resources/wiki/extending/new_config/
 */
ngx_int_t
ngx_add_module(ngx_conf_t *cf, ngx_str_t *file, ngx_module_t *module,
    char **order)
{
    void               *rv;
    ngx_uint_t          i, m, before;
    ngx_core_module_t  *core_module;

    // 加载的模块数过多(动态模块过多)
    if (cf->cycle->modules_n >= ngx_max_module) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "too many modules loaded");
        return NGX_ERROR;
    }

    // 版本不匹配
    if (module->version != nginx_version) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "module \"%V\" version %ui instead of %ui",
                           file, module->version, (ngx_uint_t) nginx_version);
        return NGX_ERROR;
    }

    // 签名不一致
    if (ngx_strcmp(module->signature, NGX_MODULE_SIGNATURE) != 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "module \"%V\" is not binary compatible",
                           file);
        return NGX_ERROR;
    }

    // 模块重复加载, 根据模块名称判断
    for (m = 0; cf->cycle->modules[m]; m++) {
        if (ngx_strcmp(cf->cycle->modules[m]->name, module->name) == 0) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "module \"%s\" is already loaded",
                               module->name);
            return NGX_ERROR;
        }
    }

    /*
     * if the module wasn't previously loaded, assign an index
     */

    if (module->index == NGX_MODULE_UNSET_INDEX) {
        // 返回一个未使用的索引
        module->index = ngx_module_index(cf->cycle);

        // 模块数目太多
        if (module->index >= ngx_max_module) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "too many modules loaded");
            return NGX_ERROR;
        }
    }

    /*
     * put the module into the cycle->modules array
     */

    /*
     * 确定新增模块的插入位置, 将新增动态库插入到 cf->cycle->modules[] 数组中.
     *
     * 若动态库中配置了 ngx_module_order, 则将新增的动态库插入到 ngx_module_order 配置项指定的模块之前.
     * 若动态库中未配置 ngx_module_order, 默认插入到 cf->cycle->modules[] 数组尾.
     */
    before = cf->cycle->modules_n;

    if (order) {
        for (i = 0; order[i]; i++) {
            // 找到模块名
            if (ngx_strcmp(order[i], module->name) == 0) {
                i++;
                break;
            }
        }

        // 从指定的模块位置开始查找
        for ( /* void */ ; order[i]; i++) {

#if 0
            ngx_log_debug2(NGX_LOG_DEBUG_CORE, cf->log, 0,
                           "module: %s before %s",
                           module->name, order[i]);
#endif

            for (m = 0; m < before; m++) {
                // 计算指定的模块在 cf->cycle->modules[] 中的位置
                if (ngx_strcmp(cf->cycle->modules[m]->name, order[i]) == 0) {

                    ngx_log_debug3(NGX_LOG_DEBUG_CORE, cf->log, 0,
                                   "module: %s before %s:%i",
                                   module->name, order[i], m);

                    before = m;
                    break;
                }
            }
        }
    }

    /* put the module before modules[before] */

    /*
     * 若新增模块要插入到 cf->cycle->modules[] 数组中间的某个位置,
     * 将 cf->cycle->modules[before] 后面的模块依次后移一个位置
     * 腾出 cf->cycle->modules[before] 位置让新增模块使用.
     */
    if (before != cf->cycle->modules_n) {
        ngx_memmove(&cf->cycle->modules[before + 1],
                    &cf->cycle->modules[before],
                    (cf->cycle->modules_n - before) * sizeof(ngx_module_t *));
    }

    // 将新增模块插入到 cf->cycle->modules[before] 这个位置
    cf->cycle->modules[before] = module;
    cf->cycle->modules_n++;

    // 若新增模块为核心模块, 执行对应的 create_conf() 函数
    if (module->type == NGX_CORE_MODULE) {

        /*
         * we are smart enough to initialize core modules;
         * other modules are expected to be loaded before
         * initialization - e.g., http modules must be loaded
         * before http{} block
         */

        core_module = module->ctx;

        if (core_module->create_conf) {
            rv = core_module->create_conf(cf->cycle);
            if (rv == NULL) {
                return NGX_ERROR;
            }

            cf->cycle->conf_ctx[module->index] = rv;
        }
    }

    return NGX_OK;
}


/*
 * brief  : 返回一个未使用的索引.
 */
static ngx_uint_t
ngx_module_index(ngx_cycle_t *cycle)
{
    ngx_uint_t     i, index;
    ngx_module_t  *module;

    index = 0;

again:

    /* find an unused index */

    for (i = 0; cycle->modules[i]; i++) {
        module = cycle->modules[i];

        if (module->index == index) {
            index++;
            goto again;
        }
    }

    /* check previous cycle */

    if (cycle->old_cycle && cycle->old_cycle->modules) {

        for (i = 0; cycle->old_cycle->modules[i]; i++) {
            module = cycle->old_cycle->modules[i];

            if (module->index == index) {
                index++;
                goto again;
            }
        }
    }

    return index;
}


/*
 * brief  : 返回一个 type 类型模块中未使用的 index
 */
static ngx_uint_t
ngx_module_ctx_index(ngx_cycle_t *cycle, ngx_uint_t type, ngx_uint_t index)
{
    ngx_uint_t     i;
    ngx_module_t  *module;

again:

    /* find an unused ctx_index */

    for (i = 0; cycle->modules[i]; i++) {
        module = cycle->modules[i];

        // 类型过滤
        if (module->type != type) {
            continue;
        }

        if (module->ctx_index == index) {
            index++;
            goto again;
        }
    }

    /* check previous cycle */

    if (cycle->old_cycle && cycle->old_cycle->modules) {

        for (i = 0; cycle->old_cycle->modules[i]; i++) {
            module = cycle->old_cycle->modules[i];

            // 类型过滤
            if (module->type != type) {
                continue;
            }

            if (module->ctx_index == index) {
                index++;
                goto again;
            }
        }
    }

    return index;
}
