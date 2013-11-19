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

#define MPC_CONF_BUFFER      4096


static int64_t mpc_open_glob(mpc_glob_t *gl);
static int64_t mpc_read_glob(mpc_glob_t *gl, mpc_str_t *name);
static void mpc_close_glob(mpc_glob_t *gl);
static int64_t mpc_conf_handler(mpc_conf_t *cf, int64_t last);
static int64_t mpc_conf_read_token(mpc_conf_t *cf);
//static int64_t mpc_conf_test_full_name(mpc_str_t *name);
static char *mpc_conf_include(mpc_conf_t *cf, mpc_command_t *cmd, void *conf);
static void mpc_conf_free_args(mpc_array_t *args);


static uint64_t argument_number[] = {
    MPC_CONF_NOARGS,
    MPC_CONF_TAKE1,
    MPC_CONF_TAKE2,
    MPC_CONF_TAKE3,
    MPC_CONF_TAKE4,
    MPC_CONF_TAKE5,
    MPC_CONF_TAKE6,
    MPC_CONF_TAKE7
};


static mpc_command_t  mpc_internal_commands[] = {

    { mpc_string("include"),
      MPC_CONF_TAKE1,
      mpc_conf_include,
      0,
      0,
      NULL },

      mpc_null_command
};


char *
mpc_conf_param(mpc_conf_t *cf, mpc_str_t *param)
{
    char             *rv;
    mpc_buf_t         b;
    mpc_conf_file_t   conf_file;

    if (param->len == 0) {
        return MPC_CONF_OK;
    }

    mpc_memzero(&conf_file, sizeof(mpc_conf_file_t));

    mpc_memzero(&b, sizeof(mpc_buf_t));

    b.start = param->data;
    b.pos = param->data;
    b.last = param->data + param->len;
    b.end = b.last;

    conf_file.file.fd = MPC_INVALID_FILE;
    conf_file.file.name.data = NULL;
    conf_file.line = 0;

    cf->conf_file = &conf_file;
    cf->conf_file->buffer = &b;

    rv = mpc_conf_parse(cf, NULL);

    cf->conf_file = NULL;

    return rv;
}


static ssize_t
mpc_read_file(mpc_file_t *file, uint8_t *buf, size_t size, off_t offset)
{
    ssize_t  n;

    if (file->sys_offset != offset) {
        if (lseek(file->fd, offset, SEEK_SET) != -1) {
            mpc_log_crit(errno, "lseek() \"%s\" failed", file->name.data);
            return MPC_ERROR;
        }

        file->sys_offset = offset;
    }

    n = read(file->fd, buf, size);

    if (n == -1) {
        mpc_log_crit(errno, "read() \"%s\" failed", file->name.data);
        return MPC_ERROR;
    }

    file->sys_offset += n;

    file->offset += n;

    return n;
}


