#include "test.h"
#include "tpa/tpa.h"

static volatile uint64_t results[2];
static volatile uint32_t tries;

tpa_op_t queue_lo_start(void);
tpa_op_t queue_hi_start(void);
tpa_op_t queue_chk_start(void);

tpa_op_t queue_lo_start(void)
{
    results[0] = 0x11;
    et_cache_flush_line((uint64_t)&results[0]);
    asm volatile("fence rw, rw");

    return tpa_stop();
}

tpa_op_t queue_hi_start(void)
{
    results[1] = 0x22;
    et_cache_flush_line((uint64_t)&results[1]);
    asm volatile("fence rw, rw");

    return tpa_stop();
}

tpa_op_t queue_chk_start(void)
{
    uint64_t lo;
    uint64_t hi;

    et_cache_evict_line((uint64_t)&results[0]);
    lo = results[0];
    hi = results[1];

    if (lo == 0x11 && hi == 0x22) {
        TEST_PASS;
        return tpa_stop();
    }

    if (tries++ > 64) {
        TEST_FAIL;
        return tpa_stop();
    }

    et_cache_flush_line((uint64_t)&tries);
    asm volatile("fence rw, rw");

    return tpa_yield(queue_chk_start);
}
