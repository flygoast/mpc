#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/epoll.h>


typedef struct {
    int                  epfd;
    struct epoll_event  *events;
} mpc_event_api_state_t;


static int
mpc_event_api_create(mpc_event_loop_t *el)
{
    mpc_event_api_state_t *state = mpc_malloc(sizeof(*state));
    if (state == NULL) {
        return MPC_ERROR;
    }

    state->events = mpc_malloc(sizeof(struct epoll_event) * el->setsize);
    if (state->events == NULL) {
        mpc_free(state);
        return MPC_ERROR;
    }

    /* 1024 is just a hint for the kernel */
    state->epfd = epoll_create(1024); 
    if (state->epfd == -1) {
        mpc_free(state->events);
        mpc_free(state);
        return MPC_ERROR;
    }

    el->api_data = state;

    return MPC_OK;
}


static void
mpc_event_api_free(mpc_event_loop_t *el)
{
    mpc_event_api_state_t *state = el->api_data;

    close(state->epfd);
    mpc_free(state->events);
    mpc_free(state);
}


static int
mpc_event_api_add_event(mpc_event_loop_t *el, int fd, int mask)
{
    int                    op;
    struct epoll_event     ee;
    mpc_event_api_state_t *state = el->api_data;

    /* If the fd was already monitored for some event, we need a MOD
       operation. Otherwise we need an ADD operation. */
    op = el->events[fd].mask == MPC_NONE ? EPOLL_CTL_ADD : EPOLL_CTL_MOD;
    ee.events = 0;
    mask |= el->events[fd].mask; /* Merge old events. */
    if (mask & MPC_READABLE) {
        ee.events |= EPOLLIN;
    }

    if (mask & MPC_WRITABLE) {
        ee.events |= EPOLLOUT;
    }
    ee.events |= EPOLLET;
    ee.data.u64 = 0;
    ee.data.fd = fd;
    if (epoll_ctl(state->epfd, op, fd, &ee) == -1) {
        return MPC_ERROR;
    }

    return MPC_OK;
}


static void
mpc_event_api_del_event(mpc_event_loop_t *el, int fd, int delmask)
{
    mpc_event_api_state_t  *state = el->api_data;
    struct epoll_event      ee;
    int                     mask = el->events[fd].mask & (~delmask);

    ee.events = 0;
    if (mask & MPC_READABLE) {
        ee.events |= EPOLLIN;
    }

    if (mask & MPC_WRITABLE) {
        ee.events |= EPOLLOUT;
    }

    ee.events |= EPOLLET;
    ee.data.u64 = 0;
    ee.data.fd = fd;
    if (mask != MPC_NONE) {
        epoll_ctl(state->epfd, EPOLL_CTL_MOD, fd, &ee);
    } else {
        /* Note, kernel < 2.6.9 requires a non null event pointer even
           for EPOLL_CTL_DEL. */
        epoll_ctl(state->epfd, EPOLL_CTL_DEL, fd, &ee);
    }
}


static int
mpc_event_api_poll(mpc_event_loop_t *el, struct timeval *tvp)
{
    int                    j, mask;
    int                    retval, numevents = 0;
    struct epoll_event    *e;
    mpc_event_api_state_t *state = el->api_data;

    retval = epoll_wait(state->epfd, state->events, el->setsize,
                        tvp ? (tvp->tv_sec * 1000 + tvp->tv_usec / 1000) : -1);
    if (retval > 0) {
        numevents = retval;
        for (j = 0; j < numevents; ++j) {
            mask = 0;
            e = state->events + j;
            if (e->events & EPOLLIN) {
                mask |= MPC_READABLE;
            }

            if (e->events & EPOLLOUT) {
                mask |= MPC_WRITABLE;
            }

            if (e->events & EPOLLERR) {
                mask |= MPC_WRITABLE;
            }

            if (e->events & EPOLLHUP) {
                mask |= MPC_WRITABLE;
            }

            el->fired[j].fd = e->data.fd;
            el->fired[j].mask = mask;
        }
    }

    return numevents;
}


static char *
mpc_event_api_name(void)
{
    return "epoll";
}
