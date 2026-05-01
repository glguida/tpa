/*-------------------------------------------------------------------------
 * Copyright (c) 2025 Ainekko, Co.
 * SPDX-License-Identifier: Apache-2.0
 *-------------------------------------------------------------------------*/

/*
 * ET-SoC-1 implementation of the TPA HAL.
 *
 * The real platform path uses cm-umode/ET-SoC-1 headers. The host fallback is
 * deliberately conservative and exists only so this foundation can be checked
 * in a repository that does not yet carry the ET-SoC SDK headers.
 */

#include "tpa/hal/etsoc1.h"

#if defined(__has_include) && __has_include(<etsoc/isa/atomic.h>) &&          \
    __has_include(<etsoc/isa/cacheops-umode.h>) &&                           \
    __has_include(<etsoc/isa/fcc.h>) && __has_include(<etsoc/isa/hart.h>) &&  \
    __has_include(<etsoc/isa/syscall.h>) &&                                  \
    __has_include(<etsoc/isa/utils.h>)
#define TPA_HAL_ETSOC1_HAS_PLATFORM 1
#else
#define TPA_HAL_ETSOC1_HAS_PLATFORM 0
#endif

#if TPA_HAL_ETSOC1_HAS_PLATFORM
#include <etsoc/isa/atomic.h>
#include <etsoc/isa/cacheops-umode.h>
#include <etsoc/isa/fcc.h>
#include <etsoc/isa/hart.h>
#include <etsoc/isa/syscall.h>
#include <etsoc/isa/utils.h>
#endif

volatile uint64_t etsoc1_runtime_shire_mask;

static uint64_t runtime_shire_mask_or_default(void)
{
    uint64_t mask = etsoc1_runtime_shire_mask;

    return mask ? mask : UINT64_C(0xffffffff);
}

static uint32_t first_runtime_shire(uint64_t shire_mask)
{
    return (uint32_t)__builtin_ctzll(shire_mask ? shire_mask :
                                                UINT64_C(0xffffffff));
}

static uint32_t __attribute__((unused)) physical_hart_from_runtime(
    uint32_t hartid)
{
    uint32_t first_shire = first_runtime_shire(runtime_shire_mask_or_default());

    return hartid + first_shire * TPA_ETSOC1_MINIONS_PER_SHIRE *
                    TPA_ETSOC1_HARTS_PER_MINION;
}

uint32_t tpa_hal_nr_harts(void)
{
    return TPA_HAL_NR_HARTS;
}

uint64_t tpa_hal_hartid(void)
{
#if TPA_HAL_ETSOC1_HAS_PLATFORM
    return get_hart_id();
#else
    return 0;
#endif
}

uint32_t tpa_hal_runtime_hartid(void)
{
    uint64_t mask = runtime_shire_mask_or_default();
    uint32_t physical_hart = (uint32_t)tpa_hal_hartid();
    uint32_t first_hart = first_runtime_shire(mask) *
                          TPA_ETSOC1_MINIONS_PER_SHIRE *
                          TPA_ETSOC1_HARTS_PER_MINION;

    if (physical_hart < first_hart)
        return UINT32_MAX;

    return physical_hart - first_hart;
}

uint32_t tpa_hal_cacheline_bytes(void)
{
    return TPA_HAL_CACHELINE_BYTES;
}

uint32_t tpa_hal_l1d_bytes(void)
{
    return TPA_HAL_L1D_BYTES;
}

uint32_t tpa_hal_atomic_or_u32(volatile uint32_t *addr, uint32_t val)
{
#if TPA_HAL_ETSOC1_HAS_PLATFORM
    return atomic_or_global_32(addr, val);
#else
    return __atomic_fetch_or(addr, val, __ATOMIC_SEQ_CST);
#endif
}

uint32_t tpa_hal_atomic_and_u32(volatile uint32_t *addr, uint32_t val)
{
#if TPA_HAL_ETSOC1_HAS_PLATFORM
    return atomic_and_global_32(addr, val);
#else
    return __atomic_fetch_and(addr, val, __ATOMIC_SEQ_CST);
#endif
}

uint32_t tpa_hal_atomic_exchange_u32(volatile uint32_t *addr, uint32_t val)
{
#if TPA_HAL_ETSOC1_HAS_PLATFORM
    return atomic_exchange_global_32(addr, val);
#else
    return __atomic_exchange_n(addr, val, __ATOMIC_SEQ_CST);
#endif
}

uint32_t tpa_hal_atomic_add_u32(volatile uint32_t *addr, uint32_t val)
{
#if TPA_HAL_ETSOC1_HAS_PLATFORM
    return atomic_add_global_32(addr, val);
#else
    return __atomic_fetch_add(addr, val, __ATOMIC_SEQ_CST);
#endif
}

uint32_t tpa_hal_atomic_compare_exchange_u32(volatile uint32_t *addr,
                                             uint32_t expect, uint32_t val)
{
#if TPA_HAL_ETSOC1_HAS_PLATFORM
    return atomic_compare_and_exchange_global_32(addr, expect, val);
#else
    __atomic_compare_exchange_n(addr, &expect, val, 0, __ATOMIC_SEQ_CST,
                                __ATOMIC_SEQ_CST);
    return expect;
#endif
}

uint64_t tpa_hal_atomic_or_u64(volatile uint64_t *addr, uint64_t val)
{
#if TPA_HAL_ETSOC1_HAS_PLATFORM
    return atomic_or_global_64(addr, val);
#else
    return __atomic_fetch_or(addr, val, __ATOMIC_SEQ_CST);
#endif
}

