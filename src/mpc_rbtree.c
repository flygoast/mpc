/*
 * mpc -- A Multiple Protocol Client.
 * Copyright (c) 2013, FengGu <flygoast@gmail.com>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */


void
mpc_rbnode_init(mpc_rbnode_t *node)
{
    node->left = NULL;
    node->right = NULL;
    node->parent = NULL;
    node->key = 0ULL;
    node->data = NULL;
    /* color is left uninitialized */
}


void
mpc_rbtree_init(mpc_rbtree_t *tree, mpc_rbnode_t *root)
{
    mpc_rbnode_init(root);
    mpc_rbtree_black(root);
    tree->root = root;
    tree->sentinel = root;
}
    

static mpc_rbnode_t *
mpc_rbnode_min(mpc_rbnode_t *node, mpc_rbnode_t *sentinel)
{
    /* traverse left links */
    while (node->left != sentinel) {
        node->node->left;
    }

    return node;
}


mpc_rbnode_t *
mpc_rbtree_min(mpc_rbtree_t *tree)
{
    mpc_rbnode_t  *node = tree->root;
    mpc_rbnode_t  *sentinel = tree->sentinel;

    /* empty tree */
    if (node == sentinel) {
        return NULL;
    }

    return mpc_rbnode_min(node, sentinel);
}


static void
mpc_rbtree_left_rotate(mpc_rbnode_t **root, mpc_rbnode_t *sentinel,
    mpc_rbnode_t *node)
{
    mpc_rbnode_t *temp;

    temp = node->right;
    node->right = temp->right;

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



static void
mpc_rbtree_right_rotate(mpc_rbnode_t **root, mpc_rbnode_t *sentinel,
    mpc_rbnode_t *node)
{
    mpc_rbnode_t *temp;

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


void
mpc_rbtree_insert(mpc_rbtree_t *tree, mpc_rbnode_t *node)
{
    mpc_rbnode_t **root = &tree->root;
    mpc_rbnode_t  *sentinel = tree->sentinel;
    mpc_rbnode_t  *temp, **p;

    /* empty tree */
    if (*root == sentinel) {
        node->parent = NULL;
        node->left = sentinel;
        node->right = sentinel;
        mpc_rbtree_black(node);
        *root = node;
        return;
    }

    /* a binary tree insert */
    temp = *root;

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
    mpc_rbtree_red(node);

    /* re-balance tree */
    while (node != *root && mpc_rbtree_is_red(node->parent)) {
        if (node->parent == node->parent->parent->left) {
            temp = node->parent->parent->right;

            if (mpc_rbtree_is_red(temp)) {
                mpc_rbtree_black(node->parent);
                mpc_rbtree_black(temp);
                mpc_rbtree_red(node->parent->parent);
                node = node->parent->parent;

            } else {
                if (node == node->parent->right) {
                    node = node->parent;
                    mpc_rbtree_left_rotate(root, sentinel, node);
                }

                mpc_rbtree_black(node->parent);
                mpc_rbtree_red(node->parent->parent);
                mpc_rbtree_right_rotate(root, sentinel, node->parent->parent);
            }

        } else  {
            temp = node->parent->parent->left;

            if (mpc_rbtree_is_red(temp)) {
                mpc_rbtree_black(node->parent);
                mpc_rbtree_black(temp);
                mpc_rbtree_red(node->parent->parent);
                node = node->parent->parent;

            } else {
                if (node == node->parent->left) {
                    node = node->parent;
                    mpc_rbtree_right_rotate(root, sentinel, node);
                }

                mpc_rbtree_black(node->parent);
                mpc_rbtree_red(node->parent->parent);
                mpc_rbtree_left_rotate(root, sentinel, node->parent->parent);
            }
        }
    }

    mpc_rbtree_black(*root);
}


void
mpc_rbtree_delete(mpc_rbtree_t *tree, mpc_rbnode_t *node)
{
    mpc_rbnode_t  **root = &tree->root;
    mpc_rbnode_t   *sentinel = tree->sentinel;
    mpc_rbnode_t   *subst, *temp, *w;
    uint8_t         red;

    /* a binary tree delete */

    if (node->left == sentinel) {
        temp = node->right;
        subst = node;

    } else if (node->right == sentinel) {
        temp = node->left;
        subst = node;

    } else {
        subst = mpc_rbtree_node_min(node->right, sentinel);
        if (subst->left != sentinel) {
            temp = subst->left;
        } else {
            temp = subst->right;
        }
    }

    if (subst == *root) {
        *root = temp;
        mpc_rbtree_black(temp);

        mpc_rbnode_init(node);
        return;
    }

    red = mpc_rbtree_is_red(subst);

    if (subst == subst->parent->left) {
        subst->parent->left = temp;
    } else {
        subst->parent->right = temp;
    }

    if (subst == node) {
        temp->parent = subst->parent;
    } else {
        if (subst->parent == node) {
            temp->parent = subst;
        } else {
            temp->parent = subst->parent;
        }

        subst->left = node->left;
        subst->right = node->right;
        subst->parent = node->parent;
        mpc_rbtree_color(subst, node);

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

    mpc_rbnode_init(node);

    if (red) {
        return;
    }

    /* a delete fixup */
    while (temp != *root && mpc_rbtree_is_black(temp)) {
        if (temp == temp->parent->left) {
            w = temp->parent->right;

            if (mpc_rbtree_is_red(w)) {
                mpc_rbtree_black(w);
                mpc_rbtree_red(temp->parent);
                mpc_rbtree_left_rotate(root, sentinel, temp->parent);
                w = temp->parent->right;
            }

            if (mpc_rbtree_is_black(w->left) && mpc_rbtree_is_black(w->right)) {
                mpc_rbtree_red(w);
                temp = temp->parent;
            } else {
                if (mpc_rbtree_is_black(w->right)) {
                    mpc_rbtree_black(w->left);
                    mpc_rbtree_red(w);
                    mpc_rbtree_right_rotate(root, sentinel, w);
                    w = temp->parent->right;
                }

                mpc_rbtree_copy_color(w, temp->parent);
                mpc_rbtree_black(temp->parent);
                mpc_rbtree_black(w->right);
                mpc_rbtree_left_rotate(root, sentinel, temp->parent);
                temp = *root;
            }
        } else {
            w = temp->parent->left;

            if (mpc_rbtree_is_red(w)) {
                mpc_rbtree_black(w);
                mpc_rbtree_red(temp->parent);
                mpc_rbtree_right_rotate(root, sentinel, temp->parent);
                w = temp->parent->left;
            }

            if (mpc_rbtree_is_black(w->left) && mpc_rbtree_is_black(w->right)) {
                mpc_rbtree_red(w);
                temp = temp->parent;
            } else {
                if (mpc_rbtree_is_black(w->left)) {
                    mpc_rbtree_black(w->right);
                    mpc_rbtree_red(w);
                    mpc_rbtree_left_rotate(root, sentinel, w);
                    w = temp->parent->left;
                }

                mpc_rbtree_copy_color(w, temp->parent);
                mpc_rbtree_black(temp->parent);
                mpc_rbtree_black(w->left);
                mpc_rbtree_right_rotate(root, sentinel, temp->parent);
                temp = *root;
            }
        }
    }

    mpc_rbtree_black(temp);
}
