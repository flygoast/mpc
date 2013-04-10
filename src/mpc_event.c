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


/* Include the best multiplexing layer supported by this system.
   The following should be ordered by performances, descending. */
#if defined(__linux__) 
#include <mpc_epoll.c>
#elif defined(__FreeBSD__)
#include <mpc_kqueue.c>
#else
#ifdef MPC_USE_POLL
#include <mpc_poll.c>
#else
#include <mpc_select.c>
#endif
#endif

#ifdef WITH_MPC_RESOLVER
#include <mpc_resolver.c>
#endif

mpc_event_loop_t *
mpc_create_event_loop(int setsize)
{
    int                i;
    mpc_event_loop_t  *el;

    el = (mpc_event_loop_t *)mpc_malloc(sizeof(mpc_event_loop_t));
    if (el == NULL) {
        return NULL;
    }

    el->events = (mpc_file_event_t *)mpc_malloc(sizeof(mpc_file_event_t)
                                                * setsize);
    if (el->events == NULL) {
        mpc_free(el);
        return NULL;
    }

    el->fired = (mpc_fired_event_t *)mpc_malloc(sizeof(mpc_fired_event_t) 
                                                * setsize);
    if (el->fired == NULL) {
        mpc_free(el->events);
        mpc_free(el);
        return NULL;
    }

    el->last_time = time(NULL);
    el->setsize = setsize;
    el->time_event_head = NULL;
    el->time_event_next_id = 0;
    el->stop = 0;
    el->maxfd = -1;
    el->before_sleep_ptr = NULL;
    if (mpc_event_api_create(el) == -1) {
        mpc_free(el);
        return NULL;
    }

    /* Events with mask == MPC_NONE are not set. So let's initialize
       the vector with it. */
    for (i = 0; i < setsize; ++i) {
        el->events[i].mask = MPC_NONE;
    }

    return el;
}


void 
mpc_free_event_loop(mpc_event_loop_t *el)
{
    mpc_time_event_t  *te, *next;

    mpc_event_api_free(el);

    /* Delete all time event to avoid memory leak. */
    te = el->time_event_head;
    while (te) {
        next = te->next;
        if (te->finalizer_ptr) {
            te->finalizer_ptr(el, te->data);
        }
        mpc_free(te);
        te = next;
    }

    mpc_free(el->events);
    mpc_free(el->fired);
    mpc_free(el);
}


void 
mpc_event_stop(mpc_event_loop_t *el)
{
    el->stop = 1;
}


/* Register a file event. */
int
mpc_create_file_event(mpc_event_loop_t *el, int fd, int mask,
    mpc_event_file_pt file_ptr, void *data)
{
    mpc_file_event_t  *fe;

    if (fd >= el->setsize) {
        errno = ERANGE;
        return MPC_ERROR;
    } 

    fe = &el->events[fd];

    if (mpc_event_api_add_event(el, fd, mask) == -1) {
        return MPC_ERROR;
    }

    fe->mask |= mask; /* On one fd, multi events can be registered. */
    if (mask & MPC_READABLE) {
        fe->r_file_ptr = file_ptr;
    }

    if (mask & MPC_WRITABLE) {
        fe->w_file_ptr = file_ptr;
    } 
    fe->data = data;

    /* Once one file event has been registered, the el->maxfd
       no longer is -1. */
    if (fd > el->maxfd) {
        el->maxfd = fd;
    }

    return MPC_OK;
}


/* Unregister a file event */
void
mpc_delete_file_event(mpc_event_loop_t *el, int fd, int mask)
{
    mpc_file_event_t *fe;

    if (fd >= el->setsize) {
        return;
    }

    fe = &el->events[fd];
    if (fe->mask == MPC_NONE) {
        return;
    }

    fe->mask = fe->mask & (~mask);

    if (fd == el->maxfd && fe->mask == MPC_NONE) {
        /* All the events on the fd were deleted, update the max fd. */
        int j;
        for (j = el->maxfd - 1; j >= 0; --j) {
            if (el->events[j].mask != MPC_NONE) {
                break;
            }
        }

        /* If all the file events on all fds deleted, max fd will get
           back to -1. */
        el->maxfd = j;
    }

    mpc_event_api_del_event(el, fd, mask);
}


int
mpc_get_file_events(mpc_event_loop_t *el, int fd)
{
    mpc_file_event_t *fe;
    if (fd >= el->setsize) {
        return 0;
    }

    fe = &el->events[fd];
    return fe->mask;
}


