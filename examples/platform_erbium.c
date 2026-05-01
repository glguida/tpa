/*-------------------------------------------------------------------------
 * Copyright (c) 2025 Ainekko, Co.
 * SPDX-License-Identifier: Apache-2.0
 *-------------------------------------------------------------------------*/

/* Erbium platform selection example. */

#include <tpa/hal/erbium.h>
#include <tpa/channel.h>
#include <tpa/scheduler.h>

#if TPA_HAL_CH_KIND(0, 1, 0, 1, 2, 3) != 0
#error "Erbium harts in the same minion should use direct channels"
#endif

#if TPA_HAL_CH_KIND(0, 2, 0, 1, 2, 3) != 1
#error "Erbium harts in different minions should use local channels"
#endif

static void example_erbium_selection(void)
{
    enum tpa_hal_channel_kind kind = tpa_hal_channel_kind(0, 2);

    (void)kind;
    (void)tpa_hal_nr_harts();
}
