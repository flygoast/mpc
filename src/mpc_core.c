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


#include <pthread.h>
#include <mpc_core.h>


static int mpc_self_pipe[2];

static void mpc_core_accept_handler(mpc_event_loop_t *mel, int fd, void *data,
    int mask);
static void mpc_core_notify_handler(mpc_event_loop_t *mel, int fd, void *data,
    int mask);
static void mpc_core_create_dispatch_thread(mpc_instance_t *ins);
static void *mpc_core_submit(void *arg);


static void
mpc_core_create_dispatch_thread(mpc_instance_t *ins)
{
    pthread_t       tid;
    pthread_attr_t  attr;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    pthread_create(&tid, &attr, mpc_core_submit, (void *)ins);
    pthread_attr_destroy(&attr);
}


int
mpc_core_run(mpc_instance_t *ins)
{
    int                listen_fd;
    mpc_event_loop_t  *el;

    mpc_url_init();

    el = mpc_create_event_loop(MPC_DEFAULT_EVENT_SIZE);
    if (el == NULL) {
        return MPC_ERROR;
    }

    if (pipe(mpc_self_pipe) < 0) {
        return MPC_ERROR;
    }

    mpc_net_nonblock(mpc_self_pipe[0]);

    if (mpc_create_file_event(el, mpc_self_pipe[0], MPC_READABLE, 
                              mpc_core_notify_handler, (void *)ins)
        == MPC_ERROR)
    {
        return MPC_ERROR;
    }


    listen_fd = mpc_net_tcp_server(ins->addr, ins->port);
    if (listen_fd == MPC_ERROR) {
        return MPC_ERROR;
    }

    if (mpc_create_file_event(el, listen_fd, MPC_READABLE, 
                              mpc_core_accept_handler, (void *)ins)
        == MPC_ERROR)
    {
        return MPC_ERROR;
    }

    mpc_core_create_dispatch_thread(ins);

    mpc_event_main(el);

    return MPC_OK;
}


static void
mpc_core_accept_handler(mpc_event_loop_t *mel, int fd, void *data, int mask)
{
    mpc_instance_t  *ins = (mpc_instance_t *)data;

    MPC_NOTUSED(mel);
    MPC_NOTUSED(mask);
    MPC_NOTUSED(data);

    if (fd == MPC_ERROR) {
        return;
    }

    mpc_log_stderr("hello accept from %s:%d", 
                   ins->addr ? ins->addr : "0.0.0.0", ins->port);
}


static void
mpc_core_notify_handler(mpc_event_loop_t *mel, int fd, void *data, int mask)
{
    int     n;
    char    buf[1024];

    for (;;) {
        n = read(fd, buf, 1024);
        buf[n] = '\0';
        if (n < 0) {
            break;
        }

        printf("%d\n", n);
    }
}


static int
mpc_core_notify()
{
    char c = 'x';
    return write(mpc_self_pipe[1], &c, 1);
}


static void *
mpc_core_submit(void *arg)
{
    mpc_instance_t *ins = (mpc_instance_t *)arg;

    if (ins->input_filename) {
        FILE      *fp;
        char      *ptr;
        char       buf[MPC_CONF_BUF_MAX_SIZE];
        int        len;
        mpc_url_t *mpc_url;
        
        if ((fp = fopen(ins->input_filename, "r")) == NULL) {
            mpc_log_stderr("fopen \"%s\" failed: %s", ins->input_filename,
                           strerror(errno));
            exit(1);
        }
            
        for (;;) {
            if (fgets(buf, MPC_CONF_BUF_MAX_SIZE, fp) == NULL) {
                break;
            }

            len = strlen(buf);

            while (len > 0 && (buf[len - 1]  == '\n' || buf[len - 1] == '\r' 
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

            /* TODO: check protocol here */
            mpc_url = mpc_url_get();

            if (mpc_core_notify() < 0) {
                mpc_log_stderr("notify failed");
                exit(1);
            }
        }

    } else {
        for (;;) {
            mpc_nanosleep(0.001);
        }
    }

    return NULL;
}
