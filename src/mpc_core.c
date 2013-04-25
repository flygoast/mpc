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


static void mpc_core_process_accept(mpc_event_loop_t *el, int fd, void *data,
    int mask);
static void mpc_core_process_notify(mpc_event_loop_t *el, int fd, void *data,
    int mask);
static int mpc_core_process_cron(mpc_event_loop_t *el, int64_t id, void *data);
static void mpc_core_create_submit_thread(mpc_instance_t *ins);
static void *mpc_core_submit(void *arg);
static char *mpc_core_getline(char *buf, int size, FILE *fp);
static int mpc_core_put_url(void *elem, void *data);
static int mpc_core_notify(mpc_instance_t *ins);
static void mpc_core_notify_end(mpc_instance_t *ins);


static int start_bench = 0;
static uint32_t mpc_task_total = 0;
static uint32_t mpc_task_processed = 0;


static void
mpc_core_create_submit_thread(mpc_instance_t *ins)
{
    pthread_t       tid;
    pthread_attr_t  attr;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    pthread_create(&tid, &attr, mpc_core_submit, (void *)ins);
    pthread_attr_destroy(&attr);
}


int
mpc_core_init(mpc_instance_t *ins)
{
    mpc_log_init(ins->log_level, ins->log_file);

    mpc_buf_init(MPC_BUF_MAX_NFREE);
    mpc_conn_init(MPC_CONN_MAX_NFREE);

    if (ins->input_filename) {
        mpc_url_init(MPC_URL_MAX_NFREE);
    }
    mpc_http_init(MPC_HTTP_MAX_NFREE);

    mpc_signal_init();

    ins->el = mpc_create_event_loop(MPC_DEFAULT_EVENT_SIZE);
    if (ins->el == NULL) {
        mpc_log_emerg(0, "create event loop failed");
        return MPC_ERROR;
    }

    srandom(time(NULL));

    return MPC_OK;
}


int
mpc_core_deinit(mpc_instance_t *ins)
{
    mpc_http_deinit();

    mpc_buf_deinit();
    mpc_conn_deinit();

    if (ins->el) {
        mpc_free_event_loop(ins->el);
    }

    if (ins->urls) {
        mpc_array_each(ins->urls, mpc_core_put_url, NULL);
        mpc_array_destroy(ins->urls);
    }

    if (ins->input_filename) {
        mpc_url_deinit();
    }

    mpc_signal_deinit();

    mpc_log_deinit();

    return MPC_OK;
}


int
mpc_core_run(mpc_instance_t *ins)
{
    int  listen_fd;

    listen_fd = mpc_net_tcp_server(ins->addr, ins->port);
    if (listen_fd == MPC_ERROR) {
        return MPC_ERROR;
    }

    if (mpc_create_file_event(ins->el, listen_fd, MPC_READABLE, 
                              mpc_core_process_accept, (void *)ins)
        == MPC_ERROR)
    {
        return MPC_ERROR;
    }

    if (mpc_create_time_event(ins->el, MPC_CRON_INTERVAL, 
                              mpc_core_process_cron, (void *)ins, NULL) 
        != MPC_OK)
    {
        mpc_log_stderr(0, "create time event failed");
        return MPC_ERROR;
    }

    if (ins->input_filename) {
        if (pipe(ins->self_pipe) < 0) {
            return MPC_ERROR;
        }

        mpc_net_nonblock(ins->self_pipe[0]);

        if (mpc_create_file_event(ins->el, ins->self_pipe[0], MPC_READABLE, 
                                  mpc_core_process_notify, (void *)ins)
            == MPC_ERROR)
        {
            return MPC_ERROR;
        }
    
        mpc_core_create_submit_thread(ins);
    }

    mpc_event_main(ins->el);

    if (ins->el->exit_code != MPC_OK) {
        return MPC_ERROR;
    }

    return MPC_OK;
}


static void
mpc_core_process_accept(mpc_event_loop_t *el, int fd, void *data, int mask)
{
    mpc_instance_t  *ins = (mpc_instance_t *)data;

    MPC_NOTUSED(el);
    MPC_NOTUSED(mask);

    ASSERT(fd > 0);

    mpc_log_stderr(0, "Connection from %s:%d", 
                   ins->addr ? ins->addr : "0.0.0.0", ins->port);
}


