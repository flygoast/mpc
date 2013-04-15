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


static uint8_t *mpc_sprintf_num(uint8_t *buf, uint8_t *last, uint64_t ui64,
    uint8_t zero, uint32_t hexadecimal, uint32_t width);


/* TBD */
int
mpc_str_cpy(mpc_str_t *dst, mpc_str_t *src)
{
    dst->data = (uint8_t *)strndup((const char *)src->data, src->len);
    if (dst->data == NULL) {
        return MPC_ERROR;
    }

    dst->len = src->len;
    dst->data[dst->len] = '\0';

    return MPC_OK;
}


void
mpc_strlow(uint8_t *dst, uint8_t *src, size_t n)
{
    while (n) {
        *dst = mpc_tolower(*src);
        dst++;
        src++;
        n--;
    }
}


uint8_t *
mpc_cpystrn(uint8_t *dst, uint8_t *src, size_t n)
{
    if (n == 0) {
        return dst;
    }

    while (--n) {
        *dst = *src;

        if (*dst == '\0') {
            return dst;
        }

        dst++;
        src++;
    }

    *dst = '\0';

    return dst;
}


/*
 * supported formats:
 *    %[0][width][x][X]O        off_t
 *    %[0][width]T              time_t
 *    %[0][width][u][x|X]z      ssize_t/size_t
 *    %[0][width][u][x|X]d      int/u_int
 *    %[0][width][u][x|X]l      long
 *    %[0][width][u][x|X]D      int32_t/uint32_t
 *    %[0][width][u][x|X]L      int64_t/uint64_t
 *    %[0][width][.width]f      double, max valid number fits to %18.15f
 *    %P                        pid_t
 *    %r                        rlim_t
 *    %p                        void *
 *    %V                        mpc_str_t *
 *    %s                        null-terminated string
 *    %*s                       length and string
 *    %Z                        '\0'
 *    %N                        '\n'
 *    %c                        char
 *    %%                        %
 *
 */


uint8_t *
mpc_sprintf(uint8_t *buf, const char *fmt, ...)
{
    uint8_t   *p;
    va_list    args;

    va_start(args, fmt);
    p = mpc_vslprintf(buf, (void *) -1, fmt, args);
    va_end(args);

    return p;
}


uint8_t *
mpc_snprintf(uint8_t *buf, size_t max, const char *fmt, ...)
{
    uint8_t   *p;
    va_list    args;

    va_start(args, fmt);
    p = mpc_vslprintf(buf, buf + max, fmt, args);
    va_end(args);

    return p;
}


uint8_t *
mpc_slprintf(uint8_t *buf, uint8_t *last, const char *fmt, ...)
{
    uint8_t   *p;
    va_list    args;

    va_start(args, fmt);
    p = mpc_vslprintf(buf, last, fmt, args);
    va_end(args);

    return p;
}


