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


#ifndef __MPC_LOG_H_INCLUDED__
#define __MPC_LOG_H_INCLUDED__


typedef struct {
    char    *name;      /* log file name */
    int      level;     /* log level */
    int      fd;        /* log file descriptor */
    int      nerror;    /* # log error */
} mpc_logger_t;


#define MPC_LOG_EMERG       0       /* system in unusable */
#define MPC_LOG_ALERT       1       /* action must be taken immediately */
#define MPC_LOG_CRIT        2       /* critical conditions */
#define MPC_LOG_ERR         3       /* error condition */
#define MPC_LOG_WARN        4       /* warning conditions */
#define MPC_LOG_NOTICE      5       /* normal buf significant condition (default) */
#define MPC_LOG_INFO        6       /* informational */
#define MPC_LOG_DEBUG       7       /* debug messages */


#define MPC_LOG_MAX_LEN     2048    /* max length of log message */


#define mpc_log_stderr(...) do {        \
    log_stderr_internal(__VA_ARGS__);   \
} while (0)


#define mpc_loga(...) do {                                  \
    mpc_log_internal(__FILE__, __LINE__, 0, __VA_ARGS__);   \
} while (0)


#endif /* __MPC_LOG_H_INCLUDED__ */
