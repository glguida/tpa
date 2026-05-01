/*-------------------------------------------------------------------------
 * Copyright (c) 2025 Ainekko, Co.
 * SPDX-License-Identifier: Apache-2.0
 *-------------------------------------------------------------------------*/

/*
 * tpa/hal/etsoc1.h - ET-SoC-1 compile-time HAL configuration
 *
 * Include this platform header before <tpa/hal.h> when building TPA for
 * ET-SoC-1. It supplies the integer constants and channel-kind preprocessor
 * expression required by public layout and image-generation code.
 */

#ifndef TPA_HAL_ETSOC1_H
#define TPA_HAL_ETSOC1_H

#include <stdint.h>

#if defined(__has_include)
#if __has_include(<etsoc/isa/hart.h>)
#include <etsoc/isa/hart.h>
#endif
#if __has_include(<etsoc/isa/cacheops-umode.h>)
#include <etsoc/isa/cacheops-umode.h>
#endif
#endif

#ifndef TPA_ETSOC1_NR_SHIRES
#define TPA_ETSOC1_NR_SHIRES 1
#endif

#ifndef SOC_MINIONS_PER_SHIRE
#if defined(TPA_HOST_SMOKE_TEST_DOUBLE)
#define SOC_MINIONS_PER_SHIRE 8u
#else
#error "SOC_MINIONS_PER_SHIRE must come from ET-SoC-1 hart.h in real device builds"
#endif
#endif

#ifndef CACHE_LINE_SIZE
#if defined(TPA_HOST_SMOKE_TEST_DOUBLE)
#define CACHE_LINE_SIZE 64u
#else
#error "CACHE_LINE_SIZE must come from ET-SoC-1 cacheops headers in real device builds"
#endif
#endif

#define TPA_ETSOC1_HARTS_PER_MINION      2u
#define TPA_ETSOC1_MINIONS_PER_SHIRE     SOC_MINIONS_PER_SHIRE

#define TPA_HAL_ETSOC1_NR_SHIRES         TPA_ETSOC1_NR_SHIRES
#define TPA_HAL_ETSOC1_NR_MINIONS        \
    (TPA_HAL_ETSOC1_NR_SHIRES * TPA_ETSOC1_MINIONS_PER_SHIRE)

#define TPA_HAL_NR_HARTS                 \
    (TPA_HAL_ETSOC1_NR_MINIONS * TPA_ETSOC1_HARTS_PER_MINION)
#define TPA_HAL_CACHELINE_BYTES          CACHE_LINE_SIZE
#define TPA_HAL_L1D_BYTES                4096u

#define TPA_HAL_ETSOC1_HART_MINION_ID(hartid)                                \
    ((hartid) / TPA_ETSOC1_HARTS_PER_MINION)
#define TPA_HAL_ETSOC1_HART_LOCAL_MINION_ID(hartid)                          \
    (TPA_HAL_ETSOC1_HART_MINION_ID(hartid) % TPA_ETSOC1_MINIONS_PER_SHIRE)
#define TPA_HAL_ETSOC1_HART_THREAD_ID(hartid)                                \
    ((hartid) & 1u)
#define TPA_HAL_ETSOC1_HART_SHIRE_ID(hartid)                                 \
    (TPA_HAL_ETSOC1_HART_MINION_ID(hartid) / TPA_ETSOC1_MINIONS_PER_SHIRE)

#define TPA_HAL_CH_KIND(src_hartid, dst_hartid, direct_kind, local_kind,      \
                        fabric_kind, external_kind)                          \
    ((TPA_HAL_ETSOC1_HART_MINION_ID(src_hartid) ==                           \
      TPA_HAL_ETSOC1_HART_MINION_ID(dst_hartid)) ?                           \
     (direct_kind) :                                                          \
     ((TPA_HAL_ETSOC1_HART_SHIRE_ID(src_hartid) ==                           \
       TPA_HAL_ETSOC1_HART_SHIRE_ID(dst_hartid)) ?                           \
      (local_kind) : (fabric_kind)))

#include <tpa/hal.h>

#endif /* TPA_HAL_ETSOC1_H */
