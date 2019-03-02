
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>


static ngx_radix_node_t *ngx_radix_alloc(ngx_radix_tree_t *tree);


/*
 * brief  : 创建基数树.
 * param  : [in] pool : 用于分配内存的内存池
 * param  : [in] preallocate : 是否预分配存储节点 (0:不预分配; -1:预分配)
 * return : NULL/ngx_radix_tree_t *
 */
ngx_radix_tree_t *
ngx_radix_tree_create(ngx_pool_t *pool, ngx_int_t preallocate)
{
    uint32_t           key, mask, inc;
    ngx_radix_tree_t  *tree;

    // 为该树的结构体分配内存
    tree = ngx_palloc(pool, sizeof(ngx_radix_tree_t));
    if (tree == NULL) {
        return NULL;
    }

    // 初始化各成员
    tree->pool = pool;
    tree->free = NULL;
    tree->start = NULL;
    tree->size = 0;

    // 为根节点分配存储空间
    tree->root = ngx_radix_alloc(tree);
    if (tree->root == NULL) {
        return NULL;
    }

    // 初始化根节点各成员
    tree->root->right = NULL;
    tree->root->left = NULL;
    tree->root->parent = NULL;
    tree->root->value = NGX_RADIX_NO_VALUE;

    // 如果指定的预分配节点数为 0, 则直接返回
    if (preallocate == 0) {
        return tree;
    }

    /*
     * Preallocation of first nodes : 0, 1, 00, 01, 10, 11, 000, 001, etc.
     *
     *           root
     *           / \
     *          /   \
     *         /     \
     *        /       \
     *       /         \
     *      0           1
     *     / \         / \
     *    /   \       /   \
     *   0     1     0     1
     *  / \   / \   / \   / \
     * 0   1 0   1 0   1 0   1
     *
     * increases TLB hits even if for first lookup iterations.
     * On 32-bit platforms the 7 preallocated bits takes continuous 4K,
     * 8 - 8K, 9 - 16K, etc.  On 64-bit platforms the 6 preallocated bits
     * takes continuous 4K, 7 - 8K, 8 - 16K, etc.  There is no sense to
     * to preallocate more than one page, because further preallocation
     * distributes the only bit per page.  Instead, a random insertion
     * may distribute several bits per page.
     *
     * 参考下面的图表, 64 位系统下, 存储 6bit 的 key, 所占存储空间为 4k,
     * 存储 7bit 的 key, 所占存储空间为 8k, key 多了 1bit, 所占存储空间从 4k 增加到了 8k.
     * 单个 key 所占的存储空间每增加 1 bit, 基数树所占的存储空间会成倍增加.
     *
     * 所以预分配超过一页的内存是没有意义的, 因为进一步的预分配仅能多存储一个 bit 的数据,
     * 随机插入每页或许能分配几个 bit.
     *
     * Thus, by default we preallocate maximum
     *     6 bits on amd64 (64-bit platform and 4K pages)
     *     7 bits on i386 (32-bit platform and 4K pages)
     *     7 bits on sparc64 in 64-bit mode (8K pages)
     *     8 bits on sparc64 in 32-bit mode (8K pages)
     */

    /*
     * 一个 x bits 的值, 对应其 Radix 树有 x + 1 层,
     * 那么节点的个数就是 2 ^ (x + 1) - 1 个.
     *
     * 例如: key = 000, x = 3
     * radix 树共有 4 层(含root节点, root 为第一层)
     * 节点个数共有 1 + 2 + 4 + 8 = 15(含root节点)
     *
     * +--------------------+------------+------------+--------------------+-----------------------+
     * | key 所占 bits 位数 | 树的总层数 | 总节点数   | 单节点所占存储空间 | 所有节点所占存储空间  |
     * | (x)                | (x+1)      | 2^(x+1)-1  | 4 * sizeof(void *) |                       |
     * +--------------------+------------+------------+--------------------+-----------------------+
     * |   1                |   2        |          3 |   32 (64位系统)    |           96byte      |
     * +--------------------+------------+------------+--------------------+-----------------------+
     * |   2                |   3        |          7 |   32               |          224byte      |
     * +--------------------+------------+------------+--------------------+-----------------------+
     * |   3                |   4        |         15 |   32               |          480byte      |
     * +--------------------+------------+------------+--------------------+-----------------------+
     * |   4                |   5        |         31 |   32               |          992byte      |
     * +--------------------+------------+------------+--------------------+-----------------------+
     * |   5                |   6        |         63 |   32               |         2016byte      |
     * +--------------------+------------+------------+--------------------+-----------------------+
     * |   6                |   7        |        127 |   32               |         4064byte 4k   |
     * +--------------------+------------+------------+--------------------+-----------------------+
     * |   7                |   8        |        255 |   32               |         8160byte 8K   |
     * +--------------------+------------+------------+--------------------+-----------------------+
     * |   8                |   9        |        511 |   32               |        16352byte      |
     * +--------------------+------------+------------+--------------------+-----------------------+
     * | ...                | ...        |       ...  |   32               | ...                   |
     * +--------------------+------------+------------+--------------------+-----------------------+
     * |  32                |  33        | 8589934591 |   32               | 274877906912byte 256G |
     * +--------------------+------------+------------+--------------------+-----------------------+
     */

    if (preallocate == -1) {
        switch (ngx_pagesize / sizeof(ngx_radix_node_t)) {

        /* amd64 */
        case 128:
            preallocate = 6;
            break;

        /* i386, sparc64 */
        case 256:
            preallocate = 7;
            break;

        /* sparc64 in 32-bit mode */
        default:
            preallocate = 8;
        }
    }

    mask = 0;
    inc = 0x80000000;

    /*
     * 假设操作系统为 64 位
     * preallocate = 7
     */
    while (preallocate--) {

        key = 0;
        mask >>= 1;
        mask |= 0x80000000;
        /*
         * mask 变化范围
         * 1000,0000,0000,0000,0000,0000,0000,0000
         * 1100,0000,0000,0000,0000,0000,0000,0000
         * 1110,0000,0000,0000,0000,0000,0000,0000
         * 1111,0000,0000,0000,0000,0000,0000,0000
         * 1111,1000,0000,0000,0000,0000,0000,0000
         * 1111,1100,0000,0000,0000,0000,0000,0000
         * 1111,1110,0000,0000,0000,0000,0000,0000
         */

        do {
            if (ngx_radix32tree_insert(tree, key, mask, NGX_RADIX_NO_VALUE)
                != NGX_OK)
            {
                return NULL;
            }

            key += inc;
            /*
             * key 的变化范围
             * 0000,0000,0000,0000,0000,0000,0000,0000
             * 1000,0000,0000,0000,0000,0000,0000,0000
             *
             * 0000,0000,0000,0000,0000,0000,0000,0000
             * 0100,0000,0000,0000,0000,0000,0000,0000
             *
             * 0000,0000,0000,0000,0000,0000,0000,0000
             * 0010,0000,0000,0000,0000,0000,0000,0000
             *
             * 0000,0000,0000,0000,0000,0000,0000,0000
             * 0001,0000,0000,0000,0000,0000,0000,0000
             *
             * 0000,0000,0000,0000,0000,0000,0000,0000
             * 0000,1000,0000,0000,0000,0000,0000,0000
             *
             * 0000,0000,0000,0000,0000,0000,0000,0000
             * 0000,0100,0000,0000,0000,0000,0000,0000
             *
             * 0000,0000,0000,0000,0000,0000,0000,0000
             * 0000,0010,0000,0000,0000,0000,0000,0000
             */

        } while (key);

        inc >>= 1;
        /*
         * inc 变化范围
         * 1000,0000,0000,0000,0000,0000,0000,0000
         * 0100,0000,0000,0000,0000,0000,0000,0000
         * 0010,0000,0000,0000,0000,0000,0000,0000
         * 0001,0000,0000,0000,0000,0000,0000,0000
         * 0000,1000,0000,0000,0000,0000,0000,0000
         * 0000,0100,0000,0000,0000,0000,0000,0000
         * 0000,0010,0000,0000,0000,0000,0000,0000
         */
    }

    return tree;
}


