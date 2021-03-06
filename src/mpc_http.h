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


#define MPC_HTTP_MAGIC                  0x48545450  /* "HTTP" */
#define MPC_HTTP_SEND_REQUEST           0
#define MPC_HTTP_PARSE_STATUS_LINE      1
#define MPC_HTTP_PARSE_HEADERS          2
#define MPC_HTTP_PARSE_BODY             3

#define MPC_HTTP_MAX_REDIRECT           10
#define MPC_HTTP_MAX_NFREE              128

#define MPC_HTTP_METHOD_GET             0
#define MPC_HTTP_METHOD_HEAD            1


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


typedef int (*mpc_http_header_handler_pt)(mpc_http_header_t *header, 
    mpc_http_t *http, void *data);


typedef struct {
    mpc_str_t                     header;
    mpc_http_header_handler_pt    handler;
    mpc_http_t                   *data;
} mpc_http_header_handler_t;


typedef struct {
    uint64_t    start;
    uint64_t    connected;
    uint64_t    first_packet_reach;
    uint64_t    end;
} mpc_http_bench_t;


struct mpc_http_s {
#ifdef WITH_DEBUG
    uint32_t                 magic;
#endif
    TAILQ_ENTRY(mpc_http_s)  next;
    uint32_t                 id;
    mpc_instance_t          *ins;
    mpc_conn_t              *conn;
    mpc_url_t               *url;
    int                      phase;
    mpc_array_t             *locations;
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
    int                      size;
    int                      length;
    mpc_http_bench_t         bench;
    unsigned                 need_redirect:1;
    unsigned                 used:1;
    unsigned                 chunked:1;
    unsigned                 discard_chunk:1;
};


TAILQ_HEAD(mpc_http_hdr_s, mpc_http_s);


void mpc_http_init(uint32_t max_nfree);
void mpc_http_deinit(void);
mpc_http_t *mpc_http_get(void);
void mpc_http_put(mpc_http_t *mpc_http);
int mpc_http_parse_url(uint8_t *url, size_t n, mpc_url_t *mpc_url);
int mpc_http_process_request(mpc_instance_t *ins, mpc_url_t *mpc_url,
    mpc_http_t *mpc_http);
void mpc_http_create_missing_requests(mpc_instance_t *ins);
uint32_t mpc_http_get_used(void);
int mpc_http_get_method(char *method);


#endif /* __MPC_HTTP_H_INCLUDED__ */
