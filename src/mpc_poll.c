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


#include <poll.h>
#include <string.h>

typedef struct ae_api_state {
    int nfds;
    struct pollfd *events;
} ae_api_state;

static int ae_api_create(ae_event_loop *el) {
    ae_api_state *state = calloc(1, sizeof(ae_api_state));

    if (!state) {
        return -1;
    }

    state->events = malloc(sizeof(struct pollfd) * el->setsize);
    if (state->events) {
        free(state);
        return -1;
    }
    memset(state->events, 0, sizeof(struct pollfd) * el->setsize);
    el->api_data = state;
    return 0;
}

static void ae_api_free(ae_event_loop *el) {
    if (el && el->api_data) {
        free(el->api_data->events);
        free(el->api_data);
    }
}

static int ae_api_add_event(ae_event_loop *el, int fd, int mask) {
    ae_api_state *state = el->api_data;
    int i = 0;
    struct pollfd *pfd = NULL;
    for (i = 0; i < state->nfds; i++) {
        if (fd == state->events[i].fd) {
            pfd = &state->events[i];
        }
    }

    if (!pfd) {
        pfd = &state->events[state->nfds++];
    }

    if (mask & AE_READABLE) {
        pfd->fd = fd;
        pfd->events |= POLLIN;
    }

    if (mask & AE_WRITABLE) {
        pfd->fd = fd;
        pfd->events |= POLLOUT;
    }
    return 0;
}

static void ae_api_del_event(ae_event_loop *el, int fd, int mask) {
    ae_api_state *state = el->api_data;
    int i = 0;
    struct pollfd *pfd = NULL;
    for (i = 0; i < state->nfds; i++) {
        if (fd == state->events[i].fd) {
            pfd = &state->events[i];
        }
    }

    if (!pfd) {
        return;
    }

    if (mask & AE_READABLE) {
        pfd->events &= ~POLLIN;
    }

    if (mask & AE_WRITABLE) {
        pfd->events &= ~POLLOUT;
    }

    if (pfd->events == 0) {
        pfd->fd = -1; /* ignore the element of pollfd array */
    }
}

static int ae_api_poll(ae_event_loop *el, struct timeval *tvp) {
    ae_api_state *state = el->api_data;
    int retval, numevents = 0;
    retval = poll(state->events, state->nfds, 
            tvp ? (tvp->tv_sec * 1000 + tvp->tv_usec / 1000) : -1);
    if (retval > 0) {
        int i = 0;
        numevents = retval;
        for (i = 0; i < numevents; ++i) {
            int mask = 0;
            struct pollfd *pfd = state->events + i;
            if (pfd->fd < 0) {
                continue; /* Skip the ignored element. */
            }
            if (pfd->revents & POLLIN) {
                mask |= AE_READABLE;
            }

            if (pfd->revents & POLLOUT) {
                mask |= AE_WRITABLE;
            }
            el->fired[i].fd = pfd->fd;
            el->fired[i].mask = mask;
        }
    }
    return numevents;
}

static char *ae_api_name(void) {
    return "poll";
}
