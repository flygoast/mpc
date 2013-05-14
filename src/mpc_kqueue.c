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


#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>


typedef struct {
    int             kqfd;
    struct kevent  *events;
} mpc_event_api_state_t;


static int
mpc_event_api_create(mpc_event_loop_t *el)
{
    mpc_event_api_state_t *state = mpc_alloc(sizeof(*state));
    if (state == NULL) {
        return MPC_ERROR;
    }

    state->events = mpc_alloc(sizeof(struct kevent) * el->setsize);
    if (state->events == NULL) {
        mpc_free(state);
        return MPC_ERROR;
    }
    
    state->kqfd = kqueue();
    if (state->kqfd == -1) {
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
    close(state->kqfd);
    mpc_free(state->events);
    mpc_free(state);
}


static int
mpc_event_api_add_event(mpc_event_loop_t *el, int fd, int mask)
{
    mpc_event_api_state_t  *state = el->api_data;
    struct kevent           ke;

    if (mask & MPC_READABLE) {
        EV_SET(&ke, fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
        if (kevent(state->kqfd, &ke, 1, NULL, 0, NULL) == -1) {
            return MPC_ERROR;
        }
    }

    if (mask & MPC_WRITABLE) {
        EV_SET(&ke, fd, EVFILT_WRITE, EV_ADD, 0, 0, NULL);
        if (kevent(state->kqfd, &ke, 1, NULL, 0, NULL) == -1) {
            return MPC_ERROR;
        }
    }

    return MPC_OK;
}


static void
mpc_event_api_del_event(mpc_event_loop_t *el, int fd, int mask)
{
    mpc_event_api_state_t  *state = el->api_data;
    struct kevent           ke;

    if (mask & MPC_READABLE) {
        EV_SET(&ke, fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
        kevent(state->kqfd, &ke, 1, NULL, 0, NULL);
    }

    if (mask & MPC_WRITABLE) {
        EV_SET(&ke, fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
        kevent(state->kqfd, &ke, 1, NULL, 0, NULL);
    }
}


static int
mpc_event_api_poll(mpc_event_loop_t *el, struct timeval *tvp)
{
    mpc_event_api_state_t  *state = el->api_data;
    int                     j, retval, numevents = 0;
    int                     mask = 0;
    struct timespec         timeout;
    struct kevent          *e;

    if (tvp != NULL) {
        timeout.tv_sec = tvp->tv_sec;
        timeout.tv_nsec = tvp->tv_usec * 1000;
        retval = kevent(state->kqfd, NULL, 0, state->events, el->setsize, 
                        &timeout);

    } else {
        retval = kevent(state->kqfd, NULL, 0, state->events, el->setsize,
                        NULL);
    }

    if (retval > 0) {
        numevents = retval;
        for (j = 0; j < numevents; ++j) {
            e = state->events + j;
            if (e->filter == EVFILT_READ) {
                mask |= MPC_READABLE;
            }

            if (e->filter == EVFILT_WRITE) {
                mask |= MPC_WRITABLE;
            }

            el->fired[j].fd = e->ident;
            el->fired[j].mask = mask;
        }
    }

    return numevents;
}


static char *
mpc_event_api_name(void)
{
    return "kqueue";
}
