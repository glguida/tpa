/*-------------------------------------------------------------------------
 * Copyright (c) 2025 Ainekko, Co.
 * SPDX-License-Identifier: Apache-2.0
 *-------------------------------------------------------------------------*/

#include "tpa/runtime.h"

#include "tpa/hal.h"
#include "tpa/scheduler.h"

static volatile uint32_t tpa_reported;

void tpa_scheduler_main(uint32_t hartid)
{
    tpa_hal_runtime_enter(hartid);
    tpa_boot_hart(hartid);

    for (;;) {
        struct tpa_proc *p;

        if (tpa_failed() || tpa_done())
            break;

        tpa_reload(hartid);
        p = tpa_runq_pop(hartid);
        if (p) {
            tpa_step(p);
            continue;
        }

        tpa_sched_set_armed(hartid, 1);
        tpa_reload(hartid);
        p = tpa_runq_pop(hartid);
        if (p) {
            tpa_sched_set_armed(hartid, 0);
            (void)tpa_sched_take_bell(hartid);
            tpa_step(p);
            continue;
        }
        if (!tpa_done() && !tpa_failed())
            tpa_hal_wait_for_wake();
        tpa_sched_set_armed(hartid, 0);
        (void)tpa_sched_take_bell(hartid);
    }

    tpa_hal_runtime_leave(hartid);
}

void tpa_hart_main(void)
{
    uint32_t hartid = tpa_hal_runtime_hartid();

    if (hartid >= TPA_HAL_NR_HARTS) {
        for (;;)
            tpa_hal_wait_for_wake();
    }

    tpa_init();
    tpa_scheduler_main(hartid);

    if (tpa_hal_atomic_compare_exchange_u32(&tpa_reported, 0, 1) == 0) {
        if (tpa_failed())
            tpa_hal_test_fail();
        else
            tpa_hal_test_pass();
    }
}

int main(void)
{
    tpa_hal_runtime_boot_init();
    tpa_hal_trace(0x74706100u);
    tpa_hal_start_all_threads();
    tpa_hal_wake_all_harts();
    tpa_hart_main();
    return 0;
}
