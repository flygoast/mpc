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


#ifndef __MPC_CONF_H_INCLUDED__
#define __MPC_CONF_H_INCLUDED__


#include <glob.h>
#include <sys/stat.h>


/*
 *        AAAA  number of arguments
 *      FF      commmand flags
 *    TT        dummy, not used
 */

#define MPC_CONF_NOARGS      0x00000001
#define MPC_CONF_TAKE1       0x00000002
#define MPC_CONF_TAKE2       0x00000004
#define MPC_CONF_TAKE3       0x00000008
#define MPC_CONF_TAKE4       0x00000010
#define MPC_CONF_TAKE5       0x00000020
#define MPC_CONF_TAKE6       0x00000040
#define MPC_CONF_TAKE7       0x00000080

#define MPC_CONF_MAX_ARGS    8

#define MPC_CONF_TAKE12      (MPC_CONF_TAKE1|MPC_CONF_TAKE2)
#define MPC_CONF_TAKE13      (MPC_CONF_TAKE1|MPC_CONF_TAKE3)

#define MPC_CONF_TAKE23      (MPC_CONF_TAKE2|MPC_CONF_TAKE3)

#define MPC_CONF_TAKE123     (MPC_CONF_TAKE1|MPC_CONF_TAKE2|MPC_CONF_TAKE3)
#define MPC_CONF_TAKE1234    (MPC_CONF_TAKE1|MPC_CONF_TAKE2|MPC_CONF_TAKE3  \
                             |MPC_CONF_TAKE4)

#define MPC_CONF_ARGS_NUMBER 0x000000ff
#define MPC_CONF_BLOCK       0x00000100
#define MPC_CONF_FLAG        0x00000200
#define MPC_CONF_ANY         0x00000400
#define MPC_CONF_1MORE       0x00000800
#define MPC_CONF_2MORE       0x00001000


#define MPC_CONF_UNSET       -1
#define MPC_CONF_UNSET_UINT  (uint64_t) -1
#define MPC_CONF_UNSET_PTR   (void *) -1
#define MPC_CONF_UNSET_SIZE  (size_t) -1


#define MPC_CONF_OK          0
#define MPC_CONF_ERROR       (void *)-1


#define MPC_CONF_BLOCK_START 1
#define MPC_CONF_BLOCK_DONE  2
#define MPC_CONF_FILE_DONE   3


#define MPC_MAX_CONF_ERRSTR  1024


typedef struct mpc_command_s  mpc_command_t;
typedef struct mpc_conf_s  mpc_conf_t;


typedef struct {
    size_t      n;
    glob_t      pglob;
    uint8_t    *pattern;
    uint64_t    test;
} mpc_glob_t;


typedef struct {
    int          fd;
    mpc_str_t    name;
    struct stat  info;
    off_t        offset;
    off_t        sys_offset;
} mpc_file_t;


typedef struct {
    mpc_file_t   file;
    mpc_buf_t   *buffer;
    uint64_t     line;
} mpc_conf_file_t;


struct mpc_command_s {
    mpc_str_t        name;
    uint64_t         type;
    char          *(*set)(mpc_conf_t *cf, mpc_command_t *cmd, void *conf);
    uint64_t         conf;   /* dummy */
    uint64_t         offset;
    void            *post;
};


#define mpc_null_command  { mpc_null_string, 0, NULL, 0, 0, NULL }


typedef char *(*mpc_conf_handler_pt)(mpc_conf_t *cf, mpc_command_t *dummy,
    void *conf);


struct mpc_conf_s {
    char                 *name;
    mpc_array_t          *args;
    mpc_conf_file_t      *conf_file;
    void                 *ctx;
    mpc_command_t        *commands;
    mpc_array_t          *args_array;
    mpc_conf_handler_pt   handler;
    char                 *handler_conf;
};


typedef char *(*mpc_conf_post_handler_pt)(mpc_conf_t *cf, void *data, void *conf);

typedef struct {
    mpc_conf_post_handler_pt  post_handler;
} mpc_conf_post_t;


typedef struct {
    mpc_conf_post_handler_pt  post_handler;
    int64_t                   low;
    int64_t                   high;
} mpc_conf_num_bounds_t;


typedef struct {
    mpc_str_t                 name;
    uint64_t                  value;
} mpc_conf_enum_t;


#define MPC_CONF_BITMASK_SET  1


typedef struct {
    mpc_str_t                 name;
    uint64_t                  mask;
} mpc_conf_bitmask_t;


char *mpc_conf_check_num_bounds(mpc_conf_t *cf, void *post, void *data);


#define mpc_conf_init_value(conf, default)               \
    if (conf == MPC_CONF_UNSET) {                        \
        conf = default;                                  \
    }

#define mpc_conf_init_ptr_value(conf, default)           \
    if (conf == MPC_CONF_UNSET_PTR) {                    \
        conf = default;                                  \
    }

#define mpc_conf_init_uint_value(conf, default)          \
    if (conf == MPC_CONF_UNSET_UINT) {                   \
        conf = default;                                  \
    }

#define mpc_conf_init_size_value(conf, default)          \
    if (conf == MPC_CONF_UNSET_SIZE) {                   \
        conf = default;                                  \
    }


int mpc_atoi(uint8_t *line, size_t n);
int mpc_hextoi(uint8_t *line, size_t n);
ssize_t mpc_atosz(uint8_t *line, size_t n);
ssize_t mpc_parse_size(mpc_str_t *line);
int64_t mpc_parse_time(mpc_str_t *line, int is_sec);
uint8_t *mpc_hex_dump(uint8_t *dst, uint8_t *src, size_t len);


void mpc_conf_log_error(uint64_t level, mpc_conf_t *cf, int err, 
    const char *fmt, ...);
char *mpc_conf_param(mpc_conf_t *cf, mpc_str_t *param);
char *mpc_conf_parse(mpc_conf_t *cf, mpc_str_t *filename);
void mpc_conf_free(mpc_conf_t *cf);


char *mpc_conf_set_flag_slot(mpc_conf_t *cf, mpc_command_t *cmd, void *conf);
char *mpc_conf_set_str_slot(mpc_conf_t *cf, mpc_command_t *cmd, void *conf);
char *mpc_conf_set_str_array_slot(mpc_conf_t *cf, mpc_command_t *cmd, void *conf);
char *mpc_conf_set_str_keyval_slot(mpc_conf_t *cf, mpc_command_t *cmd, void *conf);
char *mpc_conf_set_num_slot(mpc_conf_t *cf, mpc_command_t *cmd, void *conf);
char *mpc_conf_set_str_size_slot(mpc_conf_t *cf, mpc_command_t *cmd, void *conf);
char *mpc_conf_set_str_msec_slot(mpc_conf_t *cf, mpc_command_t *cmd, void *conf);
char *mpc_conf_set_str_sec_slot(mpc_conf_t *cf, mpc_command_t *cmd, void *conf);
char *mpc_conf_set_str_enum_slot(mpc_conf_t *cf, mpc_command_t *cmd, void *conf);
char *mpc_conf_set_str_bitmask_slot(mpc_conf_t *cf, mpc_command_t *cmd,
    void *conf);

char *mpc_conf_check_num_bounds(mpc_conf_t *cf, void *post, void *data);


#endif /* __MPC_CONF_H_INCLUDED__ */
