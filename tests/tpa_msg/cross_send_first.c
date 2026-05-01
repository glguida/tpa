#include "test.h"
#include "tpa/tpa.h"

#define MSG_VAL     0x1122334455667788ull
#define CHK_LIMIT   256

static uint64_t tx_buf = MSG_VAL;
static uint64_t *rx_buf;
static uint32_t rx_len;
static volatile uint64_t rx_val;
static volatile uint32_t tx_done;
static volatile uint32_t rx_done;
static volatile uint32_t tries;

tpa_op_t msg_cross_sf_tx_start(void);
tpa_op_t msg_cross_sf_tx_done(void);
tpa_op_t msg_cross_sf_rx_start(void);
tpa_op_t msg_cross_sf_rx_done(void);
tpa_op_t msg_cross_sf_chk(void);

tpa_op_t msg_cross_sf_tx_start(void)
{
    struct tpa_chan *ch = tpa_chan(0);

    if (!ch) {
        TEST_FAIL;
        return tpa_stop();
    }

    return tpa_send(ch, (void *)&tx_buf, sizeof(tx_buf),
                    msg_cross_sf_tx_done);
}

tpa_op_t msg_cross_sf_tx_done(void)
{
    tx_done = 1;
    et_cache_flush_line((uint64_t)&tx_done);
    asm volatile("fence rw, rw");
    return tpa_stop();
}

tpa_op_t msg_cross_sf_rx_start(void)
{
    struct tpa_chan *ch = tpa_chan(0);

    if (!ch) {
        TEST_FAIL;
        return tpa_stop();
    }

    et_cache_evict_line((uint64_t)ch);
    if (ch->state != TPA_CH_WAIT_TX)
        return tpa_yield(msg_cross_sf_rx_start);

    return tpa_recv(ch, (void **)&rx_buf, &rx_len, msg_cross_sf_rx_done);
}

tpa_op_t msg_cross_sf_rx_done(void)
{
    if (!rx_buf || rx_len != sizeof(*rx_buf)) {
        TEST_FAIL;
        return tpa_stop();
    }

    rx_val = *rx_buf;
    et_cache_flush_line((uint64_t)&rx_val);
    asm volatile("fence rw, rw");
    rx_done = 1;
    et_cache_flush_line((uint64_t)&rx_done);
    asm volatile("fence rw, rw");
    return tpa_stop();
}

tpa_op_t msg_cross_sf_chk(void)
{
    uint64_t val;
    uint32_t tx_ok;
    uint32_t rx_ok;

    et_cache_evict_line((uint64_t)&tx_done);
    et_cache_evict_line((uint64_t)&rx_done);
    et_cache_evict_line((uint64_t)&rx_val);

    tx_ok = tx_done;
    rx_ok = rx_done;
    val = rx_val;

    if (tx_ok && rx_ok && val == MSG_VAL) {
        TEST_PASS;
        return tpa_stop();
    }

    if (tries++ > CHK_LIMIT) {
        TEST_FAIL;
        return tpa_stop();
    }

    et_cache_flush_line((uint64_t)&tries);
    asm volatile("fence rw, rw");
    return tpa_yield(msg_cross_sf_chk);
}
