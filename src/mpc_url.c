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
static uint32_t         mpc_url_max_nfree;      /* max # free mpc_url_t */

static uint32_t         mpc_url_ntask;
static mpc_url_hdr_t    mpc_url_task_queue;

static pthread_mutex_t  mutex_free = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t  mutex_task = PTHREAD_MUTEX_INITIALIZER;


uint32_t
mpc_url_free_count(void)
{
    uint32_t value;

    pthread_mutex_lock(&mutex_free);
    value = mpc_url_nfree;
    pthread_mutex_unlock(&mutex_free);

    return value;
}


uint32_t
mpc_url_task_count(void)
{
    uint32_t value;

    pthread_mutex_lock(&mutex_task);
    value = mpc_url_ntask;
    pthread_mutex_unlock(&mutex_task);

    return value;
}


mpc_url_t *
mpc_url_task_get(void)
{
    mpc_url_t  *mpc_url;

    pthread_mutex_lock(&mutex_task);

    if (!STAILQ_EMPTY(&mpc_url_task_queue)) {
        mpc_url = STAILQ_FIRST(&mpc_url_task_queue);
        mpc_url_ntask--;
        STAILQ_REMOVE_HEAD(&mpc_url_task_queue, next);
        ASSERT(mpc_url->magic == MPC_URL_MAGIC);
        STAILQ_NEXT(mpc_url, next) = NULL;
        mpc_log_debug(0, "get task url(%d), total %d, host: \"%V\" uri: \"%V\"",
                      mpc_url->url_id, mpc_url_ntask,
                      &mpc_url->host, &mpc_url->uri);
        pthread_mutex_unlock(&mutex_task);

        return mpc_url;
    }

    pthread_mutex_unlock(&mutex_task);

    return NULL;
}


void
mpc_url_task_insert(mpc_url_t *mpc_url)
{
    pthread_mutex_lock(&mutex_task);

    STAILQ_INSERT_TAIL(&mpc_url_task_queue, mpc_url, next);
    mpc_url_ntask++;
    mpc_log_debug(0, "insert task url(%d), total %d, host: \"%V\" uri: \"%V\"",
                  mpc_url->url_id, mpc_url_ntask, 
                  &mpc_url->host, &mpc_url->uri);

    pthread_mutex_unlock(&mutex_task);
}


mpc_url_t *
mpc_url_get(void)
{
    mpc_url_t  *mpc_url;
    uint8_t    *buf;

    pthread_mutex_lock(&mutex_free);

    if (!STAILQ_EMPTY(&mpc_url_free_queue)) {
        mpc_url = STAILQ_FIRST(&mpc_url_free_queue);
        mpc_url_nfree--;
        STAILQ_REMOVE_HEAD(&mpc_url_free_queue, next);
        ASSERT(mpc_url->magic == MPC_URL_MAGIC);
        goto done;
    }

    buf = (uint8_t *)mpc_calloc(sizeof(mpc_url_t) + MPC_URL_BUF_SIZE, 1);
    if (buf == NULL) {
        pthread_mutex_unlock(&mutex_free);
        return NULL;
    }

    mpc_url = (mpc_url_t *)(buf + MPC_URL_BUF_SIZE);
    mpc_url->buf = buf;
    mpc_url->buf_size = MPC_URL_BUF_SIZE;

    SET_MAGIC(mpc_url, MPC_URL_MAGIC);
done:
    STAILQ_NEXT(mpc_url, next) = NULL;

    pthread_mutex_unlock(&mutex_free);

    return mpc_url;
}


static void
mpc_url_free(mpc_url_t *mpc_url)
{
    uint8_t  *buf;

    ASSERT(STAILQ_NEXT(mpc_url, next) == NULL);
    ASSERT(mpc_url->magic == MPC_URL_MAGIC);

    buf = (uint8_t *)mpc_url - MPC_URL_BUF_SIZE;
    mpc_free(buf);
}


void
mpc_url_put(mpc_url_t *mpc_url)
{
    pthread_mutex_lock(&mutex_free);

    ASSERT(STAILQ_NEXT(mpc_url, next) == NULL);
    ASSERT(mpc_url->magic == MPC_URL_MAGIC);

    if (mpc_url_max_nfree != 0 && mpc_url_nfree + 1 > mpc_url_max_nfree) {
        mpc_url_free(mpc_url);
        
    } else {
        mpc_url_nfree++;
        STAILQ_INSERT_HEAD(&mpc_url_free_queue, mpc_url, next);
    }

    pthread_mutex_unlock(&mutex_free);
}


static void
mpc_url_remove(mpc_url_hdr_t *mpc_url_hdr, mpc_url_t *mpc_url)
{
    STAILQ_REMOVE(mpc_url_hdr, mpc_url, mpc_url_s, next);
    STAILQ_NEXT(mpc_url, next) = NULL;
}


void
mpc_url_init(uint32_t max_nfree)
{
    mpc_url_nfree = 0;
    STAILQ_INIT(&mpc_url_free_queue);
    mpc_url_max_nfree = max_nfree;
    mpc_url_ntask = 0;
    STAILQ_INIT(&mpc_url_task_queue);
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

    while (!STAILQ_EMPTY(&mpc_url_task_queue)) {
        mpc_url = STAILQ_FIRST(&mpc_url_task_queue);
        mpc_url_remove(&mpc_url_task_queue, mpc_url);
        mpc_url_free(mpc_url);
        mpc_url_ntask--;
    }

    ASSERT(mpc_url_nfree == 0);
    ASSERT(mpc_url_ntask == 0);
}
