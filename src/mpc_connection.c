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


#include <mpc_core.h>


static uint32_t         mpc_conn_nfree;
static mpc_conn_hdr_t   mpc_conn_free_queue;


static mpc_conn_t *
mpc_conn_get_internal(void)
{
    mpc_conn_t  *conn;

    if (!TAILQ_EMPTY(&mpc_conn_free_queue)) {
        ASSERT(mpc_conn_nfree > 0);

        conn = TAILQ_FIRST(&mpc_conn_free_queue);
        mpc_conn_nfree--;
        TAILQ_REMOVE(&mpc_conn_free_queue, mpc_conn_s, next);
    } else {
        conn = mpc_alloc(sizeof(*conn));
        if (conn == NULL) {
            return NULL;
        }
    }

    conn->fd = -1;
    conn->rcv_buf = NULL;
    conn->snd_buf = NULL;

    conn->rcv_bytes = 0;
    conn->snd_bytes = 0;
    
    conn->connecting = 0;
    conn->connected = 0;

    return conn;
}


mpc_conn_t *
mpc_conn_get(void)
{
    mpc_conn_t  *conn;

    conn = mpc_conn_get_internal();
    if (conn == NULL) {
        return NULL;
    }

    return conn;
}


static void
mpc_conn_free(mpc_conn_t *conn)
{
    mpc_free(conn);
}


void
mpc_conn_put(mpc_conn_t *conn)
{
    ASSERT(conn->fd < 0);
    mpc_conn_nfree++;
    TAILQ_INSERT_HEAD(&mpc_conn_free_queue, mpc_conn_s, next);
}


void
mpc_conn_init(void)
{
    mpc_conn_nfree = 0;
    TAILQ_INIT(&mpc_conn_free_queue);
}


void
mpc_conn_deinit(void)
{
    mpc_conn_t *conn, *nconn;   /* current and next connection */

    for (conn = TAILQ_FIRST(&mpc_conn_free_queue); conn != NULL;
         conn = nconn, mpc_conn_nfree--)
    {
        ASSERT(mpc_conn_nfree > 0);
        nconn = TAILQ_NEXT(conn, next);
        mpc_conn_free(conn);
    }

    ASSERT(mpc_conn_nfree == 0);
}


ssize_t
mpc_conn_recv(mpc_conn_t *conn, void *buf, size_t size)
{
    ssize_t n;

    ASSERT(buf != NULL);
    ASSERT(size > 0);
    
    for ( ;; ) {
        n = read(conn->fd, buf, size);

        if (n > 0) {
            conn->rcv_bytes += (size_t)n;
            return n;
        }

        if (n == 0) {
            return 0;
        }

        if (errno == EINTR) {
            continue;
        } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;
        } else {
            conn->errno = errno;
        }
    }

    return NC_ERROR;
}


ssize_t
mpc_conn_sendv(mpc_conn_t *conn, mpc_array_t *sendv, size_t nsend)
{
    ssize_t     n;

    ASSERT(sendv->nelem > 0);
    ASSERT(nsend != 0);

    for (;;) {
    }
}