/* Register a time event. */
int64_t
mpc_create_time_event(mpc_event_loop_t *el, int64_t ms, 
    mpc_event_time_pt time_ptr, void *data,
    mpc_event_finalizer_pt finalizer_ptr)
{
    mpc_time_event_t  *te;
    int64_t            id = el->time_event_next_id++;

    te = (mpc_time_event_t *)mpc_malloc(sizeof(mpc_time_event_t));
    if (te == NULL) {
        return MPC_ERROR;
    }

    te->id = id;
    mpc_add_milliseconds_to_now(ms, &te->when_sec, &te->when_ms);
    te->time_ptr = time_ptr;
    te->finalizer_ptr = finalizer_ptr;
    te->data = data;

    /* Insert time event into the head of the linked list. */
    te->next = el->time_event_head;
    el->time_event_head = te;

    return id;
}


/* Unregister a time event. */
int
mpc_delete_time_event(mpc_event_loop_t *el, int64_t id)
{
    mpc_time_event_t *te, *prev = NULL;

    te = el->time_event_head;
    while (te) {
        if (te->id == id) {
            /* Delete a node from the time events linked list. */
            if (prev == NULL) {
                el->time_event_head = te->next;
            } else {
                prev->next = te->next;
            }

            if (te->finalizer_ptr) {
                te->finalizer_ptr(el, te->data);
            }

            mpc_free(te);
            return MPC_OK;
        }
        prev = te;
        te = te->next;
    }
    return MPC_ERROR; /* NO event with the specified ID found */
}


/*
 * Search the first timer to fire.
 * This operation is useful to know how many time the select can be
 * put in sleep without to delay any event.
 * If there are no timers NULL is returned.
 *
 * Note that's O(N) since time events are unsorted.
 * Possible optimizations (not needed by Redis so far, but ...):
 * 1) Insert the event in order, so that the nearest is just the head.
 *    Much better but still insertion or deletion of timer is O(N).
 * 2) Use a skiplist to have this operation as O(1) and insertion as
 *    O(log(N)).
 *
 */
static mpc_time_event_t *
mpc_search_nearest_timer(mpc_event_loop_t *el)
{
    mpc_time_event_t  *te = el->time_event_head;
    mpc_time_event_t  *nearest = NULL;

    while (te) {
        if (nearest == NULL
            || te->when_sec < nearest->when_sec 
            || (te->when_sec == nearest->when_sec
                && te->when_ms < nearest->when_ms))
        {
            nearest = te;
        }
        te = te->next;
    }

    return nearest;
}


/* Process time events. */
static int
process_time_events(mpc_event_loop_t *el)
{
    int                processed = 0;
    mpc_time_event_t  *te;
    int64_t            maxid;
    time_t             now = time(NULL);

    /* If the system clock is moved to the future, and then set back to the
     * right value, time events may be delayed in a random way. Often this
     * means that scheduled operations will not be performed soon enough.
     *
     * Here we try to detect system clock skews, and force all the time
     * events to be processed ASAP when this happens: the idea is that
     * processing events earlier is less dangerous than delaying them
     * indefinitely, and practice suggests it is. */
    if (now < el->last_time) {
        te = el->time_event_head;
        while (te) {
            te->when_sec = 0;
            te = te->next;
        }
    }
    el->last_time = now;

    te = el->time_event_head;
    maxid = el->time_event_next_id - 1;
    while (te) {
        int64_t  id, now_sec, now_ms;

        /* Don't process the time event registered during this process. */
        if (te->id > maxid) { 
            te = te->next;
            continue;
        }
        mpc_get_time(&now_sec, &now_ms);

        /* timeout */
        if (now_sec > te->when_sec
            || (now_sec == te->when_sec && now_ms >= te->when_ms))
        {
            int ret;
            id = te->id;
            ret = te->time_ptr(el, id, te->data);
            processed++;
            /* After an event is processed our time event list 
               may no longer be the same, so we restart from head.
               Still we make sure to don't process events registered
               by event handlers itself in order to don't loop
               forever. To do so we saved the max ID we want to 
               handle.

               FUTURE OPTIMIZATIONS:
               Note that this is NOT great algorithmically. Redis
               uses a single time event so it's not a problem but
               the right way to do this is to add the new elements
               on head, and to flag deleted elements in a special
               way for later deletion(putting references to the 
               nodes to delete into another linked list). */
            if (ret > 0) {
                mpc_add_milliseconds_to_now(ret, &te->when_sec, 
                        &te->when_ms);
            } else {
                mpc_delete_time_event(el, id);
            }
            te = el->time_event_head;

        } else {
            te = te->next;
        }
    }

    return processed;
}


