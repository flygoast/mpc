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


mpc_stat_t *
mpc_stat_create(void)
{
    mpc_stat_t  *mpc_stat = mpc_alloc(sizeof(mpc_stat_t));
    if (stat == NULL) {
        return mpc_stat;
    }

    if (mpc_stat_init(mpc_stat) != MPC_OK) {
        mpc_free(mpc_stat);
        return NULL;
    }

    return mpc_stat;
}


int
mpc_stat_init(mpc_stat_t *mpc_stat)
{
    SET_MAGIC(mpc_stat, MPC_STAT_MAGIC);

    mpc_stat->ok = 0;
    mpc_stat->failed = 0;
    mpc_stat->shortest = MPC_MAX_UINT64_VALUE;
    mpc_stat->longest = 0;
    mpc_stat->bytes = 0;
    mpc_stat->total_time = 0;
    mpc_stat->start = 0;
    mpc_stat->stop = 0;

    return MPC_OK;
}


void
mpc_stat_destroy(mpc_stat_t *mpc_stat)
{
    ASSERT(mpc_stat->magic == MPC_STAT_MAGIC);
    mpc_free(mpc_stat);
}


void
mpc_stat_set_longest(mpc_stat_t *mpc_stat, uint64_t longest)
{
    if (longest > mpc_stat->longest) {
        mpc_stat->longest = longest;
    }
}


void
mpc_stat_set_shortest(mpc_stat_t *mpc_stat, uint64_t shortest)
{
    if (shortest < mpc_stat->shortest) {
        mpc_stat->shortest = shortest;
    }
}


static uint32_t
mpc_stat_get_transactions(mpc_stat_t *mpc_stat)
{
    return mpc_stat->ok + mpc_stat->failed;
}


static double
mpc_stat_get_availability(mpc_stat_t *mpc_stat)
{
    return mpc_stat->ok / (double)(mpc_stat->ok + mpc_stat->failed) * 100;
}


static double 
mpc_stat_get_elapsed(mpc_stat_t *mpc_stat)
{
    return (mpc_stat->stop - mpc_stat->start) / (double)(1000 * 1000);
}


static double
mpc_stat_get_data_transfered(mpc_stat_t *mpc_stat)
{
    return mpc_stat->bytes / (double)(1024 * 1024);
}


static double
mpc_stat_get_response_time(mpc_stat_t *mpc_stat)
{
    return mpc_stat->total_time / 
           (double)((mpc_stat->ok + mpc_stat->failed) * 1000 * 1000);
}


static double
mpc_stat_get_transaction_rate(mpc_stat_t *mpc_stat)
{
    return (mpc_stat->ok + mpc_stat->failed) / 
           (double)((mpc_stat->stop - mpc_stat->start) / 1000 / 1000);
}


static double
mpc_stat_get_throughput(mpc_stat_t *mpc_stat)
{
    return ((mpc_stat->bytes / 1024 / 1024)) / 
           (double)((mpc_stat->stop - mpc_stat->start) / 1000 / 1000);
}


static double
mpc_stat_get_concurrency(mpc_stat_t *mpc_stat)
{
    return mpc_stat->total_time / (double)(mpc_stat->stop - mpc_stat->start);
}


static double
mpc_stat_get_longest(mpc_stat_t *mpc_stat)
{
    return mpc_stat->longest / (double)(1000 * 1000);
}


static double
mpc_stat_get_shortest(mpc_stat_t *mpc_stat)
{
    return mpc_stat->shortest / (double)(1000 * 1000);
}
    

