#ifndef __MPC_EVENT_H_INCLUDED__
#define __MPC_EVENT_H_INCLUDED__


#include <mpc_core.h>


#define MPC_NONE         0
#define MPC_READABLE     1
#define MPC_WRITABLE     2

#define MPC_FILE_EVENTS  1
#define MPC_TIME_EVENTS  2
#define MPC_ALL_EVENTS   (MPC_FILE_EVENTS | MPC_TIME_EVENTS)
#define MPC_DONT_WAIT    4

#define MPC_NOMORE       -1

#define MPC_NOTUSED(V)   ((void)V)


typedef struct mpc_event_loop_s mpc_event_loop_t;

typedef void mpc_file_proc(mpc_event_loop_t *el, int fd, 
    void *client_data, int mask);
typedef int mpc_time_proc(mpc_event_loop_t *el, long long id,
    void *client_data);
typedef void mpc_event_finalizer_proc(mpc_event_loop_t *el,
    void *client_data);
typedef void mpc_before_sleep_proc(mpc_event_loop_t *el);


/* File event structure */
typedef struct {
    int               mask;          /* one of MPC_(READABLE|WRITABLE) */
    mpc_file_proc    *r_file_proc;
    mpc_file_proc    *w_file_proc;
    void             *client_data;
} ae_file_event_t;


/* Time event structure */
typedef struct mpc_time_event_s {
    long long                  id;        /* Time event identifier. */
    long                       when_sec;  /* seconds */
    long                       when_ms;   /* milliseconds */
    mpc_time_proc_t           *time_proc;
    mpc_event_finalizer_proc  *finalizer_proc;
    void                      *client_data;
    struct mpc_time_event_s   *next;
} mpc_time_event_t;


/* A fired event */
typedef struct {
    int  fd;
    int  mask;
} mpc_fired_event_t;


/* State of an event base program */
struct mpc_event_loop_s {
    int                       maxfd;
    int                       setsize;
    long long                 time_event_next_id;
    time_t                    last_time;
    mpc_file_event_t         *events;
    mpc_fired_event_t        *fired;
    mpc_time_event_t         *time_event_head;
    int                       stop;
    void                     *api_data;
    mpc_before_sleep_proc_t  *before_sleep;
};


#ifdef WITH_MPC_RESOLVER
#include <mpc_resolver.h>
#endif



#endif /*__MPC_EVENT_H_INCLUDED__ */
