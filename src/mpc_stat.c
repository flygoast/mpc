
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
    return (mpc_stat->ok + mpc_stat->failed) * 1000 * 1000 / 
           (double)(mpc_stat->stop - mpc_stat->start);
}


static double
mpc_stat_get_throughput(mpc_stat_t *mpc_stat)
{
    return (mpc_stat->bytes * 1000 * 1000) / 
           (double)((mpc_stat->stop - mpc_stat->start) * 1024 * 1024);
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
