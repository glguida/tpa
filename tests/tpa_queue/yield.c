#include "test.h"
#include "tpa/tpa.h"

#define YIELD_GOAL  4

static volatile uint64_t steps;
static volatile uint32_t tries;

tpa_op_t queue_yield_start(void);
tpa_op_t queue_yield_chk(void);

tpa_op_t queue_yield_start(void)
{
    uint64_t n;

    n = steps + 1;
    steps = n;
    et_cache_flush_line((uint64_t)&steps);
    asm volatile("fence rw, rw");

    if (n < YIELD_GOAL)
        return tpa_yield(queue_yield_start);

    return tpa_stop();
}

tpa_op_t queue_yield_chk(void)
{
    uint64_t n;

    et_cache_evict_line((uint64_t)&steps);
    n = steps;

    if (n == YIELD_GOAL) {
        TEST_PASS;
        return tpa_stop();
    }

    if (tries++ > 64) {
        TEST_FAIL;
        return tpa_stop();
    }

    et_cache_flush_line((uint64_t)&tries);
    asm volatile("fence rw, rw");

    return tpa_yield(queue_yield_chk);
}
