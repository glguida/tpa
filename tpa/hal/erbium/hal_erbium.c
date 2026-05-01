/*-------------------------------------------------------------------------
 * Copyright (c) 2025 Ainekko, Co.
 * SPDX-License-Identifier: Apache-2.0
 *-------------------------------------------------------------------------*/

/*
 * Erbium implementation of the TPA HAL.
 *
 * The RISC-V/Erbium path mirrors the legacy arch helpers. Host fallback code
 * exists only for repository syntax checks on machines that cannot assemble
 * Erbium-specific instructions.
 */

#include "tpa/hal/erbium.h"

#include <stddef.h>
#include <stdint.h>

#if defined(__riscv)
#define TPA_HAL_ERBIUM_HAS_PLATFORM 1
#else
#define TPA_HAL_ERBIUM_HAS_PLATFORM 0
#endif

#define ET_ESR_FCC_CREDINC_H0      0x00803400C0ULL
#define ET_ESR_FCC_CREDINC_H1      0x00803400D0ULL
#define ET_ESR_THREAD0_DISABLE     0x0080F40240ULL
#define ET_ESR_THREAD1_DISABLE     0x0080F40010ULL
#define ET_H1_SHADOW_ADDR          0x0040000A00ULL

static uint32_t hart_minion(uint32_t hartid)
{
    return TPA_HAL_ERBIUM_HART_MINION_ID(hartid);
}

static int __attribute__((unused)) hart_hi(uint32_t hartid)
{
    return TPA_HAL_ERBIUM_HART_THREAD_ID(hartid) != 0u;
}

uint32_t tpa_hal_nr_harts(void)
{
    return TPA_HAL_NR_HARTS;
}

uint64_t tpa_hal_hartid(void)
{
#if TPA_HAL_ERBIUM_HAS_PLATFORM
    uint64_t val;

    asm volatile("csrr %0, mhartid" : "=r"(val));
    return val;
#else
    return 0;
#endif
}

uint32_t tpa_hal_runtime_hartid(void)
{
    return (uint32_t)tpa_hal_hartid();
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
#if TPA_HAL_ERBIUM_HAS_PLATFORM
    uint32_t old;

    asm volatile("amoorg.w %[old],%[val],(%[addr])"
                 : [old] "=r"(old)
                 : [val] "r"(val), [addr] "r"(addr)
                 : "memory");
    return old;
#else
    return __atomic_fetch_or(addr, val, __ATOMIC_SEQ_CST);
#endif
}

uint32_t tpa_hal_atomic_and_u32(volatile uint32_t *addr, uint32_t val)
{
#if TPA_HAL_ERBIUM_HAS_PLATFORM
    uint32_t old;

    asm volatile("amoandg.w %[old],%[val],(%[addr])"
                 : [old] "=r"(old)
                 : [val] "r"(val), [addr] "r"(addr)
                 : "memory");
    return old;
#else
    return __atomic_fetch_and(addr, val, __ATOMIC_SEQ_CST);
#endif
}

uint32_t tpa_hal_atomic_exchange_u32(volatile uint32_t *addr, uint32_t val)
{
#if TPA_HAL_ERBIUM_HAS_PLATFORM
    uint32_t old;

    asm volatile("amoswapg.w %[old],%[val],(%[addr])"
                 : [old] "=r"(old)
                 : [val] "r"(val), [addr] "r"(addr)
                 : "memory");
    return old;
#else
    return __atomic_exchange_n(addr, val, __ATOMIC_SEQ_CST);
#endif
}

uint32_t tpa_hal_atomic_add_u32(volatile uint32_t *addr, uint32_t val)
{
#if TPA_HAL_ERBIUM_HAS_PLATFORM
    uint32_t old;

    asm volatile("amoaddg.w %[old],%[val],(%[addr])"
                 : [old] "=r"(old)
                 : [val] "r"(val), [addr] "r"(addr)
                 : "memory");
    return old;
#else
    return __atomic_fetch_add(addr, val, __ATOMIC_SEQ_CST);
#endif
}

uint32_t tpa_hal_atomic_compare_exchange_u32(volatile uint32_t *addr,
                                             uint32_t expect, uint32_t val)
{
#if TPA_HAL_ERBIUM_HAS_PLATFORM
    uint32_t old;
    register uint32_t x31 asm("x31") = expect;

    asm volatile("amocmpswapg.w %[old],%[val],%[addr]"
                 : [old] "=r"(old), [addr] "+A"(*addr)
                 : [val] "r"(val), "r"(x31)
                 : "memory");
    return old;
#else
    __atomic_compare_exchange_n(addr, &expect, val, 0, __ATOMIC_SEQ_CST,
                                __ATOMIC_SEQ_CST);
    return expect;
#endif
}

