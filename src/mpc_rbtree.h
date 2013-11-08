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


#ifndef __MPC_RBTREE_H_INCLUDED__
#define __MPC_RBTREE_H_INCLUDED__


#define mpc_rbtree_red(node)            ((node)->color = 1)
#define mpc_rbtree_black(node)          ((node)->color = 0)
#define mpc_rbtree_is_red(node)         ((node)->color)
#define mpc_rbtree_is_black(node)       (!mpc_rbtree_is_red(node))
#define mpc_rbtree_copy_color(n1, n2)   ((n1)->color = (n2)->color)


typedef struct mpc_rbnode_s     mpc_rbnode_t;


struct mpc_rbnode_s {
    mpc_rbnode_t   *left;       /* left link */
    mpc_rbnode_t   *right;      /* right link */
    mpc_rbnode_t   *parent;     /* parent link */
    int64_t         key;        /* key for ordering */
    void           *data;       /* opaque data */
    uint8_t         color;      /* red | black */
};


typedef struct {
    mpc_rbnode_t    *root;      /* root node */
    mpc_rbnode_t    *sentinel;  /* nil node */
} mpc_rbtree_t;


void mpc_rbnode_init(mpc_rbnode_t *node);
void mpc_rbtree_init(mpc_rbtree_t *tree, mpc_rbnode_t *root);
mpc_rbnode_t *mpc_rbtree_min(mpc_rbtree_t *tree);
void mpc_rbtree_insert(mpc_rbtree_t *tree, mpc_rbnode_t *node);
void mpc_rbtree_delete(mpc_rbtree_t *tree, mpc_rbnode_t *node);
mpc_rbnode_t *mpc_rbtree_find(mpc_rbtree_t *tree, int64_t key);
mpc_rbnode_t *mpc_rbnode_get(void);
void mpc_rbnode_put(mpc_rbnode_t *node);


#endif /* __MPC_RBTREE_H_INCLUDED__ */
