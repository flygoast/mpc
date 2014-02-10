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
           |ARES_OPT_LOOKUPS
           |ARES_OPT_SOCK_STATE_CB;

    options.flags = ARES_FLAG_NOCHECKRESP
                    |ARES_FLAG_NOALIASES
                    |ARES_FLAG_NOSEARCH;
    options.lookups = "b"; /* only DNS lookup */
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

    resolver->timer_id = -1;
    resolver->el = el;
    el->resolver = resolver;

    return MPC_OK;

failed:

    mpc_free(resolver);
    return MPC_ERROR;
}


void
mpc_resolver_deinit(mpc_event_loop_t *el)
{
    mpc_resolver_t  *resolver;

    resolver = (mpc_resolver_t *) el->resolver;

    if (resolver == NULL) {
        return;
    }

    if (resolver->timer_id != -1) {
        mpc_delete_time_event(el, resolver->timer_id);
    }

    ares_destroy(resolver->channel);

    mpc_free(resolver);
    el->resolver = NULL;

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
    char                  buf[MPC_TEMP_BUF_SIZE];
    mpc_resolver_ctx_t   *ctx;

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

    return MPC_OK;
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
    int              mask;

    node = mpc_rbtree_find(&resolver->rbtree, (int64_t) sock);

    if (read || write) {
        mask = (read ? MPC_READABLE : 0) | (write ? MPC_WRITABLE : 0); 

        if (node == NULL) {
            /* new socket */

            /* if this is the first socket then create a timer event */
            if (resolver->timer_id == -1) {
                resolver->timer_id = mpc_create_time_event(resolver->el, 
                                                   MPC_RESOLVER_INTERVAL, 
                                                   mpc_resolver_process_timeout,
                                                   (void *)resolver, NULL);

                if (resolver->timer_id == MPC_ERROR) {
                    mpc_log_stderr(0, "create resolver time event failed");
                    ASSERT(0);
                }
            }

            node = mpc_rbnode_get();
            if (node == NULL) {
                mpc_log_stderr(0, "get rbtree node failed");
                ASSERT(0);
            }

            mpc_rbnode_init(node);
            node->key = (int64_t) sock;
            node->data = (void *) 0;
            mpc_rbtree_insert(&resolver->rbtree, node);
        }

        node->data = (void *) (int64_t) mask;

        mpc_create_file_event(resolver->el, sock, mask,
                              (mpc_event_file_pt) mpc_resolver_process,
                              (void *)resolver);

    } else {
        /* read == 0 and write == 0 this is c-ares's way of notifying us
         * that the socket is now closed. We must free the data associated
         * with the socket */

        node = mpc_rbtree_find(&resolver->rbtree, (int64_t) sock);
        mpc_delete_file_event(resolver->el, sock, MPC_READABLE|MPC_WRITABLE);
        ASSERT(node);
        mpc_rbtree_delete(&resolver->rbtree, node);
        mpc_rbnode_put(node);

        if (mpc_rbtree_empty(&resolver->rbtree)) {
            if (resolver->timer_id != -1) {
                mpc_delete_time_event(resolver->el, resolver->timer_id);
                resolver->timer_id = -1;
            }
        }
    }
}