char *
mpc_conf_parse(mpc_conf_t *cf, mpc_str_t *filename)
{
    char              *rv;
    int                fd;
    int64_t            rc;
    mpc_buf_t          buf;
    mpc_conf_file_t   *prev, conf_file;
    mpc_command_t     *mpc_conf_commands;
    mpc_array_t      **arg_array;
    enum {
        parse_file = 0,
        parse_block,
        parse_param
    } type;

    fd = MPC_INVALID_FILE;
    prev = NULL;
    mpc_conf_commands = cf->commands;

    if (mpc_conf_commands == NULL) {
        mpc_log_emerg(errno, "no command supplied");
        return MPC_CONF_ERROR;
    }

    if (cf->args_array == NULL) {
        cf->args_array = mpc_array_create(10, sizeof(mpc_array_t *));
        if (cf->args_array == NULL) {
            return MPC_CONF_ERROR;
        }
    }

    if (filename) {

        /* open configuration file */

        fd = open((const char *)filename->data, O_RDONLY, 0);
        if (fd == MPC_INVALID_FILE) {
            mpc_conf_log_error(MPC_LOG_EMERG, cf, errno,
                               "open \"%s\" failed", filename->data);
            return MPC_CONF_ERROR;
        }

        prev = cf->conf_file;

        mpc_memzero(&conf_file, sizeof(mpc_conf_file_t));

        cf->conf_file = &conf_file;
        if (fstat(fd, &cf->conf_file->file.info) == -1) {
            mpc_log_emerg(errno, "stat \"%s\" failed", filename->data);
            goto failed;
        }

        cf->conf_file->buffer = &buf;

        buf.start = mpc_alloc(MPC_CONF_BUFFER);
        if (buf.start == NULL) {
            goto failed;
        }

        buf.pos = buf.start;
        buf.last = buf.start;
        buf.end = buf.last + MPC_CONF_BUFFER;

        cf->conf_file->file.fd = fd;
        cf->conf_file->file.name.len = filename->len;
        cf->conf_file->file.name.data = filename->data;
        cf->conf_file->file.offset = 0;
        cf->conf_file->line = 1;

        type = parse_file;

    } else if (cf->conf_file->file.fd != MPC_INVALID_FILE) {

        type = parse_block;

    } else {
        type = parse_param;
    }


    for ( ;; ) {

        rc = mpc_conf_read_token(cf);

        /*
         * mpc_conf_read_token() may return
         *
         *    MPC_ERROR              there is error
         *    MPC_OK                 the token terminated by ";" was found
         *    MPC_CONF_BLOCK_START   the token terminated by "{" was found
         *    MPC_CONF_BLOCK_DONE    the "}" was found
         *    MPC_CONF_FILE_DONE     the configuration file is done
         */

        if (rc == MPC_ERROR) {
            goto done;
        }

        if (rc == MPC_CONF_BLOCK_DONE) {

            if (type != parse_block) {
                mpc_conf_log_error(MPC_LOG_EMERG, cf, 0, "unexpected \"}\"");
                goto failed;
            }

            goto done;
        }

        if (rc == MPC_CONF_FILE_DONE) {

            if (type == parse_block) {
                mpc_conf_log_error(MPC_LOG_EMERG, cf, 0,
                                   "unexpected end of file, expecting \"}\"");
                goto failed;
            }

            goto done;
        }

        if (rc == MPC_CONF_BLOCK_START) {

            if (type == parse_param) {
                mpc_conf_log_error(MPC_LOG_EMERG, cf, 0,
                                   "block directives are not supported "
                                   "in -g option");
                goto failed;
            }
        }

        /* rc == MPC_OK || rc == MPC_CONF_BLOCK_START */

        if (cf->handler) {

            /*
             * the custom handler
             */

            rv = (*cf->handler)(cf, NULL, cf->handler_conf);
            if (rv == MPC_CONF_OK) {
                goto next;
            }

            if (rv == MPC_CONF_ERROR) {
                goto failed;
            }

            mpc_conf_log_error(MPC_LOG_EMERG, cf, 0, rv);

            goto failed;
        }

        rc = mpc_conf_handler(cf, rc);

        if (rc == MPC_ERROR) {
            goto failed;
        }

next:

        arg_array = mpc_array_push(cf->args_array);
        *arg_array = cf->args;
        cf->args = NULL;
    }

failed:

    rc = MPC_ERROR;

done:

    if (filename) {
        if (cf->conf_file->buffer->start) {
            mpc_free(cf->conf_file->buffer->start);
        }

        if (close(fd) == MPC_FILE_ERROR) {
            mpc_log_alert(errno, "close \"%s\" failed", filename->data);
            return MPC_CONF_ERROR;
        }

        cf->conf_file = prev;
    }

    if (rc == MPC_ERROR) {
        return MPC_CONF_ERROR;
    }

    return MPC_CONF_OK;
}


