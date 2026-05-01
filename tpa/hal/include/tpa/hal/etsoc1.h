/*-------------------------------------------------------------------------
 * Copyright (c) 2025 Ainekko, Co.
 * SPDX-License-Identifier: Apache-2.0
 *-------------------------------------------------------------------------*/

#ifndef TPA_HAL_ETSOC1_H
#define TPA_HAL_ETSOC1_H

#include "tpa/hal.h"

#ifndef TPA_ETSOC1_NR_SHIRES
#define TPA_ETSOC1_NR_SHIRES 1u
#endif

#ifndef TPA_ETSOC1_MINIONS_PER_SHIRE
#define TPA_ETSOC1_MINIONS_PER_SHIRE 16u
#endif

#ifndef TPA_ETSOC1_HARTS_PER_MINION
#define TPA_ETSOC1_HARTS_PER_MINION 2u
#endif

#ifndef TPA_HAL_NR_HARTS
#define TPA_HAL_NR_HARTS                                                   \
    (TPA_ETSOC1_NR_SHIRES * TPA_ETSOC1_MINIONS_PER_SHIRE *                \
     TPA_ETSOC1_HARTS_PER_MINION)
#endif

#ifndef TPA_HAL_CACHELINE_BYTES
#define TPA_HAL_CACHELINE_BYTES 64u
#endif

#ifndef TPA_HAL_L1D_BYTES
#define TPA_HAL_L1D_BYTES 4096u
#endif

#define TPA_HAL_ETSOC1_HART_MINION_ID(hartid)                              \
    ((uint32_t)(hartid) / TPA_ETSOC1_HARTS_PER_MINION)
#define TPA_HAL_ETSOC1_HART_LOCAL_MINION_ID(hartid)                        \
    (TPA_HAL_ETSOC1_HART_MINION_ID(hartid) %                               \
     TPA_ETSOC1_MINIONS_PER_SHIRE)
#define TPA_HAL_ETSOC1_HART_THREAD_ID(hartid)                              \
    ((uint32_t)(hartid) & 1u)
#define TPA_HAL_ETSOC1_HART_SHIRE_ID(hartid)                               \
    (TPA_HAL_ETSOC1_HART_MINION_ID(hartid) /                               \
     TPA_ETSOC1_MINIONS_PER_SHIRE)

#undef TPA_HAL_CH_KIND
#define TPA_HAL_CH_KIND(src_hartid, dst_hartid, direct_kind, local_kind,    \
                        fabric_kind, external_kind)                        \
    ((TPA_HAL_ETSOC1_HART_MINION_ID(src_hartid) ==                         \
      TPA_HAL_ETSOC1_HART_MINION_ID(dst_hartid)) ?                         \
         (direct_kind) :                                                    \
         ((TPA_HAL_ETSOC1_HART_SHIRE_ID(src_hartid) ==                     \
           TPA_HAL_ETSOC1_HART_SHIRE_ID(dst_hartid)) ?                     \
              (local_kind) :                                                \
              (fabric_kind)))

#endif /* TPA_HAL_ETSOC1_H */
