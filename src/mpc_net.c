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


static int
mpc_net_socket(int domain, int type)
{
    int s, on = 1;

    if ((s = socket(domain, type, 0)) == -1) {
        return MPC_ERROR;
    }

    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1) {
        return MPC_ERROR;
    }

    return s;
}


static int
mpc_net_listen(int sockfd, struct sockaddr *sa, socklen_t len)
{
    if (bind(sockfd, sa, len) == -1) {
        close(sockfd);
        return MPC_ERROR;
    }

    if (listen(sockfd, 511) == -1) {
        close(sockfd);
        return MPC_ERROR;
    }

    return MPC_OK;
}


int
mpc_net_accept(int sockfd, struct sockaddr *sa, socklen_t *len)
{
    int   fd;

    for (;;) {
        fd = accept(sockfd, sa, len);
        if (fd < 0) {
            if (errno == EINTR) {
                continue;
            } else {
                return MPC_ERROR;
            }
        }
        break;
    }

    return fd;
}


int
mpc_net_nonblock(int fd)
{
    int flags;

    if ((flags = fcntl(fd, F_GETFL)) == -1) {
        return MPC_ERROR;
    }

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        return MPC_ERROR;
    }

    return MPC_OK;
}


int
mpc_net_read(int fd, char *buf, int count)
{
    int n, len = 0;

    while (len != count) {
        n = read(fd, buf, count - len);
        if (n == 0) {
            return len;
        }

        if (n < 0) {
            return -1;
        }

        len += n;
        buf += n;
    }

    return len;
}


int
mpc_net_write(int fd, char *buf, int count)
{
    int n, len = 0;

    while (len != count) {
        n = write(fd, buf, count - len);
        if (n == 0) {
            return len;
        }

        if (n < 0) {
            return -1;
        }

        len += n;
        buf += n;
    }

    return len;
}


int
mpc_net_tcp_server(char *ip, int port)
{
    int                 sockfd;
    struct sockaddr_in  sa;

    if ((sockfd = mpc_net_socket(AF_INET, SOCK_STREAM)) == -1) {
        return MPC_ERROR;
    }

    memset(&sa, 0, sizeof(sa));

    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    sa.sin_addr.s_addr = htonl(INADDR_ANY);
    if (ip && inet_aton(ip, &sa.sin_addr) == 0) {
        close(sockfd);
        return MPC_ERROR;
    }

    if (mpc_net_listen(sockfd, (struct sockaddr *)&sa, sizeof(sa)) == MPC_ERROR)
    {
        return MPC_ERROR;
    }

    return sockfd;
}


int
mpc_net_unix_server(char *path, mode_t perm)
{
    int                 sockfd;
    struct sockaddr_un  sa;

    if ((sockfd = mpc_net_socket(AF_LOCAL, SOCK_STREAM)) == MPC_ERROR) {
        return MPC_ERROR;
    }

    memset(&sa, 0, sizeof(sa));

    sa.sun_family = AF_LOCAL;
    strncpy(sa.sun_path, path, sizeof(sa.sun_path) - 1);

    if (mpc_net_listen(sockfd, (struct sockaddr *)&sa, sizeof(sa)) == MPC_ERROR)
    {
        return MPC_ERROR;
    }


    if (perm) {
        chmod(sa.sun_path, perm);
    }

    return sockfd;
}
