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


static uint32_t         mpc_buf_nfree;          /* # free mpc_buf */
static mpc_buf_hdr_t    mpc_buf_free_queue;     /* free mpc_buf queue */
static size_t           mpc_buf_chunk_size;
static size_t           mpc_buf_offset;


static mpc_buf_t *
mpc_buf_get_internal(void)
{
    mpc_buf_t   *mpc_buf;
    uint8_t     *buf;

    if (!STAILQ_EMPTY(&mpc_buf_free_queue)) {
        mpc_buf = STAILQ_FIRST(&mpc_buf_free_queue);
        mpc_buf_nfree--;
        STAILQ_REMOVE_HEAD(&mpc_buf_free_queue, next);
        ASSERT(mpc_buf->magic == MPC_BUF_MAGIC);
        goto done;
    }

    buf = mpc_alloc(mpc_buf_chunk_size);
    if (buf == NULL) {
        return NULL;
    }

    /*
     * mpc_buf header is at the tail end of the mpc_buf. This enables us to 
     * catch buffer overrun early by asserting on the magic value during get
     * or put operations
     *
     *   <------------- mpc_buf_chunk_size ------------->
     *   +----------------------------------------------+
     *   |     mpc_buf data         |   mpc_buf header  |
     *   |   (mpc_buf_offset)       |   (mpc_buf_t)     |
     *   +-----------------------------------------------+
     *   ^           ^        ^     ^^
     *   |           |        |     ||
     *   \           |        |     |\
     *    start      \        |     | end (one byte past valid bound)
     *                pos     |     \
     *                        \      mpc_buf
     *                        last (one byte past valid byte)
     *
     */

    mpc_buf = (mpc_buf_t *)(buf + mpc_buf_offset);
    SET_MAGIC(mpc_buf, MPC_BUF_MAGIC);

done:
    STAILQ_NEXT(mpc_buf, next) = NULL;
    return mpc_buf;
}


mpc_buf_t *
mpc_buf_get(void)
{
    mpc_buf_t  *mpc_buf;
    uint8_t    *buf;

    mpc_buf = mpc_buf_get_internal();
    if (mpc_buf == NULL) {
        return NULL;
    }

    buf = (uint8_t *)mpc_buf - mpc_buf_offset;
    mpc_buf->start = buf;
    mpc_buf->end = (uint8_t *)mpc_buf;

    ASSERT(mpc_buf->end - mpc_buf->start == mpc_buf_offset);
    ASSERT(mpc_buf->start < mpc_buf->end);

    mpc_buf->pos = mpc_buf->start;
    mpc_buf->last = mpc_buf->start;

    return mpc_buf;
}


static void
mpc_buf_free(mpc_buf_t *mpc_buf)
{
    uint8_t *buf;

    ASSERT(STAILQ_NEXT(mpc_buf, next) == NULL);
    ASSERT(mpc_buf->magic == MPC_BUF_MAGIC);

    buf = (uint8_t *)mpc_buf - mpc_buf_offset;
    mpc_free(buf);
}


void
mpc_buf_put(mpc_buf_t *mpc_buf)
{
    ASSERT(STAILQ_NEXT(mpc_buf, next) == NULL);
    ASSERT(mpc_buf->magic == MPC_BUF_MAGIC);

    mpc_buf_nfree++;
    STAILQ_INSERT_HEAD(&mpc_buf_free_queue, mpc_buf, next);
}


/*
 * Insert mpc_buf at the tail of the mpc_buf_hdr Q.
 */
void
mpc_buf_insert(mpc_buf_hdr_t *mpc_hdr, mpc_buf_t *mpc_buf)
{
    STAILQ_INSERT_TAIL(mpc_hdr, mpc_buf, next);
}


/*
 * Remove mpc_buf from mpc_buf_hdr Q.
 */
void
mpc_buf_remove(mpc_buf_hdr_t *mpc_hdr, mpc_buf_t *mpc_buf)
{
    STAILQ_REMOVE(mpc_hdr, mpc_buf, mpc_buf_s, next);
    STAILQ_NEXT(mpc_buf, next) = NULL;
}


/*
 * Copy n bytes from memory area pos to mpc_buf.
 *
 * The memory areas should not overlap and the mpc_buf should have enough
 * space for n bytes.
 */
void
mpc_buf_copy(mpc_buf_t *mpc_buf, uint8_t *pos, size_t n)
{
    if (n == 0) {
        return;
    }

    /* mpc_buf has space for n bytes. */
    assert(!mpc_buf_full(mpc_buf) && n <= mpc_buf_size(mpc_buf));

    /* no overlapping copy */
    assert(pos < mpc_buf->start || pos >= mpc_buf->end);

    mpc_memcpy(mpc_buf->last, pos, n);
    mpc_buf->last += n;
}


/*
 * Split mpc_buf h into h and t by copying data from h to t. Before the
 * copy, we invoke a precopy handler cb that will copy a predefined 
 * string to the head of t.
 *
 * Return new mpc_buf t, if the split was successful.
 */
mpc_buf_t *
mpc_buf_split(mpc_buf_hdr_t *h, uint8_t *pos, mpc_buf_copy_pt cb, void *cbarg)
{
    mpc_buf_t   *mpc_buf, *nbuf;
    size_t       size;

    ASSERT(!STAILQ_EMPTY(h));

    mpc_buf = STAILQ_LAST(h, mpc_buf_s, next);

    ASSERT(pos >= mpc_buf->pos && pos <= mpc_buf->last);

    nbuf = mpc_buf_get();
    if (nbuf == NULL) {
        return NULL;
    }

    if (cb != NULL) {
        /* precopy nbuf */
        cb(nbuf, cbarg);
    }

    /* copy data from mpc_buf to nbuf */
    size = (size_t)(mpc_buf->last - pos);
    mpc_buf_copy(nbuf, pos, size);

    /* adjust mpc_buf */
    mpc_buf->last = pos;

    return nbuf;
}


void
mpc_buf_init(void)
{
    mpc_buf_nfree = 0;
    STAILQ_INIT(&mpc_buf_free_queue);

    mpc_buf_chunk_size = MPC_BUF_SIZE;
    mpc_buf_offset = mpc_buf_chunk_size - MPC_BUF_HSIZE;
}


void
mpc_buf_deinit(void)
{
    while (!STAILQ_EMPTY(&mpc_buf_free_queue)) {
        mpc_buf_t *mpc_buf = STAILQ_FIRST(&mpc_buf_free_queue);
        mpc_buf_remove(&mpc_buf_free_queue, mpc_buf);
        mpc_buf_free(mpc_buf);
        mpc_buf_nfree--;
    }

    ASSERT(mpc_buf_nfree == 0);
}
