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


static uint32_t         mpc_url_nfree;          /* # free mpc_url_t */
static mpc_url_hdr_t    mpc_url_free_queue;     /* free mpc_url_t queue */


mpc_url_t *
mpc_url_get(void)
{
    mpc_url_t  *mpc_url;

    if (!STAILQ_EMPTY(&mpc_url_free_queue)) {
        mpc_url = STAILQ_FIRST(&mpc_url_free_queue);
        mpc_url_nfree--;
        STAILQ_REMOVE_HEAD(&mpc_url_free_queue, next);
        ASSERT(mpc_url->magic == MPC_URL_MAGIC);
        goto done;
    }

    mpc_url = (mpc_url_t *)mpc_calloc(sizeof(mpc_url_t), 1);
    if (mpc_url == NULL) {
        return NULL;
    }
    SET_MAGIC(mpc_url, MPC_URL_MAGIC);

done:
    STAILQ_NEXT(mpc_url, next) = NULL;
    return mpc_url;
}


static void
mpc_url_free(mpc_url_t *mpc_url)
{
    ASSERT(STAILQ_NEXT(mpc_url, next) == NULL);
    ASSERT(mpc_url->magic == MPC_URL_MAGIC);

    mpc_free(mpc_url);
}


void
mpc_url_put(mpc_url_t *mpc_url)
{
    ASSERT(STAILQ_NEXT(mpc_url, next) == NULL);
    ASSERT(mpc_url->magic == MPC_URL_MAGIC);

    mpc_url_nfree++;
    STAILQ_INSERT_HEAD(&mpc_url_free_queue, mpc_url, next);
}


void
mpc_url_insert(mpc_url_hdr_t *mpc_url_hdr, mpc_url_t *mpc_url)
{
    STAILQ_INSERT_TAIL(mpc_url_hdr, mpc_url, next);
}


void
mpc_url_remove(mpc_url_hdr_t *mpc_url_hdr, mpc_url_t *mpc_url)
{
    STAILQ_REMOVE(mpc_url_hdr, mpc_url, mpc_url_s, next);
    STAILQ_NEXT(mpc_url, next) = NULL;
}


void
mpc_url_init(void)
{
    mpc_url_nfree = 0;
    STAILQ_INIT(&mpc_url_free_queue);
}


void
mpc_url_deinit(void)
{
    mpc_url_t *mpc_url;

    while (!STAILQ_EMPTY(&mpc_url_free_queue)) {
        mpc_url = STAILQ_FIRST(&mpc_url_free_queue);
        mpc_url_remove(&mpc_url_free_queue, mpc_url);
        mpc_url_free(mpc_url);
        mpc_url_nfree--;
    }

    ASSERT(mpc_url_nfree == 0);
}
