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
static uint32_t         mpc_conn_max_nfree;


static void mpc_conn_default(mpc_conn_t *conn);


mpc_conn_t *
mpc_conn_get(void)
{
    mpc_conn_t  *conn;

    if (!TAILQ_EMPTY(&mpc_conn_free_queue)) {
        ASSERT(mpc_conn_nfree > 0);

        conn = TAILQ_FIRST(&mpc_conn_free_queue);
        mpc_conn_nfree--;
        TAILQ_REMOVE(&mpc_conn_free_queue, conn, next);
        ASSERT(conn->magic == MPC_CONN_MAGIC);

    } else {
        conn = mpc_alloc(sizeof(mpc_conn_t));
        if (conn == NULL) {
            return NULL;
        }

        SET_MAGIC(conn, MPC_CONN_MAGIC);
    }

    mpc_conn_default(conn);

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
    mpc_conn_default(conn);

    if (mpc_conn_max_nfree != 0 && mpc_conn_nfree + 1 > mpc_conn_max_nfree) {
        mpc_conn_free(conn);

    } else {
        mpc_conn_nfree++;
        TAILQ_INSERT_HEAD(&mpc_conn_free_queue, conn, next);
    }
}


void
mpc_conn_init(uint32_t max_nfree)
{

    mpc_conn_max_nfree = max_nfree;
    mpc_conn_nfree = 0;
    TAILQ_INIT(&mpc_conn_free_queue);
}


void
mpc_conn_deinit(void)
{
    mpc_conn_t *conn, *nconn;   /* current and next connection */

    for (conn = TAILQ_FIRST(&mpc_conn_free_queue);
         conn != NULL;
         conn = nconn, mpc_conn_nfree--)
    {
        ASSERT(mpc_conn_nfree > 0);
        nconn = TAILQ_NEXT(conn, next);
        mpc_conn_free(conn);
    }

    ASSERT(mpc_conn_nfree == 0);
}


int
mpc_conn_recv(mpc_conn_t *conn)
{
    int          n, sum;
    mpc_buf_t   *mpc_buf;

    sum = 0;
    for (;;) {
        n = read(conn->fd, conn->rcv_buf->last, 
                 conn->rcv_buf->end - conn->rcv_buf->last);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }

            if (errno == EAGAIN) {
                n = 0;
                break;
            }

            return -1;
        }

        if (n == 0) {
            conn->eof = 1;
            break;
        }

        conn->rcv_buf->last += n;
        sum += n;

        if (conn->rcv_buf->end == conn->rcv_buf->last) {
            mpc_buf = STAILQ_NEXT(conn->rcv_buf, next);
            if (mpc_buf == NULL) {
                mpc_buf = mpc_buf_get();
                mpc_buf_insert(&conn->rcv_buf_queue, mpc_buf);
            }
            conn->rcv_buf = mpc_buf;
        }
    }

    conn->rcv_bytes += sum;
    return sum;
}


static void
mpc_conn_default(mpc_conn_t *conn)
{
    ASSERT(conn->magic == MPC_CONN_MAGIC); 

    conn->fd = -1;

    STAILQ_INIT(&conn->rcv_buf_queue);
    STAILQ_INIT(&conn->snd_buf_queue);

    conn->rcv_buf = STAILQ_FIRST(&conn->rcv_buf_queue);
    conn->snd_buf = STAILQ_FIRST(&conn->snd_buf_queue);

    conn->rcv_bytes = 0;
    conn->snd_bytes = 0;

    conn->keepalive = 0;
    conn->connecting = 0;
    conn->connected = 0;
    conn->eof = 0;
    conn->done = 0;
}


int
mpc_conn_send(mpc_conn_t *conn)
{
    int  n, sum;

    sum = 0;
    for (;;) {
        n = write(conn->fd, conn->snd_buf->pos, 
                  conn->snd_buf->last - conn->snd_buf->pos);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }

            if (errno == EAGAIN) {
                n = 0;
                break;
            }

            mpc_log_err(errno, "write() failed, fd: %d", conn->fd);

            return -1;
        }

        if (n == 0) {
            break;
        }

        sum += n;
        conn->snd_buf->pos += n;

        if (conn->snd_buf->pos == conn->snd_buf->last) {
            conn->snd_buf = STAILQ_NEXT(conn->snd_buf, next);
            if (conn->snd_buf == NULL) {
                conn->done = 1;
                break;
            }
        }
    }

    conn->snd_bytes += sum;
    return sum;
}


void
mpc_conn_release(mpc_conn_t *conn)
{
     mpc_buf_t *buf, *nbuf;   /* current and next buf */

    for (buf = STAILQ_FIRST(&conn->rcv_buf_queue); buf != NULL; buf = nbuf) {
        nbuf = STAILQ_NEXT(buf, next);
        STAILQ_NEXT(buf, next) = NULL;
        mpc_buf_put(buf);
    }

    for (buf = STAILQ_FIRST(&conn->snd_buf_queue); buf != NULL; buf = nbuf) {
        nbuf = STAILQ_NEXT(buf, next);
        STAILQ_NEXT(buf, next) = NULL;
        mpc_buf_put(buf);
    }

    if (conn->fd != -1) {
        close(conn->fd);
    }

    mpc_conn_put(conn);
}


void
mpc_conn_buf_rewind(mpc_conn_t *conn)
{
    mpc_buf_queue_rewind(&conn->rcv_buf_queue);
    mpc_buf_queue_rewind(&conn->snd_buf_queue);

    conn->rcv_buf = STAILQ_FIRST(&conn->rcv_buf_queue);
    conn->snd_buf = STAILQ_FIRST(&conn->snd_buf_queue);

    conn->rcv_bytes = 0;
    conn->snd_bytes = 0;
}


void
mpc_conn_reset(mpc_conn_t *conn)
{
    ASSERT(conn->magic == MPC_CONN_MAGIC); 

    if (conn->fd != -1) {
        close(conn->fd);
        conn->fd = -1;
    }

    mpc_conn_buf_rewind(conn);

    conn->keepalive = 0;
    conn->connecting = 0;
    conn->connected = 0;
    conn->eof = 0;
    conn->done = 0;
}
