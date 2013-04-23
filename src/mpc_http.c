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


#define MPC_HTTP_HEADER_DONE    -3


static int              mpc_url_id;
static uint32_t         mpc_http_nfree;
static mpc_http_hdr_t   mpc_http_free_queue;
static uint32_t         mpc_http_max_nfree;


#ifdef WITH_MPC_RESOLVER
static void mpc_http_gethostbyname_cb(mpc_event_loop_t *el, int status,
    struct hostent *host, void *arg);
#endif

static void mpc_http_process_connect(mpc_event_loop_t *el, int fd, void *data,
    int mask);
static void mpc_http_process_response(mpc_event_loop_t *el, int fd, void *data,
    int mask);
static int mpc_http_parse_status_line(mpc_http_t *http);
static int mpc_http_parse_headers(mpc_http_t *http);
static int mpc_http_discard_body(mpc_http_t *http);
static int mpc_http_create_request(char *addr, mpc_http_t *mpc_http);
static int mpc_http_log_headers(void *elem, void *data);
static int mpc_http_release_url(void *elem, void *data);
static void mpc_http_release(mpc_http_t *http);


void
mpc_http_reset_bulk(mpc_http_t *http)
{
    mpc_conn_reset(http->conn);

    http->buf = NULL;
    http->http_major = 0;
    http->http_minor = 0;
    http->phase = MPC_HTTP_SEND_REQUEST;

    http->state = 0;
    mpc_memzero(&http->status, sizeof(http->status));
    mpc_memzero(&http->bench, sizeof(http->bench));

    if (http->headers != NULL) {
        mpc_array_destroy(http->headers);
        http->headers = NULL;
    }

    http->invalid_header = 0;
    http->header_name_start = NULL;
    http->header_name_end = NULL;
    http->header_start = NULL;
    http->header_end = NULL;
    http->content_length_n = 0;
    http->content_length_received = 0;
    http->need_redirect = 0;
}


void
mpc_http_reset(mpc_http_t *http)
{
    ASSERT(http->magic == MPC_HTTP_MAGIC);
    http->conn = NULL;
    http->url = NULL;
    http->locations = NULL;
    http->ins = NULL;

    http->buf = NULL;
    http->http_major = 0;
    http->http_minor = 0;
    http->phase = MPC_HTTP_SEND_REQUEST;

    http->state = 0;
    mpc_memzero(&http->status, sizeof(http->status));
    mpc_memzero(&http->bench, sizeof(http->bench));

    http->headers = NULL;
    http->invalid_header = 0;
    http->header_name_start = NULL;
    http->header_name_end = NULL;
    http->header_start = NULL;
    http->header_end = NULL;
    http->content_length_n = 0;
    http->content_length_received = 0;
    http->need_redirect = 0;
}


mpc_http_t *
mpc_http_get(void)
{
    mpc_http_t *http;

    if (!TAILQ_EMPTY(&mpc_http_free_queue)) {
        ASSERT(mpc_http_nfree > 0);

        http = TAILQ_FIRST(&mpc_http_free_queue);
        mpc_http_nfree--;
        TAILQ_REMOVE(&mpc_http_free_queue, http, next);
        ASSERT(http->magic == MPC_HTTP_MAGIC);

    } else {
        http = mpc_alloc(sizeof(*http));
        if (http == NULL) {
            return NULL;
        }

        SET_MAGIC(http, MPC_HTTP_MAGIC);
    }

    mpc_http_reset(http);

    return http;
}


static void
mpc_http_free(mpc_http_t *http)
{
    mpc_free(http);
}


void
mpc_http_put(mpc_http_t *http)
{
    mpc_http_reset(http);
    if (mpc_http_max_nfree != 0 && mpc_http_nfree + 1 > mpc_http_max_nfree) {
        mpc_http_free(http);

    } else {
        mpc_http_nfree++;
        TAILQ_INSERT_HEAD(&mpc_http_free_queue, http, next);
    }
}


void
mpc_http_init(uint32_t max_nfree)
{
    mpc_http_max_nfree = max_nfree;
    mpc_http_nfree = 0;
    TAILQ_INIT(&mpc_http_free_queue);
}