/*
 * brief  : 向基数树 tree 中插入新节点存储键值对 <key, value>
 * param  : [in] tree
 * param  : [in] key
 * param  : [in] mask
 * param  : [in] value
 * return : NGX_ERROR : 内存分配失败
 *          NGX_OK    : 插入成功
 *          NGX_BUSY  : key 冲突
 * note   : mask 决定存储 key 的位数
 *          例如:  key = 1000,0000(二进制), mask = 1110,0000(二进制)
 *          则只存储 key 的高 3 位, 即存储 100(二进制)
 */
ngx_int_t
ngx_radix32tree_insert(ngx_radix_tree_t *tree, uint32_t key, uint32_t mask,
    uintptr_t value)
{
    uint32_t           bit;
    ngx_radix_node_t  *node, *next;

    bit = 0x80000000;
    /*
     * bit 变化范围
     * 1000,0000,0000,0000,0000,0000,0000,0000
     * 0100,0000,0000,0000,0000,0000,0000,0000
     * 0010,0000,0000,0000,0000,0000,0000,0000
     * 0001,0000,0000,0000,0000,0000,0000,0000
     * ...
     * 0000,0000,0000,0000,0000,0000,0000,1000
     * 0000,0000,0000,0000,0000,0000,0000,0100
     * 0000,0000,0000,0000,0000,0000,0000,0010
     * 0000,0000,0000,0000,0000,0000,0000,0001
     */

    node = tree->root;
    next = tree->root;

    while (bit & mask) {
        if (key & bit) {
            next = node->right;

        } else {
            next = node->left;
        }

        if (next == NULL) {
            break;
        }

        bit >>= 1;
        node = next;
    }

    if (next) {
        // 当前节点的值域已使用, key 冲突
        if (node->value != NGX_RADIX_NO_VALUE) {
            return NGX_BUSY;
        }

        // 当前节点的值域未使用, 存储值
        node->value = value;
        return NGX_OK;
    }

    // next 为空
    while (bit & mask) {
        // 为节点分配存储空间
        next = ngx_radix_alloc(tree);
        if (next == NULL) {
            return NGX_ERROR;
        }

        // 设置节点属性
        next->right = NULL;
        next->left = NULL;
        next->parent = node;
        next->value = NGX_RADIX_NO_VALUE;

        // 确定 next 位于左支还是右支
        if (key & bit) {
            node->right = next;

        } else {
            node->left = next;
        }

        bit >>= 1;
        node = next;
    }

    node->value = value;

    return NGX_OK;
}


