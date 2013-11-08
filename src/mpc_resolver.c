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


#define MPC_RESOLVER_INTERVAL   1000
#define MPC_RESOLVER_MAGIC      0x52534c56  /* "RSLV" */


typedef struct {
    mpc_event_loop_t  *el;
    ares_channel       channel;
    int64_t            timer_id;
    int                sock_cnt;
    ares_socket_t      socks[ARES_GETSOCK_MAXNUM];
    mpc_rbnode_t       root;
    mpc_rbtree_t       rbtree;
} mpc_resolver_t;


typedef struct {
#ifdef WITH_DEBUG
    int                    magic;
#endif
    mpc_gethostbyname_cb   callback;
    void                  *arg;
    mpc_event_loop_t      *el;
} mpc_resolver_ctx_t;


static int mpc_resolver_process_timeout(mpc_event_loop_t *el, int64_t id,
    void *data);
static void mpc_resolver_process_sockstate(void *data, ares_socket_t sock,
    int read, int write);


int
mpc_resolver_init(mpc_event_loop_t *el, const char *server)
{
    int                   rc, mask;
    mpc_resolver_t       *resolver;
    struct ares_options   options;

    rc = ares_library_init(ARES_LIB_INIT_ALL);

    if (rc != ARES_SUCCESS) {
        mpc_log_err(0, "initialize resolver failed: (%d: %s)",
                    ares_strerror(rc));

        return MPC_ERROR;
    }

    resolver = mpc_calloc(1, sizeof(mpc_resolver_t));
    if (resolver == NULL) {
        mpc_log_emerg(0, "allocate memory failed");
        return MPC_ERROR;
    }

    mpc_rbtree_init(&resolver->rbtree, &resolver->root);

    mask = ARES_OPT_FLAGS
           |ARES_OPT_DOMAINS
           |ARES_OPT_SOCK_STATE_CB;

    options.flags = ARES_FLAG_NOCHECKRESP
                    |ARES_FLAG_NOALIASES
                    |ARES_FLAG_NOSEARCH;
    options.ndomains = 0;
    options.domains = NULL;
    options.sock_state_cb = mpc_resolver_process_sockstate;
    options.sock_state_cb_data = resolver;
    
    if ((rc = ares_init_options(&resolver->channel, &options, mask))
        != ARES_SUCCESS)
    {
        mpc_log_emerg(0, "ares_init failed: (%d: %s)", rc, ares_strerror(rc));
        goto failed;
    }

    if (server) {
        if ((rc = ares_set_servers_csv(resolver->channel, server))
            != ARES_SUCCESS)
        {
            mpc_log_emerg(0, "ares_set_servers_csv failed: (%d: %s)",
                          rc, ares_strerror(rc));
            goto failed;
        }
    }

    /*
    resolver->timer_id = mpc_create_time_event(el, MPC_RESOLVER_INTERVAL, 
                                               mpc_resolver_process_timeout,
                                               (void *)resolver, NULL);
    if (resolver->timer_id == MPC_ERROR) {
        mpc_log_stderr(0, "create resolver time event failed");
        goto failed;
    }
    */

    resolver->el = el;
    el->resolver = resolver;

    return MPC_OK;

failed:

    mpc_free(resolver);
    return MPC_ERROR;
}


void 
mpc_resolver_cleanup()
{
    ares_library_cleanup();
}


static void
mpc_resolver_callback(void *arg, int status, int timeouts, struct hostent *host)
{
    mpc_resolver_ctx_t  *ctx = (mpc_resolver_ctx_t *)arg;

    if (status == ARES_EDESTRUCTION) {
        /* do nothing here */
        return;
    }

    if (status != ARES_SUCCESS) {
        ctx->callback(ctx->el, status, NULL, ctx->arg);
    } else {
        ctx->callback(ctx->el, MPC_RESOLVER_OK, host, ctx->arg);
    }
}


static void
mpc_resolver_process(mpc_event_loop_t *el, int fd, void *arg, int mask)
{
    mpc_resolver_t *resolver = (mpc_resolver_t *)arg;
    
    if (mask & MPC_READABLE) {
        ares_process_fd(resolver->channel, fd, ARES_SOCKET_BAD);
    }

    if (mask & MPC_WRITABLE) {
        ares_process_fd(resolver->channel, ARES_SOCKET_BAD, fd);
    }
}


mpc_resolver_ctx_t *
mpc_resolver_get(void)
{
    return calloc(sizeof(mpc_resolver_ctx_t), 1);
}


void
mpc_resolver_put(mpc_resolver_ctx_t *ctx)
{
    free(ctx);
}


