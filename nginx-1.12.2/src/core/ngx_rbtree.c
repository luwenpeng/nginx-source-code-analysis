
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>


/*
 * The red-black tree code is based on the algorithm described in
 * the "Introduction to Algorithms" by Cormen, Leiserson and Rivest.
 */


static ngx_inline void ngx_rbtree_left_rotate(ngx_rbtree_node_t **root,
    ngx_rbtree_node_t *sentinel, ngx_rbtree_node_t *node);
static ngx_inline void ngx_rbtree_right_rotate(ngx_rbtree_node_t **root,
    ngx_rbtree_node_t *sentinel, ngx_rbtree_node_t *node);


/*
 * brief  : 向红黑树中插入一个结点．
 */
void
ngx_rbtree_insert(ngx_rbtree_t *tree, ngx_rbtree_node_t *node)
{
    ngx_rbtree_node_t  **root, *temp, *sentinel;

    /* a binary tree insert */

    root = &tree->root;
    sentinel = tree->sentinel;

    // 当要插入的结点是根结点时, 直接涂黑
    if (*root == sentinel) {
        node->parent = NULL;
        node->left = sentinel;
        node->right = sentinel;
        ngx_rbt_black(node);
        *root = node;

        return;
    }

    // 当要插入的结点的父结点是黑色时, 直接涂红

    // 自定义的插入回调函数
    tree->insert(*root, node, sentinel);

    /* re-balance tree */

    // 当要插入的结点的父结点是红色
    while (node != *root && ngx_rbt_is_red(node->parent)) {

        // 父结点是祖父结点的左支的时
        if (node->parent == node->parent->parent->left) {
            temp = node->parent->parent->right;

            // 叔叔结点为红时
            if (ngx_rbt_is_red(temp)) {
                ngx_rbt_black(node->parent);
                ngx_rbt_black(temp);
                ngx_rbt_red(node->parent->parent);
                node = node->parent->parent;

            // 叔叔结点为黑时
            } else {
                // 当插入右支时,先对父结点进行左旋
                if (node == node->parent->right) {
                    node = node->parent;
                    ngx_rbtree_left_rotate(root, sentinel, node);
                }

                ngx_rbt_black(node->parent);
                ngx_rbt_red(node->parent->parent);
                ngx_rbtree_right_rotate(root, sentinel, node->parent->parent);
            }

        // 父结点是祖父结点的右支的时
        } else {
            temp = node->parent->parent->left;

            // 叔叔结点为红时
            if (ngx_rbt_is_red(temp)) {
                ngx_rbt_black(node->parent);
                ngx_rbt_black(temp);
                ngx_rbt_red(node->parent->parent);
                node = node->parent->parent;

            // 叔叔结点为黑时
            } else {
                // 当插入左支时,先对父结点进行右旋
                if (node == node->parent->left) {
                    node = node->parent;
                    ngx_rbtree_right_rotate(root, sentinel, node);
                }

                ngx_rbt_black(node->parent);
                ngx_rbt_red(node->parent->parent);
                ngx_rbtree_left_rotate(root, sentinel, node->parent->parent);
            }
        }
    }

    ngx_rbt_black(*root);
}


/* brief  : 本函数为 tree->insert() 回调函数的一种实现, 用户可以根据实际需求自
 *          定义红黑树的排序依据.
 *          (向红黑树中插入新的结点时, 按 key 值从小到大确定插入位置)
 * param  : [in] temp 向 temp 子树中插入新结点, 通常令 temp = root
 * param  : [in] node 新插入的结点
 * param  : [in] sentinel 叶子结点
 */
void
ngx_rbtree_insert_value(ngx_rbtree_node_t *temp, ngx_rbtree_node_t *node,
    ngx_rbtree_node_t *sentinel)
{
    ngx_rbtree_node_t  **p;

    for ( ;; ) {

        p = (node->key < temp->key) ? &temp->left : &temp->right;

        if (*p == sentinel) {
            break;
        }

        temp = *p;
    }

    *p = node;
    node->parent = temp;
    node->left = sentinel;
    node->right = sentinel;

    // 新插入结点默认涂红
    ngx_rbt_red(node);
}


/* brief  : 本函数为 tree->insert() 回调函数的一种实现, 用户可以根据实际需求自
 *          定义红黑树的排序依据.
 *          (向红黑树中插入新的时间结点时, 按 key 值从小到大确定插入位置)
 * param  : [in] temp 向 temp 子树中插入新结点, 通常令 temp = root
 * param  : [in] node 新插入的结点
 * param  : [in] sentinel 叶子结点
 */