/* Process every pending time event, then every pending file event
   (that may be registered by time event callbacks just processed).
   Without special flags the function sleeps until some file event
   fires, or when the next time event occurs (if any).

   If flag is 0, the function does nothing and returns.
   if flag has mpc_ALL_EVENTS set, all the kind of events are processed.
   if flag has mpc_FILE_EVENTS set, file events are processed.
   if flag has mpc_TIME_EVENTS set, time events are processed.
   if flag has mpc_DONT_WAIT set, the function returns ASAP (As soon
   as possible) until all the events that's possible to process 
   without to wait are processed.

   The function returns the number of events processed. */
int
mpc_process_events(mpc_event_loop_t *el, int flags)
{
    int processed = 0, numevents;

    /* Nothing to do ? return ASAP */
    if (!(flags & MPC_TIME_EVENTS) && !(flags & MPC_FILE_EVENTS)) {
        return 0;
    }

    /* Note that we want call select() even if there are no file
       events to process as long as we want to process time events,
       in order to sleep until the next time event is ready to fire. */
    if (el->maxfd != -1
        || ((flags & MPC_TIME_EVENTS) && !(flags & MPC_DONT_WAIT)))
    {
        int             j;
        mpc_time_event_t *shortest = NULL;
        struct timeval    tv, *tvp;
        
        if (flags & MPC_TIME_EVENTS && !(flags & MPC_DONT_WAIT)) {
            shortest = mpc_search_nearest_timer(el);
        } 

        if (shortest) {
            int64_t now_sec, now_ms;

            /* Calculate the time missing for the nearest timer 
               to fire. */
            mpc_get_time(&now_sec, &now_ms);
            tvp = &tv;
            tvp->tv_sec = shortest->when_sec - now_sec;
            if (shortest->when_ms < now_ms) {
                tvp->tv_usec = ((shortest->when_ms + 1000) - now_ms) * 1000;
                tvp->tv_sec--;

            } else {
                tvp->tv_usec = (shortest->when_ms - now_ms) * 1000;
            }

            if (tvp->tv_sec < 0) {
                tvp->tv_sec = 0;
            } 

            if (tvp->tv_usec < 0) {
                tvp->tv_usec = 0;
            }

        } else {
            /* If we have to check for events but need to return
               ASAP because of MPC_DONT_WAIT we need to set the 
               timeout to zero. */
            if (flags & MPC_DONT_WAIT) {
                tv.tv_sec = tv.tv_usec = 0;
                tvp = &tv;

            } else {
                /* Otherwise we can block. */
                tvp = NULL; /* wait forever */
            }
        }

        numevents = mpc_event_api_poll(el, tvp);

        for (j = 0; j < numevents; ++j) {
            mpc_file_event_t *fe = &el->events[el->fired[j].fd];
            int               mask = el->fired[j].mask;
            int               fd = el->fired[j].fd;
            int               rfired = 0;
            
            /* Note the fe->mask & mask & ... code: maybe an already
               processed event removed an element that fired and we
               still didn't processed, so we check if the events is 
               still valid. */
            if (fe->mask & mask & MPC_READABLE) {
                rfired = 1;
                fe->r_file_ptr(el, fd, fe->data, mask);
            } 

            if (fe->mask & mask & MPC_WRITABLE) {
                if (!rfired || fe->w_file_ptr != fe->r_file_ptr) {
                    fe->w_file_ptr(el, fd, fe->data, mask);
                }
            }

            ++processed;
        }
    }

    /* Check time events */
    if (flags & MPC_TIME_EVENTS) {
        processed += process_time_events(el);
    }

    /* Return the number of processed file/time events */
    return processed; 
}


/* main loop of the event-driven framework */
void
mpc_event_main(mpc_event_loop_t *el)
{
    el->stop = 0;

    while (!el->stop) {
        if (el->before_sleep_ptr != NULL) {
            el->before_sleep_ptr(el);
        }
        mpc_process_events(el, MPC_ALL_EVENTS);
    }
}


char *
mpc_event_get_api_name(void)
{
    return mpc_event_api_name();
}


void
mpc_set_before_sleep_ptr(mpc_event_loop_t *el, 
    mpc_event_before_sleep_pt before_sleep_ptr)
{
    el->before_sleep_ptr = before_sleep_ptr;
}