void
mpc_http_deinit()
{
    mpc_http_t *http, *nhttp;

    for (http = TAILQ_FIRST(&mpc_http_free_queue);
         http != NULL;
         http = nhttp, mpc_http_nfree--)
    {
        ASSERT(mpc_http_nfree > 0);
        nhttp = TAILQ_NEXT(http, next);
        mpc_http_free(http);
    }

    ASSERT(mpc_http_nfree == 0);
}


int
mpc_http_parse_url(uint8_t *url, size_t n, mpc_url_t *mpc_url)
{
    uint8_t    *p, c;
    enum {
        sw_domain,
        sw_port,
        sw_uri,
        sw_done
    } state;

    if (mpc_strncasecmp(url, (uint8_t *)"http://", 7) != 0 || n <= 7) {
        return MPC_ERROR;
    }

    mpc_url->url_id = mpc_url_id++;
    mpc_url->port = 0;
    mpc_url->host.len = 0;
    mpc_url->uri.len = 1;

    url += 7;
    n -= 7;
    state = 0;

    mpc_url->host.data = url;

    while (n--) {
        c = *url++;

        switch (state) {
        case sw_domain:
            if (c == ':') {
                state = sw_port;
                p = url;
                break;
            }

            if (c == '/') {
                state = sw_uri;
                mpc_url->uri.data = url - 1; /* start from '/' */
                mpc_url->uri.len = 1;
                break;
            }

            if (c == '\0') {
                state = sw_done;
                break;
            }

            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '.'
                 || (c >= '0' && c <= '9'))
            {
                mpc_url->host.len++;
                break;
            }

            return MPC_ERROR;
        case sw_port:
            if (c == '/') {
                state = sw_uri;
                mpc_url->uri.data = url - 1; /* start from '/' */
                mpc_url->uri.len = 1;
                break;
            }

            if (c == '\0') {
                state = sw_done;
                break;
            }

            if (c < '0' || c > '9') {
                return MPC_ERROR;
            }

            mpc_url->port = mpc_url->port * 10 + c - '0';
            break;
        case sw_uri:
            if (c == '\0') {
                state = sw_done;
                break;
            }

            mpc_url->uri.len++;
            break;

        case sw_done:
            /* do nothing */
            break;
        }
    }

    mpc_url->no_resolve = 0;
    if (mpc_url->host.len == 0 || mpc_url->uri.len == 1) {
        return MPC_ERROR;
    }

    if (mpc_url->port == 0) {
        mpc_url->port = 80;
    }

    return MPC_OK;
}


int
mpc_http_process_request(mpc_http_t *mpc_http)
{
    mpc_url_t       *mpc_url = mpc_http->url;
    mpc_instance_t  *ins = mpc_http->ins;

    ASSERT(mpc_url != NULL);

    if (mpc_url->no_resolve) {
        if (mpc_http_create_request((char *)mpc_url->host.data, mpc_http)
            != MPC_OK)
        {
            mpc_log_err(0, "create http request \"http://%V%V\" failed",
                        &mpc_url->host, &mpc_url->uri);
        }

    } else {
#ifdef WITH_MPC_RESOLVER
        mpc_gethostbyname(ins->el, mpc_http_gethostbyname_cb, 
                          mpc_url->host.data, mpc_url->host.len,
                          AF_INET, (void *)mpc_http, NULL);
#else
        mpc_log_emerg(0, "mpc not compiled with resolver." CRLF
                         "Please recompile it with -DWITH_MPC_RESOLVER");
        exit(1);
#endif 
    }

    return MPC_OK;
}


#ifdef WITH_MPC_RESOLVER
static void
mpc_http_gethostbyname_cb(mpc_event_loop_t *el, int status,
    struct hostent *host, void *arg)
{
    mpc_http_t  *mpc_http = (mpc_http_t *)arg;
    mpc_url_t   *mpc_url = mpc_http->url;

    if (status == MPC_RESOLVER_OK) {
#ifdef WITH_DEBUG
        uint32_t addr = *(in_addr_t *)host->h_addr;
        mpc_log_debug(0, "gethostbyname url(%d) \"%V\" %ud.%ud.%ud.%ud", 
                      mpc_url->url_id, &mpc_url->host,
                      addr & 0xff, (addr & 0xff00) >> 8,
                      (addr & 0xff0000) >> 16, (addr & 0xff000000) >> 24);
#endif

        if (mpc_http_create_request(host->h_addr, mpc_http) != MPC_OK) {
            mpc_log_err(0, "create http request \"http://%V%V\" failed",
                        &mpc_url->host, &mpc_url->uri);
        }
    } else {
        mpc_log_err(0, "gethostbyname failed: (%d: %s)", 
                    status, ares_strerror(status));
        if (mpc_url->no_put == 0) {
            mpc_url_put(mpc_url);
        }
    }
}
#endif


