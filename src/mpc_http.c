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


static void mpc_http_gethostbyname_cb(mpc_event_loop_t *el, int status,
    struct hostent *host, void *arg);
static void mpc_http_connect_handler(mpc_event_loop_t *el, int fd, void *data,
    int mask);
static int mpc_http_response_parse(mpc_conn_t *conn);
static void mpc_http_response_rcv_handler(mpc_event_loop_t *el, int fd,
    void *data, int mask);
static void mpc_http_request_snd_handler(mpc_event_loop_t *el, int fd,
    void *data, int mask);


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
                mpc_url->uri.data = url;
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
                mpc_url->uri.data = url;
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
    mpc_conn_t  *conn;
    mpc_buf_t   *rcv_buf, *snd_buf;

    conn = mpc_conn_get();
    if (conn == NULL) {
        return MPC_ERROR;
    }

    rcv_buf = mpc_buf_get();
    if (rcv_buf == NULL) {
        return MPC_ERROR;
    }
    mpc_buf_insert(&conn->rcv_buf_queue, rcv_buf);
    conn->rcv_buf = rcv_buf;

    snd_buf = mpc_buf_get();
    if (snd_buf == NULL) {
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
        return MPC_ERROR;
    }

    conn->connecting = 1;
    conn->fd = sockfd;

    if (mpc_create_file_event(el, sockfd, MPC_WRITABLE,
                              mpc_http_connect_handler, (void *)conn)
        == MPC_ERROR)
    {
        close(sockfd);
        return MPC_ERROR;
    }

    return MPC_OK;
}


static void
mpc_http_connect_handler(mpc_event_loop_t *el, int fd, void *data, int mask)
{
    mpc_conn_t  *conn = (mpc_conn_t *)data;
    int          n;

    conn->connected = 1;

    n = mpc_net_write(fd, conn->snd_buf->pos, 
                      conn->snd_buf->last - conn->snd_buf->pos);
    conn->snd_buf->pos += n;

    if (conn->snd_buf->last == conn->snd_buf->pos) {
        mpc_delete_file_event(el, fd, MPC_WRITABLE);

        if (mpc_create_file_event(el, fd, MPC_READABLE,
                                  mpc_http_response_rcv_handler, (void *)conn)
            == MPC_ERROR)
        {
            close(fd);
            return;
        }

    } else {
        if (mpc_create_file_event(el, fd, MPC_WRITABLE,
                                  mpc_http_request_snd_handler, (void *)conn)
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
    mpc_conn_t  *conn = (mpc_conn_t *)data;
    int          n;

    do {
        n = mpc_net_write(fd, conn->snd_buf->pos, 
                          conn->snd_buf->last - conn->snd_buf->pos);
    
        if (n < 0) {
            if (errno == EAGAIN) {
                break;
            }
            mpc_log_stderr("%s", strerror(errno));
        }

        conn->snd_buf->pos += n;

    } while (n > 0);

    if (conn->snd_buf->last == conn->snd_buf->pos) {
        mpc_delete_file_event(el, fd, MPC_WRITABLE);

        if (mpc_create_file_event(el, fd, MPC_READABLE,
                                  mpc_http_response_rcv_handler, (void *)conn)
            == MPC_ERROR)
        {
            close(fd);
            return;
        }
    }
}


static void
mpc_http_response_rcv_handler(mpc_event_loop_t *el, int fd, void *data, 
    int mask)
{
    mpc_conn_t  *conn = (mpc_conn_t *)data;
    int          n;

    do {
        n = mpc_net_read(fd, conn->rcv_buf->last, 
                         conn->rcv_buf->end - conn->rcv_buf->last);
        conn->rcv_buf->last += n;

        if (n < 0) {
            if (errno == EAGAIN) {
                break;
            }
            mpc_log_stderr("%s", strerror(errno));
        } else if (n == 0) {
            mpc_delete_file_event(el, fd, MPC_READABLE);
            close(fd);
        }
    } while (n > 0);

    /* TODO */
    
    mpc_http_response_parse(conn);
}


int
mpc_http_response_parse(mpc_conn_t *conn)
{

    return MPC_OK;
}