static int64_t
mpc_conf_handler(mpc_conf_t *cf, int64_t last)
{
    char          *rv;
    mpc_str_t      *name;
    mpc_command_t  *cmd;

    name = cf->args->elem;

    cmd = cf->commands;

    for ( /* void */; cmd->name.len; cmd++) {

        if (name->len != cmd->name.len) {
            continue;
        }

        if (mpc_strcmp(name->data, cmd->name.data) != 0) {
            continue;
        }

        if (!(cmd->type & MPC_CONF_BLOCK) && last != MPC_OK) {
            mpc_conf_log_error(MPC_LOG_EMERG, cf, 0,
                               "directive \"%s\" is not terminated by \";\"",
                               name->data);
            return MPC_ERROR;
        }

        if ((cmd->type & MPC_CONF_BLOCK) && last != MPC_CONF_BLOCK_START) {
            mpc_conf_log_error(MPC_LOG_EMERG, cf, 0,
                               "directive \"%s\" has no opening \"{\"",
                               name->data);
            return MPC_OK;
        }

        /* is the directive's argument count right ? */

        if (!(cmd->type & MPC_CONF_ANY)) {

            if (cmd->type & MPC_CONF_FLAG) {

                if (cf->args->nelem != 2) {
                    goto invalid;
                }

            } else if (cmd->type & MPC_CONF_1MORE) {

                if (cf->args->nelem < 2) {
                    goto invalid;
                }

            } else if (cmd->type & MPC_CONF_2MORE) {

                if (cf->args->nelem < 3) {
                    goto invalid;
                }

            } else if (cf->args->nelem > MPC_CONF_MAX_ARGS) {

                goto invalid;

            } else if (!(cmd->type & argument_number[cf->args->nelem - 1])) {

                goto invalid;
            }
        }

        rv = cmd->set(cf, cmd, cf->ctx);

        if (rv == MPC_CONF_OK) {
            return MPC_OK;
        }

        if (rv == MPC_CONF_ERROR) {
            return MPC_ERROR;
        }

        mpc_conf_log_error(MPC_LOG_EMERG, cf, 0,
                           "\"%s\" directive %s", name->data, rv);

        return MPC_ERROR;
    }

    cmd = mpc_internal_commands;

    for ( /* void */; cmd->name.len; cmd++) {

        if (name->len != cmd->name.len) {
            continue;
        }

        if (mpc_strcmp(name->data, cmd->name.data) != 0) {
            continue;
        }

        if (!(cmd->type & MPC_CONF_BLOCK) && last != MPC_OK) {
            mpc_conf_log_error(MPC_LOG_EMERG, cf, 0,
                               "directive \"%s\" is not terminated by \";\"",
                               name->data);
            return MPC_ERROR;
        }

        if ((cmd->type & MPC_CONF_BLOCK) && last != MPC_CONF_BLOCK_START) {
            mpc_conf_log_error(MPC_LOG_EMERG, cf, 0,
                               "directive \"%s\" has no opening \"{\"",
                               name->data);
            return MPC_OK;
        }

        /* is the directive's argument count right ? */

        if (!(cmd->type & MPC_CONF_ANY)) {

            if (cmd->type & MPC_CONF_FLAG) {

                if (cf->args->nelem != 2) {
                    goto invalid;
                }

            } else if (cmd->type & MPC_CONF_1MORE) {

                if (cf->args->nelem < 2) {
                    goto invalid;
                }

            } else if (cmd->type & MPC_CONF_2MORE) {

                if (cf->args->nelem < 3) {
                    goto invalid;
                }

            } else if (cf->args->nelem > MPC_CONF_MAX_ARGS) {

                goto invalid;

            } else if (!(cmd->type & argument_number[cf->args->nelem - 1])) {

                goto invalid;
            }
        }

        rv = cmd->set(cf, cmd, cf->ctx);

        if (rv == MPC_CONF_OK) {
            return MPC_OK;
        }

        if (rv == MPC_CONF_ERROR) {
            return MPC_ERROR;
        }

        mpc_conf_log_error(MPC_LOG_EMERG, cf, 0,
                           "\"%s\" directive %s", name->data, rv);

        return MPC_ERROR;
    }

    mpc_conf_log_error(MPC_LOG_EMERG, cf, 0,
                       "unknown directive \"%s\"", name->data);

    return MPC_ERROR;

invalid:

    mpc_conf_log_error(MPC_LOG_EMERG, cf, 0,
                       "invalid number of arguments in \"%s\" directive",
                       name->data);

    return MPC_ERROR;
}


