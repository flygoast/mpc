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


#ifndef __MPC_HTTP_H_INCLUDED__
#define __MPC_HTTP_H_INCLUDED__


#define MPC_HTTP_MAGIC      0x48545450  /* "HTTP" */


typedef struct mpc_http_s mpc_http_t;
typedef struct mpc_http_hdr_s mpc_http_hdr_t;


typedef struct {
    uint32_t     http_version;
    uint32_t     code;
    uint32_t     count;
    uint8_t     *start;
    uint8_t     *end;
} mpc_http_status_t;


typedef struct {
    mpc_str_t   name;
    mpc_str_t   value;
} mpc_http_header_t;


struct mpc_http_s {
#ifdef WITH_DEBUG
    uint32_t                 magic;
#endif
    TAILQ_ENTRY(mpc_http_s)  next;
    mpc_conn_t              *conn;
    int                      http_major;
    int                      http_minor;
    mpc_buf_t               *buf;
    mpc_http_status_t        status;
    int                      state;
    mpc_array_t             *headers;
    int                      invalid_header;
    uint8_t                 *header_name_start;
    uint8_t                 *header_name_end;
    uint8_t                 *header_start;
    uint8_t                 *header_end;
    int                      content_length_n;
    int                      content_length_received;
};


TAILQ_HEAD(mpc_http_hdr_s, mpc_http_s);


int mpc_http_parse_url(uint8_t *url, size_t n, mpc_url_t *mpc_url);
int mpc_http_process_url(mpc_event_loop_t *el, mpc_url_t *mpc_url);
int mpc_http_request(char *addr, mpc_event_loop_t *el, mpc_url_t *mpc_url);


#endif /* __MPC_HTTP_H_INCLUDED__ */
