/*-------------------------------------------------------------------------
 * Copyright (c) 2025 Ainekko, Co.
 * SPDX-License-Identifier: Apache-2.0
 *-------------------------------------------------------------------------*/

#ifndef TPA_TPA_H
#define TPA_TPA_H

#include <stddef.h>
#include <stdint.h>

#include "tpa/channel.h"
#include "tpa/hal.h"
#include "tpa/process.h"
#include "tpa/scheduler.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
#define TPA_STATIC_ASSERT(cond, msg) static_assert((cond), msg)
#else
#define TPA_STATIC_ASSERT(cond, msg) _Static_assert((cond), msg)
#endif

#define TPA_CACHELINE_BYTES TPA_HAL_CACHELINE_BYTES
#define TPA_CACHELINE_PAD_BYTES(bytes) TPA_PROCESS_PAD_BYTES(bytes)
#define TPA_CACHELINE_ASSERT(type)                                           \
    TPA_STATIC_ASSERT((sizeof(type) % TPA_CACHELINE_BYTES) == 0,             \
                      #type " must be padded to cacheline size")
#define TPA_FLUSH_BYTES(ptr, bytes) tpa_hal_flush((ptr), (bytes))
#define TPA_EVICT_BYTES(ptr, bytes) tpa_hal_evict((ptr), (bytes))
#define TPA_FLUSH_OBJ(ptr) TPA_FLUSH_BYTES((ptr), sizeof(*(ptr)))
#define TPA_EVICT_OBJ(ptr) TPA_EVICT_BYTES((ptr), sizeof(*(ptr)))

#define TPA_MAX_PROCS_PER_HART TPA_PROCESS_MAX_PROCS_PER_HART
#define TPA_READY_WORDS TPA_PROCESS_READY_WORDS
#define TPA_DEAD TPA_PROCESS_DEAD
#define TPA_READY TPA_PROCESS_READY
#define TPA_WAIT TPA_PROCESS_WAIT
#define TPA_RUN TPA_PROCESS_RUN
#define TPA_FAILED TPA_PROCESS_FAILED

#define TPA_CH_KIND_DIRECT TPA_HAL_CH_KIND_DIRECT
#define TPA_CH_KIND_LOCAL TPA_HAL_CH_KIND_LOCAL
#define TPA_CH_KIND_FABRIC TPA_HAL_CH_KIND_FABRIC
#define TPA_CH_KIND_EXTERNAL TPA_HAL_CH_KIND_EXTERNAL
#define TPA_CH_DIRECT TPA_CHANNEL_KIND_DIRECT
#define TPA_CH_LOCAL TPA_CHANNEL_KIND_LOCAL
#define TPA_CH_FABRIC TPA_CHANNEL_KIND_FABRIC
#define TPA_CH_EXTERNAL TPA_CHANNEL_KIND_EXTERNAL
#define TPA_CH_KIND(src_hartid, dst_hartid)                                  \
    TPA_ARCH_CH_KIND((src_hartid), (dst_hartid), TPA_CH_KIND_DIRECT,         \
                     TPA_CH_KIND_LOCAL, TPA_CH_KIND_FABRIC,                  \
                     TPA_CH_KIND_EXTERNAL)
#define TPA_CHANNEL_KIND(src_hartid, dst_hartid)                             \
    TPA_ARCH_CH_KIND((src_hartid), (dst_hartid),                             \
                     TPA_CHANNEL_KIND_DIRECT, TPA_CHANNEL_KIND_LOCAL,        \
                     TPA_CHANNEL_KIND_FABRIC, TPA_CHANNEL_KIND_EXTERNAL)

#define tpa_chan tpa_channel

#define arch_trace(tag) tpa_hal_trace((tag))
#define arch_diag_putc(c) tpa_hal_diag_putc((c))
#define arch_runtime_hartid() tpa_hal_runtime_hartid()
#define arch_fence_rw() tpa_hal_fence_rw()
#define arch_flush(ptr, bytes) tpa_hal_flush((ptr), (bytes))
#define arch_evict(ptr, bytes) tpa_hal_evict((ptr), (bytes))
#define et_cache_flush_line(addr)                                             \
    tpa_hal_flush((const volatile void *)(uintptr_t)(addr),                   \
                  TPA_HAL_CACHELINE_BYTES)
#define et_cache_evict_line(addr)                                             \
    tpa_hal_evict((const volatile void *)(uintptr_t)(addr),                   \
                  TPA_HAL_CACHELINE_BYTES)
#define et_cache_evict_l1d_to_l2() tpa_hal_evict((const volatile void *)0, TPA_HAL_L1D_BYTES)

enum {
    TPA_OP_STOP = 0,
    TPA_OP_YIELD,
    TPA_OP_BLOCK,
    TPA_OP_SEND,
    TPA_OP_RECV,
};

struct tpa_op {
    uint32_t op;
    tpa_cont_t next;
    struct tpa_channel *ch;
    void *buf;
    void **bufp;
    uint32_t *lenp;
    uint32_t len;
};

#include "tpa/image.h"

static inline tpa_op_t tpa_stop(void)
{
    tpa_op_t op = { .op = TPA_OP_STOP };
    return op;
}

static inline tpa_op_t tpa_yield(tpa_cont_t next)
{
    tpa_op_t op = { .op = TPA_OP_YIELD, .next = next };
    return op;
}

static inline tpa_op_t tpa_block(tpa_cont_t next)
{
    tpa_op_t op = { .op = TPA_OP_BLOCK, .next = next };
    return op;
}

static inline tpa_op_t tpa_send(struct tpa_channel *ch, void *buf,
                                uint32_t len, tpa_cont_t next)
{
    tpa_op_t op = {
        .op = TPA_OP_SEND,
        .next = next,
        .ch = ch,
        .buf = buf,
        .len = len,
    };
    return op;
}

static inline tpa_op_t tpa_send_const(struct tpa_channel *ch, const void *buf,
                                      uint32_t len, tpa_cont_t next)
{
    union {
        const void *cptr;
        void *ptr;
    } ptr = { .cptr = buf };
    return tpa_send(ch, ptr.ptr, len, next);
}

static inline void *tpa_send_buf(struct tpa_channel *ch)
{
    if (!ch || !ch->nrbuf)
        return 0;
    return ch->bufv[ch->turn & 1u];
}

static inline tpa_op_t tpa_recv(struct tpa_channel *ch, void **bufp,
                                uint32_t *lenp, tpa_cont_t next)
{
    tpa_op_t op = {
        .op = TPA_OP_RECV,
        .next = next,
        .ch = ch,
        .bufp = bufp,
        .lenp = lenp,
    };
    return op;
}

void tpa_wake(struct tpa_proc *p);
void *tpa_ws(void);
struct tpa_proc *tpa_self(void);
struct tpa_channel *tpa_chan(uint16_t port);
void tpa_trace(uint32_t tag);
void tpa_fail(void);

#ifdef __cplusplus
}
#endif

#endif /* TPA_TPA_H */
