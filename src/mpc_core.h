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


#ifndef __MPC_CORE_H_INCLUDED__
#define __MPC_CORE_H_INCLUDED__


#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <sched.h>
#include <pthread.h>
#include <execinfo.h>


typedef struct mpc_instance_s mpc_instance_t;
typedef struct mpc_stat_s mpc_stat_t;


#include <mpc_signal.h>
#include <mpc_queue.h>
#include <mpc_string.h>
#include <mpc_rbtree.h>
#include <mpc_log.h>
#include <mpc_array.h>
#include <mpc_alloc.h>
#include <mpc_util.h>
#include <mpc_event.h>
#include <mpc_net.h>
#include <mpc_url.h>
#include <mpc_buf.h>
#include <mpc_connection.h>
#include <mpc_http.h>
#include <mpc_stat.h>


#define MPC_VERSION_NUM         1           /* aabbbccc */
#define MPC_VERSION_STR         "0.0.1"
#define MPC_VERSION             "mpc/" MPC_VERSION_STR

#define MPC_DEFAULT_CONF_PATH   "conf/mpc.conf"
#define MPC_DEFAULT_PORT        17748
#define MPC_DEFAULT_CONCURRENCY 50
#define MPC_MAX_CONCURRENCY     10000
#define MPC_MAX_OPENFILES       32768

#define MPC_OK                  0
#define MPC_ERROR               -1
#define MPC_AGAIN               -2

#define MPC_NOTUSED(V)          ((void)V)
#define MPC_DO_NOTHING()        /* nothing */
#define MPC_NOT_REACHED()       assert(0)
#define MPC_BUG()               assert(0)

#define LF                      (uint8_t)10
#define CR                      (uint8_t)13
#define CRLF                    "\x0d\x0a"
#define CRLF_LEN                (sizeof(CRLF) - 1)

#define MPC_MAX(a, b)           ((a < b) ? (b) : (a))
#define MPC_MIN(a, b)           ((a > b) ? (b) : (a))

#define MPC_TEMP_BUF_SIZE       512
#define MPC_CONF_BUF_MAX_SIZE   1024
#define MPC_CRON_INTERVAL       50  /* miliseconds */


#ifdef WITH_DEBUG
#define ASSERT(x)               assert(x)  
#define SET_MAGIC(s, m)         (s)->magic = m
#else
#define ASSERT(x)               /* nothing */
#define SET_MAGIC(s, m)         /* nothing */
#endif


struct mpc_instance_s {
    char                *conf_filename;
    char                *input_filename;
    char                *addr;
    int                  port;
    int                  log_level;
    char                *log_file;
    mpc_event_loop_t    *el;
    int                  self_pipe[2];
    mpc_array_t         *urls;
    int                  concurrency;
    mpc_stat_t          *stat;
    struct sockaddr_in   dst_addr;
    mpc_http_hdr_t       http_hdr;
    uint32_t             http_count;
    unsigned             follow_location:1;
    unsigned             replay:1;
    unsigned             use_dst_addr;
};


void mpc_stop();
int mpc_core_init(mpc_instance_t *ins);
int mpc_core_run(mpc_instance_t *ins);
int mpc_core_deinit(mpc_instance_t *ins);


#endif /* __MPC_CORE_H_INCLUDED__ */