static int
mpc_http_create_request(char *addr, mpc_http_t *mpc_http)
{
    int                 flags;
    int                 sockfd;
    uint8_t            *p, *last;
    mpc_buf_t          *rcv_buf, *snd_buf;
    mpc_conn_t         *conn;
    mpc_url_t          *mpc_url = mpc_http->url;
    mpc_event_loop_t   *el = mpc_http->ins->el;

    ASSERT(mpc_url != NULL);

    rcv_buf = NULL;
    snd_buf = NULL;
    conn = NULL;
    sockfd = -1;

    mpc_http->ins->cur_concurrent++;

    if (mpc_http->conn == NULL) {
        mpc_http->conn = mpc_conn_get();
        if (mpc_http->conn == NULL) {
            mpc_log_emerg(0, "get conn failed, host: \"%V\" uri:\"%V\"", 
                          &mpc_url->host, &mpc_url->uri);
            goto failed;
        }
    }

    conn = mpc_http->conn;

    if (conn->rcv_buf == NULL) {
        conn->rcv_buf = mpc_buf_get();
        if (conn->rcv_buf == NULL) {
            mpc_log_emerg(0, "get buf failed, host: \"%V\" uri:\"%V\"",
                          &mpc_url->host, &mpc_url->uri);
            goto failed;
        }

        mpc_buf_insert(&conn->rcv_buf_queue, conn->rcv_buf);
    }

    rcv_buf = conn->rcv_buf;

    if (conn->snd_buf == NULL) {
        conn->snd_buf = mpc_buf_get();
        if (conn->snd_buf == NULL) {
            mpc_log_emerg(0, "get buf failed, host: \"%V\" uri:\"%V\"",
                          &mpc_url->host, &mpc_url->uri);
            goto failed;
        }
    
        mpc_buf_insert(&conn->snd_buf_queue, conn->snd_buf);
    }

    snd_buf = conn->snd_buf;

    ASSERT(snd_buf->pos == snd_buf->start);

    last = snd_buf->end;
    p = mpc_slprintf(snd_buf->start, last,
                     "GET %V HTTP/1.1" CRLF
                     "Host: %V" CRLF
                     "Accept: *.*" CRLF
                     "UserAgent: %s" CRLF
                     "Connection: close" CRLF
                     CRLF,
                     &mpc_url->uri,
                     &mpc_url->host,
                     MPC_VERSION);
    snd_buf->last = p;

    mpc_log_debug(0, "create request, host: \"%V\" uri: \"%V\"", 
                  &mpc_url->host, &mpc_url->uri);

    flags = MPC_NET_NONBLOCK;
    if (mpc_url->no_resolve) {
        flags |= MPC_NET_NEEDATON;
    }

    mpc_http->bench.start = time_us();
    
    sockfd = mpc_net_tcp_connect(addr, mpc_url->port, MPC_NET_NONBLOCK);
    if (sockfd == MPC_ERROR) {
        mpc_log_err(errno, "tcp connect failed, host: \"%V\" uri: \"%V\"",
                    &mpc_url->host, &mpc_url->uri);
        goto failed;
    }

    conn->connecting = 1;
    conn->fd = sockfd;

    if (mpc_create_file_event(el, sockfd, MPC_WRITABLE, 
                              mpc_http_process_connect, (void *)mpc_http)
        == MPC_ERROR)
    {
        mpc_log_err(0, "create file event failed, host: \"%V\" uri: \"%V\"",
                    &mpc_url->host, &mpc_url->uri);
        goto failed;
    }

    return MPC_OK;

failed:

    mpc_http->ins->cur_concurrent--;

    ASSERT(mpc_http->ins->cur_concurrent >= 0);

    mpc_http_release(mpc_http);

    return MPC_ERROR;
}


