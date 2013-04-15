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


static struct timespec
dtotimespec(double sec)
{
    double min_representable = TYPE_MINIMUM(time_t);
    double max_representable = 
           ((TYPE_MAXIMUM(time_t) * (double)BILLION + (BILLION - 1)) / BILLION);
    struct timespec r;

    if (!(min_representable < sec)) {
        r.tv_sec = TYPE_MINIMUM(time_t);
        r.tv_nsec = 0;
    } else if (!(sec < max_representable)) {
        r.tv_sec = TYPE_MAXIMUM(time_t);
        r.tv_nsec = BILLION - 1;
    } else {
        time_t s = (time_t)sec;
        double frac = BILLION * (sec - s);
        int64_t ns = frac;
        ns += ns < frac;
        s += ns / BILLION;
        ns %= BILLION;

        if (ns < 0) {
            s--;
            ns += BILLION;
        }
        r.tv_sec = s;
        r.tv_nsec = ns;
    }

    return r;
}


int
mpc_nanosleep(double seconds)
{
    struct timespec ts_sleep = dtotimespec(seconds);

    for ( ;; ) {
        errno = 0;
        if (nanosleep(&ts_sleep, NULL) == 0) {
            break;
        }

        if (errno != EINTR && errno != 0) {
            return MPC_ERROR;
        }
    }

    return MPC_OK;
}


void
mpc_get_time(int64_t *seconds, int64_t *milliseconds)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);
    *seconds = tv.tv_sec;
    *milliseconds = tv.tv_usec / 1000;
}


void
mpc_add_milliseconds_to_now(int64_t milliseconds, 
    int64_t *sec, int64_t *ms)
{
    int64_t cur_sec, cur_ms, when_sec, when_ms;

    mpc_get_time(&cur_sec, &cur_ms);

    when_sec = cur_sec + milliseconds / 1000;
    when_ms = cur_ms + milliseconds % 1000;

    /* cur_ms < 1000, when_ms < 2000, so just one time is enough. */
    if (when_ms >= 1000) {
        when_sec++;
        when_ms -= 1000;
    }

    *sec = when_sec;
    *ms = when_ms;
}


int 
mpc_atoi(uint8_t *line, size_t n)
{
    int value;

    if (n == 0) {
        return MPC_ERROR;
    }

    for (value = 0; n--; line++) {
        if (*line < '0' || *line > '9') {
            return MPC_ERROR;
        }

        value = value * 10 + (*line - '0');
    }

    if (value < 0) {
        return MPC_ERROR;
    }

    return value;
}


int
mpc_hextoi(uint8_t *line, size_t n)
{
    uint8_t     c, ch;
    int         value;

    if (n == 0) {
        return MPC_ERROR;
    }

    for (value = 0; n--; line++) {
        ch = *line;

        if (ch >= '0' && ch <= '9') {
            value = value * 16 + (ch - '0');
            continue;
        }

        c = (uint8_t) (ch | 0x20);

        if (c >= 'a' && c <= 'f') {
            value = value * 16 + (c - 'a' + 10);
            continue;
        }

        return MPC_ERROR;
    }

    if (value < 0) {
        return MPC_ERROR;

    } else {
        return value;
    }
}


uint8_t *
mpc_hex_dump(uint8_t *dst, uint8_t *src, size_t len)
{
    static uint8_t  hex[] = "0123456789abcdef";

    while (len--) {
        *dst++ = hex[*src >> 4];
        *dst++ = hex[*src++ & 0xf];
    }

    return dst;
}