static int64_t
mpc_conf_read_token(mpc_conf_t *cf)
{
    uint8_t         *start, ch, *src, *dst;
    off_t            file_size;
    size_t           len;
    ssize_t          n, size;
    uint64_t         found, need_space, last_space, sharp_comment, variable;
    uint64_t         quoted, s_quoted, d_quoted, start_line;
    mpc_str_t       *word;
    mpc_buf_t       *b;

    found = 0;
    need_space = 0;
    last_space = 1;
    sharp_comment = 0;
    variable = 0;
    quoted = 0;
    s_quoted = 0;
    d_quoted = 0;

    if (cf->args != NULL) {
        mpc_conf_free_args(cf->args);
        cf->args = NULL;
    }

    cf->args = mpc_array_create(4, sizeof(mpc_str_t));
    if (cf->args == NULL) {
        return MPC_ERROR;
    }

    b = cf->conf_file->buffer;
    start = b->pos;
    start_line = cf->conf_file->line;

    file_size = mpc_file_size(&cf->conf_file->file.info);

    for ( ;; ) {

        if (b->pos >= b->last) {

            if (cf->conf_file->file.offset >= file_size) {

                if (cf->args->nelem > 0 || !last_space) {

                    if (cf->conf_file->file.fd == MPC_INVALID_FILE) {
                        mpc_conf_log_error(MPC_LOG_EMERG, cf, 0,
                                           "unexpected end of parameter, "
                                           "expecting \";\"");
                        return MPC_ERROR;
                    }

                    mpc_conf_log_error(MPC_LOG_EMERG, cf, 0,
                                       "unexpected end of file, "
                                       "expecting \";\" or \"}\"");
                    return MPC_ERROR;
                }

                return MPC_CONF_FILE_DONE;
            }

            len = b->pos - start;

            if (len == MPC_CONF_BUFFER) {
                cf->conf_file->line = start_line;

                if (d_quoted) {
                    ch = '"';

                } else if (s_quoted) {
                    ch = '\'';

                } else {
                    mpc_conf_log_error(MPC_LOG_EMERG, cf, 0,
                                       "too long parameter \"%*s...\" started",
                                       10, start);
                    return MPC_ERROR;
                }

                mpc_conf_log_error(MPC_LOG_EMERG, cf, 0,
                                   "too long parameter, probably "
                                   "missing terminating \"%c\" character", ch);
                return MPC_ERROR;
            }

            if (len) {
                mpc_memmove(b->start, start, len);
            }

            size = (ssize_t) (file_size - cf->conf_file->file.offset);

            if (size > b->end - (b->start + len)) {
                size = b->end - (b->start + len);
            }

            n = mpc_read_file(&cf->conf_file->file, b->start + len, size,
                              cf->conf_file->file.offset);

            if (n == MPC_ERROR) {
                return MPC_ERROR;
            }

            if (n != size) {
                mpc_conf_log_error(MPC_LOG_EMERG, cf, 0,
                                   "read returned only %z bytes instead of %z",
                                   n, size);
                return MPC_ERROR;
            }

            b->pos = b->start + len;
            b->last = b->pos + n;
            start = b->start;
        }

        ch = *b->pos++;

        if (ch == LF) {
            cf->conf_file->line++;

            if (sharp_comment) {
                sharp_comment = 0;
            }
        }

        if (sharp_comment) {
            continue;
        }

        if (quoted) {
            quoted = 0;
            continue;
        }

        if (need_space) {
            if (ch == ' ' || ch == '\t' || ch == CR || ch == LF) {
                last_space = 1;
                need_space = 0;
                continue;
            }

            if (ch == ';') {
                return MPC_OK;
            }

            if (ch == '{') {
                return MPC_CONF_BLOCK_START;
            }

            if (ch == ')') {
                last_space = 1;
                need_space = 0;

            } else {
                mpc_conf_log_error(MPC_LOG_EMERG, cf, 0,
                                   "unexpected \"%c\"", ch);
                return MPC_ERROR;
            }
        }

        if (last_space) {

            if (ch == ' ' || ch == '\t' || ch == CR || ch == LF) {
                continue;
            }

            start = b->pos - 1;
            start_line = cf->conf_file->line;

            switch (ch) {
            case ';':
            case '{':
                if (cf->args->nelem == 0) {
                    mpc_conf_log_error(MPC_LOG_EMERG, cf, 0,
                                       "unexpected \"%c\"", ch);
                    return MPC_ERROR;
                }

                if (ch == '{') {
                    return MPC_CONF_BLOCK_START;
                }

                return MPC_OK;

            case '}':
                if (cf->args->nelem != 0) {
                    mpc_conf_log_error(MPC_LOG_EMERG, cf, 0,
                                       "unexpected \"}\"");
                    return MPC_ERROR;
                }

                return MPC_CONF_BLOCK_DONE;

            case '#':
                sharp_comment = 1;
                continue;

            case '\\':
                quoted = 1;
                last_space = 0;
                continue;
            
            case '"':
                start++;
                d_quoted = 1;
                last_space = 0;
                continue;

            case '\'':
                start++;
                s_quoted = 1;
                last_space = 0;
                continue;

            default:
                last_space = 0;
            }

        } else {

            if (ch == '{' && variable) {
                continue;
            }

            variable = 0;

            if (ch == '\\') {
                quoted = 1;
                continue;
            }

            if (ch == '$') {
                variable = 1;
                continue;
            }

            if (d_quoted) {
                if (ch == '"') {
                    d_quoted = 0;
                    need_space = 1;
                    found = 1;
                }

            } else if (s_quoted) {
                if (ch == '\'') {
                    s_quoted = 0;
                    need_space = 1;
                    found = 1;
                }

            } else if (ch == ' ' || ch == '\t' || ch == CR || ch == LF
                       || ch == ';' || ch == '{')
            {
                last_space = 1;
                found = 1;
            }

            if (found) {
                word = mpc_array_push(cf->args);
                if (word == NULL) {
                    return MPC_ERROR;
                }

                word->data = mpc_alloc(b->pos - start + 1);
                if (word->data == NULL) {
                    return MPC_ERROR;
                }

                for (dst = word->data, src = start, len = 0;
                     src < b->pos - 1;
                     len++)
                {
                    if (*src == '\\') {
                        switch (src[1]) {
                        case '"':
                        case '\'':
                        case '\\':
                            src++;
                            break;

                        case 't':
                            *dst++ = '\t';
                            src += 2;
                            continue;

                        case 'r':
                            *dst++ = '\r';
                            src += 2;
                            continue;

                        case 'n':
                            *dst++ = '\n';
                            src += 2;
                            continue;
                        }
                    }
                    *dst++ = *src++;
                }
                *dst = '\0';
                word->len = len;

                if (ch == ';') {
                    return MPC_OK;
                }

                if (ch == '{') {
                    return MPC_CONF_BLOCK_START;
                }

                found = 0;
            }
        }
    }
}


