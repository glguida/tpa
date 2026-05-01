#include "test.h"
#include "tpa/tpa.h"

static volatile struct tpa_proc *blocked;
static volatile uint64_t result;
static volatile uint32_t kick_tries;
static volatile uint32_t chk_tries;

tpa_op_t queue_wake_kick(void);
tpa_op_t queue_wake_target_start(void);
tpa_op_t queue_wake_target_run(void);
tpa_op_t queue_wake_chk(void);

tpa_op_t queue_wake_kick(void)
{
    struct tpa_proc *p;

    et_cache_evict_line((uint64_t)&blocked);
    p = (struct tpa_proc *)blocked;

    if (!p) {
        if (kick_tries++ > 64) {
            TEST_FAIL;
            return tpa_stop();
        }

        et_cache_flush_line((uint64_t)&kick_tries);
        asm volatile("fence rw, rw");
        return tpa_yield(queue_wake_kick);
    }

    tpa_wake(p);
    return tpa_stop();
}

tpa_op_t queue_wake_target_start(void)
{
    blocked = tpa_self();
    et_cache_flush_line((uint64_t)&blocked);
    asm volatile("fence rw, rw");

    return tpa_block(queue_wake_target_run);
}

tpa_op_t queue_wake_target_run(void)
{
    result = 0x5a;
    et_cache_flush_line((uint64_t)&result);
    asm volatile("fence rw, rw");

    return tpa_stop();
}

tpa_op_t queue_wake_chk(void)
{
    uint64_t val;

    et_cache_evict_line((uint64_t)&result);
    val = result;

    if (val == 0x5a) {
        TEST_PASS;
        return tpa_stop();
    }

    if (chk_tries++ > 128) {
        TEST_FAIL;
        return tpa_stop();
    }

    et_cache_flush_line((uint64_t)&chk_tries);
    asm volatile("fence rw, rw");
    return tpa_yield(queue_wake_chk);
}