static void
mpc_core_process_notify(mpc_event_loop_t *el, int fd, void *data, int mask)
{
    int              n;
    char             buf[MPC_TEMP_BUF_SIZE];
    mpc_url_t       *mpc_url;
    mpc_instance_t  *ins = (mpc_instance_t *)data;
    mpc_http_t      *mpc_http;
    uint32_t         count;

    for (;;) {

        n = read(fd, buf, MPC_TEMP_BUF_SIZE);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }

            if (errno != EAGAIN) {
                mpc_log_err(errno, "read pipe (%d) failed", fd);
                mpc_delete_file_event(el, fd, MPC_READABLE);
                mpc_event_stop(el, MPC_ERROR);
                return;
            }
            break;
        }

        if (n == 0) {
            //mpc_log_err(0, "pipe write end abnormal closed");
            if (ins->stat->stop == 0) {
                ins->stat->stop = time_us();
            }

            mpc_delete_file_event(el, fd, MPC_READABLE);
            mpc_event_stop(el, 0);
            return;
        }

        if (ins->replay) {
            if (ins->stat->start == 0) {
                printf("start mpc\n");
                ins->stat->start = time_us();
            }

            while (mpc_http_get_used() < ins->concurrency) {
                mpc_url = mpc_url_task_get();
                if (mpc_url == NULL) {
                    break;
                }
    
                mpc_http = mpc_http_get();
                if (mpc_http == NULL) {
                    mpc_log_emerg(0, "oom when get http");
                    exit(1);
                }
    
                mpc_task_processed++;

                mpc_http->ins = ins;
                mpc_http->url = mpc_url;
    
                mpc_log_debug(0, "receive http url(%d), "
                                 "host: \"%V\" uri: \"%V\"",
                              mpc_url->url_id, &mpc_url->host, &mpc_url->uri);
    
                if (mpc_http_process_request(mpc_http) != MPC_OK) {
                    mpc_log_err(0, "process url \"http://%V%V\" failed, "
                                   "ignored",
                                &mpc_url->host, &mpc_url->uri);
                    mpc_url_put(mpc_url);
                    mpc_http_put(mpc_http);
                }
            }

            count = __sync_fetch_and_add(&mpc_task_total, 0);

            if (count == mpc_task_processed && mpc_http_get_used() == 0) {
                mpc_core_notify_end(ins);
            }
    
        } else {
            start_bench = 1;
            printf("start mpc\n");
            ins->stat->start = time_us();
        }
    }
}


static int
mpc_core_put_url(void *elem, void *data)
{
    mpc_url_t  **mpc_url_p = (mpc_url_t **)elem;

    MPC_NOTUSED(data);

    (*mpc_url_p)->no_put = 0;
    mpc_url_put(*mpc_url_p);

    return MPC_OK;
}


static int
mpc_core_process_cron(mpc_event_loop_t *el, int64_t id, void *data)
{
    mpc_instance_t   *ins = (mpc_instance_t *)data;

    if (ins->replay) {
        mpc_core_notify(ins);

    } else {
        if (start_bench) {
            mpc_http_create_missing_requests(ins);
        }
    }

    return MPC_CRON_INTERVAL;
}


static int
mpc_core_notify(mpc_instance_t *ins)
{
    char c = 'x';
    return write(ins->self_pipe[1], &c, 1);
}


static void
mpc_core_notify_end(mpc_instance_t *ins)
{
    close(ins->self_pipe[1]);
    ins->self_pipe[1] = -1;
}


