#include "test.h"
#include "tpa/tpa.h"

#define LO0_GOAL  3
#define LO1_GOAL  5
#define HI_GOAL   2

static volatile uint64_t steps[3];
static volatile uint32_t tries;

tpa_op_t queue_many_lo0(void);
tpa_op_t queue_many_lo1(void);
tpa_op_t queue_many_hi(void);
tpa_op_t queue_many_chk(void);

static tpa_op_t bump(uint32_t idx, uint64_t goal, tpa_cont_t next)
{
    uint64_t n;

    n = steps[idx] + 1;
    steps[idx] = n;
    et_cache_flush_line((uint64_t)&steps[idx]);
    asm volatile("fence rw, rw");

    if (n < goal)
        return tpa_yield(next);

    return tpa_stop();
}

tpa_op_t queue_many_lo0(void)
{
    return bump(0, LO0_GOAL, queue_many_lo0);
}

tpa_op_t queue_many_lo1(void)
{
    return bump(1, LO1_GOAL, queue_many_lo1);
}

tpa_op_t queue_many_hi(void)
{
    return bump(2, HI_GOAL, queue_many_hi);
}

tpa_op_t queue_many_chk(void)
{
    uint64_t lo0;
    uint64_t lo1;
    uint64_t hi;

    et_cache_evict_line((uint64_t)&steps[0]);
    et_cache_evict_line((uint64_t)&steps[1]);
    et_cache_evict_line((uint64_t)&steps[2]);

    lo0 = steps[0];
    lo1 = steps[1];
    hi = steps[2];

    if (lo0 == LO0_GOAL && lo1 == LO1_GOAL && hi == HI_GOAL) {
        TEST_PASS;
        return tpa_stop();
    }

    if (tries++ > 128) {
        TEST_FAIL;
        return tpa_stop();
    }

    et_cache_flush_line((uint64_t)&tries);
    asm volatile("fence rw, rw");

    return tpa_yield(queue_many_chk);
}
