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


static struct option long_options[] = {
    { "help",       no_argument,        NULL,   'h' },
    { "version",    no_argument,        NULL,   'v' },
    { "test",       no_argument,        NULL,   't' },
    { "daemon",     no_argument,        NULL,   'd' },
    { "conf",       required_argument,  NULL,   'c' },
    { "file",       required_argument,  NULL,   'f' },
    { "addr",       required_argument,  NULL,   'a' },
    { "port",       required_argument,  NULL,   'p' },
    { NULL,         0,                  NULL,    0  }
};


static char *short_options = "hvtdc:f:a:p:";


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

        case 'c':
            ins->conf_filename = optarg;
            break;

        case 'f':
            ins->input_filename = optarg;
            break;

        case 'a':
            ins->addr = optarg;
            break;

        case 'p':
            ins->port = mpc_atoi(optarg, strlen(optarg));
            if (ins->port == MPC_ERROR) {
                mpc_log_stderr("option '-p' requires a number");
                return MPC_ERROR;
            }

            if (ins->port < 1 || ins->port > UINT16_MAX) {
                mpc_log_stderr("option '-p' value '%d' is not a valid port",
                               ins->port); 
                return MPC_ERROR;
            }
            break;

        case '?':
            switch (optopt) {
            case 'f':
            case 'c':
                mpc_log_stderr("option -%c requires a file name", optopt);
                break;

            case 'a':
                mpc_log_stderr("option -%c requires a string", optopt);
                break;

            case 'p':
                mpc_log_stderr("option -%c requires a number", optopt);
                break;

            default:
                mpc_log_stderr("invalid option -- '%c'", optopt);
                break;
            }

            return MPC_ERROR;
        }
    }

    return MPC_OK;
}


static void
mpc_show_usage()
{
    
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
}


int 
main(int argc, char **argv)
{
    int              rc;
    mpc_instance_t  *ins;

    ins = (mpc_instance_t *)mpc_malloc(sizeof(mpc_instance_t));
    if (ins == NULL) {
        mpc_log_stderr("oom!");
        exit(1);
    }
    
    mpc_set_default_option(ins);

    rc = mpc_get_options(argc, argv, ins);
    if (rc != MPC_OK) {
        mpc_show_usage();
        exit(1);
    }

    if (show_version) {
        mpc_log_stderr("mpc: Multiple Protocol Client" CRLF
                       "Version: %s" CRLF
                       "Copyright (c) 2013, FengGu, <flygoast@gmail.com>" CRLF
                       "Repo: https://github.com/flygoast/mpc",
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

    if (mpc_core_run(ins) == MPC_ERROR) {
        exit(1);
    }

    exit(0);
}
