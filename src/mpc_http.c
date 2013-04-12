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


static uint32_t         mpc_http_nfree;
static mpc_http_hdr_t   mpc_http_free_queue;


static void mpc_http_gethostbyname_cb(mpc_event_loop_t *el, int status,
    struct hostent *host, void *arg);
static void mpc_http_connect_handler(mpc_event_loop_t *el, int fd, void *data,
    int mask);
static void mpc_http_request_snd_handler(mpc_event_loop_t *el, int fd,
    void *data, int mask);
static int mpc_http_parse_status_line(mpc_http_t *http);
static int mpc_http_process_body(mpc_http_t *http);
static int mpc_http_process_headers(mpc_http_t *http);
static void mpc_http_process_headers_handler(mpc_event_loop_t *el, int fd,
    void *data, int mask);
static void mpc_http_process_status_line(mpc_event_loop_t *el, int fd,
    void *data, int mask);


void
mpc_http_reset(mpc_http_t *http)
{
    ASSERT(http->magic == MPC_HTTP_MAGIC);
    http->state = 0;
    http->conn = NULL;
    http->http_major = 0;
    http->http_minor = 0;
    http->buf = NULL;
    memset(&http->status, 0, sizeof(http->status));
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
    mpc_http_nfree++;
    TAILQ_INSERT_HEAD(&mpc_http_free_queue, http, next);
}


void
mpc_http_init()
{
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
        sw_uri
    } state;

    if (strncasecmp((char *)url, "http://", 7) != 0) {
        return MPC_ERROR;
    }

    mpc_url->port = 80;

    url += 7;
    state = 0;

    mpc_url->host.data = url;

    while (n--) {
        c = *url++;
        if (c == '\0') {
            break;
        }

        switch (state) {
        case sw_domain:
            if (c == ':') {
                mpc_url->host.len = url - 1 - mpc_url->host.data;
                state = sw_port;
                p = url;
                break;
            }

            if (c == '/') {
                mpc_url->host.len = url - 1 - mpc_url->host.data;
                state = sw_uri;
                mpc_url->uri.data = url - 1;
                break;
            }
            break;
        case sw_port:
            if (c == '/') {
                mpc_url->port = mpc_atoi(p, url - 1 - p);
                if (mpc_url->port < 1 || mpc_url->port > UINT16_MAX) {
                    return MPC_ERROR;
                }
                state = sw_uri;
                mpc_url->uri.data = url - 1;
                break;
            }
            break;
        case sw_uri:
            break;
        }
    }

    mpc_url->uri.len = url - 1 - mpc_url->uri.data;
    mpc_url->no_resolve = 0;

    return MPC_OK;
}


int
mpc_http_process_url(mpc_event_loop_t *el, mpc_url_t *mpc_url)
{
    if (mpc_url->no_resolve) {
        printf("do something\n");

    } else {
        mpc_gethostbyname(el, mpc_http_gethostbyname_cb, mpc_url->host.data,
                          mpc_url->host.len, AF_INET, (void *)mpc_url, NULL);
    }

    return MPC_OK;
}


static void
mpc_http_gethostbyname_cb(mpc_event_loop_t *el, int status,
    struct hostent *host, void *arg)
{
    if (status == MPC_RESOLVER_OK) {
        if (mpc_http_request(host->h_addr, el, (mpc_url_t *)arg) != MPC_OK) {
            mpc_log_stderr("failed");
        }
    } else {
        mpc_log_stderr("%s", ares_strerror(status));
    }
}


int
mpc_http_request(char *addr, mpc_event_loop_t *el, mpc_url_t *mpc_url)
{
    int          sockfd;
    mpc_http_t  *http;
    mpc_conn_t  *conn;
    mpc_buf_t   *rcv_buf, *snd_buf;

    conn = mpc_conn_get();
    if (conn == NULL) {
        mpc_log_stderr("mpc_conn_get failed");
        return MPC_ERROR;
    }

    rcv_buf = mpc_buf_get();
    if (rcv_buf == NULL) {
        mpc_log_stderr("mpc_buf_get failed");
        return MPC_ERROR;
    }
    mpc_buf_insert(&conn->rcv_buf_queue, rcv_buf);
    conn->rcv_buf = rcv_buf;

    snd_buf = mpc_buf_get();
    if (snd_buf == NULL) {
        mpc_log_stderr("mpc_buf_get failed");
        return MPC_ERROR;
    }
    mpc_buf_insert(&conn->snd_buf_queue, snd_buf);
    conn->snd_buf = snd_buf;

    snd_buf->last += snprintf((char *)snd_buf->start, mpc_buf_size(snd_buf),
                             "GET %.*s HTTP/1.1" CRLF
                             "Host: %.*s" CRLF
                             "Accept: *.*" CRLF
                             "UserAgent: %s" CRLF
                             "Connection: close" CRLF
                             CRLF,
                             (int)mpc_url->uri.len, mpc_url->uri.data,
                             (int)mpc_url->host.len, mpc_url->host.data,
                             MPC_VERSION);

    sockfd = mpc_net_tcp_connect(addr, mpc_url->port, MPC_NET_NONBLOCK);
    if (sockfd == MPC_ERROR) {
        mpc_log_stderr("mpc_net_tcp_connect failed");
        return MPC_ERROR;
    }

    conn->connecting = 1;
    conn->fd = sockfd;

    http = mpc_http_get();
    if (http == NULL) {
        mpc_log_stderr("mpc_http_get failed");
        return MPC_ERROR;
    }
    http->conn = conn;

    if (mpc_create_file_event(el, sockfd, MPC_WRITABLE,
                              mpc_http_connect_handler, (void *)http)
        == MPC_ERROR)
    {
        close(sockfd);
        mpc_log_stderr("mpc_event_create_file_event failed");
        return MPC_ERROR;
    }

    return MPC_OK;
}