static void
mpc_http_release(mpc_http_t *http)
{
    ASSERT(http->magic == MPC_HTTP_MAGIC);

    if (http->url != NULL) {
        if (http->url->no_put == 0) {
            mpc_url_put(http->url);
            http->url = NULL;
        }
    }

    if (http->conn != NULL) {
        mpc_conn_release(http->conn);
        http->conn = NULL;
    }

    if (http->headers != NULL) {
        mpc_array_destroy(http->headers);
        http->headers = NULL;
    }

    if (http->locations != NULL) {
        mpc_array_each(http->locations, mpc_http_release_url, http);
        mpc_array_destroy(http->locations);
    }

    mpc_http_put(http);
}


static void
mpc_http_process_connect(mpc_event_loop_t *el, int fd, void *data, int mask)
{
    mpc_http_t  *http = (mpc_http_t *)data;
    mpc_conn_t  *conn = http->conn;
    mpc_url_t   *mpc_url = http->url;
    int          n;

    http->bench.connected = time_us();

    mpc_log_debug(0, "connecting time: %uLms, host: \"%V\" uri: \"%V\"",
                  (http->bench.connected - http->bench.start) / 1000,
                  &mpc_url->host, &mpc_url->uri);

    conn->connected = 1;
    conn->connecting = 0;
    http->phase = MPC_HTTP_SEND_REQUEST;

    n = mpc_conn_send(conn);

    if (n < 0) {
        mpc_log_err(errno, "send request failed, host: \"%V\" uri: \"%V\"",
                    &mpc_url->host, &mpc_url->uri);
        mpc_delete_file_event(el, fd, MPC_WRITABLE);
        mpc_http_release(http);
        return;
    }

    mpc_log_debug(0, "send request bytes (%d:%d), host: \"%V\" uri: \"%V\"",
                  n, conn->snd_bytes, &mpc_url->host, &mpc_url->uri);

    if (conn->done) {
        conn->done = 0;
        mpc_log_debug(0, "send request over, prepare process response"
                          " host: \"%V\" uri: \"%V\"",
                          &mpc_url->host, &mpc_url->uri);
        mpc_delete_file_event(el, fd, MPC_WRITABLE);
        if (mpc_create_file_event(el, fd, MPC_READABLE,
                          mpc_http_process_response, (void *)http) == MPC_ERROR)
        {
            mpc_http_release(http);
            return;
        }
    }
}


