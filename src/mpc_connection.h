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

#ifndef __MPC_CONNECTION_H_INCLUDED__
#define __MPC_CONNECTION_H_INCLUDED__


#define MPC_CONN_MAGIC      0x4e4e4f43   /* "CONN" */


typedef struct mpc_conn_s mpc_conn_t;
typedef struct mpc_conn_hdr_s mpc_conn_hdr_t;


struct mpc_conn_s {
#ifdef WITH_DEBUG
    uint32_t                    magic;
#endif 
    TAILQ_ENTRY(mpc_conn_s)     next;
    int                         fd;
    int                         err;
    socklen_t                   addrlen;
    struct sockaddr            *addr;
    mpc_buf_hdr_t               rcv_buf_queue;
    mpc_buf_hdr_t               snd_buf_queue;
    mpc_buf_t                  *rcv_buf;
    mpc_buf_t                  *snd_buf;
    size_t                      rcv_bytes;
    size_t                      snd_bytes;
    unsigned                    keepalive:1;
    unsigned                    eof:1;
    unsigned                    done:1;
    unsigned                    connecting:1;
    unsigned                    connected:1;
};


TAILQ_HEAD(mpc_conn_hdr_s, mpc_conn_s);


mpc_conn_t *mpc_conn_get(void);
void mpc_conn_put(mpc_conn_t *conn);
void mpc_conn_init(void);
void mpc_conn_deinit(void);
int mpc_conn_recv(mpc_conn_t *conn);
int mpc_conn_send(mpc_conn_t *conn);
void mpc_conn_release(mpc_conn_t *conn);


#endif /* __MPC_CONNECTION_H_INCLUDED__ */