/*
 * brief  : 从基数树 tree 中删除键为 key 的节点, 将指向 val 的指针设置为了
 *          NGX_RADIX_NO_VALUE, 并未释放 val 的内存.
 *          将存储 key 的基数树节点进行了逻辑删除, 挂到了 free 链表的首部.
 * param  : [in] tree
 * param  : [in] key
 * param  : [in] mask
 * return : NGX_ERROR : key 不存在
 *          NGX_OK    : 删除成功
 * note   : mask 决定 key 的有效位数
 *          例如:  key  = 1000,0000(二进制), mask = 1110,0000(二进制)
 *          则 key 的的有效位为高 3 位, 即 100(二进制)
 */
ngx_int_t
ngx_radix32tree_delete(ngx_radix_tree_t *tree, uint32_t key, uint32_t mask)
{
    uint32_t           bit;
    ngx_radix_node_t  *node;

    bit = 0x80000000;
    node = tree->root;

    while (node && (bit & mask)) {
        if (key & bit) {
            node = node->right;

        } else {
            node = node->left;
        }

        bit >>= 1;
    }

    // 未找到
    if (node == NULL) {
        return NGX_ERROR;
    }

    // node 至少拥有一个孩子
    if (node->right || node->left) {
        // 删除 value 值域的指针
        if (node->value != NGX_RADIX_NO_VALUE) {
            node->value = NGX_RADIX_NO_VALUE;
            return NGX_OK;
        }

        // node 值域为空, key 不存在
        return NGX_ERROR;
    }

    // node 的左右孩子都不存在
    for ( ;; ) {
        // 解除其父节点到 node 节点的指针关系
        if (node->parent->right == node) {
            node->parent->right = NULL;

        } else {
            node->parent->left = NULL;
        }

        // 将 node 节点放入 free 链表的头部
        node->right = tree->free;
        tree->free = node;

        // 检查从 node 到其父节点, 祖父节点的路径上, 是否都为空
        node = node->parent;

        // 至少拥有一个孩子
        if (node->right || node->left) {
            break;
        }

        // 没有孩子, 但是拥有值域
        if (node->value != NGX_RADIX_NO_VALUE) {
            break;
        }

        // 没有孩子, 也没有值域, 且没有父节点
        if (node->parent == NULL) {
            break;
        }

        // 没有孩子, 也没有值域, 有父节点
    }

    return NGX_OK;
}


