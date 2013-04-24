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


#include <getopt.h>
#include <mpc_core.h>


static int   show_help;
static int   show_version;
static int   test_conf;
static int   daemonize;
static mpc_instance_t   *mpc_ins;


static struct option long_options[] = {
    { "help",        no_argument,        NULL,   'h' },
    { "version",     no_argument,        NULL,   'v' },
    { "test",        no_argument,        NULL,   't' },
    { "daemon",      no_argument,        NULL,   'd' },
    { "follow",      no_argument,        NULL,   'F' },
    { "replay",      no_argument,        NULL,   'r' },
    { "log",         required_argument,  NULL,   'l' },
    { "level",       required_argument,  NULL,   'L' },
    { "conf",        required_argument,  NULL,   'C' },
    { "file",        required_argument,  NULL,   'f' },
    { "addr",        required_argument,  NULL,   'a' },
    { "port",        required_argument,  NULL,   'p' },
    { "concurrency", required_argument,  NULL,   'c' },
    { NULL,          0,                  NULL,    0  }
};


static char *short_options = "hvtdFrl:L:C:f:a:p:c:";


static int
mpc_get_options(int argc, char **argv, mpc_instance_t *ins)
{
    int c;

    opterr = 0;

    while ((c = getopt_long(argc, argv, short_options, long_options, NULL))
           != -1)
    {
        switch (c) {
        case 'h':
            show_help = 1;
            show_version = 1;
            break;

        case 'v':
            show_version = 1;
            break;

        case 't':
            test_conf = 1;
            break;

        case 'd':
            daemonize = 1;
            break;

        case 'F':
            ins->follow_location = 1;
            break;

        case 'r':
            ins->replay = 1;
            break;

        case 'C':
            ins->conf_filename = optarg;
            break;

        case 'f':
            ins->input_filename = optarg;
            break;

        case 'l':
            ins->log_file = optarg;
            break;

        case 'L':
            ins->log_level = mpc_log_get_level(optarg);
            if (ins->log_level == MPC_ERROR) {
                mpc_log_stderr(0, "option '-L' requires a valid log level");
                return MPC_ERROR;
            }
            break;

        case 'a':
            ins->addr = optarg;
            break;

        case 'p':
            ins->port = mpc_atoi((uint8_t *)optarg, strlen(optarg));
            if (ins->port == MPC_ERROR) {
                mpc_log_stderr(0, "option '-p' requires a number");
                return MPC_ERROR;
            }

            if (ins->port < 1 || ins->port > UINT16_MAX) {
                mpc_log_stderr(0, "option '-p' value '%d' is not a valid port",
                               ins->port); 
                return MPC_ERROR;
            }
            break;

        case 'c':
            ins->concurrency = mpc_atoi((uint8_t *)optarg, strlen(optarg));
            if (ins->concurrency == MPC_ERROR) {
                mpc_log_stderr(0, "option '-C' requires a number");
                return MPC_ERROR;
            }

            if (ins->concurrency < 1) {
                mpc_log_stderr(0, "option '-C' value must be positive",
                               ins->concurrency); 
                return MPC_ERROR;
            }
            break;

        case '?':
            switch (optopt) {
            case 'f':
            case 'c':
            case 'l':
                mpc_log_stderr(0, "option -%c requires a file name", optopt);
                break;

            case 'a':
            case 'L':
                mpc_log_stderr(0, "option -%c requires a string", optopt);
                break;

            case 'p':
                mpc_log_stderr(0, "option -%c requires a number", optopt);
                break;

            default:
                mpc_log_stderr(0, "invalid option -- '%c'", optopt);
                break;
            }

            return MPC_ERROR;
        }
    }

    return MPC_OK;
}


static void
mpc_show_usage(void)
{
    printf("Usage: mpc [-?hvtdFr] [-l log file] [-L log level] " CRLF
           "           [-c concurrency]" CRLF
           CRLF
           "Options:" CRLF
           "  -h, --help            : this help" CRLF
           "  -v, --version         : show version and exit" CRLF
           "  -t, --test-conf       : test configuration syntax and exit" CRLF
           "  -F, --follow          : follow 302 redirect" CRLF
           "  -r, --replay          : replay a url file" CRLF
           "  -l, --log=S           : log file" CRLF
           "  -L, --level=S         : log level" CRLF
           "  -c, --concurrency=N   : concurrency" CRLF
           CRLF);
}


static int
mpc_test_conf()
{
    return MPC_OK;
}


void
mpc_set_default_option(mpc_instance_t *ins)
{
    ins->conf_filename = MPC_DEFAULT_CONF_PATH;
    ins->input_filename = NULL;
    ins->addr = NULL;
    ins->port = MPC_DEFAULT_PORT;
    ins->urls = NULL;
    ins->concurrency = MPC_DEFAULT_CONCURRENT;

    ins->follow_location = 0;
    ins->replay = 0;
    ins->log_level = MPC_LOG_INFO;
    ins->log_file = NULL;

    ins->el = NULL;
    ins->self_pipe[0] = -1;
    ins->self_pipe[1] = -1;
}


int 
main(int argc, char **argv)
{
    int              rc;

    mpc_ins = (mpc_instance_t *)mpc_calloc(sizeof(mpc_instance_t), 1);
    if (mpc_ins == NULL) {
        mpc_log_stderr(errno, "oom!");
        exit(1);
    }
    
    mpc_set_default_option(mpc_ins);

    mpc_ins->stat = mpc_stat_create();
    if (mpc_ins->stat == NULL) {
        mpc_log_stderr(errno, "oom!");
        exit(1);
    }

    rc = mpc_get_options(argc, argv, mpc_ins);
    if (rc != MPC_OK) {
        mpc_show_usage();
        exit(1);
    }

    if (show_version) {
        printf("mpc: Multiple Protocol Client" CRLF
               "Version: %s" CRLF,
               MPC_VERSION_STR);

        if (show_help) {
            mpc_show_usage();
        }

        exit(0);
    }

    if (test_conf) {
        if (!mpc_test_conf()) {
            exit(1);
        }
        exit(0);
    }

    if (mpc_core_init(mpc_ins) != MPC_OK) {
        exit(1);
    }

    if (mpc_core_run(mpc_ins) != MPC_OK) {
        exit(1);
    }

    if (mpc_core_deinit(mpc_ins) != MPC_OK) {
        printf("here\n");
        exit(1);
    }

    mpc_stat_print(mpc_ins->stat);

    mpc_stat_destroy(mpc_ins->stat);

    mpc_free(mpc_ins);

    exit(0);
}


void
mpc_stop()
{
    mpc_ins->stat->stop = time_us();
    mpc_event_stop(mpc_ins->el, 0);
}
