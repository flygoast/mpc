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


#ifndef __MPC_UTIL_H_INCLUDED__
#define __MPC_UTIL_H_INCLUDED__


#include <limits.h>


#define MPC_INT32_LEN   sizeof("-2147483648") - 1
#define MPC_INT64_LEN   sizeof("-9223372036854775808") - 1

#define MPC_MAX_INT32_VALUE     (uint32_t)0x7fffffff
#define MPC_MAX_UINT32_VALUE    (uint32_t)0xffffffff

#define MPC_MAX_INT64_VALUE     (uint64_t)0x7fffffffffffffffLL
#define MPC_MAX_UINT64_VALUE    (uint64_t)0xffffffffffffffffLL

/* True if negative values of the signed integer type T uses two's
 * complement, one's complement, or signed magnitude representation,
 * respectively. */
#define TYPE_TWOS_COMPLEMENT(t) ((t) ~(t)0 == (t)-1)
#define TYPE_ONES_COMPLEMENT(t) ((t) ~(t)0 == 0)
#define TYPE_SIGNED_MAGNITUDE(t) ((t) ~(t)0 < (t)-1)

#define TYPE_SIGNED(t)  (!((t)0 < (t)-1))
#define TYPE_MAXIMUM(t)             \
    ((t)(! TYPE_SIGNED(t)           \
        ? (t)-1                     \
        : ((((t)1 <<(sizeof(t) * CHAR_BIT - 2)) - 1) * 2 + 1)))
#define TYPE_MINIMUM(t)             \
    ((t)(! TYPE_SIGNED(t)           \
        ? (t)0                      \
        : TYPE_SIGNED_MAGNITUDE(t)  \
        ? ~(t)0                     \
        : ~ TYPE_MAXIMUM(t)))
#define BILLION         (1000 * 1000 * 1000)


int mpc_nanosleep(double seconds);
void mpc_add_milliseconds_to_now(int64_t ms_delta, int64_t *sec, int64_t *ms);
void mpc_get_time(int64_t *seconds, int64_t *milliseconds);
int mpc_atoi(uint8_t *line, size_t n);
int mpc_hextoi(uint8_t *line, size_t n);
uint8_t *mpc_hex_dump(uint8_t *dst, uint8_t *src, size_t len);
uint64_t time_ms(void);
uint64_t time_us(void);



#endif /* __MPC_UTIL_H_INCLUDED__ */