/*
 * brief  : 从基数树 tree 中查找 key 所对应的 value.
 * param  : [in] tree
 * param  : [in] key
 * return : NGX_RADIX_NO_VALUE : 不存在
 *          uintptr_t : 返回 value 或指向 value 的指针
 */
uintptr_t
ngx_radix32tree_find(ngx_radix_tree_t *tree, uint32_t key)
{
    uint32_t           bit;
    uintptr_t          value;
    ngx_radix_node_t  *node;

    bit = 0x80000000;
    value = NGX_RADIX_NO_VALUE;
    node = tree->root;

    while (node) {
        if (node->value != NGX_RADIX_NO_VALUE) {
            value = node->value;
        }

        if (key & bit) {
            node = node->right;

        } else {
            node = node->left;
        }

        bit >>= 1;
    }

    return value;
}


// 用于存储 IPv6 地址
#if (NGX_HAVE_INET6)

/*
 * IPv6 长度为 128bit, 不能用整型存储, key 与 mask 都改为了字符储存.
 *
 * uint32_t bit;       改为了    u_char bit;
 * bit = 0x80000000;  =======>   bit = 0x80;
 * if (bit & mask)               if (bit & mask[i])
 * {}                            {}
 */

ngx_int_t
ngx_radix128tree_insert(ngx_radix_tree_t *tree, u_char *key, u_char *mask,
    uintptr_t value)
{
    u_char             bit;
    ngx_uint_t         i;
    ngx_radix_node_t  *node, *next;

    i = 0;
    bit = 0x80;
    /*
     * bit 变化范围
     * 1000,0000
     * 0100,0000
     * 0010,0000
     * 0001,0000
     * 0000,1000
     * 0000,0100
     * 0000,0010
     * 0000,0001
     * 0000,0000
     */

    node = tree->root;
    next = tree->root;

    while (bit & mask[i]) {
        if (key[i] & bit) {
            next = node->right;

        } else {
            next = node->left;
        }

        if (next == NULL) {
            break;
        }

        bit >>= 1;
        node = next;

        if (bit == 0) {
            if (++i == 16) {
                break;
            }

            // 世道轮回
            bit = 0x80;
        }
    }

    if (next) {
        if (node->value != NGX_RADIX_NO_VALUE) {
            return NGX_BUSY;
        }

        node->value = value;
        return NGX_OK;
    }

    while (bit & mask[i]) {
        next = ngx_radix_alloc(tree);
        if (next == NULL) {
            return NGX_ERROR;
        }

        next->right = NULL;
        next->left = NULL;
        next->parent = node;
        next->value = NGX_RADIX_NO_VALUE;

        if (key[i] & bit) {
            node->right = next;

        } else {
            node->left = next;
        }

        bit >>= 1;
        node = next;

        if (bit == 0) {
            if (++i == 16) {
                break;
            }

            bit = 0x80;
        }
    }

    node->value = value;

    return NGX_OK;
}


