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


#ifndef __MPC_STAT_H_INCLUDED__
#define __MPC_STAT_H_INCLUDED__


#define MPC_STAT_MAGIC      0x53544154      /* "STAT" */


struct mpc_stat_s {
#ifdef WITH_DEBUG
    uint32_t    magic;
#endif
    uint32_t    failed;
    uint32_t    ok;
    uint64_t    shortest;
    uint64_t    longest;
    uint64_t    bytes;
    uint64_t    total_time;
    uint64_t    start;
    uint64_t    stop;
};


#define mpc_stat_inc_bytes(s, b)        (s)->bytes += (b)
#define mpc_stat_inc_ok(s)              (s)->ok++
#define mpc_stat_inc_failed(s)          (s)->failed++
#define mpc_stat_inc_total_time(s, e)   (s)->total_time += (e)
#define mpc_stat_get_ok(s)              (s)->ok
#define mpc_stat_get_failed(s)          (s)->failed


mpc_stat_t *mpc_stat_create(void);
int mpc_stat_init(mpc_stat_t *mpc_stat);
void mpc_stat_destroy(mpc_stat_t *mpc_stat);
void mpc_stat_set_longest(mpc_stat_t *mpc_stat, uint64_t longest);
void mpc_stat_set_shortest(mpc_stat_t *mpc_stat, uint64_t shortest);
void mpc_stat_print(mpc_stat_t *mpc_stat);
int mpc_stat_result_record(int fd, mpc_stat_t *mpc_stat, char *mark);
int mpc_stat_result_create(const char *file);
int mpc_stat_result_close(int fd);


#endif /* __MPC_STAT_H_INCLUDED__ */