uint64_t tpa_hal_atomic_or_u64(volatile uint64_t *addr, uint64_t val)
{
#if TPA_HAL_ERBIUM_HAS_PLATFORM
    uint64_t old;

    asm volatile("amoorg.d %[old],%[val],(%[addr])"
                 : [old] "=r"(old)
                 : [val] "r"(val), [addr] "r"(addr)
                 : "memory");
    return old;
#else
    return __atomic_fetch_or(addr, val, __ATOMIC_SEQ_CST);
#endif
}

uint64_t tpa_hal_atomic_exchange_u64(volatile uint64_t *addr, uint64_t val)
{
#if TPA_HAL_ERBIUM_HAS_PLATFORM
    uint64_t old;

    asm volatile("amoswapg.d %[old],%[val],(%[addr])"
                 : [old] "=r"(old)
                 : [val] "r"(val), [addr] "r"(addr)
                 : "memory");
    return old;
#else
    return __atomic_exchange_n(addr, val, __ATOMIC_SEQ_CST);
#endif
}

void tpa_hal_fence_rw(void)
{
#if TPA_HAL_ERBIUM_HAS_PLATFORM
    asm volatile("fence rw, rw" ::: "memory");
#else
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
#endif
}

static void cache_flush_line(uint64_t addr)
{
#if TPA_HAL_ERBIUM_HAS_PLATFORM
    uint64_t enc = (3ULL << 58) |
                   (addr & ~((uint64_t)TPA_HAL_CACHELINE_BYTES - 1ULL));
    register uint64_t stride asm("x31") = TPA_HAL_CACHELINE_BYTES;

    asm volatile("csrw 0x8bf, %[enc]" :: [stride] "r"(stride), [enc] "r"(enc));
    asm volatile("csrwi 0x830, 6");
#else
    (void)addr;
#endif
}

static void cache_evict_line(uint64_t addr)
{
#if TPA_HAL_ERBIUM_HAS_PLATFORM
    uint64_t enc = addr & ~((uint64_t)TPA_HAL_CACHELINE_BYTES - 1ULL);
    register uint64_t stride asm("x31") = TPA_HAL_CACHELINE_BYTES;

    asm volatile("csrw 0x89f, %[enc]" :: [stride] "r"(stride), [enc] "r"(enc));
    asm volatile("csrwi 0x830, 6");
#else
    (void)addr;
#endif
}

static void cache_flush_l1d_to_mem(void)
{
#if TPA_HAL_ERBIUM_HAS_PLATFORM
    for (int way = 0; way < 4; way++) {
        uint64_t enc = (3ULL << 58) | ((uint64_t)way << 6) | 15ULL;
        asm volatile("csrw 0x7fb, %[enc]" :: [enc] "r"(enc));
    }
    asm volatile("csrwi 0x830, 6");
#endif
}

static void cache_evict_l1d_to_l2(void)
{
#if TPA_HAL_ERBIUM_HAS_PLATFORM
    tpa_hal_fence_rw();
    for (int way = 0; way < 4; way++) {
        uint64_t enc = (1ULL << 58) | ((uint64_t)way << 6) | 15ULL;
        asm volatile("csrw 0x7f9, %[enc]" :: [enc] "r"(enc));
    }
    asm volatile("csrwi 0x830, 6");
#endif
    tpa_hal_fence_rw();
}

void tpa_hal_flush(const volatile void *ptr, size_t bytes)
{
    uintptr_t addr = (uintptr_t)ptr;

    if (bytes >= TPA_HAL_L1D_BYTES) {
        cache_flush_l1d_to_mem();
        tpa_hal_fence_rw();
        return;
    }

    for (size_t off = 0; off < bytes; off += TPA_HAL_CACHELINE_BYTES)
        cache_flush_line((uint64_t)(addr + off));

    tpa_hal_fence_rw();
}

void tpa_hal_evict(const volatile void *ptr, size_t bytes)
{
    uintptr_t addr = (uintptr_t)ptr;

    if (bytes >= TPA_HAL_L1D_BYTES) {
        cache_evict_l1d_to_l2();
        return;
    }

    for (size_t off = 0; off < bytes; off += TPA_HAL_CACHELINE_BYTES)
        cache_evict_line((uint64_t)(addr + off));

    tpa_hal_fence_rw();
}

