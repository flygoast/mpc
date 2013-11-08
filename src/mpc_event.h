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

#ifndef __MPC_EVENT_H_INCLUDED__
#define __MPC_EVENT_H_INCLUDED__


#define MPC_NONE         0
#define MPC_READABLE     1
#define MPC_WRITABLE     2

#define MPC_FILE_EVENTS  1
#define MPC_TIME_EVENTS  2
#define MPC_DONT_WAIT    4
#define MPC_ALL_EVENTS   (MPC_FILE_EVENTS | MPC_TIME_EVENTS)

#define MPC_NOMORE       -1

#define MPC_DEFAULT_EVENT_SIZE  65535


typedef struct mpc_event_loop_s mpc_event_loop_t;
typedef struct mpc_time_event_s mpc_time_event_t;

typedef void (*mpc_event_file_pt)(mpc_event_loop_t *el, int fd, void *data,
    int mask);
typedef int (*mpc_event_time_pt)(mpc_event_loop_t *el, int64_t id, void *data);
typedef void (*mpc_event_finalizer_pt)(mpc_event_loop_t *el, void *data);
typedef void (*mpc_event_before_sleep_pt)(mpc_event_loop_t *el);


/* file event structure */
typedef struct {
    int                mask;        /* MPC_(READABLE|WRITABLE|NONE) */
    mpc_event_file_pt  r_file_ptr;
    mpc_event_file_pt  w_file_ptr;
    void              *data;
} mpc_file_event_t;


/* time event structure */
struct mpc_time_event_s {
    int64_t                    id;        /* time event identifier */
    int64_t                    when_sec;  /* seconds */
    int64_t                    when_ms;   /* milliseconds */
    mpc_event_time_pt          time_ptr;
    mpc_event_finalizer_pt     finalizer_ptr;
    void                      *data;
    mpc_time_event_t          *next;
};


/* fired event */
typedef struct {
    int  fd;
    int  mask;
} mpc_fired_event_t;


/* state of an event base program */
struct mpc_event_loop_s {
    int                         maxfd;
    int                         setsize;
    int64_t                     time_event_next_id;
    time_t                      last_time;
    mpc_file_event_t           *events;
    mpc_fired_event_t          *fired;
    mpc_time_event_t           *time_event_head;
    int                         stop;
    int                         exit_code;
    void                       *api_data;
    mpc_event_before_sleep_pt   before_sleep_ptr;
    void                       *resolver;
};


#ifdef WITH_MPC_RESOLVER
#include <mpc_resolver.h>
#endif


mpc_event_loop_t *mpc_create_event_loop(int setsize);
void mpc_free_event_loop(mpc_event_loop_t *el);
void mpc_event_stop(mpc_event_loop_t *el, int exit);
int mpc_create_file_event(mpc_event_loop_t *el, int fd, int mask,
    mpc_event_file_pt file_ptr, void *data);
void mpc_delete_file_event(mpc_event_loop_t *el, int fd, int mask);
int mpc_get_file_events(mpc_event_loop_t *el, int fd);
int64_t mpc_create_time_event(mpc_event_loop_t *el, int64_t ms, 
    mpc_event_time_pt time_ptr, void *data,
    mpc_event_finalizer_pt finalizer_ptr);
int mpc_delete_time_event(mpc_event_loop_t *el, int64_t id);
int mpc_process_events(mpc_event_loop_t *el, int flags);
void mpc_event_main(mpc_event_loop_t *el);
char *mpc_event_get_api_name(void);
void mpc_set_before_sleep_ptr(mpc_event_loop_t *el,
    mpc_event_before_sleep_pt before_sleep_ptr);


#endif /*__MPC_EVENT_H_INCLUDED__ */