uint8_t *
mpc_vslprintf(uint8_t *buf, uint8_t *last, const char *fmt, va_list args)
{
    uint8_t                *p, zero;
    int                     d;
    double                  f;
    size_t                  len, slen;
    int64_t                 i64;
    uint64_t                ui64, frac;
    uint32_t                width, sign, hex, max_width, frac_width, scale, n;
    mpc_str_t              *v;

    while (*fmt && buf < last) {

        /*
         * "buf < last" means that we could copy at least one character:
         * the plain character, "%%", "%c", and minus without the checking
         */

        if (*fmt == '%') {

            i64 = 0;
            ui64 = 0;

            zero = (uint8_t) ((*++fmt == '0') ? '0' : ' ');
            width = 0;
            sign = 1;
            hex = 0;
            max_width = 0;
            frac_width = 0;
            slen = (size_t) -1;

            while (*fmt >= '0' && *fmt <= '9') {
                width = width * 10 + *fmt++ - '0';
            }


            for ( ;; ) {
                switch (*fmt) {

                case 'u':
                    sign = 0;
                    fmt++;
                    continue;

                case 'm':
                    max_width = 1;
                    fmt++;
                    continue;

                case 'X':
                    hex = 2;
                    sign = 0;
                    fmt++;
                    continue;

                case 'x':
                    hex = 1;
                    sign = 0;
                    fmt++;
                    continue;

                case '.':
                    fmt++;

                    while (*fmt >= '0' && *fmt <= '9') {
                        frac_width = frac_width * 10 + *fmt++ - '0';
                    }

                    break;

                case '*':
                    slen = va_arg(args, size_t);
                    fmt++;
                    continue;

                default:
                    break;
                }

                break;
            }


            switch (*fmt) {

            case 'V':
                v = va_arg(args, mpc_str_t *);

                len = MPC_MIN(((size_t) (last - buf)), v->len);
                buf = mpc_cpymem(buf, v->data, len);
                fmt++;

                continue;

            case 's':
                p = va_arg(args, uint8_t *);

                if (slen == (size_t) -1) {
                    while (*p && buf < last) {
                        *buf++ = *p++;
                    }

                } else {
                    len = MPC_MIN(((size_t) (last - buf)), slen);
                    buf = mpc_cpymem(buf, p, len);
                }

                fmt++;

                continue;

            case 'O':
                i64 = (int64_t) va_arg(args, off_t);
                sign = 1;
                break;

            case 'T':
                i64 = (int64_t) va_arg(args, time_t);
                sign = 1;
                break;

            case 'z':
                if (sign) {
                    i64 = (int64_t) va_arg(args, ssize_t);
                } else {
                    ui64 = (uint64_t) va_arg(args, size_t);
                }
                break;

            case 'd':
                if (sign) {
                    i64 = (int64_t) va_arg(args, int);
                } else {
                    ui64 = (uint64_t) va_arg(args, u_int);
                }
                break;

            case 'l':
                if (sign) {
                    i64 = (int64_t) va_arg(args, long);
                } else {
                    ui64 = (uint64_t) va_arg(args, u_long);
                }
                break;

            case 'D':
                if (sign) {
                    i64 = (int64_t) va_arg(args, int32_t);
                } else {
                    ui64 = (uint64_t) va_arg(args, uint32_t);
                }
                break;

            case 'L':
                if (sign) {
                    i64 = va_arg(args, int64_t);
                } else {
                    ui64 = va_arg(args, uint64_t);
                }
                break;

            case 'f':
                f = va_arg(args, double);

                if (f < 0) {
                    *buf++ = '-';
                    f = -f;
                }

                ui64 = (int64_t) f;
                frac = 0;

                if (frac_width) {

                    scale = 1;
                    for (n = frac_width; n; n--) {
                        scale *= 10;
                    }

                    frac = (uint64_t) ((f - (double) ui64) * scale + 0.5);

                    if (frac == scale) {
                        ui64++;
                        frac = 0;
                    }
                }

                buf = mpc_sprintf_num(buf, last, ui64, zero, 0, width);

                if (frac_width) {
                    if (buf < last) {
                        *buf++ = '.';
                    }

                    buf = mpc_sprintf_num(buf, last, frac, '0', 0, frac_width);
                }

                fmt++;

                continue;

            case 'r':
                i64 = (int64_t) va_arg(args, rlim_t);
                sign = 1;
                break;

            case 'p':
                ui64 = (uintptr_t) va_arg(args, void *);
                hex = 2;
                sign = 0;
                zero = '0';
                width = sizeof(void *) * 2;
                break;

            case 'c':
                d = va_arg(args, int);
                *buf++ = (uint8_t) (d & 0xff);
                fmt++;

                continue;

            case 'Z':
                *buf++ = '\0';
                fmt++;

                continue;

            case 'N':
                *buf++ = LF;
                fmt++;

                continue;

            case '%':
                *buf++ = '%';
                fmt++;

                continue;

            default:
                *buf++ = *fmt++;

                continue;
            }

            if (sign) {
                if (i64 < 0) {
                    *buf++ = '-';
                    ui64 = (uint64_t) -i64;

                } else {
                    ui64 = (uint64_t) i64;
                }
            }

            buf = mpc_sprintf_num(buf, last, ui64, zero, hex, width);

            fmt++;

        } else {
            *buf++ = *fmt++;
        }
    }

    return buf;
}


