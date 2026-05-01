/*-------------------------------------------------------------------------
 * Copyright (c) 2025 Ainekko, Co.
 * SPDX-License-Identifier: Apache-2.0
 *-------------------------------------------------------------------------*/

#ifndef TPA_CHANNEL_H
#define TPA_CHANNEL_H

#include <stdint.h>

#include "tpa/hal.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef TPA_HAL_CACHELINE_BYTES
#error "TPA_HAL_CACHELINE_BYTES must be defined by the selected platform build"
#endif

#define TPA_CHANNEL_KIND_DIRECT    TPA_HAL_CH_KIND_DIRECT
#define TPA_CHANNEL_KIND_LOCAL     TPA_HAL_CH_KIND_LOCAL
#define TPA_CHANNEL_KIND_FABRIC    TPA_HAL_CH_KIND_FABRIC
#define TPA_CHANNEL_KIND_EXTERNAL  TPA_HAL_CH_KIND_EXTERNAL

#define TPA_CHANNEL_STATE_EMPTY    0u
#define TPA_CHANNEL_STATE_TX       1u
#define TPA_CHANNEL_STATE_WAIT_TX  2u
#define TPA_CHANNEL_STATE_RX       3u
#define TPA_CHANNEL_STATE_WAIT_RX  4u

#define TPA_CHANNEL_PAD_BYTES(bytes)                                        \
    ((TPA_HAL_CACHELINE_BYTES - ((bytes) % TPA_HAL_CACHELINE_BYTES)) %      \
     TPA_HAL_CACHELINE_BYTES)

struct tpa_proc;

typedef struct tpa_op tpa_op_t;
typedef tpa_op_t (*tpa_cont_t)(void);

struct tpa_channel_ep {
    uint16_t hartid;
    uint16_t slot;
} __attribute__((packed));

struct tpa_channel_payload {
    volatile uint32_t state;
    uint16_t kind;
    uint8_t nrbuf;
    uint8_t turn;
    uint32_t len;
    struct tpa_channel_ep tx;
    struct tpa_channel_ep rx;
    void *buf;
    void *bufv[2];
    uint32_t cap;
} __attribute__((packed));

struct tpa_channel {
    volatile uint32_t state;
    uint16_t kind;
    uint8_t nrbuf;
    uint8_t turn;
    uint32_t len;
    struct tpa_channel_ep tx;
    struct tpa_channel_ep rx;
    void *buf;
    void *bufv[2];
    uint32_t cap;
    uint8_t __tpa_cacheline_pad[TPA_CHANNEL_PAD_BYTES(
        sizeof(struct tpa_channel_payload))];
} __attribute__((packed, aligned(TPA_HAL_CACHELINE_BYTES)));

typedef char tpa_channel_cacheline_check[
    (sizeof(struct tpa_channel) % TPA_HAL_CACHELINE_BYTES) == 0 ? 1 : -1];

struct tpa_channel_op {
    tpa_cont_t next;
    struct tpa_channel *ch;
    void *buf;
    void **bufp;
    uint32_t *lenp;
    uint32_t len;
};

/*
 * Process/scheduler integration points needed by the channel state machine.
 * Later process and scheduler modules should provide thin adapters for these
 * callbacks so channel.c remains independent from run-queue internals.
 */
struct tpa_channel_runtime {
    void (*bug)(void);
    void (*trace)(uint32_t tag);
    void (*wake_ep)(const struct tpa_channel_ep *ep);
    void (*run_proc)(struct tpa_proc *p, tpa_cont_t next);
    void (*wait_send)(struct tpa_proc *p, const struct tpa_channel_op *op);
    void (*wait_recv)(struct tpa_proc *p, const struct tpa_channel_op *op);
    void (*publish_msg)(const struct tpa_channel_ep *ep, void *buf,
                        uint32_t len);
};

enum {
    TPA_CHANNEL_TRACE_SEND_BEG = 0xd3520010u,
    TPA_CHANNEL_TRACE_SEND_END = 0xd3520011u,
    TPA_CHANNEL_TRACE_RECV_BEG = 0xd3520020u,
    TPA_CHANNEL_TRACE_RECV_END = 0xd3520021u,
};

uint32_t tpa_channel_state(struct tpa_channel *ch);
void tpa_channel_send(struct tpa_proc *p, const struct tpa_channel_op *op,
                      const struct tpa_channel_runtime *rt);
void tpa_channel_recv(struct tpa_proc *p, const struct tpa_channel_op *op,
                      const struct tpa_channel_runtime *rt);

#ifdef __cplusplus
}
#endif

#endif /* TPA_CHANNEL_H */
