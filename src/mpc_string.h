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

void mpc_strlow(uint8_t *dst, uint8_t *src, size_t n);


#define mpc_strncmp(s1, s2, n)  \
    strncmp((const char *)(s1), (const char *)(s2), n)

#define mpc_strcmp(s1, s2)      strcmp((const char *)(s1), (const char *)(s2))
#define mpc_strstr(s1, s2)      strstr((const char *)(s1), (const char *)(s2))
#define mpc_strlen(s)           strlen((const char *)(s))

#define mpc_strchr(s1, c)       strchr((char char *)s1, (int)c)


static inline uint8_t *
mpc_strlchr(uint8_t *p, uint8_t *last, uint8_t c) 
{
    while (p < last) {

        if (*p == c) {
            return p;
        }

        p++;
    }

    return NULL;
}


#define mpc_memzero(buf, n)     (void) memset(buf, 0, n)
#define mpc_memset(buf, c, n)   (void) memset(buf, c, n)

#define mpc_memcpy(dst, src, n) (void) memcpy(dst, src, n)
#define mpc_cpymem(dst, src, n) (((uint8_t *)memcpy(dst, src, n)) + (n))

#define mpc_memmove(dst, src, n)    (void) memmove(dst, src, n)
#define mpc_movemem(dst, src, n)    (((uint8_t *) memmove(dst, src, n)) + (n))

#define mpc_memcmp(s1, s2, n)   memcmp((const char *)s1, (const char *)s2, n)


uint8_t *mpc_cpystrn(uint8_t *dst, uint8_t *src, size_t n);
uint8_t *mpc_sprintf(uint8_t *buf, const char *fmt, ...);
uint8_t *mpc_snprintf(uint8_t *buf, size_t max, const char *fmt, ...);
uint8_t *mpc_slprintf(uint8_t *buf, uint8_t *last, const char *fmt, ...);
uint8_t *mpc_vslprintf(uint8_t *buf, uint8_t *last, const char *fmt,
    va_list args);
#define mpc_vsnprintf(buf, max, fmt, args)  \
    mpc_vslprintf(buf, buf + (max), fmt, args)

int mpc_strcasecmp(uint8_t *s1, uint8_t *s2);
int mpc_strncasecmp(uint8_t *s1, uint8_t *s2, size_t n);
uint8_t *mpc_strstrn(uint8_t *s1, char *s2, size_t n);
uint8_t *mpc_strcasestrn(uint8_t *s1, char *s2, size_t n);


#endif /* __MPC_STRING_H_INCLUDED__ */
