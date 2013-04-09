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


mpc_array_t *
mpc_array_create(uint32_t n, size_t size)
{
    mpc_array_t  *a;

    a = mpc_alloc(sizeof(a));
    if (a == NULL) {
        return NULL;
    }

    if (mpc_array_init(a, n, size) != MPC_OK) {
        mpc_free(a);
        return NULL;
    }

    return a;
}


void
mpc_array_destroy(mpc_array_t *a) 
{
    mpc_array_deinit(a);
    mpc_free(a);
}


int
mpc_array_init(mpc_array_t *a, uint32_t n, size_t size)
{
#ifdef DEBUG
    assert(n != 0 && size != 0);
#endif

    a->elem = mpc_alloc(n * size);
    if (a->elem == NULL) {
        mpc_free(a);
        return MPC_ERROR;
    }

    a->nelem = 0;
    a->size = size;
    a->nalloc = n;

    return MPC_OK;
}


void
mpc_array_deinit(mpc_array_t *a)
{
    if (a->elem != NULL) {
        mpc_free(a->elem);
    }
}


uint32_t
mpc_array_idx(mpc_array_t *a, void *elem)
{
    uint8_t   *p, *q;
    uint32_t   off, idx;

#ifdef DEBUG
    assert(elem >= a->elem);
#endif

    p = a->elem;
    q = elem;
    off = (uint32_t)(q - p);

#ifdef DEBUG
    assert(off % (uint32_t)a->size == 0);
#endif

    idx = off / (uint32_t)a->size;

    return idx;
}


void *
mpc_array_push(mpc_array_t *a)
{
    void   *elem, *new;
    size_t  size;

    if (a->nelem == a->nalloc) {

        /* the array is full; allocate new array */
        size = a->size * a->nalloc;
        new = mpc_realloc(a->elem, 2 * size);

        if (new == NULL) {
            return NULL;
        }

        a->elem = new;
        a->nalloc *= 2;
    }

    elem = (uint8_t *)a->elem + a->size * a->nelem;
    a->nelem++;

    return elem;
}


void *
mpc_array_pop(mpc_array_t *a)
{
    void *elem;
#ifdef DEBUG
    assert(a->nelem != 0);
#endif

    a->nelem--;
    elem = (uint8_t *)a->elem + a->size * a->nelem;

    return elem;
}


void *
mpc_array_get(mpc_array_t *a, uint32_t idx)
{
    void  *elem;

#ifdef DEBUG
    assert(a->nelem != 0);
    assert(idx < a->nelem);
#endif

    elem = (uint8_t *)a->elem + (a->size * idx);

    return elem;
}


void *
mpc_array_top(mpc_array_t *a)
{
    ASSERT(a->nelem != 0);

    return array_get(a, a->nelem - 1);
}


void
mpc_array_swap(mpc_array_t *a, mpc_array_t *b)
{
    mpc_array_t     tmp;

    tmp = *a;
    *a = *b;
    *b = tmp;
}


/*
 * Sort nelem elements of the array in ascending order based on the
 * compare comparator.
 */
void
mpc_array_sort(mpc_array_t *a, mpc_array_cmp_pt cmp)
{
    ASSERT(a->nelem != 0);
    qsort(a->elem, a->nelem, a->size, cmp);
}


/*
 * Call the func once for each element in the array as long as func
 * returns success. On failure short-circuits and returns the error.
 */
int
mpc_array_each(mpc_array_t *a, mpc_array_each_pt func, void *data)
{
    uint32_t  i, nelem;
    int       rc;
    void     *elem;

    ASSERT(a->nelem != 0);
    ASSERT(func != NULL);

    for (i = 0, nelem = a->nelem; i < nelem; i++) {
        elem = array_get(a, i);

        rc = func(elem, data);
        if (rc != MPC_OK) {
            return rc;
        }
    }

    return MPC_OK;
}
