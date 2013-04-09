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

static void mpc_log_internal(const char *file, int line, int panic, 
    const char *fmt, ...);


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
            mpc_log_stderr("opening log file '%s' failed: %s", name,
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
            mpc_log_stderr("reopening log file '%s' failed, ignored: %s",
                           l->name, strerror(errno));
        }
    }
}


void
mpc_log_level_up(void)
{
    mpc_logger_t *l = &mpc_logger;

    if (l->level < MPC_LOG_DEBUG) {
        l->level++;
        mpc_loga("up log level to %d", l->level);
    }
}


void
mpc_log_level_down(void)
{
    mpc_logger_t *l = &mpc_logger;

    if (l->level > MPC_LOG_EMERG) {
        l->level--;
        mpc_loga("set log level to %d", l->level);
    }
}


void
mpc_log_level_set(int level)
{
    mpc_logger_t *l = &mpc_logger;

    l->level = MPC_MAX(MPC_LOG_EMERG, MPC_MIN(level, MPC_LOG_DEBUG));
    mpc_loga("set log level to %d", l->level);
}


static void
mpc_log_internal(const char *file, int line, int panic, const char *fmt, ...)
{
    mpc_logger_t    *l = &mpc_logger;
    int              len, size, errno_save;
    char             buf[MPC_LOG_MAX_LEN], *timestr;
    va_list          args;
    struct tm       *local;
    time_t           t;
    ssize_t          n;

    if (l->fd < 0) {
        return;
    }

    errno_save = errno;
    len = 0;                    /* length of output buffer */
    size = MPC_LOG_MAX_LEN;     /* size of output buffer */

    t = time(NULL);
    local = localtime(&t);
    timestr = asctime(local);

    len += snprintf(buf + len, size - len, "[%.*s] %s:%d ",
                    (int)(strlen(timestr) - 1), timestr, file, line);
    va_start(args, fmt);
    len += vsnprintf(buf + len, size - len, fmt, args);
    va_end(args);

    buf[len++] = '\n';

    n = write(l->fd, buf, len);
    if (n < 0) {
        l->nerror++;
    }

    errno = errno_save;

    if (panic) {
        abort();
    }
}


void
mpc_log_stderr(const char *fmt, ...)
{
    mpc_logger_t   *l = &mpc_logger;
    int             len, size, errno_save;
    char            buf[4 * MPC_LOG_MAX_LEN];
    va_list         args;
    ssize_t         n;

    errno_save = errno;
    len = 0;                /* length of output buffer */
    size = 4 * MPC_LOG_MAX_LEN; /* size of output buffer */

    va_start(args, fmt);
    len += vsnprintf(buf, size, fmt, args);
    va_end(args);

    buf[len++] = '\n';

    n = write(STDERR_FILENO, buf, len);
    if (n < 0) {
        l->nerror++;
    }

    errno = errno_save;
}
