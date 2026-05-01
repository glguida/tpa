#include "test.h"
#include "tpa/tpa.h"

#define RACE_ITERS      4096u
#define CHK_LIMIT       200000u
#define SRC_PING        0x70696e67u
#define SRC_PONG        0x706f6e67u

struct race_msg {
    uint32_t seq;
    uint32_t inv;
    uint32_t src;
    uint32_t mix;
};

struct race_ws {
    uint32_t seq;
    uint32_t rx_len;
    void *rx;
    struct race_msg tx;
};

static volatile uint32_t ping_done;
static volatile uint32_t pong_done;
static volatile uint32_t chk_tries;
static volatile uint32_t fail_code;

tpa_op_t msg_race_ping_start(void);
tpa_op_t msg_race_ping_recv(void);
tpa_op_t msg_race_ping_check(void);
tpa_op_t msg_race_pong_start(void);
tpa_op_t msg_race_pong_reply(void);
tpa_op_t msg_race_pong_next(void);
tpa_op_t msg_race_check(void);

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

static void fill_msg(struct race_msg *msg, uint32_t seq, uint32_t src)
{
    msg->seq = seq;
    msg->inv = ~seq;
    msg->src = src;
    msg->mix = seq ^ src ^ 0xa55a33ccu;
}

static int check_msg(const void *buf, uint32_t len, uint32_t seq, uint32_t src)
{
    const struct race_msg *msg = (const struct race_msg *)buf;

    if (!msg || len != sizeof(*msg))
        return 0;

    TPA_EVICT_BYTES(msg, sizeof(*msg));

    return msg->seq == seq &&
           msg->inv == ~seq &&
           msg->src == src &&
           msg->mix == (seq ^ src ^ 0xa55a33ccu);
}

static tpa_op_t send_msg(uint16_t port, struct race_msg *msg, tpa_cont_t next,
                         uint32_t fail_id)
{
    struct tpa_chan *ch = tpa_chan(port);

    if (!ch) {
        fail(fail_id);
        return tpa_stop();
    }

    return tpa_send(ch, msg, sizeof(*msg), next);
}

static tpa_op_t recv_msg(uint16_t port, struct race_ws *w, tpa_cont_t next,
                         uint32_t fail_id)
{
    struct tpa_chan *ch = tpa_chan(port);

    if (!ch) {
        fail(fail_id);
        return tpa_stop();
    }

    w->rx = 0;
    w->rx_len = 0;
    return tpa_recv(ch, &w->rx, &w->rx_len, next);
}

tpa_op_t msg_race_ping_start(void)
{
    struct race_ws *w = (struct race_ws *)tpa_ws();

    w->seq = 0;
    fill_msg(&w->tx, w->seq, SRC_PING);
    return send_msg(0, &w->tx, msg_race_ping_recv, 0x1001u);
}

tpa_op_t msg_race_ping_recv(void)
{
    struct race_ws *w = (struct race_ws *)tpa_ws();

    return recv_msg(1, w, msg_race_ping_check, 0x1002u);
}

tpa_op_t msg_race_ping_check(void)
{
    struct race_ws *w = (struct race_ws *)tpa_ws();

    if (!check_msg(w->rx, w->rx_len, w->seq, SRC_PONG)) {
        fail(0x1003u);
        return tpa_stop();
    }

    w->seq++;
    if (w->seq >= RACE_ITERS) {
        ping_done = 1;
        flush_u32(&ping_done);
        return tpa_stop();
    }

    fill_msg(&w->tx, w->seq, SRC_PING);
    return send_msg(0, &w->tx, msg_race_ping_recv, 0x1004u);
}

tpa_op_t msg_race_pong_start(void)
{
    struct race_ws *w = (struct race_ws *)tpa_ws();

    w->seq = 0;
    return recv_msg(0, w, msg_race_pong_reply, 0x2001u);
}

tpa_op_t msg_race_pong_reply(void)
{
    struct race_ws *w = (struct race_ws *)tpa_ws();

    if (!check_msg(w->rx, w->rx_len, w->seq, SRC_PING)) {
        fail(0x2002u);
        return tpa_stop();
    }

    fill_msg(&w->tx, w->seq, SRC_PONG);
    return send_msg(1, &w->tx, msg_race_pong_next, 0x2003u);
}

tpa_op_t msg_race_pong_next(void)
{
    struct race_ws *w = (struct race_ws *)tpa_ws();

    w->seq++;
    if (w->seq >= RACE_ITERS) {
        pong_done = 1;
        flush_u32(&pong_done);
        return tpa_stop();
    }

    return recv_msg(0, w, msg_race_pong_reply, 0x2004u);
}

tpa_op_t msg_race_check(void)
{
    uint32_t ping_ok;
    uint32_t pong_ok;
    uint32_t failed;

    TPA_EVICT_BYTES(&ping_done, sizeof(ping_done));
    TPA_EVICT_BYTES(&pong_done, sizeof(pong_done));
    TPA_EVICT_BYTES(&fail_code, sizeof(fail_code));

    failed = fail_code;
    if (failed) {
        TEST_FAIL;
        return tpa_stop();
    }

    ping_ok = ping_done;
    pong_ok = pong_done;
    if (ping_ok && pong_ok) {
        TEST_PASS;
        return tpa_stop();
    }

    if (chk_tries++ > CHK_LIMIT) {
        fail(0x3001u);
        return tpa_stop();
    }

    flush_u32(&chk_tries);
    return tpa_yield(msg_race_check);
}
