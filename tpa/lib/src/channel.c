/*-------------------------------------------------------------------------
 * Copyright (c) 2025 Ainekko, Co.
 * SPDX-License-Identifier: Apache-2.0
 *-------------------------------------------------------------------------*/

#include "tpa/channel.h"

#include <string.h>

static void channel_bug(const struct tpa_channel_runtime *rt)
{
    if (rt && rt->bug)
        rt->bug();

    for (;;)
        ;
}

static void channel_trace(const struct tpa_channel_runtime *rt, uint32_t tag)
{
    if (rt && rt->trace)
        rt->trace(tag);
    else
        tpa_hal_trace(tag);
}

uint32_t tpa_channel_state(struct tpa_channel *ch)
{
    return tpa_hal_atomic_or_u32(&ch->state, 0);
}

static int channel_try_state(struct tpa_channel *ch, uint32_t from,
                             uint32_t to)
{
    return tpa_hal_atomic_compare_exchange_u32(&ch->state, from, to) == from;
}

static void channel_set_state(struct tpa_channel *ch, uint32_t state)
{
    tpa_hal_atomic_exchange_u32(&ch->state, state);
}

static void channel_check_len(struct tpa_channel *ch, uint32_t len,
                              const struct tpa_channel_runtime *rt)
{
    if (!ch->cap || len > ch->cap)
        channel_bug(rt);
}

static void *channel_rbuf(struct tpa_channel *ch)
{
    if (!ch->nrbuf)
        return 0;

    return ch->bufv[ch->turn & 1u];
}

static void channel_rotate_buf(struct tpa_channel *ch)
{
    if (ch->nrbuf > 1u)
        ch->turn ^= 1u;
}

static void *channel_copy_to_storage(struct tpa_channel *ch, void *src,
                                     uint32_t len,
                                     const struct tpa_channel_runtime *rt)
{
    void *dst = channel_rbuf(ch);

    if (!dst)
        channel_bug(rt);

    if (dst != src)
        memcpy(dst, src, len);

    tpa_hal_flush(dst, len);
    channel_rotate_buf(ch);
    tpa_hal_flush(ch, sizeof(*ch));
    return dst;
}

static void *channel_deliver_buf(struct tpa_channel *ch, void *src,
                                 uint32_t len,
                                 const struct tpa_channel_runtime *rt,
                                 int force_storage)
{
    if (!force_storage && !ch->nrbuf)
        return src;

    return channel_copy_to_storage(ch, src, len, rt);
}

static int channel_requires_storage(const struct tpa_channel *ch)
{
    return ch->kind == TPA_CHANNEL_KIND_FABRIC;
}

static void channel_wake_ep(const struct tpa_channel_runtime *rt,
                            const struct tpa_channel_ep *ep)
{
    if (!rt || !rt->wake_ep)
        channel_bug(rt);

    rt->wake_ep(ep);
}

static void channel_publish_msg(const struct tpa_channel_runtime *rt,
                                const struct tpa_channel_ep *ep, void *buf,
                                uint32_t len)
{
    if (!rt || !rt->publish_msg)
        channel_bug(rt);

    rt->publish_msg(ep, buf, len);
}

static void channel_run_proc(const struct tpa_channel_runtime *rt,
                             struct tpa_proc *p, tpa_cont_t next)
{
    if (!rt || !rt->run_proc)
        channel_bug(rt);

    rt->run_proc(p, next);
}

static void channel_wait_send(const struct tpa_channel_runtime *rt,
                              struct tpa_proc *p,
                              const struct tpa_channel_op *op)
{
    if (!rt || !rt->wait_send)
        channel_bug(rt);

    rt->wait_send(p, op);
}

static void channel_wait_recv(const struct tpa_channel_runtime *rt,
                              struct tpa_proc *p,
                              const struct tpa_channel_op *op)
{
    if (!rt || !rt->wait_recv)
        channel_bug(rt);

    rt->wait_recv(p, op);
}