static void
mpc_http_process_response(mpc_event_loop_t *el, int fd, void *data, int mask)
{
    mpc_http_t   *http = (mpc_http_t *)data;
    mpc_conn_t   *conn = http->conn;
    mpc_url_t    *mpc_url = http->url;
    mpc_url_t    *temp_url;
    mpc_url_t   **url_index;
    int           n;
    int           rc;

    http->bench.first_packet_reach = time_us();

    mpc_log_debug(0, "first packet: %uLms, host: \"%V\" uri: \"%V\"",
                (http->bench.first_packet_reach - http->bench.connected) / 1000,
                  &mpc_url->host, &mpc_url->uri);

    n = mpc_conn_recv(conn);
    if (n < 0) {
        mpc_log_err(errno, "recv response failed, host: \"%V\" uri: \"%V\"",
                    &mpc_url->host, &mpc_url->uri);
        mpc_delete_file_event(el, fd, MPC_READABLE);
        mpc_http_release(http);
        return;
    }

    mpc_log_debug(0, "recv response bytes (%d:%d), host: \"%V\" uri: \"%V\"",
                  n, conn->rcv_bytes, &mpc_url->host, &mpc_url->uri);

    if (n == 0) {
        return;
    }

    if (http->buf == NULL) {
        http->buf = STAILQ_FIRST(&http->conn->rcv_buf_queue);
    }

    switch (http->phase) {
    case MPC_HTTP_SEND_REQUEST:
        http->phase = MPC_HTTP_PARSE_STATUS_LINE;
        /* fall through */
    case MPC_HTTP_PARSE_STATUS_LINE:
        goto parse_status_line;
    case MPC_HTTP_PARSE_HEADERS:
        goto parse_headers;
    case MPC_HTTP_PARSE_BODY:
        goto parse_body;
    default:
        MPC_BUG();
    }

parse_status_line:

    rc = mpc_http_parse_status_line(http);

    if (rc == MPC_ERROR) {
        mpc_log_err(0, "parse status failed, host: \"%V\" uri: \"%V\"",
                    &mpc_url->host, &mpc_url->uri);
        mpc_delete_file_event(el, fd, MPC_READABLE);
        mpc_http_release(http);
        return;

    } else if (rc == MPC_AGAIN) {
        return;
    }

    http->phase = MPC_HTTP_PARSE_HEADERS;
    mpc_log_debug(0, "http status %d, host: \"%V\" uri: \"%V\"",
                  http->status.code, &mpc_url->host, &mpc_url->uri);

parse_headers:

    rc = mpc_http_parse_headers(http);

    if (rc == MPC_ERROR) {
        mpc_log_debug(0, "parse header failed, host: \"%V\" uri: \"%V\"",
                      &mpc_url->host, &mpc_url->uri);
        mpc_delete_file_event(el, fd, MPC_READABLE);
        mpc_http_release(http);
        return;

    } else if (rc == MPC_AGAIN) {
        return;
    }

#ifdef WITH_DEBUG
    mpc_array_each(http->headers, mpc_http_log_headers, (void *)http);
#endif
    http->phase = MPC_HTTP_PARSE_BODY;

parse_body:

    rc = mpc_http_discard_body(http);
    if (rc == MPC_ERROR) {
        mpc_log_err(0, "parse body failed, host: \"%V\" uri: \"%V\"",
                    &mpc_url->host, &mpc_url->uri);
        mpc_delete_file_event(el, fd, MPC_READABLE);
        mpc_http_release(http);
        return;

    } else if (rc == MPC_AGAIN) {
        if (conn->eof) {
            mpc_log_err(0, "connection closed by server prematurely"
                         " host: \"%V\" uri: \"%V\"",
                         &mpc_url->host, &mpc_url->uri);
            mpc_delete_file_event(el, fd, MPC_READABLE);
            mpc_http_release(http);
        }

        mpc_conn_buf_rewind(http->conn);
        return;
    }

    http->bench.end = time_us();

    http->ins->cur_concurrent--;
    ASSERT(http->ins->cur_concurrent >= 0);

    if (conn->eof) {
        mpc_log_debug(0, "request over server close connection,"
                         " host: \"%V\" uri: \"%V\"",
                         &mpc_url->host, &mpc_url->uri);
    } else {
        mpc_log_debug(0, "request over mpc close connection,"
                         "host: \"%V\" uri: \"%V\"",
                         &mpc_url->host, &mpc_url->uri);
    }

    close(http->conn->fd);
    http->conn->fd = -1;
    mpc_delete_file_event(el, fd, MPC_READABLE);

    if (http->ins->follow_location && http->need_redirect) {
        url_index = (mpc_url_t **)mpc_array_top(http->locations);
        temp_url = *url_index;
        *url_index = http->url;
        http->url = temp_url;
        mpc_log_debug(0, "redirect to \"http://%V%V\"", 
                      &temp_url->host, &temp_url->uri);
        mpc_http_reset_bulk(http);
        if (mpc_http_process_request(http) != MPC_OK) {
            mpc_log_err(0, "process url \"http://%V%V\" failed, ignored",
                        &mpc_url->host, &mpc_url->uri);
        }

        return;
    }

    mpc_http_release(http);
    return;
}


static int
mpc_http_release_url(void *elem, void *data)
{
    mpc_url_t  *mpc_url = *(mpc_url_t **)elem;

    if (mpc_url->no_put == 0) {
        mpc_url_put(mpc_url);
    }

    return MPC_OK;
}


static int
mpc_http_log_headers(void *elem, void *data)
{
    mpc_http_header_t  *header = (mpc_http_header_t *)elem;
    mpc_http_t         *http = (mpc_http_t *)data;
    mpc_url_t          *mpc_url = http->url;

    mpc_log_debug(0, "http header \"%V: %V\", host: \"%V\" uri: \"%V\"",
                  &header->name, &header->value, &mpc_url->host, &mpc_url->uri);
    return MPC_OK;
}


