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


#ifndef __MPC_STRING_H_INCLUDED__
#define __MPC_STRING_H_INCLUDED__

typedef struct  {
    size_t     len;
    uint8_t   *data;
} mpc_str_t;


#define mpc_string(str)     { sizeof(str) - 1, (uint8_t *) str }
#define mpc_null_string     { 0, NULL }
#define mpc_str_set(str, text)  \
    (str)->len = sizeof(text) - 1; (str)->data = (uint8_t *) text
#define mpc_str_null(str)   (str)->len = 0; (str)->data = NULL

#define mpc_tolower(c)      (uint8_t) ((c >= 'A' && c <= 'Z') ? (c | 0x20) : c)
#define mpc_toupper(c)      (uint8_t) ((c >= 'a' && c <= 'z') ? (c & ~0x20) : c)

#define mpc_str_empty(str)  ((str)->len == 0 ? 1 : 0)
#define mpc_str_dup(dst, src)   \
    mpc_str_cpy(dst, src, (src)->len)


#define mpc_memcpy(d, c, n)     memcpy(d, c, n)
#define mpc_memmove(d, c, n)    memmove(d, c, n)
#define mpc_memchr(d, c, n)     memchr(d, c, n)
#define mpc_strlen(s)           strlen((char *)(s))
#define mpc_strcmp(s1, s2)      strcmp((char *)(s1), (char *)(s2))
#define mpc_strncmp(s1, s2, n)  strncmp((char *)(s1), (char *)(s2), n)
#define mpc_strncasecmp(s1, s2, n)  \
    strncasecmp((char *)(s1), (char *)(s2), n)
#define mpc_strdup(s)           strdup((char *)(s))
#define mpc_strndup(s, n)       strndup((char *)(s), n) 
#define mpc_strlchr(s, l, c)    \
    _mpc_strlchr((uint8_t *)(s), (uint8_t *)(l), (uint8_t)(c))


static inline uint8_t *
_mpc_strlchr(uint8_t *p, uint8_t *last, uint8_t c) 
{
    while (p < last) {
        if (*p == c) {
            return p;
        }

        p++;
    }

    return NULL;
}


#endif /* __MPC_STRING_H_INCLUDED__ */