uint64_t tpa_hal_atomic_exchange_u64(volatile uint64_t *addr, uint64_t val)
{
#if TPA_HAL_ETSOC1_HAS_PLATFORM
    return atomic_exchange_global_64(addr, val);
#else
    return __atomic_exchange_n(addr, val, __ATOMIC_SEQ_CST);
#endif
}

void tpa_hal_fence_rw(void)
{
#if TPA_HAL_ETSOC1_HAS_PLATFORM
    FENCE;
#else
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
#endif
}

void tpa_hal_flush(const volatile void *ptr, size_t bytes)
{
#if TPA_HAL_ETSOC1_HAS_PLATFORM
    uintptr_t addr = (uintptr_t)ptr;

    for (size_t off = 0; off < bytes; off += TPA_HAL_CACHELINE_BYTES)
        cache_ops_flush_va(0, to_Mem, addr + off, 0, TPA_HAL_CACHELINE_BYTES, 0);

    WAIT_CACHEOPS;
    tpa_hal_fence_rw();
#else
    (void)ptr;
    (void)bytes;
    tpa_hal_fence_rw();
#endif
}

void tpa_hal_evict(const volatile void *ptr, size_t bytes)
{
#if TPA_HAL_ETSOC1_HAS_PLATFORM
    uintptr_t addr = (uintptr_t)ptr;

    for (size_t off = 0; off < bytes; off += TPA_HAL_CACHELINE_BYTES)
        cache_ops_evict_va(0, to_L2, addr + off, 0, TPA_HAL_CACHELINE_BYTES, 0);

    WAIT_CACHEOPS;
    tpa_hal_fence_rw();
#else
    (void)ptr;
    (void)bytes;
    tpa_hal_fence_rw();
#endif
}

void tpa_hal_wake_hart(uint32_t hartid)
{
#if TPA_HAL_ETSOC1_HAS_PLATFORM
    uint32_t physical_hart = physical_hart_from_runtime(hartid);

    SEND_FCC(TPA_HAL_ETSOC1_HART_SHIRE_ID(physical_hart),
             TPA_HAL_ETSOC1_HART_THREAD_ID(physical_hart), FCC_0,
             1u << TPA_HAL_ETSOC1_HART_LOCAL_MINION_ID(physical_hart));
#else
    (void)hartid;
#endif
}

static uint64_t minion_wake_mask(void)
{
    if (TPA_ETSOC1_MINIONS_PER_SHIRE >= 64u)
        return UINT64_MAX;

    return (UINT64_C(1) << TPA_ETSOC1_MINIONS_PER_SHIRE) - 1u;
}

void tpa_hal_wake_all_harts(void)
{
#if TPA_HAL_ETSOC1_HAS_PLATFORM
    uint64_t shire_mask = runtime_shire_mask_or_default();
    uint64_t minion_mask = minion_wake_mask();
    volatile uint64_t *broadcast_data = (volatile uint64_t *)ESR_SHIRE_PROT_ADDR(
        PRV_U, THIS_SHIRE, ESR_SHIRE_BROADCAST0);
    volatile uint64_t *broadcast_address = (volatile uint64_t *)ESR_SHIRE_PROT_ADDR(
        PRV_U, THIS_SHIRE, ESR_SHIRE_BROADCAST1);
    uint64_t broadcast_shires = shire_mask & UINT64_C(0xffffffff);

    if (broadcast_shires) {
        *broadcast_data = minion_mask;
        *broadcast_address = broadcast_shires |
            (((ESR_SHIRE(0, FCC_CREDINC_0) >> 3) & UINT64_C(0x7ffff))
             << ESR_BROADCAST_ESR_ADDR_SHIFT);
        *broadcast_data = minion_mask;
        *broadcast_address = broadcast_shires |
            (((ESR_SHIRE(0, FCC_CREDINC_2) >> 3) & UINT64_C(0x7ffff))
             << ESR_BROADCAST_ESR_ADDR_SHIFT);
    }

    if (shire_mask & (UINT64_C(1) << 32)) {
        SEND_FCC(32, THREAD_0, FCC_0, minion_mask);
        SEND_FCC(32, THREAD_1, FCC_0, minion_mask);
    }
#else
    (void)minion_wake_mask();
#endif
}

void tpa_hal_wait_for_wake(void)
{
#if TPA_HAL_ETSOC1_HAS_PLATFORM
    WAIT_FCC(FCC_0);
#endif
}

void tpa_hal_runtime_boot_init(void) {}
void tpa_hal_start_all_threads(void) {}
void tpa_hal_runtime_enter(uint32_t hartid) { (void)hartid; }
void tpa_hal_runtime_leave(uint32_t hartid) { (void)hartid; }
void tpa_hal_trace(uint32_t tag) { (void)tag; }
void tpa_hal_diag_putc(char c) { (void)c; }
void tpa_hal_test_pass(void) {}

void tpa_hal_test_fail(void)
{
    extern void tpa_fail(void);

    tpa_fail();
}

enum tpa_hal_channel_kind tpa_hal_channel_kind(uint32_t src_hartid,
                                               uint32_t dst_hartid)
{
    if (TPA_HAL_ETSOC1_HART_MINION_ID(src_hartid) ==
        TPA_HAL_ETSOC1_HART_MINION_ID(dst_hartid))
        return TPA_HAL_CHANNEL_DIRECT;

    if (TPA_HAL_ETSOC1_HART_SHIRE_ID(src_hartid) ==
        TPA_HAL_ETSOC1_HART_SHIRE_ID(dst_hartid))
        return TPA_HAL_CHANNEL_LOCAL;

    return TPA_HAL_CHANNEL_FABRIC;
}