static int
mpc_http_parse_status_line(mpc_http_t *http)
{
    uint8_t     ch;
    mpc_buf_t  *buf;
    enum {
        sw_start = 0,
        sw_H,
        sw_HT,
        sw_HTT,
        sw_HTTP,
        sw_first_major_digit,
        sw_major_digit,
        sw_first_minor_digit,
        sw_minor_digit,
        sw_status,
        sw_space_after_status,
        sw_status_text,
        sw_almost_done
    } state;

    ASSERT(http->buf != NULL);
    state = http->state;
    buf = http->buf;

    while (buf) {
        while (buf->pos < buf->last) {
            ch = *buf->pos++;

            switch (state) {
            
            /* "HTTP/" */
            case sw_start:
                switch (ch) {
                case 'H':
                    state = sw_H;
                    break;
                default:
                    return MPC_ERROR;
                }
                break;

            case sw_H:
                switch (ch) {
                case 'T':
                    state = sw_HT;
                    break;
                default:
                    return MPC_ERROR;
                }
                break;

            case sw_HT:
                switch (ch) {
                case 'T':
                    state = sw_HTT;
                    break;
                default:
                    return MPC_ERROR;
                }
                break;

            case sw_HTT:
                switch (ch) {
                case 'P':
                    state = sw_HTTP;
                    break;
                default:
                    return MPC_ERROR;
                }
                break;

            case sw_HTTP:
                switch (ch) {
                case '/':
                    state = sw_first_major_digit;
                    break;
                default:
                    return MPC_ERROR;
                }
                break;

            /* the first digit of major HTTP version */
            case sw_first_major_digit:
                if (ch < '1' || ch > '9') {
                    return MPC_ERROR;
                }
                http->http_major = ch - '0';
                state = sw_major_digit;
                break;

            /* the major HTTP version or dot */
            case sw_major_digit:
                if (ch == '.') {
                    state = sw_first_minor_digit;
                    break;
                }

                if (ch < '0' || ch > '9') {
                    return MPC_ERROR;
                }

                http->http_major = http->http_major * 10 + ch - '0';
                break;

            /* the first digit of minor HTTP version */
            case sw_first_minor_digit:
                if (ch < '0' || ch > '9') {
                    return MPC_ERROR;
                }

                http->http_minor = ch - '0';
                state = sw_minor_digit;
                break;

            /* the minor HTTP version or the end of the request line */
            case sw_minor_digit:
                if (ch == ' ') {
                    state = sw_status;
                    break;
                }

                if (ch < '0' || ch > '9') {
                    return MPC_ERROR;
                }

                http->http_minor = http->http_minor * 10 + ch - '0';
                break;

            /* HTTP status code */
            case sw_status:
                if (ch == ' ') {
                    break;
                }

                if (ch < '0' || ch > '9') {
                    return MPC_ERROR;
                }

                http->status.code = http->status.code * 10 + ch - '0';

                if (++http->status.count == 3) {
                    state = sw_space_after_status;
                    http->status.start = buf->pos - 2;
                }

                break;

            /* space or end of line */
            case sw_space_after_status:
                switch (ch) {
                case ' ':
                    state = sw_status_text;
                    break;

                case '.':                  /* IIS may send 403.1, 403.2, etc */
                    state = sw_status_text;
                    break;

                case CR:
                    state = sw_almost_done;
                    break;

                case LF:
                    goto done;

                default:
                    return MPC_ERROR;
                }
                break;

            /* any text until end of line */
            case sw_status_text:
                switch (ch) {
                case CR:
                    state = sw_almost_done;
                    break;

                case LF:
                    goto done;
                }
                break;

            /* end of status line */
            case sw_almost_done:
                http->status.end = buf->pos - 1;
                switch (ch) {
                case LF:
                    goto done;

                default:
                    return MPC_ERROR;
                }
            }
        }

        if (buf->pos == buf->last) {
            buf = STAILQ_NEXT(buf, next);
            http->buf = buf;
        }
    }

    http->state = state;

    return MPC_AGAIN;

done:
    http->state = sw_start;

    if (http->status.end == NULL) {
        http->status.end = buf->pos;
    }

    http->status.http_version = http->http_major * 1000 + http->http_minor;

    return MPC_OK;
}


