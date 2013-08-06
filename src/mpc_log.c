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


static mpc_logger_t     mpc_logger;


static mpc_str_t err_levels[] = {
    mpc_string("emerg"),
    mpc_string("alert"),
    mpc_string("crit"),
    mpc_string("error"),
    mpc_string("warn"),
    mpc_string("notice"),
    mpc_string("info"),
    mpc_string("debug"),
    mpc_null_string,
};


int
mpc_log_init(int level, char *name)
{
    mpc_logger_t  *l = &mpc_logger;

    l->level = MPC_MAX(MPC_LOG_EMERG, MPC_MIN(level, MPC_LOG_DEBUG));
    l->name = name;
    if (name == NULL || !strlen(name)) {
        l->fd = STDERR_FILENO;
    } else {
        l->fd = open(name, O_WRONLY|O_APPEND|O_CREAT, 0644);
        if (l->fd < 0) {
            mpc_log_stderr(errno, "opening log file '%s' failed: %s", name,
                           strerror(errno));
            return -1;
        }
    }

    return 0;
}


void
mpc_log_deinit(void)
{
    mpc_logger_t *l = &mpc_logger;

    if (l->fd < 0 || l->fd == STDERR_FILENO) {
        return;
    }

    close(l->fd);
}


void
mpc_log_reopen(void)
{
    mpc_logger_t *l = &mpc_logger;

    if (l->fd != STDERR_FILENO) {
        close(l->fd);
        l->fd = open(l->name, O_WRONLY|O_APPEND|O_CREAT, 0644);
        if (l->fd < 0) {
            mpc_log_stderr(errno, "reopening log file '%s' failed, ignored",
                           l->name);
        }
    }
}


void
mpc_log_level_up(void)
{
    mpc_logger_t *l = &mpc_logger;

    if (l->level < MPC_LOG_DEBUG) {
        l->level++;
    }
}


void
mpc_log_level_down(void)
{
    mpc_logger_t *l = &mpc_logger;

    if (l->level > MPC_LOG_EMERG) {
        l->level--;
    }
}


void
mpc_log_level_set(int level)
{
    mpc_logger_t *l = &mpc_logger;

    l->level = MPC_MAX(MPC_LOG_EMERG, MPC_MIN(level, MPC_LOG_DEBUG));
}


int
mpc_log_get_level(char *log_level)
{
    mpc_str_t  *str;
    int         i, len; 

    len = strlen(log_level);

    for (str = err_levels, i = 0; str->len; str++, i++) {
        if (str->len != len) {
            continue;
        }

        if (mpc_strncasecmp(str->data, (uint8_t *)log_level, len) != 0) {
            continue;
        }

        return i;
    }

    return MPC_ERROR;
}


void
mpc_log_core(const char *file, int line, int level, int err, 
    const char *fmt, ...)
{
    mpc_logger_t    *l = &mpc_logger;
    uint8_t          errstr[MPC_LOG_MAX_LEN];
    uint8_t         *p, *last;
    va_list          args;
    struct tm        local;
    time_t           t;
    ssize_t          n;

    if (l->fd < 0 || level > l->level) {
        return;
    }

    last = errstr + MPC_LOG_MAX_LEN;
    p = errstr;

    *p++ = '[';

    t = time(NULL);
    localtime_r(&t, &local);
    asctime_r(&local, (char *)p);
    p += 24;

    p = mpc_slprintf(p, last, "] %s:%d [%V] ", file, line, &err_levels[level]);

    va_start(args, fmt);
    p = mpc_vslprintf(p, last, fmt, args);
    va_end(args);

    if (err) {
        p = mpc_log_errno(p, last, err);
    }

    if (p > last - 1) {
        p = last - 1;
    }

    *p++ = LF;

    n = write(l->fd, errstr, p - errstr);
    if (n < 0) {
        l->nerror++;
    }
}


uint8_t *
mpc_log_errno(uint8_t *buf, uint8_t *last, int err)
{
    if (buf > last - 50) {

        /* leave a space for an error code */

        buf = last - 50;
        *buf++ = '.';
        *buf++ = '.';
        *buf++ = '.';
    }

    buf = mpc_slprintf(buf, last, " (%d: %s", err, strerror(err));

    if (buf < last) {
        *buf++ = ')';
    }

    return buf;
}


void
mpc_log_stderr(int err, const char *fmt, ...)
{
    mpc_logger_t   *l = &mpc_logger;
    uint8_t         errstr[MPC_LOG_MAX_LEN];
    uint8_t        *p, *last;
    va_list         args;
    ssize_t         n;

    last = errstr + MPC_LOG_MAX_LEN;
    p = errstr + 5;

    mpc_memcpy(errstr, "mpc: ", 5);

    va_start(args, fmt);
    p = mpc_vslprintf(p, last, fmt, args);
    va_end(args);

    if (err) {
        p = mpc_log_errno(p, last, err);
    }

    if (p > last - 1) {
        p = last - 1;
    }

    *p++ = LF;

    n = write(STDERR_FILENO, errstr, p - errstr);
    if (n < 0) {
        l->nerror++;
    }
}
