/*-------------------------------------------------------------------------
 * Copyright (c) 2025 Ainekko, Co.
 * SPDX-License-Identifier: Apache-2.0
 *-------------------------------------------------------------------------*/

/*
 * tpa/hal/erbium.h - Erbium compile-time HAL configuration
 *
 * Include this platform header before <tpa/hal.h> when building TPA for
 * Erbium. It supplies the integer constants and channel-kind preprocessor
 * expression required by public layout and image-generation code.
 */

#ifndef TPA_HAL_ERBIUM_H
#define TPA_HAL_ERBIUM_H

#include <stdint.h>

#define TPA_HAL_ERBIUM_NR_MINIONS      8u
#define TPA_HAL_ERBIUM_HARTS_PER_MINION 2u
#define TPA_HAL_NR_HARTS                                                    \
    (TPA_HAL_ERBIUM_NR_MINIONS * TPA_HAL_ERBIUM_HARTS_PER_MINION)
#define TPA_HAL_CACHELINE_BYTES       64u
#define TPA_HAL_L1D_BYTES             4096u

#ifndef TPA_HAL_ERBIUM_NR_MINIONS_PER_ISLAND
#define TPA_HAL_ERBIUM_NR_MINIONS_PER_ISLAND TPA_HAL_ERBIUM_NR_MINIONS
#endif

#define TPA_HAL_ERBIUM_HART_MINION_ID(hartid)                               \
    ((hartid) / TPA_HAL_ERBIUM_HARTS_PER_MINION)
#define TPA_HAL_ERBIUM_HART_THREAD_ID(hartid)                               \
    ((hartid) % TPA_HAL_ERBIUM_HARTS_PER_MINION)
#define TPA_HAL_ERBIUM_HART_ISLAND_ID(hartid)                               \
    (TPA_HAL_ERBIUM_HART_MINION_ID(hartid) /                                \
     TPA_HAL_ERBIUM_NR_MINIONS_PER_ISLAND)

#define TPA_HAL_CH_KIND(src_hartid, dst_hartid, direct_kind, local_kind,     \
                        fabric_kind, external_kind)                         \
    ((TPA_HAL_ERBIUM_HART_MINION_ID(src_hartid) ==                          \
      TPA_HAL_ERBIUM_HART_MINION_ID(dst_hartid)) ?                          \
     (direct_kind) : (local_kind))

#include <tpa/hal.h>

#endif /* TPA_HAL_ERBIUM_H */