int
mpc_gethostbyname(mpc_event_loop_t *el, const uint8_t *name, size_t len,
    mpc_gethostbyname_cb callback, void *arg)
{
    mpc_resolver_t       *resolver;
    int                   mask, anyevent, sock;
    ares_socket_t         socks[ARES_GETSOCK_MAXNUM];
    size_t                i;
    char                  buf[MPC_TEMP_BUF_SIZE];
    mpc_resolver_ctx_t   *ctx;
    mpc_rbnode_t         *node;

    resolver = el->resolver;
    ASSERT(resolver);

    ctx = mpc_resolver_get();
    if (ctx == NULL) {
        mpc_log_emerg(0, "get resolver context failed");
        return MPC_ERROR;
    }

    mpc_memzero(buf, sizeof(buf));
    mpc_memcpy(buf, name, MPC_MIN(len, sizeof(buf)));

    ctx->callback = callback;
    ctx->arg = arg;
    ctx->el = el;

    ares_gethostbyname(resolver->channel, buf, AF_INET, mpc_resolver_callback,
                       ctx);

    mask = ares_getsock(resolver->channel, socks, ARES_GETSOCK_MAXNUM);
    if (mask == 0) {
        goto failed;
    }

    for (i = 0; i < ARES_GETSOCK_MAXNUM; i++) {
        anyevent = 0;

        sock = socks[i];
        node = mpc_rbtree_find(&resolver->rbtree, (int64_t) sock);

        if (ARES_GETSOCK_READABLE(mask, i)) {

            anyevent = 1;

            if (node == NULL) {
                node = mpc_rbnode_get();
                if (node == NULL) {
                    goto failed;
                }
                mpc_rbnode_init(node);
                node->key = (int64_t) sock;
                node->data = 0;
            }

            if (!((int64_t )node->data & MPC_READABLE)) {
                node->data = (void *) (((int64_t) node->data)|MPC_READABLE);
                mpc_create_file_event(el, socks[i], MPC_READABLE, 
                                      (mpc_event_file_pt)mpc_resolver_process,
                                      (void *)resolver);
                mpc_rbtree_insert(&resolver->rbtree, node);
            }
        }

        if (ARES_GETSOCK_WRITABLE(mask, i)) {

            anyevent = 1;

            if (node == NULL) {
                node = mpc_rbnode_get();
                if (node == NULL) {
                    goto failed;
                }
                mpc_rbnode_init(node);
                node->key = (int64_t) sock;
                node->data = 0;
            }

            if (!((int64_t) node->data & MPC_WRITABLE)) {
                node->data = (void *) (((int64_t) node->data)|MPC_WRITABLE);
                mpc_create_file_event(el, socks[i], MPC_WRITABLE, 
                                      (mpc_event_file_pt)mpc_resolver_process,
                                      (void *)resolver);
                mpc_rbtree_insert(&resolver->rbtree, node);
            }
        }

        if (anyevent == 0) {
            /* assume no further sockets are returned */
            break;
        }

        resolver->sock_cnt++;
    }

    return MPC_OK;

failed:

    mpc_resolver_put(ctx);

    mpc_rbnode_put(node);

    return MPC_ERROR;
}


/* used to call back into c-ares for possibly processing timeouts */
static int
mpc_resolver_process_timeout(mpc_event_loop_t *el, int64_t id, void *data)
{
    mpc_resolver_t   *resolver = (mpc_resolver_t *)data;

    ares_process_fd(resolver->channel, ARES_SOCKET_BAD, ARES_SOCKET_BAD);
    
    return MPC_RESOLVER_INTERVAL;
}


/* callback from ares when socket operation is started */
static void
mpc_resolver_process_sockstate(void *data, ares_socket_t sock,
    int read, int write)
{
    mpc_resolver_t  *resolver = (mpc_resolver_t *) data;
    mpc_rbnode_t    *node;

    if (read || write) {
        printf("REGISTER %s\n", __func__);

    } else {
        /* read == 0 and write == 0 this is c-ares's way of notifying us
         * that the socket is now closed. We must free the data associated
         * with the socket */

        printf("DELETE %s\n", __func__);

        node = mpc_rbtree_find(&resolver->rbtree, (int64_t) sock);
        ASSERT(node);
        mpc_rbtree_delete(&resolver->rbtree, node);
        mpc_rbnode_put(node);
    }
}


#if 0
static void *
mpc_resolver_key_dup(void *privdata, const void *key) {
    mpc_str_t  *domain = (mpc_str_t *)key;
    mpc_str_t  *keydup = mpc_strndup(domain.data, domain.len);

    DICT_NOTUSED(privdata);

    return keydup;
}


static int
mpc_resolver_key_cmp(void *privdata, const void *key1, 
        const void *key2) {
    DICT_NOTUSED(privdata);

    return mpc_strcasecmp(key1, key2) == 0;
}


static void _demo_destructor(void *privdata, void *key) {
    DICT_NOTUSED(privdata);
    mpc_str_free(key);
}


dict_type mpc_resolver_cache_dict = {
    dict_gen_case_hash,         /* hash function */   
    mpc_resolver_key_dup,       /* key dup */
    NULL,                       /* val dup */
    mpc_resolver_key_cmp,       /* key compare */
    mpc_resolver_key_destruct,  /* key destructor */
    NULL,                       /* val destructor */
};
#endif