static void
mpc_http_connect_handler(mpc_event_loop_t *el, int fd, void *data, int mask)
{
    mpc_http_t  *http = (mpc_http_t *)data;
    mpc_conn_t  *conn = http->conn;
    int          n;

    conn->connected = 1;
    conn->connecting = 0;

    n = mpc_conn_send(conn);

    if (n < 0) {
        mpc_log_stderr("%s\n", strerror(errno));
        return;
    }

    if (conn->done) {
        conn->done = 0;
        mpc_delete_file_event(el, fd, MPC_WRITABLE);

        if (mpc_create_file_event(el, fd, MPC_READABLE,
                                  mpc_http_process_status_line, (void *)http)
            == MPC_ERROR)
        {
            close(fd);
            return;
        }

    } else {
        if (mpc_create_file_event(el, fd, MPC_WRITABLE,
                                  mpc_http_request_snd_handler, (void *)http)
            == MPC_ERROR)
        {
            close(fd);
            return;
        }
    }
}


static void
mpc_http_request_snd_handler(mpc_event_loop_t *el, int fd, void *data, int mask)
{
    mpc_http_t  *http = (mpc_http_t *)data;
    mpc_conn_t  *conn = http->conn;
    int          n;

    n = mpc_conn_send(conn);

    if (n < 0) {
        mpc_log_stderr("send request failed");
        return;
    }

    if (conn->done) {
        conn->done = 0;
        mpc_delete_file_event(el, fd, MPC_WRITABLE);

        if (mpc_create_file_event(el, fd, MPC_READABLE,
                                  mpc_http_process_status_line, (void *)http)
            == MPC_ERROR)
        {
            close(fd);
            return;
        }
    }
}


static void
mpc_http_process_status_line(mpc_event_loop_t *el, int fd, void *data, 
    int mask)
{
    mpc_http_t  *http = (mpc_http_t *)data;
    mpc_conn_t  *conn = http->conn;
    int          n;
    int          rc;

    n = mpc_conn_recv(conn);
    if (n < 0) {
        mpc_log_stderr("recv response failed: (%d) %s", errno, strerror(errno));
        return;
    }

    if (n == 0) {
        return;
    }

    http->buf = STAILQ_FIRST(&http->conn->rcv_buf_queue);

    rc = mpc_http_parse_status_line(http);

    if (rc == MPC_ERROR) {
        mpc_log_stderr("parse status failed: %s", strerror(errno));
        return;

    } else if (rc == MPC_AGAIN) {
        return;
    }

    rc = mpc_http_process_headers(http);
    if (rc == MPC_ERROR) {
        mpc_log_stderr("parse status failed: %s", strerror(errno));
        return;
    } else if (rc == MPC_AGAIN) {
        if (mpc_create_file_event(el, fd, MPC_READABLE,
                                  mpc_http_process_headers_handler,
                                  (void *)http)
            == MPC_ERROR)
        {
            close(fd);
        }

        return;
    }

    rc = mpc_http_process_body(http);
    if (rc == MPC_ERROR) {
        mpc_log_stderr("parse status failed: %s", strerror(errno));
        return;
    } else if (rc == MPC_AGAIN) {
        return;
    }

    if (conn->eof) {
        conn->rcv_buf->last = conn->rcv_buf->start;
        return;
    }
}


static void
mpc_http_process_headers_handler(mpc_event_loop_t *el, int fd, void *data, 
    int mask)
{
    mpc_http_t  *http = (mpc_http_t *)data;
    int          rc;

    rc = mpc_http_process_headers(http);
    if (rc == MPC_ERROR) {
        mpc_log_stderr("parse status failed: %s", strerror(errno));
        return;

    } else if (rc == MPC_AGAIN) {
        return;
    }

    rc = mpc_http_process_body(http);
    if (rc == MPC_ERROR) {
        mpc_log_stderr("parse status failed: %s", strerror(errno));
        return;
    } else if (rc == MPC_AGAIN) {
        return;
    }

    if (http->conn->eof) {
        /* TODO */
        return;
    }
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
        while (buf->pos++ < buf->last) {
            ch = *(buf->pos - 1);

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
        while (buf->pos++ < buf->last) {
            ch = *(buf->pos - 1);

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
mpc_http_process_headers(mpc_http_t *http)
{
    int                  rc;
    mpc_http_header_t   *header;

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

            /* ugly code */
            if (header->name.len == sizeof("Content-Length") 
                && mpc_strncasecmp(header->name.data, "Content-Length",
                                   sizeof("Content-Length")))
            {
                http->content_length_n = mpc_atoi(header->value.data,
                                                  header->value.len);
                if (http->content_length_n == MPC_ERROR) {
                    return MPC_ERROR;
                }
            }

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
mpc_http_process_body(mpc_http_t *http)
{
    mpc_buf_t *buf = http->buf;

    while (buf) {
        while (buf->pos++ < buf->last) {
            http->content_length_received++;
        }

        if (buf->pos == buf->last) {
            buf = STAILQ_NEXT(buf, next);
            http->buf = buf;
        }
    }

    if (http->content_length_received == http->content_length_n) {
        return MPC_OK;
    }

    if (http->conn->eof) {
        mpc_log_stderr("unmatch content-length");
        return MPC_ERROR;
    }

    return MPC_AGAIN;
}