char *
mpc_conf_include(mpc_conf_t *cf, mpc_command_t *cmd, void *conf)
{
    char        *rv;
    int64_t      n;
    mpc_str_t   *value, file, name;
    mpc_glob_t   gl;

    value = cf->args->elem;
    file = value[1];
    
    if (strpbrk((char *)file.data, "*?[") == NULL) {
        return mpc_conf_parse(cf, &file);
    }

    mpc_memzero(&gl, sizeof(mpc_glob_t));
    gl.pattern = file.data;
    gl.test = 1;

    if (mpc_open_glob(&gl) != MPC_OK) {
        mpc_conf_log_error(MPC_LOG_EMERG, cf, errno,
                           "glob() \"%s\" failed", file.data);
        return MPC_CONF_ERROR;
    }

    rv = MPC_CONF_OK;

    for ( ;; ) {

        n = mpc_read_glob(&gl, &name);

        if (n != MPC_OK) {
            break;
        }

        file.len = name.len++;

        file.data = mpc_alloc(file.len);
        if (file.data == NULL) {
            return MPC_CONF_ERROR;
        }
        mpc_cpystrn(file.data, name.data, file.len);

        rv = mpc_conf_parse(cf, &file);

        mpc_free(file.data);

        if (rv != MPC_CONF_OK) {
            break;
        }
    }

    mpc_close_glob(&gl);

    return rv;
}


void
mpc_conf_log_error(uint64_t level, mpc_conf_t *cf, int err, 
    const char *fmt, ...)
{
    uint8_t  errstr[MPC_MAX_CONF_ERRSTR], *p, *last;
    va_list  args;

    last = errstr + MPC_MAX_CONF_ERRSTR;

    va_start(args, fmt);
    p = mpc_vslprintf(errstr, last, fmt, args);
    va_end(args);

    if (err) {
        p = mpc_log_errno(p, last, err);
    }

    if (cf->conf_file == NULL) {
        mpc_log_core(__FILE__, __LINE__, level, 0, "%*s", p - errstr, errstr);
        return;
    }

    if (cf->conf_file->file.fd == MPC_INVALID_FILE) {
        mpc_log_core(__FILE__, __LINE__, level, 0, "%*s in command line",
                     p - errstr, errstr);
        return;
    }

    mpc_log_core(__FILE__, __LINE__, level, 0, "%*s in %s:%ud",
                 p - errstr, errstr,
                 cf->conf_file->file.name.data, cf->conf_file->line);
}


static void
mpc_conf_free_args(mpc_array_t *args)
{
    int         i;
    mpc_str_t  *arg;

    if (args != NULL) {
        arg = args->elem;
        for (i = 0; i < args->nelem; i++) {
            mpc_free(arg[i].data);
            arg[i].data = NULL;
            arg[i].len = 0;
        }

        mpc_array_destroy(args);
    }
}


void
mpc_conf_free(mpc_conf_t *cf)
{
    int64_t       i;
    mpc_array_t  *arg_array, **args_array;

    if (cf->args_array) {
        args_array = cf->args_array->elem;
        for (i = 0; i < cf->args_array->nelem; i++) {
            arg_array = args_array[i];
            mpc_conf_free_args(arg_array);
        }
        mpc_array_destroy(cf->args_array);
    }

    if (cf->args) {
        mpc_conf_free_args(cf->args);
    }
}


char *
mpc_conf_set_flag_slot(mpc_conf_t *cf, mpc_command_t *cmd, void *conf)
{
    char             *p = conf;
    mpc_str_t        *value;
    mpc_flag_t       *fp;
    mpc_conf_post_t  *post;

    fp = (mpc_flag_t *)(p + cmd->offset);

    if (*fp != MPC_CONF_UNSET) {
        return "is duplimpcte";
    }

    value = cf->args->elem;

    if (mpc_strcasecmp(value[1].data, (uint8_t *) "on") == 0) {
        *fp = 1;

    } else if (mpc_strcasecmp(value[1].data, (uint8_t *) "off") == 0) {
        *fp = 0;

    } else {
        mpc_conf_log_error(MPC_LOG_EMERG, cf, 0, 
                           "invalid value \"%s\" in \"%s\" directive,"
                           "it must be \"on\" or \"off\"",
                           value[1].data, cmd->name.data);

        return MPC_CONF_ERROR;
    }

    if (cmd->post) {
        post = cmd->post;
        return post->post_handler(cf, post, fp);
    }

    return MPC_CONF_OK;
}


char *
mpc_conf_set_str_slot(mpc_conf_t *cf, mpc_command_t *cmd, void *conf)
{
    char             *p = conf;
    mpc_str_t        *field, *value;
    mpc_conf_post_t  *post;

    field = (mpc_str_t *) (p + cmd->offset);

    if (field->data) {
        return "is duplimpcte";
    }

    value = cf->args->elem;

    *field = value[1];

    if (cmd->post) {
        post = cmd->post;
        return post->post_handler(cf, post, field);
    }

    return MPC_CONF_OK;
}


