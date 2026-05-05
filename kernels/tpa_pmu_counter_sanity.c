#include <stdint.h>

#include "test.h"
#include "tpa/tpa.h"

#define PMU_COUNTER_SANITY_PID 721u

#define PMU_EVENT_NONE          0ull
#define PMU_EVENT_RETIRED_INST0 2ull

/*
 * This target intentionally exercises only documented PMU CSRs. The
 * PMU-control ESR enable/disable path is not touched here because the reviewed
 * TPA process-level API for Erbium/ET-SoC-1 PMU control is still unresolved.
 */

#define PMU_TRACE_BEGIN              0x706d0000u
#define PMU_TRACE_PASS               0x706d00ffu
#define PMU_TRACE_FAIL               0x706d00eeu
#define PMU_TRACE_MCYCLE             0x706d0100u
#define PMU_TRACE_MINSTRET           0x706d0101u
#define PMU_TRACE_MHPMCOUNTER9       0x706d0102u
#define PMU_TRACE_NONE_BEFORE        0x706d0110u
#define PMU_TRACE_NONE_AFTER         0x706d0111u
#define PMU_TRACE_NONE_DELTA         0x706d0112u
#define PMU_TRACE_RETIRED_BEFORE     0x706d0120u
#define PMU_TRACE_RETIRED_MID        0x706d0121u
#define PMU_TRACE_RETIRED_AFTER      0x706d0122u
#define PMU_TRACE_RETIRED_DELTA1     0x706d0123u
#define PMU_TRACE_RETIRED_DELTA2     0x706d0124u
#define PMU_TRACE_WORK_SINK          0x706d0130u
#define PMU_TRACE_POST_MCYCLE        0x706d0140u
#define PMU_TRACE_POST_MINSTRET      0x706d0141u

#define PMU_FAIL_MCYCLE_NONZERO       0x706df001u
#define PMU_FAIL_MINSTRET_NONZERO     0x706df002u
#define PMU_FAIL_MHPMCOUNTER9_NONZERO 0x706df003u
#define PMU_FAIL_NONE_COUNTED         0x706df004u
#define PMU_FAIL_RETIRED_STUCK_1      0x706df005u
#define PMU_FAIL_RETIRED_STUCK_2      0x706df006u
#define PMU_FAIL_POST_MCYCLE_NONZERO  0x706df007u
#define PMU_FAIL_POST_MINSTRET_NONZERO 0x706df008u

TPA_PROC_MEM_META(pmu_counter_sanity_meta, PMU_COUNTER_SANITY_PID, 0u);

tpa_op_t pmu_counter_sanity_start(void);

static void pmu_trace(uint32_t tag)
{
    arch_trace(tag);
}

static void pmu_trace_u64(uint32_t tag, uint64_t value)
{
    arch_trace(tag);
    arch_trace((uint32_t)value);
    arch_trace((uint32_t)(value >> 32));
}

static inline uint64_t pmu_read_mcycle(void)
{
    uint64_t value;
    asm volatile("csrr %0, mcycle" : "=r"(value));
    return value;
}

static inline uint64_t pmu_read_minstret(void)
{
    uint64_t value;
    asm volatile("csrr %0, minstret" : "=r"(value));
    return value;
}

static inline uint64_t pmu_read_mhpmcounter3(void)
{
    uint64_t value;
    asm volatile("csrr %0, mhpmcounter3" : "=r"(value));
    return value;
}

static inline uint64_t pmu_read_mhpmcounter9(void)
{
    uint64_t value;
    asm volatile("csrr %0, mhpmcounter9" : "=r"(value));
    return value;
}

static inline void pmu_write_mhpmevent3(uint64_t event)
{
    asm volatile("csrw mhpmevent3, %0" : : "r"(event) : "memory");
}

static inline void pmu_write_mhpmcounter3(uint64_t value)
{
    asm volatile("csrw mhpmcounter3, %0" : : "r"(value) : "memory");
}

static inline void pmu_clear_counter3(void)
{
    pmu_write_mhpmevent3(PMU_EVENT_NONE);
    pmu_write_mhpmcounter3(0ull);
    asm volatile("fence rw, rw" : : : "memory");
}

static uint32_t pmu_deterministic_work(uint32_t rounds)
{
    uint32_t acc = 0x12345678u;

    for (uint32_t i = 0; i < rounds; i++) {
        acc ^= (i + 0x9e3779b9u);
        asm volatile("addi %0, %0, 1" : "+r"(acc));
    }

    return acc;
}

