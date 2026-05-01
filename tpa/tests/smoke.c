/*-------------------------------------------------------------------------
 * Copyright (c) 2025 Ainekko, Co.
 * SPDX-License-Identifier: Apache-2.0
 *-------------------------------------------------------------------------*/

#include <tpa/channel.h>
#include <tpa/process.h>
#include <tpa/scheduler.h>

void tpa_fail(void) {}

int main(void)
{
    struct tpa_proc proc;
    struct tpa_sched_node node;
    struct tpa_channel chan = {0};

    tpa_proc_init(&proc, 0, 0, 0);
    tpa_sched_node_init(&node, 0);
    chan.kind = TPA_CHANNEL_KIND_LOCAL;

    return tpa_hal_nr_harts() == 0u || chan.kind == TPA_CHANNEL_KIND_EXTERNAL;
}