void tpa_hal_wake_hart(uint32_t hartid)
{
#if TPA_HAL_ERBIUM_HAS_PLATFORM
    uint32_t minionid = hart_minion(hartid);

    if (hart_hi(hartid))
        *((volatile uint64_t *)ET_ESR_FCC_CREDINC_H1) = UINT64_C(1) << minionid;
    else
        *((volatile uint64_t *)ET_ESR_FCC_CREDINC_H0) = UINT64_C(1) << minionid;
#else
    (void)hartid;
#endif
}

void tpa_hal_wake_all_harts(void)
{
    for (uint32_t i = 0; i < TPA_HAL_NR_HARTS; i++)
        tpa_hal_wake_hart(i);
}

void tpa_hal_wait_for_wake(void)
{
#if TPA_HAL_ERBIUM_HAS_PLATFORM
    asm volatile("csrwi fcc, 0");
#endif
}

void tpa_hal_runtime_boot_init(void)
{
#if TPA_HAL_ERBIUM_HAS_PLATFORM
    volatile uint32_t *shadow = (volatile uint32_t *)ET_H1_SHADOW_ADDR;

    *shadow = 0;
    cache_flush_line((uint64_t)shadow);
    tpa_hal_fence_rw();
#endif
}

void tpa_hal_start_all_threads(void)
{
#if TPA_HAL_ERBIUM_HAS_PLATFORM
    *((volatile uint64_t *)ET_ESR_THREAD0_DISABLE) = 0x00;
    *((volatile uint64_t *)ET_ESR_THREAD1_DISABLE) = 0x00;
#endif
}

void tpa_hal_runtime_enter(uint32_t hartid)
{
#if TPA_HAL_ERBIUM_HAS_PLATFORM
    volatile uint32_t *shadow = (volatile uint32_t *)ET_H1_SHADOW_ADDR;
    uint32_t bit = 1u << hart_minion(hartid);
    uint32_t old;

    if (!hart_hi(hartid))
        return;

    old = tpa_hal_atomic_or_u32(shadow, bit);
    *((volatile uint64_t *)ET_ESR_THREAD0_DISABLE) = (uint64_t)(old | bit);
#else
    (void)hartid;
#endif
}

void tpa_hal_runtime_leave(uint32_t hartid)
{
#if TPA_HAL_ERBIUM_HAS_PLATFORM
    volatile uint32_t *shadow = (volatile uint32_t *)ET_H1_SHADOW_ADDR;
    uint32_t bit = 1u << hart_minion(hartid);
    uint32_t old;

    if (!hart_hi(hartid))
        return;

    old = tpa_hal_atomic_and_u32(shadow, ~bit);
    *((volatile uint64_t *)ET_ESR_THREAD0_DISABLE) = (uint64_t)(old & ~bit);
#else
    (void)hartid;
#endif
}

void tpa_hal_trace(uint32_t tag)
{
#if TPA_HAL_ERBIUM_HAS_PLATFORM
    asm volatile("csrw validation0, %0" :: "r"(tag) : "memory");
#else
    (void)tag;
#endif
}

void tpa_hal_diag_putc(char c)
{
#if TPA_HAL_ERBIUM_HAS_PLATFORM
    uint64_t v = (UINT64_C(1) << 56) | (uint8_t)c;
    asm volatile("csrw validation1, %0" :: "r"(v) : "memory");
#else
    (void)c;
#endif
}

void tpa_hal_test_pass(void)
{
#if TPA_HAL_ERBIUM_HAS_PLATFORM
    asm volatile("li a7, 0x1feed000; csrw validation0, a7" ::: "a7");
#endif
}

void tpa_hal_test_fail(void)
{
#if TPA_HAL_ERBIUM_HAS_PLATFORM
    asm volatile("li a7, 0x50bad000; csrw validation0, a7" ::: "a7");
#endif
}

enum tpa_hal_channel_kind tpa_hal_channel_kind(uint32_t src_hartid,
                                               uint32_t dst_hartid)
{
    if (hart_minion(src_hartid) == hart_minion(dst_hartid))
        return TPA_HAL_CHANNEL_DIRECT;

    return TPA_HAL_CHANNEL_LOCAL;
}