static void pmu_fail(uint32_t reason)
{
    pmu_clear_counter3();
    pmu_trace(reason);
    pmu_trace(PMU_TRACE_FAIL);
    TEST_FAIL;
}

tpa_op_t pmu_counter_sanity_start(void)
{
    uint64_t mcycle;
    uint64_t minstret;
    uint64_t mhpmcounter9;
    uint64_t none_before;
    uint64_t none_after;
    uint64_t none_delta;
    uint64_t retired_before;
    uint64_t retired_mid;
    uint64_t retired_after;
    uint64_t retired_delta1;
    uint64_t retired_delta2;
    uint32_t sink;

    pmu_trace(PMU_TRACE_BEGIN);

    mcycle = pmu_read_mcycle();
    minstret = pmu_read_minstret();
    mhpmcounter9 = pmu_read_mhpmcounter9();
    pmu_trace_u64(PMU_TRACE_MCYCLE, mcycle);
    pmu_trace_u64(PMU_TRACE_MINSTRET, minstret);
    pmu_trace_u64(PMU_TRACE_MHPMCOUNTER9, mhpmcounter9);

    if (mcycle != 0ull) {
        pmu_fail(PMU_FAIL_MCYCLE_NONZERO);
        return tpa_stop();
    }
    if (minstret != 0ull) {
        pmu_fail(PMU_FAIL_MINSTRET_NONZERO);
        return tpa_stop();
    }
    if (mhpmcounter9 != 0ull) {
        pmu_fail(PMU_FAIL_MHPMCOUNTER9_NONZERO);
        return tpa_stop();
    }

    pmu_clear_counter3();
    none_before = pmu_read_mhpmcounter3();
    sink = pmu_deterministic_work(64u);
    none_after = pmu_read_mhpmcounter3();
    none_delta = none_after - none_before;
    pmu_trace_u64(PMU_TRACE_NONE_BEFORE, none_before);
    pmu_trace_u64(PMU_TRACE_NONE_AFTER, none_after);
    pmu_trace_u64(PMU_TRACE_NONE_DELTA, none_delta);

    if (none_delta != 0ull) {
        pmu_fail(PMU_FAIL_NONE_COUNTED);
        return tpa_stop();
    }

    pmu_write_mhpmevent3(PMU_EVENT_RETIRED_INST0);
    pmu_write_mhpmcounter3(0ull);
    asm volatile("fence rw, rw" : : : "memory");
    retired_before = pmu_read_mhpmcounter3();
    sink ^= pmu_deterministic_work(128u);
    retired_mid = pmu_read_mhpmcounter3();
    sink ^= pmu_deterministic_work(256u);
    retired_after = pmu_read_mhpmcounter3();
    retired_delta1 = retired_mid - retired_before;
    retired_delta2 = retired_after - retired_mid;
    pmu_trace_u64(PMU_TRACE_RETIRED_BEFORE, retired_before);
    pmu_trace_u64(PMU_TRACE_RETIRED_MID, retired_mid);
    pmu_trace_u64(PMU_TRACE_RETIRED_AFTER, retired_after);
    pmu_trace_u64(PMU_TRACE_RETIRED_DELTA1, retired_delta1);
    pmu_trace_u64(PMU_TRACE_RETIRED_DELTA2, retired_delta2);
    pmu_trace_u64(PMU_TRACE_WORK_SINK, sink);

    if (retired_delta1 == 0ull) {
        pmu_fail(PMU_FAIL_RETIRED_STUCK_1);
        return tpa_stop();
    }
    if (retired_delta2 == 0ull || retired_delta2 <= retired_delta1) {
        pmu_fail(PMU_FAIL_RETIRED_STUCK_2);
        return tpa_stop();
    }

    pmu_clear_counter3();
    mcycle = pmu_read_mcycle();
    minstret = pmu_read_minstret();
    pmu_trace_u64(PMU_TRACE_POST_MCYCLE, mcycle);
    pmu_trace_u64(PMU_TRACE_POST_MINSTRET, minstret);

    if (mcycle != 0ull) {
        pmu_fail(PMU_FAIL_POST_MCYCLE_NONZERO);
        return tpa_stop();
    }
    if (minstret != 0ull) {
        pmu_fail(PMU_FAIL_POST_MINSTRET_NONZERO);
        return tpa_stop();
    }

    pmu_trace(PMU_TRACE_PASS);
    TEST_PASS;
    return tpa_stop();
}
