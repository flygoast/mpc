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


#ifndef __MPC_URL_H_INCLUDED__
#define __MPC_URL_H_INCLUDED__


#define MPC_URL_MAGIC           0x4d55524c  /* "MURL" */
#define MPC_URL_BUF_SIZE        8192
#define MPC_URL_MAX_NFREE       128


typedef struct mpc_url_s mpc_url_t;
typedef struct mpc_url_hdr_s mpc_url_hdr_t;


struct mpc_url_s {
#ifdef WITH_DEBUG
    uint32_t                    magic;
#endif
    STAILQ_ENTRY(mpc_url_s)     next;
    mpc_str_t                   host;
    mpc_str_t                   uri;
    int                         url_id;
    int                         port;
    uint8_t                    *buf;
    uint32_t                    buf_size;
    unsigned                    no_resolve:1;
    unsigned                    no_put:1;
};


STAILQ_HEAD(mpc_url_hdr_s, mpc_url_s);


mpc_url_t *mpc_url_get(void);
void mpc_url_put(mpc_url_t *mpc_url);
void mpc_url_init(uint32_t max_nfree);
void mpc_url_deinit(void);
mpc_url_t *mpc_url_task_get(void);
void mpc_url_task_insert(mpc_url_t *mpc_url);
uint32_t mpc_url_task_count(void);
uint32_t mpc_url_free_count(void);


#endif /* __MPC_URL_H_INCLUDED__ */