int
mpc_http_parse_header_line(mpc_http_t *http)
{
    uint8_t              c, ch;
    mpc_buf_t           *buf;
    enum {
        sw_start = 0,
        sw_name,
        sw_space_before_value,
        sw_value,
        sw_space_after_value,
        sw_ignore_line,
        sw_almost_done,
        sw_header_almost_done
    } state;

    /* the last '\0' s not needed because string is zero terminated */
    static uint8_t lowcase[] = 
        "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
        "\0\0\0\0\0\0\0\0\0\0\0\0\0-\0\0" "0123456789\0\0\0\0\0\0"
        "\0abcdefghijklmnopqrstuvwxyz\0\0\0\0\0"
        "\0abcdefghijklmnopqrstuvwxyz\0\0\0\0\0"
        "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
        "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
        "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
        "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";

    ASSERT(http->buf != NULL);
    state = http->state;

    buf = http->buf;

    while (buf) {
        while (buf->pos < buf->last) {
            ch = *buf->pos++;

            switch (state) {

            /* first char */
            case sw_start:
                http->header_name_start = buf->pos - 1;
                http->invalid_header = 0;

                switch (ch) {
                case CR:
                    http->header_end = buf->pos - 1;
                    state = sw_header_almost_done;
                    break;

                case LF:
                    http->header_end = buf->pos - 1;
                    goto header_done;

                default:
                    state = sw_name;
                    c = lowcase[ch];

                    if (c) {
                        break;
                    }

                    if (ch == '\0') {
                        return MPC_ERROR;
                    }

                    http->invalid_header = 1;
                    break;
                }
                break;

            /* header name */
            case sw_name:
                c = lowcase[ch];
                
                if (c) {
                    break;
                }

                if (ch == ':') {
                    http->header_name_end = buf->pos - 1;
                    state = sw_space_before_value;
                    break;
                }

                if (ch == CR) {
                    http->header_name_end = buf->pos - 1;
                    http->header_start = buf->pos - 1;
                    http->header_end = buf->pos - 1;
                    state = sw_almost_done;
                    break;
                }

                if (ch == LF) {
                    http->header_name_end = buf->pos - 1;
                    http->header_start = buf->pos - 1;
                    http->header_end = buf->pos - 1;
                    goto done;
                }
                
                /* IIS may send the duplicate "HTTP/1.1 ..." lines */

                if (ch == '/'
                    && http->header_name_end - http->header_name_start == 4
                    && mpc_strncmp(http->header_start, "HTTP", 4) == 0)
                {
                    state = sw_ignore_line;
                    break;
                }

                if (ch == '\0') {
                    return MPC_ERROR;
                }

                http->invalid_header = 1;
                break;

            /* space* before header value */
            case sw_space_before_value:
                switch (ch) {
                case ' ':
                    break;

                case CR:
                    http->header_start = buf->pos - 1;
                    http->header_end = buf->pos - 1;
                    state = sw_almost_done;
                    break;

                case LF:
                    http->header_start = buf->pos - 1;
                    http->header_end = buf->pos - 1;
                    goto done;

                case '\0':
                    return MPC_ERROR;

                default:
                    http->header_start = buf->pos - 1;
                    state = sw_value;
                    break;
                }
                break;

            /* header value */
            case sw_value:
                switch (ch) {
                case ' ':
                    http->header_end = buf->pos - 1;
                    state = sw_space_after_value;
                    break;

                case CR:
                    http->header_end = buf->pos - 1;
                    state = sw_almost_done;
                    break;

                case LF:
                    http->header_end = buf->pos - 1;
                    goto done;

                case '\0':
                    return MPC_ERROR;
                }
                break;

            /* space* before end of header line */
            case sw_space_after_value:
                switch (ch) {
                case ' ':
                    break;

                case CR:
                    state = sw_almost_done;
                    break;

                case LF:
                    goto done;

                case '\0':
                    return MPC_ERROR;

                default:
                    state = sw_value;
                    break;
                }
                break;

            /* ignore header line */
            case sw_ignore_line:
                switch (ch) {
                case LF:
                    state = sw_start;
                    break;
                default:
                    break;
                }
                break;

            /* end of header line */
            case sw_almost_done:
                switch (ch) {
                case LF:
                    goto done;
                case CR:
                    break;
                default:
                    return MPC_ERROR;
                }
                break;

            /* end of header */
            case sw_header_almost_done:
                switch (ch) {
                case LF:
                    goto header_done;
                default:
                    return MPC_ERROR;
                }
            }
        }

        if (buf->pos == buf->last) {
            buf = STAILQ_NEXT(buf, next);
            http->buf = buf;
        }
    }

    http->state = state;
    return MPC_AGAIN;

done:
    http->state = sw_start;
    return MPC_OK;

header_done:
    http->state = sw_start;
    return MPC_HTTP_HEADER_DONE;
}