char *
mpc_conf_set_str_array_slot(mpc_conf_t *cf, mpc_command_t *cmd, void *conf)
{
    char              *p = conf;
    mpc_str_t         *value, *s;
    mpc_array_t      **a;
    mpc_conf_post_t   *post;

    a = (mpc_array_t **) (p + cmd->offset);

    if (*a == MPC_CONF_UNSET_PTR) {
        *a = mpc_array_create(4, sizeof(mpc_str_t));
        if (*a == NULL) {
            return MPC_CONF_ERROR;
        }
    }

    s = mpc_array_push(*a);
    if (s == NULL) {
        return MPC_CONF_ERROR;
    }

    value = cf->args->elem;

    *s = value[1];

    if (cmd->post) {
        post = cmd->post;
        return post->post_handler(cf, post, a);
    }

    return MPC_CONF_OK;
}


char *
mpc_conf_set_keyval_slot(mpc_conf_t *cf, mpc_command_t *cmd, void *conf)
{
    char              *p = conf;
    mpc_str_t         *value;
    mpc_array_t      **a;
    mpc_keyval_t      *kv;
    mpc_conf_post_t   *post;

    a = (mpc_array_t **) (p + cmd->offset);

    if (*a == NULL) {
        *a = mpc_array_create(4, sizeof(mpc_keyval_t));
        if (*a == NULL) {
            return MPC_CONF_ERROR;
        }
    }

    kv = mpc_array_push(*a);
    if (kv == NULL) {
        return MPC_CONF_ERROR;
    }

    value = cf->args->elem;

    kv->key = value[1];
    kv->value = value[2];
    
    if (cmd->post) {
        post = cmd->post;
        return post->post_handler(cf, post, kv);
    }

    return MPC_CONF_OK;
}


char *
mpc_conf_set_num_slot(mpc_conf_t *cf, mpc_command_t *cmd, void *conf)
{
    char             *p = conf;
    int64_t          *np;
    mpc_str_t        *value;
    mpc_conf_post_t  *post;

    np = (int64_t *) (p + cmd->offset);

    if (*np != MPC_CONF_UNSET) {
        return "is duplimpcte";
    }

    value = cf->args->elem;

    *np = mpc_atoi(value[1].data, value[1].len);
    if (*np == MPC_ERROR) {
        return "invalid number";
    }

    if (cmd->post) {
        post = cmd->post;
        return post->post_handler(cf, post, np);
    }

    return MPC_CONF_OK;
}


char *
mpc_conf_set_size_slot(mpc_conf_t *cf, mpc_command_t *cmd, void *conf)
{
    char             *p = conf;
    size_t           *sp;
    mpc_str_t        *value;
    mpc_conf_post_t  *post;

    sp = (size_t *) (p + cmd->offset);

    if (*sp != MPC_CONF_UNSET_SIZE) {
        return "is duplimpcte";
    }

    value = cf->args->elem;

    *sp = mpc_parse_size(&value[1]);
    if (*sp == (size_t) MPC_ERROR) {
        return "invalid value";
    }

    if (cmd->post) {
        post = cmd->post;
        return post->post_handler(cf, post, sp);
    }

    return MPC_CONF_OK;
}


char *
mpc_conf_set_msec_slot(mpc_conf_t *cf, mpc_command_t *cmd, void *conf)
{
    char             *p = conf;
    uint64_t         *msp;
    mpc_str_t        *value;
    mpc_conf_post_t  *post;

    msp = (uint64_t *) (p + cmd->offset);
    if (*msp != MPC_CONF_UNSET_UINT) {
        return "is duplimpcte";
    }

    value = cf->args->elem;

    *msp = mpc_parse_time(&value[1], 0);
    if (*msp == MPC_ERROR) {
        return "invalid value";
    }

    if (cmd->post) {
        post = cmd->post;
        return post->post_handler(cf, post, msp);
    }

    return MPC_CONF_OK;
}


char *
mpc_conf_set_sec_slot(mpc_conf_t *cf, mpc_command_t *cmd, void *conf)
{
    char             *p = conf;
    time_t           *sp;
    mpc_str_t        *value;
    mpc_conf_post_t  *post;

    sp = (time_t *) (p + cmd->offset);
    if (*sp != MPC_CONF_UNSET) {
        return "is dupliate";
    }

    value = cf->args->elem;

    *sp = mpc_parse_time(&value[1], 1);
    if (*sp == (time_t) MPC_ERROR) {
        return "invalid value";
    }

    if (cmd->post) {
        post = cmd->post;
        return post->post_handler(cf, post, sp);
    }

    return MPC_CONF_OK;
}