static void *
mpc_core_submit(void *arg)
{
    mpc_instance_t *ins = (mpc_instance_t *)arg;

    if (ins->input_filename) {
        FILE        *fp;
        char        *ptr;
        char         buf[MPC_CONF_BUF_MAX_SIZE];
        mpc_url_t   *mpc_url;
        mpc_url_t  **mpc_url_p;
        int          len;
        uint8_t     *p, *last;
        uint32_t     n = 0;
        
        if ((fp = fopen(ins->input_filename, "r")) == NULL) {
            mpc_log_stderr(errno, "fopen \"%s\" failed",
                           ins->input_filename);
            exit(1);
        }

        if (ins->replay) {
            for (;;) {
                ptr = mpc_core_getline(buf, MPC_CONF_BUF_MAX_SIZE, fp);
    
                if (ptr == NULL) {
                    break;
                }
    
                mpc_url = mpc_url_get();
    
                if (mpc_url == NULL) {
                    mpc_log_emerg(errno, "oom!");
                    exit(1);
                }
    
                len = strlen(ptr);
                last = mpc_url->buf + mpc_url->buf_size;
    
                p = mpc_slprintf(mpc_url->buf, last, "%s", ptr);
                if (p == last) {
                    mpc_url_put(mpc_url);
                    mpc_log_err(0, "url buf size (%d) too small for \"%s\", "
                                   "ignored",
                                   mpc_url->buf_size, ptr);
                    continue;
                }
    
                if (mpc_http_parse_url(mpc_url->buf, len, mpc_url) != MPC_OK) {
                    mpc_log_err(0, "parse http url \"%s\" failed, ignored",
                                ptr);
                    mpc_url_put(mpc_url);
                    continue;
                }
    
                mpc_log_debug(0, "parse url (%d), host: \"%V\" uri: \"%V\"",
                              mpc_url->url_id, &mpc_url->host, &mpc_url->uri);
    
                mpc_url_task_insert(mpc_url);
    
                if (mpc_core_notify(ins) < 0) {
                    mpc_log_emerg(errno, "write pipe (%d) failed", 
                                  ins->self_pipe[1]);
                    exit(1);
                }
                n++;
                sched_yield();
            }

            __sync_add_and_fetch(&mpc_task_total, n);
    
        } else {
            ins->urls = mpc_array_create(500, sizeof(mpc_url_t *));
            if (ins->urls == NULL) {
                mpc_log_emerg(errno, "oom!");
                exit(1);
            }

            for (;;) {

                ptr = mpc_core_getline(buf, MPC_CONF_BUF_MAX_SIZE, fp);
    
                if (ptr == NULL) {
                    break;
                }
    
                mpc_url_p = mpc_array_push(ins->urls);
                if (mpc_url_p == NULL) {
                    mpc_log_emerg(errno, "oom!");
                    exit(1);
                }

                mpc_url = mpc_url_get();
   
                len = strlen(ptr);
                last = mpc_url->buf + mpc_url->buf_size;
    
                p = mpc_slprintf(mpc_url->buf, last, "%s", ptr);
                if (p == last) {
                    mpc_url_put(mpc_url);
                    mpc_array_pop(ins->urls);
                    mpc_log_err(0, "url buf size (%d) too small for \"%s\", "
                                   "ignored",
                                   mpc_url->buf_size, ptr);
                    continue;
                }
    
                if (mpc_http_parse_url(mpc_url->buf, len, mpc_url) != MPC_OK) {
                    mpc_url_put(mpc_url);
                    mpc_array_pop(ins->urls);
                    mpc_log_err(0, "parse http url \"%s\" failed, ignored",
                                ptr);
                    continue;
                }
    
                mpc_log_debug(0, "parse url (%d), host: \"%V\" uri: \"%V\"",
                              mpc_url->url_id, &mpc_url->host, &mpc_url->uri);
                mpc_url->no_put = 1;
                *mpc_url_p = mpc_url;
            }

            if (ins->urls->nelem == 0) {
                mpc_log_stderr(0, "no url in vector");
                exit(1);
            }

            if (mpc_core_notify(ins) < 0) {
                mpc_log_emerg(errno, "write pipe (%d) failed", 
                              ins->self_pipe[1]);
                exit(1);
            }

            sched_yield();
        }

        fclose(fp);

    } else {
        for (;;) {
            mpc_nanosleep(0.001);
        }
    }

    return NULL;
}


static char * 
mpc_core_getline(char *buf, int size, FILE *fp)
{
    int    len;
    char  *ptr;

    for (;;) {
        if (fgets(buf, size, fp) == NULL) {
            return NULL;
        }
    
        len = strlen(buf);
    
        while (len > 0 && (buf[len - 1]  == LF || buf[len - 1] == CR
                           || buf[len - 1] == '\t' || buf[len - 1] == ' '))
        {
            buf[len - 1] = '\0';
            len--;
        }
    
        if (len == 0) {  /* skip empty line */
            continue;
        }
    
        ptr = buf;
    
        while (*ptr != '\0' && (*ptr == '\t' || *ptr == ' ')) {
            ptr++;
        }
    
        if (*ptr == '#') { /* skip comment */
            continue;
        }

        return ptr;
    }
}
