/*-------------------------------------------------------------------------
 * Copyright (c) 2025 Ainekko, Co.
 * SPDX-License-Identifier: Apache-2.0
 *-------------------------------------------------------------------------*/

/*
 * tpa/hal.h - Core-facing TPA Hardware Abstraction Layer
 *
 * Code under tpa/lib/ should include this header instead of platform headers.
 * Platform implementations live below tpa/hal/<platform>/ and provide the
 * symbols declared here plus the compile-time configuration macros required
 * by public layout and image-generation code.
 *
 * The HAL intentionally has two surfaces:
 *
 * 1. Compile-time constants and preprocessor policy.
 *    These are required where C object layout, array bounds, alignment, or
 *    image-generation #if expressions need integer constant expressions.
 *    They must be supplied by the selected platform build configuration before
 *    this header is included. They are not callable functions.
 *
 * 2. Link-time callable operations.
 *    These are the runtime operations currently needed by core scheduling,
 *    channel, cache coherency, wakeup, tracing, and lifecycle code. They are
 *    declared as normal C functions so the core does not include machine
 *    headers. Embedded builds may provide them as thin wrappers, archive-local
 *    objects, or LTO-inlineable definitions in the selected HAL library.
 */

#ifndef TPA_HAL_H
#define TPA_HAL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Compile-time platform configuration.
 *
 * Required platform/build definitions:
 * - TPA_HAL_NR_HARTS: total runtime hart slots visible to TPA.
 * - TPA_HAL_CACHELINE_BYTES: cacheline size used for ABI layout/alignment.
 *
 * Optional platform/build definitions:
 * - TPA_HAL_L1D_BYTES: L1D size used by cache-maintenance implementations.
 * - TPA_HAL_CH_KIND(src, dst, direct, local, fabric, external): integer
 *   preprocessor expression selecting channel transport for image generation.
 *
 * Keep these names platform-neutral. Do not include Erbium, ET-SoC-1, or other
 * machine headers from public/core runtime headers to obtain them.
 */
#define TPA_HAL_CH_KIND_DIRECT    0
#define TPA_HAL_CH_KIND_LOCAL     1
#define TPA_HAL_CH_KIND_FABRIC    2
#define TPA_HAL_CH_KIND_EXTERNAL  3

#ifndef TPA_HAL_CH_KIND
#define TPA_HAL_CH_KIND(src_hartid, dst_hartid, direct_kind, local_kind,      \
                        fabric_kind, external_kind)                          \
    (fabric_kind)
#endif

/*
 * Compatibility aliases for code being migrated from the legacy arch surface.
 * Later extraction jobs can replace ARCH_* and TPA_ARCH_CH_KIND users with the
 * TPA_HAL_* names directly. Platforms may define ARCH_* themselves during the
 * transition; the aliases below only fill gaps from the new HAL constants.
 */
#if !defined(ARCH_NR_HARTS) && defined(TPA_HAL_NR_HARTS)
#define ARCH_NR_HARTS TPA_HAL_NR_HARTS
#endif

#if !defined(ARCH_CACHELINE_BYTES) && defined(TPA_HAL_CACHELINE_BYTES)
#define ARCH_CACHELINE_BYTES TPA_HAL_CACHELINE_BYTES
#endif

#if !defined(ARCH_L1D_BYTES) && defined(TPA_HAL_L1D_BYTES)
#define ARCH_L1D_BYTES TPA_HAL_L1D_BYTES
#endif

#ifndef TPA_ARCH_CH_KIND
#define TPA_ARCH_CH_KIND(src_hartid, dst_hartid, direct_kind, local_kind,     \
                         fabric_kind, external_kind)                         \
    TPA_HAL_CH_KIND((src_hartid), (dst_hartid), (direct_kind), (local_kind),  \
                    (fabric_kind), (external_kind))
#endif

enum tpa_hal_channel_kind {
    TPA_HAL_CHANNEL_DIRECT = TPA_HAL_CH_KIND_DIRECT,
    TPA_HAL_CHANNEL_LOCAL = TPA_HAL_CH_KIND_LOCAL,
    TPA_HAL_CHANNEL_FABRIC = TPA_HAL_CH_KIND_FABRIC,
    TPA_HAL_CHANNEL_EXTERNAL = TPA_HAL_CH_KIND_EXTERNAL,
};

/* Hart topology and identity. */
uint32_t tpa_hal_nr_harts(void);
uint64_t tpa_hal_hartid(void);
uint32_t tpa_hal_runtime_hartid(void);

/* Cache geometry query for code that does not need an integer constant. */
uint32_t tpa_hal_cacheline_bytes(void);
uint32_t tpa_hal_l1d_bytes(void);

/* Global atomic operations used by the runtime. Return the previous value. */
uint32_t tpa_hal_atomic_or_u32(volatile uint32_t *addr, uint32_t val);
uint32_t tpa_hal_atomic_and_u32(volatile uint32_t *addr, uint32_t val);
uint32_t tpa_hal_atomic_exchange_u32(volatile uint32_t *addr, uint32_t val);
uint32_t tpa_hal_atomic_add_u32(volatile uint32_t *addr, uint32_t val);
uint32_t tpa_hal_atomic_compare_exchange_u32(volatile uint32_t *addr,
                                             uint32_t expect, uint32_t val);
uint64_t tpa_hal_atomic_or_u64(volatile uint64_t *addr, uint64_t val);
uint64_t tpa_hal_atomic_exchange_u64(volatile uint64_t *addr, uint64_t val);

/* Memory ordering and cache maintenance. */
void tpa_hal_fence_rw(void);
void tpa_hal_flush(const volatile void *ptr, size_t bytes);
void tpa_hal_evict(const volatile void *ptr, size_t bytes);

/* Hart wake/sleep coordination. */
void tpa_hal_wake_hart(uint32_t hartid);
void tpa_hal_wake_all_harts(void);
void tpa_hal_wait_for_wake(void);

/* Runtime lifecycle hooks. */
void tpa_hal_runtime_boot_init(void);
void tpa_hal_start_all_threads(void);
void tpa_hal_runtime_enter(uint32_t hartid);
void tpa_hal_runtime_leave(uint32_t hartid);

/* Optional diagnostics/tracing hooks. No-op implementations are valid. */
void tpa_hal_trace(uint32_t tag);
void tpa_hal_diag_putc(char c);
void tpa_hal_test_pass(void);
void tpa_hal_test_fail(void);

/* Runtime equivalent of TPA_HAL_CH_KIND() for non-preprocessor users. */
enum tpa_hal_channel_kind tpa_hal_channel_kind(uint32_t src_hartid,
                                               uint32_t dst_hartid);

#ifdef __cplusplus
}
#endif

#endif /* TPA_HAL_H */
