/*-------------------------------------------------------------------------
 * Copyright (c) 2025 Ainekko, Co.
 * SPDX-License-Identifier: Apache-2.0
 *-------------------------------------------------------------------------*/

#ifndef TPA_HAL_ERBIUM_H
#define TPA_HAL_ERBIUM_H

#include "tpa/hal.h"

#ifndef TPA_HAL_ERBIUM_NR_MINIONS
#define TPA_HAL_ERBIUM_NR_MINIONS 8u
#endif

#ifndef TPA_HAL_NR_HARTS
#define TPA_HAL_NR_HARTS (TPA_HAL_ERBIUM_NR_MINIONS * 2u)
#endif

#ifndef TPA_HAL_CACHELINE_BYTES
#define TPA_HAL_CACHELINE_BYTES 64u
#endif

#ifndef TPA_HAL_L1D_BYTES
#define TPA_HAL_L1D_BYTES 4096u
#endif

#ifndef TPA_HAL_ERBIUM_MINIONS_PER_ISLAND
#define TPA_HAL_ERBIUM_MINIONS_PER_ISLAND TPA_HAL_ERBIUM_NR_MINIONS
#endif

#define TPA_HAL_ERBIUM_HARTS_PER_MINION 2u
#define TPA_HAL_ERBIUM_HART_MINION_ID(hartid)                              \
    ((uint32_t)(hartid) / TPA_HAL_ERBIUM_HARTS_PER_MINION)
#define TPA_HAL_ERBIUM_HART_THREAD_ID(hartid)                              \
    ((uint32_t)(hartid) % TPA_HAL_ERBIUM_HARTS_PER_MINION)
#define TPA_HAL_ERBIUM_HART_ISLAND_ID(hartid)                              \
    (TPA_HAL_ERBIUM_HART_MINION_ID(hartid) /                               \
     TPA_HAL_ERBIUM_MINIONS_PER_ISLAND)

#undef TPA_HAL_CH_KIND
#define TPA_HAL_CH_KIND(src_hartid, dst_hartid, direct_kind, local_kind,    \
                        fabric_kind, external_kind)                        \
    ((TPA_HAL_ERBIUM_HART_MINION_ID(src_hartid) ==                         \
      TPA_HAL_ERBIUM_HART_MINION_ID(dst_hartid)) ?                         \
         (direct_kind) :                                                    \
         (local_kind))

#endif /* TPA_HAL_ERBIUM_H */
