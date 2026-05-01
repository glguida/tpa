/*-------------------------------------------------------------------------
 * Copyright (c) 2025 Ainekko, Co.
 * SPDX-License-Identifier: Apache-2.0
 *-------------------------------------------------------------------------*/

/* ET-SoC-1 platform selection example. */

#include <tpa/hal/etsoc1.h>
#include <tpa/channel.h>
#include <tpa/scheduler.h>

#if TPA_HAL_CH_KIND(0, 1, 0, 1, 2, 3) != 0
#error "ET-SoC-1 harts in the same minion should use direct channels"
#endif

#if TPA_HAL_CH_KIND(0, 2, 0, 1, 2, 3) != 1
#error "ET-SoC-1 harts in the same shire should use local channels"
#endif

#if TPA_HAL_CH_KIND(0, 16, 0, 1, 2, 3) != 2
#error "ET-SoC-1 harts in different shires should use fabric channels"
#endif

static void example_etsoc1_selection(void)
{
    enum tpa_hal_channel_kind kind = tpa_hal_channel_kind(0, 16);

    (void)kind;
    (void)tpa_hal_nr_harts();
}