void
mpc_stat_print(mpc_stat_t *mpc_stat)
{
    printf("Transactions:                       %12u hits" CRLF
           "Availability:                       %12.2f %%" CRLF
           "Elapsed time:                       %12.2f secs" CRLF
           "Data transferred:                   %12.2f MB" CRLF
           "Response time:                      %12.2f secs" CRLF
           "Transaction rate:                   %12.2f trans/sec" CRLF
           "Throughput:                         %12.2f MB/sec" CRLF
           "Concurrency:                        %12.2f" CRLF
           "Successful transactions:            %12u" CRLF
           "Failed transactions:                %12u" CRLF
           "Longest transaction:                %12.2f" CRLF
           "Shortest transaction:               %12.2f" CRLF
           CRLF,
           mpc_stat_get_transactions(mpc_stat),
           mpc_stat_get_availability(mpc_stat),
           mpc_stat_get_elapsed(mpc_stat),
           mpc_stat_get_data_transfered(mpc_stat),
           mpc_stat_get_response_time(mpc_stat),
           mpc_stat_get_transaction_rate(mpc_stat),
           mpc_stat_get_throughput(mpc_stat),
           mpc_stat_get_concurrency(mpc_stat),
           mpc_stat_get_ok(mpc_stat),
           mpc_stat_get_failed(mpc_stat),
           mpc_stat_get_longest(mpc_stat),
           mpc_stat_get_shortest(mpc_stat));
}


int
mpc_stat_result_create(const char *file)
{
    int    fd;
    char   head[] = "        Date & Time,     Trans,  Elap Time,  Data Trans,  "
     "Resp Time,  Trans Rate,  Throughput,  Concurrent,      OKAY,    Failed\n";

    if (access(file, F_OK) == 0) {
        if ((fd = open(file, O_WRONLY|O_APPEND, 0644)) < 0) {
            mpc_log_err(errno, "open result file \"%s\" failed", file);
            return MPC_ERROR;
        }

        return fd;
    }

    if ((fd = open(file, O_CREAT|O_WRONLY|O_APPEND, 0644)) < 0) {
        mpc_log_err(errno, "open result file \"%s\" failed", file);
        return MPC_ERROR;
    }

    if (write(fd, head, sizeof(head) - 1) < 0) {
        mpc_log_err(errno, "write head to fd:%d failed", fd);
        close(fd);
        return MPC_ERROR;
    }

    return fd;
}


int
mpc_stat_result_record(int fd, mpc_stat_t *mpc_stat, char *mark)
{
    char        entry[MPC_TEMP_BUF_SIZE];
    char        date[65];
    struct tm   keepsake;
    struct tm  *tmp;
    time_t      now;

    if (mark != NULL) {
        snprintf(entry, sizeof(entry), "**** %s ****\n", mark);

        if (write(fd, entry, strlen(entry)) < 0) {
            mpc_log_err(errno, "write mark \"%s\" to fd:%d failed", mark, fd);
            close(fd);
            return MPC_ERROR;
        }
    }

    now = time(NULL);
    tmp = (struct tm *)localtime_r(&now, &keepsake);

    setlocale(LC_TIME, "C");
    strftime(date, sizeof(date), "%Y-%m-%d %H:%M:%S", tmp);

    snprintf(entry, sizeof(entry), 
             "%19.19s,%10d,%11.2f,%12u,%11.2f,%12.2f,%12.2f,%12.2f,%10u,%10u\n",
             date, 
             mpc_stat_get_transactions(mpc_stat),
             mpc_stat_get_elapsed(mpc_stat),
             (uint32_t)mpc_stat_get_data_transfered(mpc_stat),
             mpc_stat_get_response_time(mpc_stat),
             mpc_stat_get_transaction_rate(mpc_stat),
             mpc_stat_get_throughput(mpc_stat),
             mpc_stat_get_concurrency(mpc_stat),
             mpc_stat_get_ok(mpc_stat),
             mpc_stat_get_failed(mpc_stat));

    if (write(fd, entry, strlen(entry)) < 0) {
        mpc_log_err(errno, "write entry \"%s\" to fd:%d failed", entry, fd);
        close(fd);
        return MPC_ERROR;
    }

    return MPC_OK;
}


int
mpc_stat_result_close(int fd)
{
    ASSERT(fd != MPC_ERROR);

    if (close(fd) < 0) {
        mpc_log_err(errno, "close result fd:%d failed", fd);
        return MPC_ERROR;
    }

    return MPC_OK;
}