char *
mpc_conf_set_enum_slot(mpc_conf_t *cf, mpc_command_t *cmd, void *conf)
{
    char             *p = conf;
    uint64_t         *np, i;
    mpc_str_t        *value;
    mpc_conf_enum_t  *e;

    np = (uint64_t *) (p + cmd->offset);

    if (*np != MPC_CONF_UNSET_UINT) {
        return "is duplimpcte";
    }

    value = cf->args->elem;
    e = cmd->post;

    for (i = 0; e[i].name.len != 0; i++) {
        if (e[i].name.len != value[i].len
            || mpc_strcasecmp(e[i].name.data, value[1].data) != 0)
        {
            continue;
        }

        *np = e[i].value;

        return MPC_CONF_OK;
    }

    mpc_conf_log_error(MPC_LOG_WARN, cf, 0,
                       "invalid value \"%s\"", value[1].data);

    return MPC_CONF_ERROR;
}


char *
mpc_conf_set_bitmask_slot(mpc_conf_t *cf, mpc_command_t *cmd, void *conf)
{
    char                *p = conf;
    uint64_t            *np, i, m;
    mpc_str_t           *value;
    mpc_conf_bitmask_t  *mask;

    np = (uint64_t *) (p + cmd->offset);
    value = cf->args->elem;
    mask = cmd->post;

    for (i = 1; i < cf->args->nelem; i++) {
        for (m = 0; mask[m].name.len != 0; m++) {

            if (mask[m].name.len != value[i].len
                || mpc_strcasecmp(mask[m].name.data, value[i].data) != 0)
            {
                continue;
            }

            if (*np & mask[m].mask) {
                mpc_conf_log_error(MPC_LOG_WARN, cf, 0,
                                   "duplimpcte value \"%s\"", value[i].data);

            } else {
                *np |= mask[m].mask;
            }

            break;
        }

        if (mask[m].name.len == 0) {
            mpc_conf_log_error(MPC_LOG_WARN, cf, 0,
                               "invalid value \"%s\"", value[i].data);

            return MPC_CONF_ERROR;
        }
    }

    return MPC_CONF_OK;
}


char *
mpc_conf_check_num_bounds(mpc_conf_t *cf, void *post, void *data)
{
    mpc_conf_num_bounds_t  *bounds = post;
    int64_t                *np = data;

    if (bounds->high == -1) {
        if (*np >= bounds->low) {
            return MPC_CONF_OK;
        }

        mpc_conf_log_error(MPC_LOG_EMERG, cf, 0,
                           "value must be equal to or greater than %i",
                           bounds->low);

        return MPC_CONF_ERROR;
    }

    if (*np >= bounds->low && *np <= bounds->high) {
        return MPC_CONF_OK;
    }

    mpc_conf_log_error(MPC_LOG_EMERG, cf, 0,
                       "value must be between %i and %i",
                       bounds->low, bounds->high);

    return MPC_CONF_ERROR;
}


static int64_t
mpc_open_glob(mpc_glob_t *gl)
{
    int  n;

    n = glob((char *) gl->pattern, 0, NULL, &gl->pglob);
    if (n == 0) {
        return MPC_OK;
    }

#ifdef GLOB_NOMATCH

    if (n == GLOB_NOMATCH && gl->test) {
        return MPC_OK;
    }

#endif

    return MPC_ERROR;
}


static int64_t
mpc_read_glob(mpc_glob_t *gl, mpc_str_t *name)
{
    size_t  count;

#ifdef GLOB_NOMATCH
    count = (size_t) gl->pglob.gl_pathc;
#else
    count = (size_t) gl->pglob.gl_matchc;
#endif

    if (gl->n < count) {

        name->len = (size_t) mpc_strlen(gl->pglob.gl_pathv[gl->n]);
        name->data = (uint8_t *) gl->pglob.gl_pathv[gl->n];
        gl->n++;

        return MPC_OK;
    }

    return MPC_DONE;
}


static void
mpc_close_glob(mpc_glob_t *gl)
{
    globfree(&gl->pglob);
}


int 
mpc_atoi(uint8_t *line, size_t n)
{
    int  value;

    if (n == 0) {
        return MPC_ERROR;
    }

    for (value = 0; n--; line++) {
        if (*line < '0' || *line > '9') {
            return MPC_ERROR;
        }

        value = value * 10 + (*line - '0');
    }

    if (value < 0) {
        return MPC_ERROR;
    }

    return value;
}


int
mpc_hextoi(uint8_t *line, size_t n)
{
    uint8_t     c, ch;
    int         value;

    if (n == 0) {
        return MPC_ERROR;
    }

    for (value = 0; n--; line++) {
        ch = *line;

        if (ch >= '0' && ch <= '9') {
            value = value * 16 + (ch - '0');
            continue;
        }

        c = (uint8_t) (ch | 0x20);

        if (c >= 'a' && c <= 'f') {
            value = value * 16 + (c - 'a' + 10);
            continue;
        }

        return MPC_ERROR;
    }

    if (value < 0) {
        return MPC_ERROR;

    } else {
        return value;
    }
}


