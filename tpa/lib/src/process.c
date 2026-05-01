/*-------------------------------------------------------------------------
 * Copyright (c) 2025 Ainekko, Co.
 * SPDX-License-Identifier: Apache-2.0
 *-------------------------------------------------------------------------*/

#include "tpa/process.h"

static void process_zero(void *ptr, size_t bytes)
{
    unsigned char *p = (unsigned char *)ptr;

    for (size_t i = 0; i < bytes; i++)
        p[i] = 0;
}

static void process_flush_obj(const volatile void *ptr, size_t bytes)
{
    tpa_hal_flush(ptr, bytes);
}

static void process_evict_obj(const volatile void *ptr, size_t bytes)
{
    tpa_hal_evict(ptr, bytes);
}

static void process_msg_flush(struct tpa_proc *p)
{
    process_flush_obj(p, TPA_HAL_CACHELINE_BYTES);
}

static void process_msg_evict(struct tpa_proc *p)
{
    process_evict_obj(p, TPA_HAL_CACHELINE_BYTES);
}

static void process_clear_mailbox(struct tpa_proc *p)
{
    p->bufp = 0;
    p->lenp = 0;
    p->msg_ready = 0;
    p->msg_buf = 0;
    p->msg_len = 0;
}

void tpa_process_table_init(struct tpa_process_table *table)
{
    if (!table)
        return;

    process_zero(table, sizeof(*table));
    process_flush_obj(table, sizeof(*table));
    tpa_hal_fence_rw();
}

void tpa_proc_init(struct tpa_proc *p, uint32_t hartid, void *ws,
                   tpa_cont_t k)
{
    if (!p)
        return;

    p->k = k;
    p->ws = ws;
    p->portv = 0;
    p->next = 0;
    p->hartid = (uint16_t)hartid;
    p->state = TPA_PROCESS_DEAD;
    p->slot = TPA_PROCESS_NO_SLOT;
    p->nr_portv = 0;
    process_clear_mailbox(p);
    process_flush_obj(p, sizeof(*p));
}

void tpa_proc_boot_init(struct tpa_proc *p,
                        const struct tpa_process_boot *boot)
{
    if (!p || !boot)
        return;

    p->k = boot->start;
    p->ws = boot->ws;
    p->portv = boot->portv;
    p->next = 0;
    p->hartid = (uint16_t)boot->hartid;
    p->state = TPA_PROCESS_DEAD;
    p->slot = boot->slot;
    p->nr_portv = boot->nr_portv;
    process_clear_mailbox(p);
    process_flush_obj(p, sizeof(*p));
}

int tpa_process_register(struct tpa_process_table *table, struct tpa_proc *p)
{
    uint32_t hartid;
    uint16_t slot;

    if (!table || !p)
        return 0;

    hartid = p->hartid;
    if (hartid >= TPA_HAL_NR_HARTS)
        return 0;

    slot = p->slot;
    if (slot == TPA_PROCESS_NO_SLOT)
        slot = table->nslot[hartid].n;

    if (slot >= TPA_PROCESS_MAX_PROCS_PER_HART)
        return 0;

    table->slotv[hartid].v[slot] = p;
    p->slot = slot;
    if (table->nslot[hartid].n <= slot)
        table->nslot[hartid].n = (uint16_t)(slot + 1u);

    process_flush_obj(&table->slotv[hartid], sizeof(table->slotv[hartid]));
    process_flush_obj(&table->nslot[hartid], sizeof(table->nslot[hartid]));
    process_flush_obj(p, sizeof(*p));
    tpa_hal_fence_rw();

    return 1;
}

struct tpa_proc *tpa_process_lookup(struct tpa_process_table *table,
                                    uint32_t hartid, uint32_t slot)
{
    if (!table || hartid >= TPA_HAL_NR_HARTS ||
        slot >= TPA_PROCESS_MAX_PROCS_PER_HART)
        return 0;

    process_evict_obj(&table->slotv[hartid], sizeof(table->slotv[hartid]));
    return table->slotv[hartid].v[slot];
}

void tpa_process_wait_send(struct tpa_proc *p,
                           const struct tpa_process_wait *wait)
{
    if (!p || !wait)
        return;

    p->k = wait->next;
    process_clear_mailbox(p);
    p->state = TPA_PROCESS_WAIT;
    tpa_hal_fence_rw();
}

void tpa_process_wait_recv(struct tpa_proc *p,
                           const struct tpa_process_wait *wait)
{
    if (!p || !wait)
        return;

    p->k = wait->next;
    p->bufp = wait->bufp;
    p->lenp = wait->lenp;
    p->msg_ready = 0;
    p->msg_buf = 0;
    p->msg_len = 0;
    process_msg_flush(p);
    p->state = TPA_PROCESS_WAIT;
    tpa_hal_fence_rw();
}

void tpa_process_publish_msg(struct tpa_proc *p, void *buf, uint32_t len)
{
    if (!p)
        return;

    p->msg_buf = buf;
    p->msg_len = len;
    p->msg_ready = 1;
    process_msg_flush(p);
    tpa_hal_fence_rw();
}

void tpa_process_complete_wait(struct tpa_proc *p)
{
    void *buf = 0;
    uint32_t len = 0;

    if (!p || (!p->bufp && !p->lenp))
        return;

    process_msg_evict(p);

    if (p->msg_ready) {
        buf = p->msg_buf;
        len = p->msg_len;
    }

    if (buf && len)
        tpa_hal_evict(buf, len);

    if (p->bufp)
        *p->bufp = buf;

    if (p->lenp)
        *p->lenp = len;

    process_clear_mailbox(p);
    process_msg_flush(p);
}

void tpa_process_mark_failed(struct tpa_proc *p)
{
    if (!p)
        return;

    p->state = TPA_PROCESS_FAILED;
    process_flush_obj(p, sizeof(*p));
    tpa_hal_fence_rw();
}
