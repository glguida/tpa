#include "test.h"
#include "tpa/tpa.h"

#include "attention_common.h"
#include "attention_et.h"

struct attention_output_ws {
    struct attention_softmax_packet *pkt[ATTENTION_HEADS];
    float output[ATTENTION_HEADS][ATTENTION_SEQ_LEN][ATTENTION_HEAD_DIM]
        __attribute__((aligned(64)));
    uint32_t len[ATTENTION_HEADS];
    uint32_t next_port;
    uint32_t received_mask;
};

TPA_STATIC_ASSERT(sizeof(struct attention_output_ws) <= ATTENTION_OUTPUT_WS_BYTES,
                  "attention output manifest workspace too small");
TPA_PROC_MEM_META(attention_output_meta, 1204u,
                  ATTENTION_TENSOR_SCRATCH_BYTES);

tpa_op_t attention_output_start(void);
tpa_op_t attention_output_recv_next(void);
tpa_op_t attention_output_after_recv(void);
tpa_op_t attention_output_check(void);

static int attention_compute_output_packet(
    const struct attention_softmax_packet *pkt,
    float output[ATTENTION_SEQ_LEN][ATTENTION_HEAD_DIM], uint32_t port)
{
    if (!pkt || pkt->head != port || pkt->head >= ATTENTION_HEADS)
        return 0;

    return attention_et_matmul_16x16(output, pkt->weight, pkt->v,
                                     ATTENTION_ET_TENSOR_LOAD_NORMAL) == 0;
}

static int attention_validate_output(const struct attention_softmax_packet *pkt,
                                     const float output[ATTENTION_SEQ_LEN]
                                                       [ATTENTION_HEAD_DIM],
                                     uint32_t port)
{
    if (!pkt || pkt->head != port || pkt->head >= ATTENTION_HEADS)
        return 0;

    for (uint32_t row = 0; row < ATTENTION_SEQ_LEN; row++) {
        for (uint32_t dim = 0; dim < ATTENTION_HEAD_DIM; dim++) {
            float got = output[row][dim];
            float expect = attention_reference_output_value(pkt->head, row,
                                                            dim);
            float diff = attention_absf(got - expect);

            if (!(diff <= ATTENTION_TOLERANCE))
                return 0;
        }
    }

    return 1;
}

tpa_op_t attention_output_start(void)
{
    struct attention_output_ws *w = tpa_ws();

    attention_trace(ATTENTION_TRACE_OUTPUT_BEGIN);

    if (!w) {
        attention_fail();
        return tpa_stop();
    }

    for (uint32_t head = 0; head < ATTENTION_HEADS; head++) {
        w->pkt[head] = 0;
        w->len[head] = 0u;
    }
    w->next_port = 0u;
    w->received_mask = 0u;

    return attention_output_recv_next();
}

tpa_op_t attention_output_recv_next(void)
{
    struct attention_output_ws *w = tpa_ws();
    struct tpa_channel *ch;

    if (!w || w->next_port >= ATTENTION_HEADS) {
        attention_fail();
        return tpa_stop();
    }

    ch = tpa_chan((uint16_t)w->next_port);
    if (!ch) {
        attention_fail();
        return tpa_stop();
    }

    return tpa_recv(ch, (void **)&w->pkt[w->next_port], &w->len[w->next_port],
                    attention_output_after_recv);
}

tpa_op_t attention_output_after_recv(void)
{
    struct attention_output_ws *w = tpa_ws();
    uint32_t port;

    if (!w || w->next_port >= ATTENTION_HEADS) {
        attention_fail();
        return tpa_stop();
    }

    port = w->next_port;
    if (!w->pkt[port] || w->len[port] != sizeof(*w->pkt[port]) ||
        w->pkt[port]->head != port) {
        attention_fail();
        return tpa_stop();
    }

    w->received_mask |= 1u << port;
    w->next_port++;
    if (w->next_port < ATTENTION_HEADS)
        return attention_output_recv_next();

    attention_trace(ATTENTION_TRACE_OUTPUT_ALL_RECEIVED);
    return attention_output_check();
}

tpa_op_t attention_output_check(void)
{
    struct attention_output_ws *w = tpa_ws();
    uint32_t all_heads = (1u << ATTENTION_HEADS) - 1u;

    if (!w || w->received_mask != all_heads) {
        attention_fail();
        return tpa_stop();
    }

    for (uint32_t head = 0; head < ATTENTION_HEADS; head++) {
        if (w->len[head] != sizeof(*w->pkt[head])) {
            attention_fail();
            return tpa_stop();
        }

        attention_trace_head(ATTENTION_TRACE_OUTPUT_PRODUCT_BEGIN, head);
        if (!attention_compute_output_packet(w->pkt[head], w->output[head],
                                             head)) {
            attention_fail();
            return tpa_stop();
        }
        attention_trace_head(ATTENTION_TRACE_OUTPUT_PRODUCT_END, head);
    }

    for (uint32_t head = 0; head < ATTENTION_HEADS; head++) {
        attention_trace_head(ATTENTION_TRACE_OUTPUT_VALIDATE_BEGIN, head);
        if (!attention_validate_output(w->pkt[head], w->output[head], head)) {
            attention_fail();
            return tpa_stop();
        }
        attention_trace_head(ATTENTION_TRACE_OUTPUT_VALIDATE_END, head);
    }

    attention_trace(ATTENTION_TRACE_OUTPUT_END);
    attention_trace(ATTENTION_TRACE_PASS);
    TEST_PASS;
    return tpa_stop();
}