void
ngx_rbtree_insert_timer_value(ngx_rbtree_node_t *temp, ngx_rbtree_node_t *node,
    ngx_rbtree_node_t *sentinel)
{
    ngx_rbtree_node_t  **p;

    for ( ;; ) {

        /*
         * Timer values
         * 1) are spread in small range, usually several minutes,
         * 2) and overflow each 49 days, if milliseconds are stored in 32 bits.
         * The comparison takes into account that overflow.
         *
         * (2^32 - 1) / 3600 / 24 = 49710.269
         */

        /*  node->key < temp->key */

        p = ((ngx_rbtree_key_int_t) (node->key - temp->key) < 0)
            ? &temp->left : &temp->right;

        if (*p == sentinel) {
            break;
        }

        temp = *p;
    }

    *p = node;
    node->parent = temp;
    node->left = sentinel;
    node->right = sentinel;

    // 新插入结点默认涂红
    ngx_rbt_red(node);
}


/*
 * brief  : 从 tree 中删除 node 结点
 */
void
ngx_rbtree_delete(ngx_rbtree_t *tree, ngx_rbtree_node_t *node)
{
    ngx_uint_t           red;
    ngx_rbtree_node_t  **root, *sentinel, *subst, *temp, *w;

    /* a binary tree delete */

    root = &tree->root;
    sentinel = tree->sentinel;

    /*
     * 获取补位结点
     *
     * node  : 为要删除的结点
     * subst : 为真删除的结点
     * temp  : 为补位结点(可能为nil)
     *
     * 分析:
     * case1: 要删除的结点 node 可能没有孩子
     *        此时真正要删的结点 subst = node.
     *        此时为 subst 补位的结点 temp = node->right (nil)
     * case2: 要删除的结点 node 可能有一个孩子
     *        此时真正要删的结点 subst = node
     *        此时为 subst 补位的结点 temp = node->only_chirld
     * case3: 要删除的结点 node 可能有两个孩子
     *        此时真正要删的结点 subst = min(node->right)
     *        此时为 subst 补位的结点 temp = subst->right(可能为 nil)
     */

    if (node->left == sentinel) {
        temp = node->right;
        subst = node;

    } else if (node->right == sentinel) {
        temp = node->left;
        subst = node;

    } else {
        subst = ngx_rbtree_min(node->right, sentinel);

        if (subst->left != sentinel) {
            temp = subst->left;
        } else {
            temp = subst->right;
        }
    }

    // 真正删除的是根结点, 意味着该红黑树中仅仅就剩下一个根结点
    if (subst == *root) {
        *root = temp;
        ngx_rbt_black(temp);

        /* DEBUG stuff */
        node->left = NULL;
        node->right = NULL;
        node->parent = NULL;
        node->key = 0;

        return;
    }

    /*
     * 修改指针
     *
     * 删除 subst 结点, 使用 temp 结点补位
     *
     * 解除 subst 与 rbtree 的指针关系,
     * 建立 temp  与 rbtree 的指针关系.
     *
     * 由[获取补位结点]处理以后, subst 存在三种状态
     * case1 & case2 : subst = node
     * case3.1       : subst = node->right
     * case3.2       : subst = node->right->left->...->left
     */

    red = ngx_rbt_is_red(subst);

    // 修改 parent --> temp 指针
    if (subst == subst->parent->left) {
        subst->parent->left = temp;

    } else {
        subst->parent->right = temp;
    }

    // 修改 temp --> parent 指针
    // case1 & case2 : subst = node
    if (subst == node) {

        temp->parent = subst->parent;

    } else {

        // case3.1: subst = node->right
        if (subst->parent == node) {
            temp->parent = subst;

        // case3.2: subst = node->right->left->...->left
        } else {
            temp->parent = subst->parent;
        }

        // subst 继承 node 的指针关系和颜色
        subst->left = node->left;
        subst->right = node->right;
        subst->parent = node->parent;
        ngx_rbt_copy_color(subst, node);

        // 修改 node->parent --> subst
        if (node == *root) {
            *root = subst;

        } else {
            if (node == node->parent->left) {
                node->parent->left = subst;
            } else {
                node->parent->right = subst;
            }
        }

        if (subst->left != sentinel) {
            subst->left->parent = subst;
        }

        if (subst->right != sentinel) {
            subst->right->parent = subst;
        }
    }

    /* DEBUG stuff */
    node->left = NULL;
    node->right = NULL;
    node->parent = NULL;
    node->key = 0;

    // 如果删除的 subst 结点为红色, 对红黑树没有任何影响
    if (red) {
        return;
    }

    /* a delete fixup */

    while (temp != *root && ngx_rbt_is_black(temp)) {

        // 位于左支
        if (temp == temp->parent->left) {
            w = temp->parent->right;

            // 兄弟结点为红
            if (ngx_rbt_is_red(w)) {
                ngx_rbt_black(w);
                ngx_rbt_red(temp->parent);
                ngx_rbtree_left_rotate(root, sentinel, temp->parent);
                w = temp->parent->right;
            }

            // 兄弟结点两子为黑
            if (ngx_rbt_is_black(w->left) && ngx_rbt_is_black(w->right)) {
                ngx_rbt_red(w);
                temp = temp->parent;

            } else {
                // 兄弟结点右子为黑
                if (ngx_rbt_is_black(w->right)) {
                    ngx_rbt_black(w->left);
                    ngx_rbt_red(w);
                    ngx_rbtree_right_rotate(root, sentinel, w);
                    w = temp->parent->right;
                }

                ngx_rbt_copy_color(w, temp->parent);
                ngx_rbt_black(temp->parent);
                ngx_rbt_black(w->right);
                ngx_rbtree_left_rotate(root, sentinel, temp->parent);
                temp = *root;
            }

        // 位于右支
        } else {
            w = temp->parent->left;

            // 兄弟结点为红
            if (ngx_rbt_is_red(w)) {
                ngx_rbt_black(w);
                ngx_rbt_red(temp->parent);
                ngx_rbtree_right_rotate(root, sentinel, temp->parent);
                w = temp->parent->left;
            }

            // 兄弟结点两子为黑
            if (ngx_rbt_is_black(w->left) && ngx_rbt_is_black(w->right)) {
                ngx_rbt_red(w);
                temp = temp->parent;

            } else {
                // 兄弟结点左子为黑
                if (ngx_rbt_is_black(w->left)) {
                    ngx_rbt_black(w->right);
                    ngx_rbt_red(w);
                    ngx_rbtree_left_rotate(root, sentinel, w);
                    w = temp->parent->left;
                }

                ngx_rbt_copy_color(w, temp->parent);
                ngx_rbt_black(temp->parent);
                ngx_rbt_black(w->left);
                ngx_rbtree_right_rotate(root, sentinel, temp->parent);
                temp = *root;
            }
        }
    }

    ngx_rbt_black(temp);
}


