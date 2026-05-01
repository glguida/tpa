/*-------------------------------------------------------------------------
 * Copyright (c) 2025 Ainekko, Co.
 * SPDX-License-Identifier: Apache-2.0
 *-------------------------------------------------------------------------*/

#include "test.h"
#include "tpa/tpa.h"

#define MSG_VAL     0x56414c55455f4544ull
#define CHK_LIMIT   256u

extern unsigned char tpa_msg_edge_storage_ch0_buf[];

static uint64_t tx_buf = MSG_VAL;
static uint64_t *rx_buf;
static uint32_t rx_len;
static volatile uint64_t rx_val;
static volatile uint32_t tx_done;
static volatile uint32_t rx_done;
static volatile uint32_t fail_code;
static volatile uint32_t tries;

tpa_op_t msg_edge_sf_tx_start(void);
tpa_op_t msg_edge_sf_rx_start(void);
tpa_op_t msg_edge_rf_tx_start(void);
tpa_op_t msg_edge_rf_rx_start(void);
tpa_op_t msg_edge_tx_done(void);
tpa_op_t msg_edge_rx_done(void);
tpa_op_t msg_edge_chk(void);

static void flush_u32(volatile uint32_t *p)
{
    TPA_FLUSH_BYTES(p, sizeof(*p));
    arch_fence_rw();
}

static void fail(uint32_t code)
{
    fail_code = code;
    flush_u32(&fail_code);
    TEST_FAIL;
}

static tpa_op_t send_start(tpa_cont_t next)
{
    struct tpa_chan *ch = tpa_chan(0);

    if (!ch) {
        fail(0x1000u);
        return tpa_stop();
    }

    return tpa_send(ch, (void *)&tx_buf, sizeof(tx_buf), next);
}

static tpa_op_t recv_start(tpa_cont_t next)
{
    struct tpa_chan *ch = tpa_chan(0);

    if (!ch) {
        fail(0x2000u);
        return tpa_stop();
    }

    return tpa_recv(ch, (void **)&rx_buf, &rx_len, next);
}

tpa_op_t msg_edge_sf_tx_start(void)
{
    return send_start(msg_edge_tx_done);
}

tpa_op_t msg_edge_sf_rx_start(void)
{
    struct tpa_chan *ch = tpa_chan(0);

    if (!ch) {
        fail(0x3000u);
        return tpa_stop();
    }

    TPA_EVICT_OBJ(ch);
    if (ch->state != TPA_CH_WAIT_TX)
        return tpa_yield(msg_edge_sf_rx_start);

    return recv_start(msg_edge_rx_done);
}

tpa_op_t msg_edge_rf_tx_start(void)
{
    struct tpa_chan *ch = tpa_chan(0);

    if (!ch) {
        fail(0x4000u);
        return tpa_stop();
    }

    TPA_EVICT_OBJ(ch);
    if (ch->state != TPA_CH_WAIT_RX)
        return tpa_yield(msg_edge_rf_tx_start);

    return send_start(msg_edge_tx_done);
}

tpa_op_t msg_edge_rf_rx_start(void)
{
    return recv_start(msg_edge_rx_done);
}

tpa_op_t msg_edge_tx_done(void)
{
    tx_done = 1;
    flush_u32(&tx_done);
    return tpa_stop();
}

tpa_op_t msg_edge_rx_done(void)
{
    if (!rx_buf || rx_len != sizeof(*rx_buf)) {
        fail(0x5000u);
        return tpa_stop();
    }

    if ((void *)rx_buf != (void *)tpa_msg_edge_storage_ch0_buf) {
        fail(0x5001u);
        return tpa_stop();
    }

    if ((void *)rx_buf == (void *)&tx_buf) {
        fail(0x5002u);
        return tpa_stop();
    }

    TPA_EVICT_BYTES(rx_buf, rx_len);
    rx_val = *rx_buf;
    TPA_FLUSH_BYTES(&rx_val, sizeof(rx_val));
    arch_fence_rw();

    rx_done = 1;
    flush_u32(&rx_done);
    return tpa_stop();
}

tpa_op_t msg_edge_chk(void)
{
    uint64_t val;
    uint32_t tx_ok;
    uint32_t rx_ok;
    uint32_t failed;

    TPA_EVICT_BYTES(&tx_done, sizeof(tx_done));
    TPA_EVICT_BYTES(&rx_done, sizeof(rx_done));
    TPA_EVICT_BYTES(&rx_val, sizeof(rx_val));
    TPA_EVICT_BYTES(&fail_code, sizeof(fail_code));

    tx_ok = tx_done;
    rx_ok = rx_done;
    val = rx_val;
    failed = fail_code;

    if (failed) {
        TEST_FAIL;
        return tpa_stop();
    }

    if (tx_ok && rx_ok && val == MSG_VAL) {
        TEST_PASS;
        return tpa_stop();
    }

    if (tries++ > CHK_LIMIT) {
        fail(0x6000u);
        return tpa_stop();
    }

    flush_u32(&tries);
    return tpa_yield(msg_edge_chk);
}