uint8_t *
mpc_hex_dump(uint8_t *dst, uint8_t *src, size_t len)
{
    static uint8_t  hex[] = "0123456789abcdef";

    while (len--) {
        *dst++ = hex[*src >> 4];
        *dst++ = hex[*src++ & 0xf];
    }

    return dst;
}


ssize_t
mpc_atosz(uint8_t *line, size_t n)
{
    ssize_t  value;

    if (n == 0) {
        return MPC_ERROR;
    }

    for (value = 0; n--; line++) {
        if (*line < '0' || *line > '9') {
            return MPC_ERROR;
        }

        value = value * 10 + (*line - '0');
    }

    if (value < 0) {
        return MPC_ERROR;

    } else {
        return value;
    }
}


ssize_t
mpc_parse_size(mpc_str_t *line)
{
    uint8_t    unit;
    size_t     len;
    ssize_t    size;
    int64_t    scale;

    len = line->len;
    unit = line->data[len - 1];

    switch (unit) {
    case 'K':
    case 'k':
        len--;
        scale = 1024;
        break;

    case 'M':
    case 'm':
        len--;
        scale = 1024 * 1024;
        break;

    case 'G':
    case 'g':
        len--;
        scale = 1024 * 1024 * 1024;
        break;

    default:
        scale = 1;
    }

    size = mpc_atosz(line->data, len);
    if (size == MPC_ERROR) {
        return MPC_ERROR;
    }

    size *= scale;

    return size;
}


int64_t
mpc_parse_time(mpc_str_t *line, int is_sec)
{
    uint8_t     *p, *last;
    int64_t      value, total, scale;
    uint64_t     max, valid;
    enum {
        st_start = 0,
        st_year,
        st_month,
        st_week,
        st_day,
        st_hour,
        st_min,
        st_sec,
        st_msec,
        st_last
    } step;

    valid = 0;
    value = 0;
    total = 0;
    step = is_sec ? st_start : st_month;
    scale = is_sec ? 1 : 1000;

    p = line->data;
    last = p + line->len;

    while (p < last) {

        if (*p >= '0' && *p <= '9') {
            value = value * 10 + (*p++ - '0');
            valid = 1;
            continue;
        }

        switch (*p++) {

        case 'y':
            if (step > st_start) {
                return MPC_ERROR;
            }
            step = st_year;
            max = MPC_MAX_INT32_VALUE / (60 * 60 * 24 * 365);
            scale = 60 * 60 * 24 * 365;
            break;

        case 'M':
            if (step >= st_month) {
                return MPC_ERROR;
            }
            step = st_month;
            max = MPC_MAX_INT32_VALUE / (60 * 60 * 24 * 30);
            scale = 60 * 60 * 24 * 30;
            break;

        case 'w':
            if (step >= st_week) {
                return MPC_ERROR;
            }
            step = st_week;
            max = MPC_MAX_INT32_VALUE / (60 * 60 * 24 * 7);
            scale = 60 * 60 * 24 * 7;
            break;

        case 'd':
            if (step >= st_day) {
                return MPC_ERROR;
            }
            step = st_day;
            max = MPC_MAX_INT32_VALUE / (60 * 60 * 24);
            scale = 60 * 60 * 24;
            break;

        case 'h':
            if (step >= st_hour) {
                return MPC_ERROR;
            }
            step = st_hour;
            max = MPC_MAX_INT32_VALUE / (60 * 60);
            scale = 60 * 60;
            break;

        case 'm':
            if (*p == 's') {
                if (is_sec || step >= st_msec) {
                    return MPC_ERROR;
                }
                p++;
                step = st_msec;
                max = MPC_MAX_INT32_VALUE;
                scale = 1;
                break;
            }

            if (step >= st_min) {
                return MPC_ERROR;
            }
            step = st_min;
            max = MPC_MAX_INT32_VALUE / 60;
            scale = 60;
            break;

        case 's':
            if (step >= st_sec) {
                return MPC_ERROR;
            }
            step = st_sec;
            max = MPC_MAX_INT32_VALUE;
            scale = 1;
            break;

        case ' ':
            if (step >= st_sec) {
                return MPC_ERROR;
            }
            step = st_last;
            max = MPC_MAX_INT32_VALUE;
            scale = 1;
            break;

        default:
            return MPC_ERROR;
        }

        if (step != st_msec && !is_sec) {
            scale *= 1000;
            max /= 1000;
        }

        if ((uint64_t) value > max) {
            return MPC_ERROR;
        }

        total += value * scale;

        if ((uint64_t) total > MPC_MAX_INT32_VALUE) {
            return MPC_ERROR;
        }

        value = 0;
        scale = is_sec ? 1 : 1000;

        while (p < last && *p == ' ') {
            p++;
        }
    }

    if (valid) {
        return total + value * scale;
    }

    return MPC_ERROR;
}
