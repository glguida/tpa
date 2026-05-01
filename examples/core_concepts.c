/*-------------------------------------------------------------------------
 * Copyright (c) 2025 Ainekko, Co.
 * SPDX-License-Identifier: Apache-2.0
 *-------------------------------------------------------------------------*/

/*
 * Minimal core-facing usage example.
 *
 * Real platform builds should include <tpa/hal/erbium.h> or
 * <tpa/hal/etsoc1.h> instead of defining these constants locally. This file
 * uses fixed host constants so the public core headers can be syntax-checked
 * before the final build system lands.
 */

#define TPA_HAL_NR_HARTS 2u
#define TPA_HAL_CACHELINE_BYTES 64u

#include <string.h>

#include <tpa/channel.h>
#include <tpa/process.h>
#include <tpa/scheduler.h>

static void example_core_concepts(void)
{
    struct tpa_process_table table;
    struct tpa_proc proc;
    struct tpa_sched_node node;
    struct tpa_channel ch;
    struct tpa_channel_op op;

    memset(&ch, 0, sizeof(ch));
    memset(&op, 0, sizeof(op));
    ch.state = TPA_CHANNEL_STATE_EMPTY;
    ch.kind = TPA_CHANNEL_KIND_DIRECT;
    ch.tx.hartid = 0;
    ch.tx.slot = 0;
    ch.rx.hartid = 1;
    ch.rx.slot = 0;
    op.ch = &ch;

    tpa_process_table_init(&table);
    tpa_proc_init(&proc, 0, 0, 0);
    (void)tpa_process_register(&table, &proc);

    tpa_sched_init();
    tpa_sched_node_init(&node, 0);
    (void)tpa_sched_run(&node);
    (void)tpa_sched_pop(0);

    (void)op;
}