static int
mpc_http_parse_headers(mpc_http_t *http)
{
    int                  rc;
    uint8_t             *p, *last;
    mpc_http_header_t   *header;
    mpc_url_t          **url;

    for (;;) {

        rc = mpc_http_parse_header_line(http);

        if (rc == MPC_OK) {
            if (http->headers == NULL) {
                http->headers = mpc_array_create(8, sizeof(mpc_http_header_t));
                if (http->headers == NULL) {
                    return MPC_ERROR;
                }
            }
    
            header = mpc_array_push(http->headers);
            if (header == NULL) {
                return MPC_ERROR;
            }

            header->name.data = http->header_name_start;
            header->name.len = http->header_name_end - header->name.data;

            header->value.data = http->header_start;
            header->value.len = http->header_end - http->header_start;

            if (header->name.len == sizeof("Content-Length") - 1
                && mpc_strncasecmp(header->name.data, 
                                   (uint8_t *)"Content-Length",
                                   sizeof("Content-Length") - 1) == 0)
            {
                http->content_length_n = mpc_atoi(header->value.data,
                                                  header->value.len);
                if (http->content_length_n == MPC_ERROR) {
                    return MPC_ERROR;
                }
            }

            if (header->name.len == sizeof("Location") - 1
                && mpc_strncasecmp(header->name.data, (uint8_t *)"Location",
                                   sizeof("Location") - 1) == 0)
            {
                if (http->locations == NULL) {
                    http->locations = mpc_array_create(4, sizeof(mpc_url_t *));
                    if (http->locations == NULL) {
                        return MPC_ERROR;
                    }
                }

                if (http->locations->nelem > MPC_HTTP_MAX_REDIRECT) {
                    mpc_log_err(0, "http beyond max redirect(%d),"
                                   " main url(%ud)",
                                   MPC_HTTP_MAX_REDIRECT, http->url->url_id);
                    return MPC_ERROR;
                }

                url = mpc_array_push(http->locations);
                if (url == NULL) {
                    return MPC_ERROR;
                }

                *url = mpc_url_get();
                if (*url == NULL) {
                    return MPC_ERROR;
                }

                last = (*url)->buf + (*url)->buf_size;

                p = mpc_slprintf((*url)->buf, last, "%V", &header->value);

                if (p == last) {
                    mpc_log_err(0, "url buf size (%d) too small for \"%V\"",
                                (*url)->buf_size, &header->value);
                    return MPC_ERROR;
                }

                if (mpc_http_parse_url((*url)->buf, header->value.len, *url)
                    != MPC_OK)
                {
                    mpc_log_err(0, "parse http url \"%V\" failed", 
                                &header->value);
                    return MPC_ERROR;
                }

                http->need_redirect = 1;
            }

            continue; 

        } else if (rc == MPC_HTTP_HEADER_DONE) {
            return MPC_OK;
        }

        if (rc == MPC_AGAIN) {
            return MPC_AGAIN;
        }

        return MPC_ERROR;
    }

    MPC_NOT_REACHED();
}


static int
mpc_http_discard_body(mpc_http_t *http)
{
    mpc_buf_t *buf = http->buf;
    mpc_url_t *mpc_url = http->url;

    while (buf) {
        /*
        while (buf->pos < buf->last) {
            buf->pos++;
            http->content_length_received++;
        }
        */

        http->content_length_received += buf->last - buf->pos;
        buf->pos = buf->last;

        if (buf->pos == buf->last) {
            buf = STAILQ_NEXT(buf, next);
            http->buf = buf;
        }
    }

    mpc_log_debug(0, "http rcv body bytes(%d), host: \"%V\" uri: \"%V\"",
                  http->content_length_received, &mpc_url->host, &mpc_url->uri);

    if (http->content_length_received == http->content_length_n) {
        return MPC_OK;
    }

    if (http->conn->eof) {
        mpc_log_err(0, "server close connection prematurely,"
                       " host: \"%V\" uri: \"%V\"",
                       &mpc_url->host, &mpc_url->uri);
        return MPC_ERROR;
    }

    return MPC_AGAIN;
}
