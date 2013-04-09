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


static mpc_signal_t signals[] = {
    { SIGUSR1, "SIGUSR1", 0,                 mpc_signal_handler },
    { SIGUSR2, "SIGUSR2", 0,                 mpc_signal_handler },
    { SIGTTIN, "SIGTTIN", 0,                 mpc_signal_handler },
    { SIGTTOU, "SIGTTOU", 0,                 mpc_signal_handler },
    { SIGHUP,  "SIGHUP",  0,                 mpc_signal_handler },
    { SIGINT,  "SIGINT",  0,                 mpc_signal_handler },
    { SIGSEGV, "SIGSEGV", (int)SA_RESETHAND, mpc_signal_handler },
    { SIGPIPE, "SIGPIPE", 0,                 SIG_IGN },
    { 0,       NULL,      0,                 NULL}
};


int
mpc_signal_init(void)
{
    mpc_signal_t      *sig;
    struct sigaction   sa;
    int                status;

    for (sig = signals; sig->signo != 0; sig++) {
        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = sig->handler;
        sa.sa_flags = sig->flags;
        sigemptyset(&sa.sa_mask);

        status = sigaction(sig->signo, &sa, NULL);
        if (status < 0) {
            return NC_ERROR;
        }
    }

    return NC_OK;
}


void
mpc_signal_deinit(void)
{
    MPC_DO_NOTHING();
}


void
mpc_signal_handler(int signo)
{
    mpc_signal_t  *sig;
    void         (*action)(void);
    char          *action_str;
    int            done;

    for (sig = signals; sig->signo != 0; sig++) {
        if (sig->signo == signo) {
            break;
        }
    }

#ifdef WITH_DEBUG
    assert(sig->signo != 0);
#endif

    action_str = "";
    action = NULL;
    done = 0;

    switch (signo) {
    case SIGUSR1:
        break;

    case SIGUSR2:
        break;

    case SIGTTIN:
        action_str = ", up logging level";
        action = mpc_log_level_up;
        break;

    case SIGTTOU:
        action_str = ", down logging level";
        action = mpc_log_level_up;
        break;

    case SIGHUP:
        action_str = ", reopening log file";
        action = mpc_log_reopen;
        break;

    case SIGINT:
        action_str = ", exiting";
        done = 1;
        break;

    case SIGSEGV:
        mpc_backtrace(1);
        action_str = ", core dumping";
        raise(SIGSEGV);
        break;

    default:
        NOT_REACHED();
    }

    if (action != NULL) {
        action();
    }

    if (done) {
        exit(1);
    }
}


void
mpc_backtrace(int skip_count)
{
    void   *stack[64];
    char  **symbols;
    int     size, i, j;

    size = backtrace(stack, 64);
    symbols = backtrace_symbols(stack, size);
    if (symbols == NULL) {
        return;
    }

    skip_count++; /* skip the current frame also */

    for (i = skip_count, j = 0;  i < size; i++, j++) {
        /* TODO log */
    }

    free(symbols);
}
