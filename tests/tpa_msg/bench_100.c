#include "test.h"
#include "tpa/tpa.h"

#define MSG_COUNT   100u
#define CHK_LIMIT   1024u
#define MARK_BEGIN  0x7a110000ull
#define MARK_TX0    0x7a110010ull
#define MARK_RX0    0x7a110011ull
#define MARK_END    0x7a110001ull

#define BENCH_MARK(tag) do { \
    arch_trace((uint32_t)(tag)); \
} while (0)

static uint64_t tx_buf[2];
static uint64_t *rx_buf;
static uint32_t rx_len;
static volatile uint32_t tx_idx;
static volatile uint32_t rx_idx;
static volatile uint32_t tx_done;
static volatile uint32_t rx_done;
static volatile uint32_t tries;

tpa_op_t msg_bench_tx_start(void);
tpa_op_t msg_bench_tx_next(void);
tpa_op_t msg_bench_rx_start(void);
tpa_op_t msg_bench_rx_next(void);
tpa_op_t msg_bench_chk(void);

static void flush_u32(volatile uint32_t *p)
{
    et_cache_flush_line((uint64_t)p);
    asm volatile("fence rw, rw");
}

static void flush_u64(volatile uint64_t *p)
{
    et_cache_flush_line((uint64_t)p);
    asm volatile("fence rw, rw");
}

tpa_op_t msg_bench_tx_start(void)
{
    struct tpa_chan *ch = tpa_chan(0);

    if (!ch) {
        TEST_FAIL;
        return tpa_stop();
    }

    tx_buf[0] = 1;
    tx_idx = 0;
    flush_u64(&tx_buf[0]);
    flush_u32(&tx_idx);
    BENCH_MARK(MARK_BEGIN);
    BENCH_MARK(MARK_TX0);
    return tpa_send(ch, (void *)&tx_buf[0], sizeof(tx_buf[0]),
                    msg_bench_tx_next);
}

tpa_op_t msg_bench_tx_next(void)
{
    struct tpa_chan *ch = tpa_chan(0);
    uint32_t idx;

    if (!ch) {
        TEST_FAIL;
        return tpa_stop();
    }

    et_cache_evict_line((uint64_t)&tx_idx);
    idx = tx_idx + 1;
    tx_idx = idx;
    flush_u32(&tx_idx);

    if (idx >= MSG_COUNT) {
        tx_done = 1;
        flush_u32(&tx_done);
        return tpa_stop();
    }

    tx_buf[idx & 1u] = (uint64_t)idx + 1ull;
    flush_u64(&tx_buf[idx & 1u]);
    return tpa_send(ch, (void *)&tx_buf[idx & 1u],
                    sizeof(tx_buf[idx & 1u]),
                    msg_bench_tx_next);
}

tpa_op_t msg_bench_rx_start(void)
{
    struct tpa_chan *ch = tpa_chan(0);

    if (!ch) {
        TEST_FAIL;
        return tpa_stop();
    }

    return tpa_recv(ch, (void **)&rx_buf, &rx_len, msg_bench_rx_next);
}

tpa_op_t msg_bench_rx_next(void)
{
    uint32_t idx;
    uint64_t expect;
    uint64_t got;

    et_cache_evict_line((uint64_t)&rx_idx);
    idx = rx_idx;
    if (!rx_buf || rx_len != sizeof(*rx_buf)) {
        TEST_FAIL;
        return tpa_stop();
    }

    got = *rx_buf;
    expect = (uint64_t)idx + 1ull;

    if (got != expect) {
        TEST_FAIL;
        return tpa_stop();
    }

    if (!idx)
        BENCH_MARK(MARK_RX0);

    idx++;
    rx_idx = idx;
    flush_u32(&rx_idx);

    if (idx >= MSG_COUNT) {
        rx_done = 1;
        flush_u32(&rx_done);
        BENCH_MARK(MARK_END);
        return tpa_stop();
    }

    return tpa_recv(tpa_chan(0), (void **)&rx_buf, &rx_len, msg_bench_rx_next);
}

tpa_op_t msg_bench_chk(void)
{
    uint32_t tx_ok;
    uint32_t rx_ok;

    et_cache_evict_line((uint64_t)&tx_done);
    et_cache_evict_line((uint64_t)&rx_done);
    et_cache_evict_line((uint64_t)&tx_idx);
    et_cache_evict_line((uint64_t)&rx_idx);

    tx_ok = tx_done;
    rx_ok = rx_done;

    if (tx_ok && rx_ok && tx_idx == MSG_COUNT && rx_idx == MSG_COUNT) {
        TEST_PASS;
        return tpa_stop();
    }

    if (tries++ > CHK_LIMIT) {
        TEST_FAIL;
        return tpa_stop();
    }

    flush_u32(&tries);
    return tpa_yield(msg_bench_chk);
}
