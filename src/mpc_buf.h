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


#ifndef __MPC_BUF_H_INCLUDED__
#define __MPC_BUF_H_INCLUDED__


#define MPC_BUF_MAGIC       0x4d425546      /* "MBUF" */
#define MPC_BUF_MIN_SIZE    512
#define MPC_BUF_MAX_SIZE    65536
#define MPC_BUF_SIZE        16384
#define MPC_BUF_HSIZE       sizeof(mpc_buf_t)
#define MPC_BUF_MAX_NFREE   128

#define mpc_buf_empty(buf)  (((buf)->pos == (buf)->last) ? 1 : 0)
#define mpc_buf_full(buf)   (((buf)->last == (buf)->end) ? 1 : 0)


typedef struct mpc_buf_s mpc_buf_t;
typedef struct mpc_buf_hdr_s mpc_buf_hdr_t;
typedef void (*mpc_buf_copy_pt)(mpc_buf_t *, void *);


struct mpc_buf_s {
#ifdef WITH_DEBUG
    uint32_t                   magic;
#endif
    STAILQ_ENTRY(mpc_buf_s)    next;        /* next mpc_buf */
    uint8_t                   *pos;         /* read marker */
    uint8_t                   *last;        /* write marker */
    uint8_t                   *start;       /* start of buffer */
    uint8_t                   *end;         /* end of buffer */
};


STAILQ_HEAD(mpc_buf_hdr_s, mpc_buf_s);


/*
 * Rewind the mpc_buf by discarding any of the read or unread data that it
 * might hold.
 */
#define mpc_buf_rewind(mpc_buf)     \
    mpc_buf->pos = mpc_buf->start; mpc_buf->last = mpc_buf->start

/*
 * Return the length of data in mpc_buf. Mpc_buf cannot contail more than
 * 2^32 bytes (4G) .
 */
#define mpc_buf_length(mpc_buf)   (uint32_t) ((mpc_buf)->last - (mpc_buf)->pos)

/*
 * Return the remaining space size for any new data in mpc_buf. Mpc_buf cannot
 * contain more than 2^32 bytes (4G).
 */
#define mpc_buf_size(mpc_buf)     (uint32_t) ((mpc_buf)->end - (mpc_buf)->last)

/*
 * Return the maximum available space size for data in any mpc_buf. Mpc_buf
 * cannot contain more than 2^32 bytes (4G).
 */
#define mpc_buf_data_size         mpc_buf_offset


void mpc_buf_init(uint32_t max_nfree);
void mpc_buf_deinit(void);
void mpc_buf_queue_rewind(mpc_buf_hdr_t *mpc_hdr);
void mpc_buf_put(mpc_buf_t *mpc_buf);
void mpc_buf_insert(mpc_buf_hdr_t *mpc_hdr, mpc_buf_t *mpc_buf);
void mpc_buf_remove(mpc_buf_hdr_t *mpc_hdr, mpc_buf_t *mpc_buf);
void mpc_buf_copy(mpc_buf_t *mpc_buf, uint8_t *pos, size_t n);
mpc_buf_t *mpc_buf_get(void);
mpc_buf_t *mpc_buf_split(mpc_buf_hdr_t *h, uint8_t *pos, mpc_buf_copy_pt cb,
    void *cbarg);


#endif /* __MPC_BUF_H_INCLUDED__ */