/*
 * brief  : 红黑树左旋
 * param  : [in] root 根结点
 * param  : [in] sentinel 叶子结点
 * param  : [in] node 左旋前的 high 结点
 * note   : 一共需要修改 6 个指针(为了便于描述, 计 node->right->left 为 targ)
 *          ||                                ||
 *        [node]                            [temp]
 *       //    \\                          //    \\
 *      //      \\                        //      \\
 *     //        \\                      //        \\
 * [node_l]     [temp]   == 左旋 =>   [node]     [temp_r]
 *             //    \\              //    \\
 *            //      \\            //      \\
 *           //        \\          //        \\
 *        [targ]     [temp_r]   [node_l]    [targ]
 */
static ngx_inline void
ngx_rbtree_left_rotate(ngx_rbtree_node_t **root, ngx_rbtree_node_t *sentinel,
    ngx_rbtree_node_t *node)
{
    ngx_rbtree_node_t  *temp;

    temp = node->right;
    node->right = temp->left;

    if (temp->left != sentinel) {
        temp->left->parent = node;
    }

    temp->parent = node->parent;

    if (node == *root) {
        *root = temp;

    } else if (node == node->parent->left) {
        node->parent->left = temp;

    } else {
        node->parent->right = temp;
    }

    temp->left = node;
    node->parent = temp;
}

/*
 * brief  : 红黑树右旋
 * param  : [in] root 根结点
 * param  : [in] sentinel 叶子结点
 * param  : [in] node 右旋前的 high 结点
 * note   : 一共需要修改 6 个指针(为了便于描述, 计 node->left->right 为 targ)
 *               ||                             ||
 *             [node]                         [temp]
 *            //    \\                       //    \\
 *           //      \\                     //      \\
 *          //        \\                   //        \\
 *       [temp]     [node_r] == 右旋 => [temp_l]    [node]
 *      //    \\                                   //    \\
 *     //      \\                                 //      \\
 *    //        \\                               //        \\
 * [temp_l]    [targ]                          [targ]    [node_r]
 */
static ngx_inline void
ngx_rbtree_right_rotate(ngx_rbtree_node_t **root, ngx_rbtree_node_t *sentinel,
    ngx_rbtree_node_t *node)
{
    ngx_rbtree_node_t  *temp;

    temp = node->left;
    node->left = temp->right;

    if (temp->right != sentinel) {
        temp->right->parent = node;
    }

    temp->parent = node->parent;

    if (node == *root) {
        *root = temp;

    } else if (node == node->parent->right) {
        node->parent->right = temp;

    } else {
        node->parent->left = temp;
    }

    temp->right = node;
    node->parent = temp;
}


/*
 * brief  : 返回仅比当前结点大的结点
 */
ngx_rbtree_node_t *
ngx_rbtree_next(ngx_rbtree_t *tree, ngx_rbtree_node_t *node)
{
    ngx_rbtree_node_t  *root, *sentinel, *parent;

    sentinel = tree->sentinel;

    // 返回当前结点右子树中的最小结点
    if (node->right != sentinel) {
        return ngx_rbtree_min(node->right, sentinel);
    }

    root = tree->root;

    for ( ;; ) {
        parent = node->parent;

        // node 为根结点
        if (node == root) {
            return NULL;
        }

        // node 为父结点的左孩子
        if (node == parent->left) {
            return parent;
        }

        /* case:
         *   a
         *  /   Correct order should be: B, C, A. Assume we are at C,
         * b    so node->right is empty, node != root, and node != parent->left.
         *  \   we should return A, and this is what happens in the code.
         *   c
         */
        node = parent;
    }
}
