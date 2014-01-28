/*
 * mpc -- A Multiple Protocol Client.
 * Copyright (c) 2013-2014, FengGu <flygoast@gmail.com>
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
#include <stddef.h>
#include <mpc_core.h>


static int              show_help;
static int              show_version;
static mpc_instance_t  *mpc_ins;


static void mpc_rlimit_reset();
static void mpc_instance_init(mpc_instance_t *ins);
static void mpc_instance_merge(mpc_instance_t *ins, mpc_instance_t *tmp_ins);
static char *mpc_conf_log_level(mpc_conf_t *cf, mpc_command_t *cmd, void *conf);
static char *mpc_conf_address(mpc_conf_t *cf, mpc_command_t *cmd, void *conf);
static char *mpc_conf_http_method(mpc_conf_t *cf, mpc_command_t *cmd,
    void *conf);
static char *mpc_conf_run_time(mpc_conf_t *cf, mpc_command_t *cmd, void *conf);


static mpc_command_t  mpc_conf_commands[] = {
    
    { mpc_string("log_file"),
      MPC_CONF_TAKE1,
      mpc_conf_set_str_slot,
      0,
      offsetof(mpc_instance_t, log_file),
      NULL },

    { mpc_string("log_level"),
      MPC_CONF_TAKE1,
      mpc_conf_log_level,
      0,
      0,
      NULL },

    { mpc_string("follow_location"),
      MPC_CONF_FLAG,
      mpc_conf_set_flag_slot,
      0,
      offsetof(mpc_instance_t, follow_location),
      NULL },

    { mpc_string("replay"),
      MPC_CONF_FLAG,
      mpc_conf_set_flag_slot,
      0,
      offsetof(mpc_instance_t, replay),
      NULL },

    { mpc_string("url_file"),
      MPC_CONF_TAKE1,
      mpc_conf_set_str_slot,
      0,
      offsetof(mpc_instance_t, url_file),
      NULL },

    { mpc_string("address"),
      MPC_CONF_TAKE1,
      mpc_conf_address,
      0,
      0,
      NULL },

    { mpc_string("concurrency"),
      MPC_CONF_TAKE1,
      mpc_conf_set_num_slot,
      0,
      offsetof(mpc_instance_t, concurrency),
      NULL },

    { mpc_string("http_method"),
      MPC_CONF_TAKE1,
      mpc_conf_http_method,
      0,
      0,
      NULL },

    { mpc_string("result_file"),
      MPC_CONF_TAKE1,
      mpc_conf_set_str_slot,
      0,
      offsetof(mpc_instance_t, result_file),
      NULL },

    { mpc_string("result_mark"),
      MPC_CONF_TAKE1,
      mpc_conf_set_str_slot,
      0,
      offsetof(mpc_instance_t, result_mark),
      NULL },

    { mpc_string("run_time"),
      MPC_CONF_TAKE1,
      mpc_conf_run_time,
      0,
      0,
      NULL },

      mpc_null_command
};


static struct option  long_options[] = {
    { "help",            no_argument,        NULL,   'h' },
    { "version",         no_argument,        NULL,   'v' },
    { "follow-location", no_argument,        NULL,   'f' },
    { "replay",          no_argument,        NULL,   'r' },
    { "log-file",        required_argument,  NULL,   'l' },
    { "log-level",       required_argument,  NULL,   'L' },
    { "conf",            required_argument,  NULL,   'C' },
    { "url-file",        required_argument,  NULL,   'u' },
    { "address",         required_argument,  NULL,   'a' },
    { "concurrency",     required_argument,  NULL,   'c' },
    { "method",          required_argument,  NULL,   'm' },
    { "result-file",     required_argument,  NULL,   'R' },
    { "result-mark",     required_argument,  NULL,   'M' },
    { "run-time",        required_argument,  NULL,   't' },
    { NULL,              0,                  NULL,    0  }
};


static char *short_options = "hvfrl:L:C:u:a:c:m:R:M:t:";


static int
mpc_get_options(int argc, char **argv, mpc_instance_t *ins)
{
    int              c;
    struct hostent  *he;
    mpc_str_t        t;

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

        case 'f':
            ins->follow_location = 1;
            break;

        case 'r':
            ins->replay = 1;
            break;

        case 'C':
            if (ins->conf_file.len != 0) {
                mpc_log_stderr(0, "duplicate option '-C'");
                return MPC_ERROR;
            }
            ins->conf_file.data = (unsigned char *)optarg;
            ins->conf_file.len = mpc_strlen(optarg);
            break;

        case 'u':
            if (ins->url_file.len != 0) {
                mpc_log_stderr(0, "duplicate option '-u'");
                return MPC_ERROR;
            }
            ins->url_file.data = (unsigned char *)optarg;
            ins->url_file.len = mpc_strlen(optarg);
            break;

        case 'l':
            if (ins->log_file.len != 0) {
                mpc_log_stderr(0, "duplicate option '-l'");
                return MPC_ERROR;
            }
            ins->log_file.data = (unsigned char *)optarg;
            ins->log_file.len = mpc_strlen(optarg);
            break;

        case 'L':
            ins->log_level = mpc_log_get_level(optarg);
            if (ins->log_level == MPC_ERROR) {
                mpc_log_stderr(0, "option '-L' requires a valid log level");
                return MPC_ERROR;
            }
            break;

        case 'a':
            if (inet_aton(optarg, &ins->addr.sin_addr) == 0) {
                if ((he = gethostbyname(optarg)) == NULL) {
                    mpc_log_stderr(0, "option '-a' host \"%s\" not found",
                                   optarg);
                    return MPC_ERROR;
                }
                mpc_memcpy(&ins->addr.sin_addr, he->h_addr, 
                           sizeof(struct in_addr));
            }
            ins->use_addr = 1;
            break;

        case 'c':
            ins->concurrency = mpc_atoi((uint8_t *)optarg, strlen(optarg));
            if (ins->concurrency == MPC_ERROR) {
                mpc_log_stderr(0, "option '-c' requires a number");
                return MPC_ERROR;
            }

            if (ins->concurrency < 1 
                || ins->concurrency > MPC_MAX_CONCURRENCY)
            {
                mpc_log_stderr(0, "option '-c' value must be between 1 and %ud",
                               MPC_MAX_CONCURRENCY); 
                return MPC_ERROR;
            }
            break;

        case 'm':
            ins->http_method = mpc_http_get_method(optarg);
            if (ins->http_method == MPC_ERROR) {
                mpc_log_stderr(0, 
                          "option '-m' requires a valid http method: GET|HEAD");
                return MPC_ERROR;
            }
            break;

        case 'R':
            if (ins->result_file.len != 0) {
                mpc_log_stderr(0, "duplicate option '-R'");
                return MPC_ERROR;
            }
            ins->result_file.data = (unsigned char *)optarg;
            ins->result_file.len = mpc_strlen(optarg);
            break;

        case 'M':
            if (ins->result_mark.len != 0) {
                mpc_log_stderr(0, "duplicate option '-R'");
                return MPC_ERROR;
            }
            ins->result_mark.data = (unsigned char *)optarg;
            ins->result_mark.len = mpc_strlen(optarg);
            break;

        case 't':
            t.data = (uint8_t *) optarg;
            t.len = mpc_strlen(optarg);

            ins->run_time = mpc_parse_time(&t, 1);
            if (ins->run_time == MPC_ERROR) {
                mpc_log_stderr(0, "option '-t' requires a valid time" CRLF
                                  "such as: 2d2h2m2s");
                return MPC_ERROR;
            }
            break;

        default:
            mpc_log_stderr(0, "invalid option -- '%c'", optopt);
            return MPC_ERROR;
        }
    }

    return MPC_OK;
}


static void
mpc_show_usage(void)
{
    printf("Usage: mpc [-hvfr] [-l log file] [-L log level] " CRLF
           "           [-c concurrency] [-u url file] [-m http method]" CRLF
           "           [-R result file] [-M result mark string] " CRLF
           "           [-a specified address] [-t run time]" CRLF
           CRLF
           "Options:" CRLF
           "  -h, --help            : this help" CRLF
           "  -v, --version         : show version and exit" CRLF
           "  -C, --conf=S          : configuration file" CRLF
           "  -u, --url-file        : url file" CRLF
           "  -a, --address=S       : use address specified instead of DNS" CRLF
           "  -f, --follow-location : follow 302 redirect" CRLF
           "  -r, --replay          : replay the url file" CRLF
           "  -l, --log-file=S      : log file" CRLF
           "  -L, --log-level=S     : log level" CRLF
           "  -c, --concurrency=N   : concurrency" CRLF
           "  -m, --http-method=S   : http method GET, HEAD" CRLF
           "  -R, --result-file=S   : show result in a file" CRLF
           "  -M, --result-mark=S   : result file mark string" CRLF
           "  -t, --run-time=Nm     : timed testing where \"m\" is modifer" CRLF
           "                          S(second), M(minute), H(hour), D(day)"
           CRLF
           CRLF);
}


static char *
mpc_conf_log_level(mpc_conf_t *cf, mpc_command_t *cmd, void *conf)
{
    mpc_instance_t  *ins = (mpc_instance_t *)conf;
    mpc_str_t       *value;

    if (ins->log_level != MPC_CONF_UNSET) {
        return "duplicate \"log_level\"";
    }

    value = cf->args->elem;

    ins->log_level = mpc_log_get_level((char *)value[1].data);
    if (ins->log_level == MPC_ERROR) {
        mpc_conf_log_error(MPC_LOG_EMERG, cf, 0,
                           "invalid log level \"%s\"", value[1].data);
        return MPC_CONF_ERROR;
    }

    return MPC_CONF_OK;
}


static char *
mpc_conf_address(mpc_conf_t *cf, mpc_command_t *cmd, void *conf)
{
    mpc_instance_t  *ins = (mpc_instance_t *)conf;
    mpc_str_t       *value;
    struct hostent  *he;

    if (ins->use_addr) {
        return "duplicate \"address\"";
    }

    value = cf->args->elem;

    if (inet_aton((char *)value[1].data, &ins->addr.sin_addr) == 0) {
        if ((he = gethostbyname((char *)value[1].data)) == NULL) {
            mpc_conf_log_error(MPC_LOG_EMERG, cf, h_errno,
                               "invalid addr \"%s\"", value[1].data);
            return MPC_CONF_ERROR;
        }

        mpc_memcpy(&ins->addr.sin_addr, he->h_addr, sizeof(struct in_addr));
    }

    ins->use_addr = 1;

    return MPC_CONF_OK;
}


static char *
mpc_conf_http_method(mpc_conf_t *cf, mpc_command_t *cmd, void *conf)
{
    mpc_instance_t  *ins = (mpc_instance_t *)conf;
    mpc_str_t       *value;

    if (ins->http_method != MPC_CONF_UNSET) {
        return "duplicate \"http_method\"";
    }

    value = cf->args->elem;

    ins->http_method = mpc_http_get_method((char *)value[1].data);
    if (ins->http_method == MPC_ERROR) {
        mpc_conf_log_error(MPC_LOG_EMERG, cf, 0,
                           "invalid http method \"%s\"", value[1].data);
        return MPC_CONF_ERROR;
    }

    return MPC_CONF_OK;
}


static char *
mpc_conf_run_time(mpc_conf_t *cf, mpc_command_t *cmd, void *conf)
{
    mpc_instance_t  *ins = (mpc_instance_t *)conf;
    mpc_str_t       *value;

    if (ins->run_time != MPC_CONF_UNSET_UINT) {
        return "duplicate \"run_time\"";
    }

    value = cf->args->elem;

    ins->run_time = mpc_parse_time(&value[1], 1);
    if (ins->run_time == MPC_ERROR) {
        mpc_conf_log_error(MPC_LOG_EMERG, cf, 0,
                           "valid run time \"%V\"", &value[1]);
        return MPC_CONF_ERROR;
    }

    return MPC_CONF_OK;
}


static void
mpc_instance_merge(mpc_instance_t *ins, mpc_instance_t *tmp_ins)
{
    mpc_conf_merge_str_value(ins->url_file, tmp_ins->url_file, "");
    mpc_conf_merge_str_value(ins->result_file, tmp_ins->result_file, "");
    mpc_conf_merge_str_value(ins->result_mark, tmp_ins->result_mark, "");
    mpc_conf_merge_str_value(ins->log_file, tmp_ins->log_file, "");

    mpc_conf_merge_value(ins->log_level, tmp_ins->log_level, MPC_LOG_INFO);
    mpc_conf_merge_value(ins->http_method, tmp_ins->http_method, 
                         MPC_HTTP_METHOD_GET);
    mpc_conf_merge_uint_value(ins->concurrency, tmp_ins->concurrency, 
                              MPC_DEFAULT_CONCURRENCY);
    mpc_conf_merge_uint_value(ins->run_time, tmp_ins->run_time, 0);
    mpc_conf_merge_value(ins->follow_location, tmp_ins->follow_location,
                         MPC_CONF_UNSET);
    mpc_conf_merge_value(ins->replay, tmp_ins->replay, 0);

    if (ins->use_addr == 0 && tmp_ins->use_addr) {
        ins->use_addr = 1;
        ins->addr = tmp_ins->addr;
    }
}


static void
mpc_instance_init(mpc_instance_t *ins)
{
    mpc_str_null(&ins->conf_file);
    mpc_str_null(&ins->url_file);
    mpc_str_null(&ins->result_file);
    mpc_str_null(&ins->result_mark);
    mpc_str_null(&ins->log_file);

    ins->log_level = MPC_CONF_UNSET;
    ins->http_method = MPC_CONF_UNSET;
    ins->concurrency = MPC_CONF_UNSET_UINT;
    ins->run_time = MPC_CONF_UNSET_UINT;

    ins->follow_location = MPC_CONF_UNSET;
    ins->replay = MPC_CONF_UNSET;

    ins->use_addr = 0;
    ins->http_count = 0;
    TAILQ_INIT(&ins->http_hdr);

    ins->urls = NULL;
    ins->el = NULL;
    ins->self_pipe[0] = -1;
    ins->self_pipe[1] = -1;
    ins->stat = NULL;
}


int 
main(int argc, char **argv)
{
    int             fd;
    mpc_conf_t      conf;
    mpc_instance_t  tmp_ins;

    mpc_ins = (mpc_instance_t *)mpc_calloc(sizeof(mpc_instance_t), 1);
    if (mpc_ins == NULL) {
        mpc_log_stderr(errno, "oom!");
        exit(1);
    }
    
    mpc_instance_init(mpc_ins);

    if (mpc_get_options(argc, argv, mpc_ins) != MPC_OK) {
        mpc_show_usage();
        exit(1);
    }

    if (show_version) {
        printf("mpc: A asynchronous HTTP benchmark tool" CRLF
               "version: %s-%s" CRLF
               "compiled at %s %s" CRLF
               CRLF,
               MPC_VERSION_STR, MPC_VERSION_STATUS, __DATE__, __TIME__);

        if (show_help) {
            mpc_show_usage();
        }

        exit(0);
    }

    if (mpc_ins->conf_file.len != 0) {
        mpc_memzero(&tmp_ins, sizeof(mpc_instance_t));
        mpc_instance_init(mpc_ins);

        mpc_memzero(&conf, sizeof(mpc_conf_t));
        conf.ctx = (void *)&tmp_ins;
        conf.commands = mpc_conf_commands;

        if (mpc_conf_parse(&conf, &mpc_ins->conf_file) != MPC_OK) {
            exit(0);
        }

        mpc_instance_merge(mpc_ins, &tmp_ins);
    }

    if (mpc_ins->url_file.len == 0) {
        mpc_log_stderr(errno, "no url file specified");
        mpc_show_usage();
        exit(1);
    }

    mpc_rlimit_reset();

    mpc_ins->stat = mpc_stat_create();
    if (mpc_ins->stat == NULL) {
        mpc_log_stderr(errno, "oom!");
        exit(1);
    }

    if (mpc_core_init(mpc_ins) != MPC_OK) {
        exit(1);
    }

    if (mpc_core_run(mpc_ins) != MPC_OK) {
        exit(1);
    }

    if (mpc_core_deinit(mpc_ins) != MPC_OK) {
        exit(1);
    }

    mpc_stat_print(mpc_ins->stat);

    if (mpc_ins->result_file.len != 0) {
        fd = mpc_stat_result_create((char *)mpc_ins->result_file.data);
        if (fd != MPC_ERROR) {
            mpc_stat_result_record(fd, mpc_ins->stat, 
                                   (char *)mpc_ins->result_mark.data);
            mpc_stat_result_close(fd);
        }
    }

    mpc_stat_destroy(mpc_ins->stat);

    if (mpc_ins->conf_file.len != 0) {
        mpc_conf_free(&conf);
    }

    mpc_free(mpc_ins);

    exit(0);
}


void
mpc_stop()
{
    mpc_ins->stat->stop = mpc_time_ms();
    mpc_event_stop(mpc_ins->el, 0);
}


static void
mpc_rlimit_reset()
{
    struct rlimit  rlim;

    rlim.rlim_cur = MPC_MAX_OPENFILES;
    rlim.rlim_max = MPC_MAX_OPENFILES;
    setrlimit(RLIMIT_NOFILE, &rlim);

    rlim.rlim_cur = 1 << 29;
    rlim.rlim_max = 1 << 29;
    setrlimit(RLIMIT_CORE, &rlim);
}
