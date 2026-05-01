/*-------------------------------------------------------------------------
 * Copyright (c) 2025 Ainekko, Co.
 * SPDX-License-Identifier: Apache-2.0
 *-------------------------------------------------------------------------*/

#include "tpa/runtime.h"

#include "tpa/channel.h"
#include "tpa/image.h"
#include "tpa/tpa.h"

#include <stddef.h>

struct tpa_runtime_node {
    struct tpa_sched_node node;
    struct tpa_proc *proc;
};

static struct tpa_process_table proc_table;
static struct tpa_runtime_node nodes[TPA_HAL_NR_HARTS]
                                    [TPA_PROCESS_MAX_PROCS_PER_HART];
static struct tpa_proc *current_proc[TPA_HAL_NR_HARTS];
static volatile uint32_t init_state;
static volatile uint32_t live_procs;
static volatile uint32_t failed;

static struct tpa_runtime_node *runtime_node_for(const struct tpa_proc *p)
{
    if (!p || p->hartid >= TPA_HAL_NR_HARTS ||
        p->slot >= TPA_PROCESS_MAX_PROCS_PER_HART)
        return 0;

    return &nodes[p->hartid][p->slot];
}

static void runtime_bug(void)
{
    tpa_fail();
}

static void runtime_schedule_local(struct tpa_proc *p, tpa_cont_t next)
{
    struct tpa_runtime_node *node = runtime_node_for(p);

    if (!node)
        return;

    p->k = next;
    p->state = TPA_PROCESS_READY;
    tpa_hal_flush(p, sizeof(*p));
    (void)tpa_sched_run(&node->node);
}

static void runtime_wake_slot(uint32_t hartid, uint32_t slot)
{
    if (hartid >= TPA_HAL_NR_HARTS || slot >= TPA_PROCESS_MAX_PROCS_PER_HART)
        return;

    tpa_sched_set_ready_slot(hartid, slot);
    tpa_sched_ring_if_armed(hartid);
}

static void runtime_wake_ep(const struct tpa_channel_ep *ep)
{
    if (ep)
        runtime_wake_slot(ep->hartid, ep->slot);
}

static void runtime_run_proc(struct tpa_proc *p, tpa_cont_t next)
{
    uint32_t hartid = tpa_hal_runtime_hartid();

    if (!p)
        return;

    p->k = next;
    tpa_process_complete_wait(p);

    if (p->hartid == hartid)
        runtime_schedule_local(p, next);
    else
        runtime_wake_slot(p->hartid, p->slot);
}

static void runtime_wait_send(struct tpa_proc *p,
                              const struct tpa_channel_op *op)
{
    struct tpa_process_wait wait;

    if (!op)
        return;

    wait.next = op->next;
    wait.bufp = 0;
    wait.lenp = 0;
    tpa_process_wait_send(p, &wait);
}

static void runtime_wait_recv(struct tpa_proc *p,
                              const struct tpa_channel_op *op)
{
    struct tpa_process_wait wait;

    if (!op)
        return;

    wait.next = op->next;
    wait.bufp = op->bufp;
    wait.lenp = op->lenp;
    tpa_process_wait_recv(p, &wait);
}

static void runtime_publish_msg(const struct tpa_channel_ep *ep, void *buf,
                                uint32_t len)
{
    struct tpa_proc *p;

    if (!ep)
        return;

    p = tpa_process_lookup(&proc_table, ep->hartid, ep->slot);
    tpa_process_publish_msg(p, buf, len);
}

static const struct tpa_channel_runtime channel_rt = {
    .bug = runtime_bug,
    .trace = tpa_hal_trace,
    .wake_ep = runtime_wake_ep,
    .run_proc = runtime_run_proc,
    .wait_send = runtime_wait_send,
    .wait_recv = runtime_wait_recv,
    .publish_msg = runtime_publish_msg,
};

void tpa_init(void)
{
    if (tpa_hal_atomic_compare_exchange_u32(&init_state, 0, 1) == 0) {
        tpa_process_table_init(&proc_table);
        tpa_sched_init();
        live_procs = __tpa_boot_count;
        failed = 0;
        tpa_hal_fence_rw();
        tpa_hal_atomic_exchange_u32(&init_state, 2);
        tpa_hal_wake_all_harts();
        return;
    }

    while (tpa_hal_atomic_or_u32(&init_state, 0) != 2)
        tpa_hal_wait_for_wake();
}