ngx_int_t
ngx_radix128tree_delete(ngx_radix_tree_t *tree, u_char *key, u_char *mask)
{
    u_char             bit;
    ngx_uint_t         i;
    ngx_radix_node_t  *node;

    i = 0;
    bit = 0x80;
    node = tree->root;

    while (node && (bit & mask[i])) {
        if (key[i] & bit) {
            node = node->right;

        } else {
            node = node->left;
        }

        bit >>= 1;

        if (bit == 0) {
            if (++i == 16) {
                break;
            }

            bit = 0x80;
        }
    }

    if (node == NULL) {
        return NGX_ERROR;
    }

    if (node->right || node->left) {
        if (node->value != NGX_RADIX_NO_VALUE) {
            node->value = NGX_RADIX_NO_VALUE;
            return NGX_OK;
        }

        return NGX_ERROR;
    }

    for ( ;; ) {
        if (node->parent->right == node) {
            node->parent->right = NULL;

        } else {
            node->parent->left = NULL;
        }

        node->right = tree->free;
        tree->free = node;

        node = node->parent;

        if (node->right || node->left) {
            break;
        }

        if (node->value != NGX_RADIX_NO_VALUE) {
            break;
        }

        if (node->parent == NULL) {
            break;
        }
    }

    return NGX_OK;
}


uintptr_t
ngx_radix128tree_find(ngx_radix_tree_t *tree, u_char *key)
{
    u_char             bit;
    uintptr_t          value;
    ngx_uint_t         i;
    ngx_radix_node_t  *node;

    i = 0;
    bit = 0x80;
    value = NGX_RADIX_NO_VALUE;
    node = tree->root;

    while (node) {
        if (node->value != NGX_RADIX_NO_VALUE) {
            value = node->value;
        }

        if (key[i] & bit) {
            node = node->right;

        } else {
            node = node->left;
        }

        bit >>= 1;

        if (bit == 0) {
            i++;
            bit = 0x80;
        }
    }

    return value;
}

#endif


/*
 * brief  : 为基数树节点分配内存
 * param  : [in] tree
 * return : NULL/ngx_radix_node_t *
 */
static ngx_radix_node_t *
ngx_radix_alloc(ngx_radix_tree_t *tree)
{
    ngx_radix_node_t  *p;

    // 从空闲节点中分配一个
    if (tree->free) {
        p = tree->free;
        tree->free = tree->free->right;
        return p;
    }

    /*
     * 若 ngx_radix_tree_t 已分配内存中还未使用的内存大小不足容纳一个节点,
     * 则从内存池中以 ngx_pagesize 大小进行内存对齐的方式分配 ngx_pagesize 大小的内存块.
     * ngx_pmemalign() 实际在堆上分配, 分配后被内存池管理.
     */

    if (tree->size < sizeof(ngx_radix_node_t)) {
        tree->start = ngx_pmemalign(tree->pool, ngx_pagesize, ngx_pagesize);
        if (tree->start == NULL) {
            return NULL;
        }

        tree->size = ngx_pagesize;
    }

    /*
     * 从新申请的内存块为基数树节点分配内存
     * 设置已分配内存中还未使用的内存首地址
     * 设置已分配内存中还未使用的内存大小
     */
    p = (ngx_radix_node_t *) tree->start;
    tree->start += sizeof(ngx_radix_node_t);
    tree->size -= sizeof(ngx_radix_node_t);

    return p;
}
