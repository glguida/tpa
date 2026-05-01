/*-------------------------------------------------------------------------
 * Copyright (c) 2025 Ainekko, Co.
 * SPDX-License-Identifier: Apache-2.0
 *-------------------------------------------------------------------------*/

/*
 * tpa/scheduler.h - TPA core scheduler/run-queue interface
 *
 * This module owns per-hart run queues and remote-ready notification state.
 * It is platform-independent: all hart count, atomics, fences, and wakeups go
 * through the HAL. Process extraction can embed `struct tpa_sched_node` inside
 * its process object and use these helpers instead of touching run queues
 * directly.
 */

#ifndef TPA_SCHEDULER_H
#define TPA_SCHEDULER_H

#include <stddef.h>
#include <stdint.h>

#include <tpa/hal.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef TPA_SCHED_MAX_PROCS_PER_HART
#define TPA_SCHED_MAX_PROCS_PER_HART 128u
#endif

#define TPA_SCHED_READY_WORDS (TPA_SCHED_MAX_PROCS_PER_HART / 64u)
#define TPA_SCHED_NO_SLOT     UINT16_C(0xffff)

enum tpa_sched_state {
    TPA_SCHED_DEAD = 0,
    TPA_SCHED_READY,
    TPA_SCHED_WAIT,
    TPA_SCHED_RUN,
    TPA_SCHED_FAILED,
};

struct tpa_sched_node {
    struct tpa_sched_node *next;
    uint16_t hartid;
    uint16_t slot;
    uint16_t state;
};

/* Reset all scheduler queues and remote-ready notification state. */
void tpa_sched_init(void);

/* Initialize a schedulable node before process registration. */
void tpa_sched_node_init(struct tpa_sched_node *node, uint32_t hartid);

/* Enqueue `node` on its home hart. Returns 0 if the hart id is invalid. */
int tpa_sched_run(struct tpa_sched_node *node);

/* Pop the next runnable node for `hartid`, or NULL if none is ready. */
struct tpa_sched_node *tpa_sched_pop(uint32_t hartid);

/* Mark a process slot ready on a hart; used by channel/process wake paths. */
void tpa_sched_set_ready_slot(uint32_t hartid, uint32_t slot);

/* Ring a hart only if it has armed its wait bell. */
void tpa_sched_ring_if_armed(uint32_t hartid);

/* Atomically drain one ready-bit word for reload logic. */
uint64_t tpa_sched_take_ready_word(uint32_t hartid, uint32_t word);

/* Wait-state bell helpers used by runtime loop extraction. */
void tpa_sched_set_armed(uint32_t hartid, uint32_t armed);
uint32_t tpa_sched_take_bell(uint32_t hartid);

#ifdef __cplusplus
}
#endif

#endif /* TPA_SCHEDULER_H */