static void channel_send_handoff(struct tpa_proc *p,
                                 const struct tpa_channel_op *op,
                                 const struct tpa_channel_runtime *rt,
                                 int force_storage)
{
    struct tpa_channel *ch = op->ch;
    void *buf;
    uint32_t state;

    if (!ch)
        return;

    channel_trace(rt, TPA_CHANNEL_TRACE_SEND_BEG);
    channel_check_len(ch, op->len, rt);
    tpa_hal_flush(op->buf, op->len);

    for (;;) {
        state = tpa_channel_state(ch);

        if (state == TPA_CHANNEL_STATE_TX || state == TPA_CHANNEL_STATE_RX)
            continue;

        if (state == TPA_CHANNEL_STATE_EMPTY) {
            if (!channel_try_state(ch, TPA_CHANNEL_STATE_EMPTY,
                                   TPA_CHANNEL_STATE_TX))
                continue;

            ch->buf = op->buf;
            ch->len = op->len;
            tpa_hal_flush(ch, sizeof(*ch));
            channel_wait_send(rt, p, op);
            channel_set_state(ch, TPA_CHANNEL_STATE_WAIT_TX);
            channel_trace(rt, TPA_CHANNEL_TRACE_SEND_END);
            return;
        }

        if (state == TPA_CHANNEL_STATE_WAIT_RX) {
            if (!channel_try_state(ch, TPA_CHANNEL_STATE_WAIT_RX,
                                   TPA_CHANNEL_STATE_TX))
                continue;

            buf = channel_deliver_buf(ch, op->buf, op->len, rt,
                                      force_storage);
            channel_publish_msg(rt, &ch->rx, buf, op->len);
            channel_set_state(ch, TPA_CHANNEL_STATE_EMPTY);
            channel_wake_ep(rt, &ch->rx);
            channel_run_proc(rt, p, op->next);
            channel_trace(rt, TPA_CHANNEL_TRACE_SEND_END);
            return;
        }
    }
}

static void channel_recv_handoff(struct tpa_proc *p,
                                 const struct tpa_channel_op *op,
                                 const struct tpa_channel_runtime *rt,
                                 int force_storage)
{
    struct tpa_channel *ch = op->ch;
    void *buf;
    uint32_t len;
    uint32_t state;

    if (!ch)
        return;

    channel_trace(rt, TPA_CHANNEL_TRACE_RECV_BEG);
    for (;;) {
        state = tpa_channel_state(ch);

        if (state == TPA_CHANNEL_STATE_TX || state == TPA_CHANNEL_STATE_RX)
            continue;

        if (state == TPA_CHANNEL_STATE_EMPTY) {
            if (!channel_try_state(ch, TPA_CHANNEL_STATE_EMPTY,
                                   TPA_CHANNEL_STATE_RX))
                continue;

            channel_wait_recv(rt, p, op);
            channel_set_state(ch, TPA_CHANNEL_STATE_WAIT_RX);
            channel_trace(rt, TPA_CHANNEL_TRACE_RECV_END);
            return;
        }

        if (state == TPA_CHANNEL_STATE_WAIT_TX) {
            if (!channel_try_state(ch, TPA_CHANNEL_STATE_WAIT_TX,
                                   TPA_CHANNEL_STATE_RX))
                continue;

            len = ch->len;
            channel_check_len(ch, len, rt);
            tpa_hal_evict(ch->buf, len);
            buf = channel_deliver_buf(ch, ch->buf, len, rt, force_storage);

            if (op->bufp)
                *op->bufp = buf;
            if (op->lenp)
                *op->lenp = len;

            channel_set_state(ch, TPA_CHANNEL_STATE_EMPTY);
            channel_wake_ep(rt, &ch->tx);
            channel_run_proc(rt, p, op->next);
            channel_trace(rt, TPA_CHANNEL_TRACE_RECV_END);
            return;
        }
    }
}

void tpa_channel_send(struct tpa_proc *p, const struct tpa_channel_op *op,
                      const struct tpa_channel_runtime *rt)
{
    struct tpa_channel *ch;

    if (!op || !op->ch)
        return;

    ch = op->ch;
    switch (ch->kind) {
    case TPA_CHANNEL_KIND_DIRECT:
    case TPA_CHANNEL_KIND_LOCAL:
        channel_send_handoff(p, op, rt, 0);
        return;
    case TPA_CHANNEL_KIND_FABRIC:
        channel_send_handoff(p, op, rt, channel_requires_storage(ch));
        return;
    case TPA_CHANNEL_KIND_EXTERNAL:
    default:
        channel_bug(rt);
        return;
    }
}

void tpa_channel_recv(struct tpa_proc *p, const struct tpa_channel_op *op,
                      const struct tpa_channel_runtime *rt)
{
    struct tpa_channel *ch;

    if (!op || !op->ch)
        return;

    ch = op->ch;
    switch (ch->kind) {
    case TPA_CHANNEL_KIND_DIRECT:
    case TPA_CHANNEL_KIND_LOCAL:
        channel_recv_handoff(p, op, rt, 0);
        return;
    case TPA_CHANNEL_KIND_FABRIC:
        channel_recv_handoff(p, op, rt, channel_requires_storage(ch));
        return;
    case TPA_CHANNEL_KIND_EXTERNAL:
    default:
        channel_bug(rt);
        return;
    }
}
