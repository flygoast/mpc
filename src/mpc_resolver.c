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


#define MPC_RESOLVER_MAGIC   0x52534c56  /* "RSLV" */


typedef struct {
    mpc_event_loop_t  *el;
    ares_channel       channel;
    int                mask;
    ares_socket_t      socks[ARES_GETSOCK_MAXNUM];
    unsigned           channel_over:1;
} mpc_resolver_t;


typedef struct {
#ifdef WITH_DEBUG
    int                    magic;
#endif
    mpc_gethostbyname_cb   callback;
    void                  *arg;
    mpc_event_loop_t      *el;
} mpc_resolver_ctx_t;


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

    mask = ARES_OPT_FLAGS
           |ARES_OPT_DOMAINS;

    options.flags = ARES_FLAG_NOCHECKRESP
                    |ARES_FLAG_NOALIASES
                    |ARES_FLAG_NOSEARCH;

    options.ndomains = 0;
    options.domains = NULL;
    
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

    resolver->el = el;
    el->resolver = resolver;

    return MPC_OK;

failed:

    mpc_free(resolver);
    return MPC_ERROR;
}


/*
void 
mpc_resolver_cleanup()
{
    ares_library_cleanup();
}
*/


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
    size_t          i;
    mpc_resolver_t *resolver = (mpc_resolver_t *)arg;
    
    if (mask & MPC_READABLE) {
        ares_process_fd(resolver->channel, fd, ARES_SOCKET_BAD);
    }

    if (mask & MPC_WRITABLE) {
        ares_process_fd(resolver->channel, ARES_SOCKET_BAD, fd);
    }

    if (resolver->channel_over) {
        for (i = 0; i < ARES_GETSOCK_MAXNUM; ++i) {
            if (ARES_GETSOCK_READABLE(resolver->mask, i)) {
                mpc_delete_file_event(resolver->el, resolver->socks[i], 
                                      MPC_READABLE);
            }

            if (ARES_GETSOCK_WRITABLE(resolver->mask, i)) {
                mpc_delete_file_event(resolver->el, resolver->socks[i], 
                                      MPC_WRITABLE);
            }
        }

        ares_destroy(resolver->channel);
        mpc_free(resolver);
    }
}


mpc_resolver_ctx_t *
mpc_resolver_get()
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
    int                   mask;
    size_t                i;
    char                  buf[MPC_TEMP_BUF_SIZE];
    mpc_resolver_ctx_t   *ctx;

    resolver = el->resolver;
    assert(resolver);

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

    mask = ares_getsock(resolver->channel, resolver->socks, 
                        ARES_GETSOCK_MAXNUM);

    resolver->mask = mask;

    for (i = 0; i < ARES_GETSOCK_MAXNUM; i++) {
        if (ARES_GETSOCK_READABLE(mask, i)) {
            mpc_create_file_event(el, resolver->socks[i], MPC_READABLE, 
                                  (mpc_event_file_pt)mpc_resolver_process,
                                  (void *)resolver);
        }

        if (ARES_GETSOCK_WRITABLE(mask, i)) {
            mpc_create_file_event(el, resolver->socks[i], MPC_WRITABLE, 
                                  (mpc_event_file_pt)mpc_resolver_process,
                                  (void *)resolver);
        }
    }

    return MPC_OK;
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
