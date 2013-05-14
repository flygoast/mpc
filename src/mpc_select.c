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


#include <string.h>


typedef struct ae_api_state {
    fd_set rfds, wfds;
    /* We need to have a copy of the fd sets as it's not safe to
       reuse FD sets after select(). */
    fd_set _rfds, _wfds;
} ae_api_state;

static int ae_api_create(ae_event_loop *el) {
    ae_api_state *state = malloc(sizeof(*state));
    if (!state) {
        return -1;
    }
    FD_ZERO(&state->rfds);
    FD_ZERO(&state->wfds);
    el->api_data = state;
    return 0;
}

static void ae_api_free(ae_event_loop *el) {
    if (el && el->api_data) {
        free(el->api_data);
    }
}

static int ae_api_add_event(ae_event_loop *el, int fd, int mask) {
    ae_api_state *state = el->api_data;

    if (mask & AE_READABLE) {
        FD_SET(fd, &state->rfds);
    }

    if (mask & AE_WRITABLE) {
        FD_SET(fd, &state->wfds);
    }
    return 0;
}

static void ae_api_del_event(ae_event_loop *el, int fd, int mask) {
    ae_api_state *state = el->api_data;
    if (mask & AE_READABLE) {
        FD_CLR(fd, &state->rfds);
    }

    if (mask & AE_WRITABLE) {
        FD_CLR(fd, &state->wfds);
    }
}

static int ae_api_poll(ae_event_loop *el, struct timeval *tvp) {
    ae_api_state *state = el->api_data;
    int ret, j, numevents = 0;
    memcpy(&state->_rfds, &state->rfds, sizeof(fd_set));
    memcpy(&state->_wfds, &state->wfds, sizeof(fd_set));

    ret = select(el->maxfd + 1, &state->_rfds, &state->_wfds, NULL, tvp);
    if (ret > 0) {
        for (j = 0; j <= el->maxfd; ++j) {
            int mask = 0;
            ae_file_event *fe = &el->events[j];
            if (fe->mask == AE_NONE) {
                continue;
            }
            if (fe->mask & AE_READABLE && FD_ISSET(j, &state->_rfds)) {
                mask |= AE_READABLE;
            }
            if (fe->mask & AE_WRITABLE && FD_ISSET(j, &state->_wfds)) {
                mask |= AE_WRITABLE;
            }
            el->fired[numevents].fd = j;
            el->fired[numevents].mask = mask;
            numevents++;
        }
    }
    return numevents;
}

static char *ae_api_name(void) {
    return "select";
}