void tpa_boot_hart(uint32_t hartid)
{
    if (hartid >= TPA_HAL_NR_HARTS)
        return;

    current_proc[hartid] = 0;

    for (uint32_t i = 0; i < __tpa_boot_count; i++) {
        const struct tpa_boot_ent *ent = __tpa_boot_vector[i];
        struct tpa_process_boot boot;
        struct tpa_runtime_node *node;

        if (!ent || ent->hartid != hartid)
            continue;

        if (!ent->proc || ent->slot >= TPA_PROCESS_MAX_PROCS_PER_HART) {
            tpa_fail();
            continue;
        }

        boot.hartid = ent->hartid;
        boot.slot = ent->slot;
        boot.nr_portv = ent->nr_portv;
        boot.proc = ent->proc;
        boot.start = ent->start;
        boot.ws = ent->ws;
        boot.portv = ent->portv;
        tpa_proc_boot_init(ent->proc, &boot);

        if (!tpa_process_register(&proc_table, ent->proc)) {
            tpa_fail();
            continue;
        }

        node = &nodes[ent->hartid][ent->slot];
        tpa_sched_node_init(&node->node, ent->hartid);
        node->node.slot = ent->slot;
        node->proc = ent->proc;
        runtime_schedule_local(ent->proc, ent->start);
    }
}

struct tpa_proc *tpa_runq_pop(uint32_t hartid)
{
    struct tpa_sched_node *node = tpa_sched_pop(hartid);
    struct tpa_runtime_node *rt_node = (struct tpa_runtime_node *)node;

    return rt_node ? rt_node->proc : 0;
}

void tpa_reload(uint32_t hartid)
{
    for (uint32_t word = 0; word < TPA_PROCESS_READY_WORDS; word++) {
        uint64_t bits = tpa_sched_take_ready_word(hartid, word);

        while (bits) {
            uint32_t bit = (uint32_t)__builtin_ctzll(bits);
            uint32_t slot = word * 64u + bit;
            struct tpa_proc *p = tpa_process_lookup(&proc_table, hartid, slot);

            if (p && p->state != TPA_PROCESS_DEAD &&
                p->state != TPA_PROCESS_FAILED) {
                tpa_process_complete_wait(p);
                runtime_schedule_local(p, p->k);
            }

            bits &= bits - 1u;
        }
    }
}

void tpa_step(struct tpa_proc *p)
{
    tpa_op_t op;
    struct tpa_channel_op ch_op;
    uint32_t hartid;

    if (!p || !p->k)
        return;

    hartid = tpa_hal_runtime_hartid();
    if (hartid < TPA_HAL_NR_HARTS)
        current_proc[hartid] = p;

    p->state = TPA_PROCESS_RUN;
    op = p->k();

    switch (op.op) {
    case TPA_OP_STOP:
        p->state = TPA_PROCESS_DEAD;
        if (tpa_hal_atomic_add_u32(&live_procs, UINT32_MAX) == 1u)
            tpa_hal_wake_all_harts();
        break;
    case TPA_OP_YIELD:
        runtime_schedule_local(p, op.next);
        break;
    case TPA_OP_BLOCK:
        p->k = op.next;
        p->state = TPA_PROCESS_WAIT;
        break;
    case TPA_OP_SEND:
        ch_op.next = op.next;
        ch_op.ch = op.ch;
        ch_op.buf = op.buf;
        ch_op.bufp = 0;
        ch_op.lenp = 0;
        ch_op.len = op.len;
        tpa_channel_send(p, &ch_op, &channel_rt);
        break;
    case TPA_OP_RECV:
        ch_op.next = op.next;
        ch_op.ch = op.ch;
        ch_op.buf = 0;
        ch_op.bufp = op.bufp;
        ch_op.lenp = op.lenp;
        ch_op.len = 0;
        tpa_channel_recv(p, &ch_op, &channel_rt);
        break;
    default:
        tpa_fail();
        break;
    }

    if (hartid < TPA_HAL_NR_HARTS)
        current_proc[hartid] = 0;

    tpa_hal_flush(p, sizeof(*p));
    tpa_hal_fence_rw();
}

uint32_t tpa_done(void)
{
    return tpa_hal_atomic_or_u32(&live_procs, 0) == 0;
}

uint32_t tpa_failed(void)
{
    return tpa_hal_atomic_or_u32(&failed, 0) != 0;
}

void tpa_wake(struct tpa_proc *p)
{
    if (p)
        runtime_wake_slot(p->hartid, p->slot);
}

void *tpa_ws(void)
{
    struct tpa_proc *p = tpa_self();

    return p ? p->ws : 0;
}

struct tpa_proc *tpa_self(void)
{
    uint32_t hartid = tpa_hal_runtime_hartid();

    return hartid < TPA_HAL_NR_HARTS ? current_proc[hartid] : 0;
}

struct tpa_channel *tpa_chan(uint16_t port)
{
    struct tpa_proc *p = tpa_self();

    if (!p)
        return 0;

    for (uint16_t i = 0; i < p->nr_portv; i++) {
        if (p->portv[i].id == port)
            return (struct tpa_channel *)p->portv[i].ch;
    }

    return 0;
}

void tpa_trace(uint32_t tag)
{
    tpa_hal_trace(tag);
}

void tpa_fail(void)
{
    tpa_process_mark_failed(tpa_self());
    tpa_hal_atomic_exchange_u32(&failed, 1);
    tpa_hal_wake_all_harts();
}