static uint8_t *
mpc_sprintf_num(uint8_t *buf, uint8_t *last, uint64_t ui64, uint8_t zero,
    uint32_t hexadecimal, uint32_t width)
{
    uint8_t         *p, temp[MPC_INT64_LEN + 1];
    size_t           len;
    uint32_t         ui32;
    static uint8_t   hex[] = "0123456789abcdef";
    static uint8_t   HEX[] = "0123456789ABCDEF";

    p = temp + MPC_INT64_LEN;

    if (hexadecimal == 0) {

        if (ui64 <= MPC_MAX_UINT32_VALUE) {

            /*
             * To divide 64-bit numbers and to find remainders
             * on the x86 platform gcc and icc call the libc functions
             * [u]divdi3() and [u]moddi3(), they call another function
             * in its turn.  On FreeBSD it is the qdivrem() function,
             * its source code is about 170 lines of the code.
             * The glibc counterpart is about 150 lines of the code.
             *
             * For 32-bit numbers and some divisors gcc and icc use
             * a inlined multiplication and shifts.  For example,
             * unsigned "i32 / 10" is compiled to
             *
             *     (i32 * 0xCCCCCCCD) >> 35
             */

            ui32 = (uint32_t) ui64;

            do {
                *--p = (uint8_t) (ui32 % 10 + '0');
            } while (ui32 /= 10);

        } else {
            do {
                *--p = (uint8_t) (ui64 % 10 + '0');
            } while (ui64 /= 10);
        }

    } else if (hexadecimal == 1) {

        do {

            /* the "(uint32_t)" cast disables the BCC's warning */
            *--p = hex[(uint32_t) (ui64 & 0xf)];

        } while (ui64 >>= 4);

    } else { /* hexadecimal == 2 */

        do {

            /* the "(uint32_t)" cast disables the BCC's warning */
            *--p = HEX[(uint32_t) (ui64 & 0xf)];

        } while (ui64 >>= 4);
    }

    /* zero or space padding */

    len = (temp + MPC_INT64_LEN) - p;

    while (len++ < width && buf < last) {
        *buf++ = zero;
    }

    /* number safe copy */

    len = (temp + MPC_INT64_LEN) - p;

    if (buf + len > last) {
        len = last - buf;
    }

    return mpc_cpymem(buf, p, len);
}


/*
 * We use mpc_strcasecmp()/mpc_strncasecmp() for 7-bit ASCII strings only,
 * and implement our own mpc_strcasecmp()/mpc_strncasecmp()
 * to avoid libc locale overhead.
 */
int
mpc_strcasecmp(uint8_t *s1, uint8_t *s2)
{
    int  c1, c2;

    for ( ;; ) {
        c1 = (int) *s1++;
        c2 = (int) *s2++;

        c1 = (c1 >= 'A' && c1 <= 'Z') ? (c1 | 0x20) : c1;
        c2 = (c2 >= 'A' && c2 <= 'Z') ? (c2 | 0x20) : c2;

        if (c1 == c2) {

            if (c1) {
                continue;
            }

            return 0;
        }

        return c1 - c2;
    }
}


int
mpc_strncasecmp(uint8_t *s1, uint8_t *s2, size_t n)
{
    int  c1, c2;

    while (n) {
        c1 = (int) *s1++;
        c2 = (int) *s2++;

        c1 = (c1 >= 'A' && c1 <= 'Z') ? (c1 | 0x20) : c1;
        c2 = (c2 >= 'A' && c2 <= 'Z') ? (c2 | 0x20) : c2;

        if (c1 == c2) {

            if (c1) {
                n--;
                continue;
            }

            return 0;
        }

        return c1 - c2;
    }

    return 0;
}


uint8_t *
mpc_strnstr(uint8_t *s1, char *s2, size_t len)
{
    uint8_t  c1, c2;
    size_t   n;

    c2 = *(uint8_t *) s2++;

    n = mpc_strlen(s2);

    do {
        do {
            if (len-- == 0) {
                return NULL;
            }

            c1 = *s1++;

            if (c1 == 0) {
                return NULL;
            }

        } while (c1 != c2);

        if (n > len) {
            return NULL;
        }

    } while (mpc_strncmp(s1, (uint8_t *) s2, n) != 0);

    return --s1;
}


/*
 * mpc_strstrn() and mpc_strcasestrn() are intended to search for static
 * substring with known length in null-terminated string. The argument n
 * must be length of the second substring - 1.
 */

uint8_t *
mpc_strstrn(uint8_t *s1, char *s2, size_t n)
{
    uint8_t  c1, c2;

    c2 = *(uint8_t *) s2++;

    do {
        do {
            c1 = *s1++;

            if (c1 == 0) {
                return NULL;
            }

        } while (c1 != c2);

    } while (mpc_strncmp(s1, (uint8_t *) s2, n) != 0);

    return --s1;
}


uint8_t *
mpc_strcasestrn(uint8_t *s1, char *s2, size_t n)
{
    uint32_t  c1, c2;

    c2 = (uint32_t) *s2++;
    c2 = (c2 >= 'A' && c2 <= 'Z') ? (c2 | 0x20) : c2;

    do {
        do {
            c1 = (uint32_t) *s1++;

            if (c1 == 0) {
                return NULL;
            }

            c1 = (c1 >= 'A' && c1 <= 'Z') ? (c1 | 0x20) : c1;

        } while (c1 != c2);

    } while (mpc_strncasecmp(s1, (uint8_t *) s2, n) != 0);

    return --s1;
}
