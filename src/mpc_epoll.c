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


#include <sys/epoll.h>


typedef struct {
    int                  epfd;
    struct epoll_event  *events;
} mpc_event_api_state_t;


static int
mpc_event_api_create(mpc_event_loop_t *el)
{
    mpc_event_api_state_t *state = mpc_alloc(sizeof(*state));
    if (state == NULL) {
        mpc_log_err(errno, "mpc_alloc failed");
        return MPC_ERROR;
    }

    state->events = mpc_alloc(sizeof(struct epoll_event) * el->setsize);
    if (state->events == NULL) {
        mpc_free(state);
        mpc_log_err(errno, "mpc_alloc failed");
        return MPC_ERROR;
    }

    /* 1024 is just a hint for the kernel */
    state->epfd = epoll_create(1024); 
    if (state->epfd == -1) {
        mpc_log_err(errno, "epoll_create failed");
        mpc_free(state->events);
        mpc_free(state);
        return MPC_ERROR;
    }

    el->api_data = state;

    return MPC_OK;
}


static void
mpc_event_api_free(mpc_event_loop_t *el)
{
    mpc_event_api_state_t *state = el->api_data;

    close(state->epfd);
    mpc_free(state->events);
    mpc_free(state);
}


static int
mpc_event_api_add_event(mpc_event_loop_t *el, int fd, int mask)
{
    int                    op;
    struct epoll_event     ee;
    mpc_event_api_state_t *state = el->api_data;

    /* If the fd was already monitored for some event, we need a MOD
       operation. Otherwise we need an ADD operation. */
    op = el->events[fd].mask == MPC_NONE ? EPOLL_CTL_ADD : EPOLL_CTL_MOD;
    ee.events = 0;
    mask |= el->events[fd].mask; /* Merge old events. */
    if (mask & MPC_READABLE) {
        ee.events |= EPOLLIN;
    }

    if (mask & MPC_WRITABLE) {
        ee.events |= EPOLLOUT;
    }
    ee.events |= EPOLLET;
    ee.data.u64 = 0;
    ee.data.fd = fd;
    if (epoll_ctl(state->epfd, op, fd, &ee) == -1) {
        mpc_log_err(errno, "epoll_ctl %s fd(%d) failed", 
                    op == EPOLL_CTL_ADD ? "ADD" : "MOD", fd);
        return MPC_ERROR;
    }

    return MPC_OK;
}


static void
mpc_event_api_del_event(mpc_event_loop_t *el, int fd, int delmask)
{
    mpc_event_api_state_t  *state = el->api_data;
    struct epoll_event      ee;
    int                     mask = el->events[fd].mask & (~delmask);

    ee.events = 0;
    if (mask & MPC_READABLE) {
        ee.events |= EPOLLIN;
    }

    if (mask & MPC_WRITABLE) {
        ee.events |= EPOLLOUT;
    }

    ee.events |= EPOLLET;
    ee.data.u64 = 0;
    ee.data.fd = fd;
    if (mask != MPC_NONE) {
        if (epoll_ctl(state->epfd, EPOLL_CTL_MOD, fd, &ee) < 0) {
            mpc_log_alert(errno, "epoll_ctl MOD fd(%d) failed", fd);
        }

    } else {
        /* Note, kernel < 2.6.9 requires a non null event pointer even
           for EPOLL_CTL_DEL. */
        if (epoll_ctl(state->epfd, EPOLL_CTL_DEL, fd, &ee) < 0) {
            mpc_log_alert(errno, "epoll_ctl DEL fd(%d) failed", fd);
        }
    }
}


static int
mpc_event_api_poll(mpc_event_loop_t *el, struct timeval *tvp)
{
    int                     j, mask;
    int                     retval, numevents = 0;
    struct epoll_event     *e;
    mpc_event_api_state_t  *state = el->api_data;

    retval = epoll_wait(state->epfd, state->events, el->setsize,
                        tvp ? (tvp->tv_sec * 1000 + tvp->tv_usec / 1000) : -1);
    if (retval > 0) {
        numevents = retval;
        for (j = 0; j < numevents; ++j) {
            mask = 0;
            e = state->events + j;
            if (e->events & EPOLLIN) {
                mask |= MPC_READABLE;
            }

            if (e->events & EPOLLOUT) {
                mask |= MPC_WRITABLE;
            }

            if (e->events & EPOLLERR) {
                mpc_log_warn(errno, "EPOLLERR occured");
                mask |= MPC_WRITABLE;
            }

            if (e->events & EPOLLHUP) {
                mpc_log_warn(errno, "EPOLLHUP occured");
                mask |= MPC_WRITABLE;
            }

            el->fired[j].fd = e->data.fd;
            el->fired[j].mask = mask;
        }
    }

    return numevents;
}


static char *
mpc_event_api_name(void)
{
    return "epoll";
}
