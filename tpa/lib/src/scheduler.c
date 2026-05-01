/*-------------------------------------------------------------------------
 * Copyright (c) 2025 Ainekko, Co.
 * SPDX-License-Identifier: Apache-2.0
 *-------------------------------------------------------------------------*/

#include <tpa/scheduler.h>

#if !defined(TPA_HAL_NR_HARTS)
#error "TPA_HAL_NR_HARTS must be defined by the selected HAL platform config"
#endif

struct tpa_sched_runq {
    volatile uint32_t lock;
    struct tpa_sched_node *head;
    struct tpa_sched_node *tail;
};

struct tpa_sched_rx {
    volatile uint64_t bits[TPA_SCHED_READY_WORDS];
    volatile uint32_t armed;
    volatile uint32_t bell;
};

static struct tpa_sched_runq runq[TPA_HAL_NR_HARTS];
static struct tpa_sched_rx rx[TPA_HAL_NR_HARTS];

static int valid_hart(uint32_t hartid)
{
    return hartid < TPA_HAL_NR_HARTS;
}

static void flush_runq(uint32_t hartid)
{
    tpa_hal_flush(&runq[hartid], sizeof(runq[hartid]));
}

static void flush_rx(uint32_t hartid)
{
    tpa_hal_flush(&rx[hartid], sizeof(rx[hartid]));
}

void tpa_sched_init(void)
{
    for (uint32_t hartid = 0; hartid < TPA_HAL_NR_HARTS; hartid++) {
        runq[hartid].lock = 0;
        runq[hartid].head = 0;
        runq[hartid].tail = 0;

        for (uint32_t word = 0; word < TPA_SCHED_READY_WORDS; word++)
            rx[hartid].bits[word] = 0;
        rx[hartid].armed = 0;
        rx[hartid].bell = 0;
    }

    tpa_hal_flush(runq, sizeof(runq));
    tpa_hal_flush(rx, sizeof(rx));
    tpa_hal_fence_rw();
}

void tpa_sched_node_init(struct tpa_sched_node *node, uint32_t hartid)
{
    if (!node)
        return;

    node->next = 0;
    node->hartid = (uint16_t)hartid;
    node->slot = TPA_SCHED_NO_SLOT;
    node->state = TPA_SCHED_DEAD;
    tpa_hal_flush(node, sizeof(*node));
}

static void runq_lock(struct tpa_sched_runq *q)
{
    while (tpa_hal_atomic_compare_exchange_u32(&q->lock, 0, 1) != 0)
        ;
}

static void runq_unlock(struct tpa_sched_runq *q)
{
    tpa_hal_atomic_exchange_u32(&q->lock, 0);
}

static void runq_push(struct tpa_sched_runq *q, struct tpa_sched_node *node)
{
    struct tpa_sched_node *head;
    struct tpa_sched_node *tail;

    node->next = 0;
    node->state = TPA_SCHED_READY;

    head = q->head;
    tail = q->tail;
    if (head)
        tail->next = node;
    else
        q->head = node;

    q->tail = node;
}

int tpa_sched_run(struct tpa_sched_node *node)
{
    uint32_t hartid;

    if (!node)
        return 0;

    hartid = node->hartid;
    if (!valid_hart(hartid))
        return 0;

    runq_lock(&runq[hartid]);
    runq_push(&runq[hartid], node);
    flush_runq(hartid);
    tpa_hal_flush(node, sizeof(*node));
    tpa_hal_fence_rw();
    runq_unlock(&runq[hartid]);
    return 1;
}

struct tpa_sched_node *tpa_sched_pop(uint32_t hartid)
{
    struct tpa_sched_runq *q;
    struct tpa_sched_node *node;

    if (!valid_hart(hartid))
        return 0;

    q = &runq[hartid];
    runq_lock(q);
    node = q->head;
    if (node) {
        q->head = node->next;
        if (!q->head)
            q->tail = 0;

        node->next = 0;
        node->state = TPA_SCHED_RUN;
        tpa_hal_flush(node, sizeof(*node));
    }

    flush_runq(hartid);
    tpa_hal_fence_rw();
    runq_unlock(q);

    return node;
}

void tpa_sched_set_ready_slot(uint32_t hartid, uint32_t slot)
{
    uint32_t word = slot >> 6;
    uint32_t bit = slot & 63u;

    if (!valid_hart(hartid) || word >= TPA_SCHED_READY_WORDS)
        return;

    tpa_hal_atomic_or_u64(&rx[hartid].bits[word], UINT64_C(1) << bit);
}

void tpa_sched_ring_if_armed(uint32_t hartid)
{
    struct tpa_sched_rx *r;

    if (!valid_hart(hartid))
        return;

    r = &rx[hartid];
    if (!(tpa_hal_atomic_or_u32(&r->armed, 0) & 1u))
        return;

    if (tpa_hal_atomic_or_u32(&r->bell, 1) & 1u)
        return;

    tpa_hal_fence_rw();
    tpa_hal_wake_hart(hartid);
}

uint64_t tpa_sched_take_ready_word(uint32_t hartid, uint32_t word)
{
    if (!valid_hart(hartid) || word >= TPA_SCHED_READY_WORDS)
        return 0;

    return tpa_hal_atomic_exchange_u64(&rx[hartid].bits[word], 0);
}

void tpa_sched_set_armed(uint32_t hartid, uint32_t armed)
{
    if (!valid_hart(hartid))
        return;

    tpa_hal_atomic_exchange_u32(&rx[hartid].armed, armed ? 1u : 0u);
    flush_rx(hartid);
}

uint32_t tpa_sched_take_bell(uint32_t hartid)
{
    if (!valid_hart(hartid))
        return 0;

    return tpa_hal_atomic_exchange_u32(&rx[hartid].bell, 0);
}
