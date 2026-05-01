/*-------------------------------------------------------------------------
 * Copyright (c) 2025 Ainekko, Co.
 * SPDX-License-Identifier: Apache-2.0
 *-------------------------------------------------------------------------*/

#ifndef TPA_PROCESS_H
#define TPA_PROCESS_H

#include <stddef.h>
#include <stdint.h>

#include "tpa/hal.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef TPA_HAL_CACHELINE_BYTES
#error "TPA_HAL_CACHELINE_BYTES must be defined by the selected platform build"
#endif

#ifndef TPA_HAL_NR_HARTS
#error "TPA_HAL_NR_HARTS must be defined by the selected platform build"
#endif

#define TPA_PROCESS_NO_SLOT             0xffffu
#define TPA_PROCESS_MAX_PROCS_PER_HART  128u
#define TPA_PROCESS_READY_WORDS         (TPA_PROCESS_MAX_PROCS_PER_HART / 64u)
#define TPA_PROCESS_MSG_BYTES                                           \
    (sizeof(uint32_t) + sizeof(uint32_t) + sizeof(void *))
#define TPA_PROCESS_PAD_BYTES(bytes)                                    \
    ((TPA_HAL_CACHELINE_BYTES - ((bytes) % TPA_HAL_CACHELINE_BYTES)) %  \
     TPA_HAL_CACHELINE_BYTES)

typedef struct tpa_op tpa_op_t;
typedef tpa_op_t (*tpa_cont_t)(void);

struct tpa_port_ref {
    uint16_t id;
    uint16_t pad;
    void *ch;
};

enum tpa_process_state {
    TPA_PROCESS_DEAD = 0,
    TPA_PROCESS_READY,
    TPA_PROCESS_WAIT,
    TPA_PROCESS_RUN,
    TPA_PROCESS_FAILED,
};

struct tpa_process_payload {
    volatile uint32_t msg_ready;
    uint32_t msg_len;
    void *msg_buf;
    uint8_t __tpa_msg_pad[TPA_PROCESS_PAD_BYTES(TPA_PROCESS_MSG_BYTES)];
    tpa_cont_t k;
    void *ws;
    const struct tpa_port_ref *portv;
    struct tpa_proc *next;
    uint16_t hartid;
    uint16_t state;
    uint16_t slot;
    uint16_t nr_portv;
    void **bufp;
    uint32_t *lenp;
} __attribute__((packed));

struct tpa_proc {
    volatile uint32_t msg_ready;
    uint32_t msg_len;
    void *msg_buf;
    uint8_t __tpa_msg_pad[TPA_PROCESS_PAD_BYTES(TPA_PROCESS_MSG_BYTES)];
    tpa_cont_t k;
    void *ws;
    const struct tpa_port_ref *portv;
    struct tpa_proc *next;
    uint16_t hartid;
    uint16_t state;
    uint16_t slot;
    uint16_t nr_portv;
    void **bufp;
    uint32_t *lenp;
    uint8_t __tpa_cacheline_pad[TPA_PROCESS_PAD_BYTES(
        sizeof(struct tpa_process_payload))];
} __attribute__((packed, aligned(TPA_HAL_CACHELINE_BYTES)));

typedef char tpa_process_cacheline_check[
    (sizeof(struct tpa_proc) % TPA_HAL_CACHELINE_BYTES) == 0 ? 1 : -1];
typedef char tpa_process_mailbox_check[
    offsetof(struct tpa_proc, k) == TPA_HAL_CACHELINE_BYTES ? 1 : -1];

struct tpa_process_slot_count {
    uint16_t n;
    uint8_t __tpa_cacheline_pad[TPA_PROCESS_PAD_BYTES(sizeof(uint16_t))];
} __attribute__((packed, aligned(TPA_HAL_CACHELINE_BYTES)));

struct tpa_process_slots_payload {
    struct tpa_proc *v[TPA_PROCESS_MAX_PROCS_PER_HART];
} __attribute__((packed));

struct tpa_process_slots {
    struct tpa_proc *v[TPA_PROCESS_MAX_PROCS_PER_HART];
    uint8_t __tpa_cacheline_pad[TPA_PROCESS_PAD_BYTES(
        sizeof(struct tpa_process_slots_payload))];
} __attribute__((packed, aligned(TPA_HAL_CACHELINE_BYTES)));

struct tpa_process_table {
    struct tpa_process_slot_count nslot[TPA_HAL_NR_HARTS];
    struct tpa_process_slots slotv[TPA_HAL_NR_HARTS];
};

struct tpa_process_boot {
    uint32_t hartid;
    uint16_t slot;
    uint16_t nr_portv;
    struct tpa_proc *proc;
    tpa_cont_t start;
    void *ws;
    const struct tpa_port_ref *portv;
};

struct tpa_process_wait {
    tpa_cont_t next;
    void **bufp;
    uint32_t *lenp;
};

void tpa_process_table_init(struct tpa_process_table *table);
void tpa_proc_init(struct tpa_proc *p, uint32_t hartid, void *ws,
                   tpa_cont_t k);
void tpa_proc_boot_init(struct tpa_proc *p,
                        const struct tpa_process_boot *boot);
int tpa_process_register(struct tpa_process_table *table, struct tpa_proc *p);
struct tpa_proc *tpa_process_lookup(struct tpa_process_table *table,
                                    uint32_t hartid, uint32_t slot);

void tpa_process_wait_send(struct tpa_proc *p,
                           const struct tpa_process_wait *wait);
void tpa_process_wait_recv(struct tpa_proc *p,
                           const struct tpa_process_wait *wait);
void tpa_process_publish_msg(struct tpa_proc *p, void *buf, uint32_t len);
void tpa_process_complete_wait(struct tpa_proc *p);
void tpa_process_mark_failed(struct tpa_proc *p);

#ifdef __cplusplus
}
#endif

#endif /* TPA_PROCESS_H */
