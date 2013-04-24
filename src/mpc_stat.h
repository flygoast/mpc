#ifndef __MPC_STAT_H_INCLUDED__
#define __MPC_STAT_H_INCLUDED__


#define MPC_STAT_MAGIC      0x53544154      /* "STAT" */


struct mpc_stat_s {
#ifdef WITH_DEBUG
    uint32_t    magic;
#endif
    uint32_t    failed;
    uint32_t    ok;
    uint64_t    shortest;
    uint64_t    longest;
    uint64_t    bytes;
    uint64_t    total_time;
    uint64_t    start;
    uint64_t    stop;
};


#define mpc_stat_inc_bytes(s, b)        (s)->bytes += (b)
#define mpc_stat_inc_ok(s)              (s)->ok++
#define mpc_stat_inc_failed(s)          (s)->failed++
#define mpc_stat_inc_total_time(s, e)   (s)->total_time += (e)
#define mpc_stat_get_ok(s)              (s)->ok
#define mpc_stat_get_failed(s)          (s)->failed


mpc_stat_t *mpc_stat_create(void);
int mpc_stat_init(mpc_stat_t *mpc_stat);
void mpc_stat_destroy(mpc_stat_t *mpc_stat);
void mpc_stat_set_longest(mpc_stat_t *mpc_stat, uint64_t longest);
void mpc_stat_set_shortest(mpc_stat_t *mpc_stat, uint64_t shortest);
void mpc_stat_print(mpc_stat_t *mpc_stat);


#endif /* __MPC_STAT_H_INCLUDED__ */
