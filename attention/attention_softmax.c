#include "test.h"
#include "tpa/tpa.h"

#include "attention_common.h"

struct attention_softmax_ws {
    struct attention_softmax_packet out __attribute__((aligned(64)));
    struct attention_score_packet *in;
    uint32_t in_len;
};

TPA_STATIC_ASSERT(sizeof(struct attention_softmax_ws) <=
                      ATTENTION_SOFTMAX_WS_BYTES,
                  "attention softmax manifest workspace too small");
TPA_PROC_MEM_META(attention_softmax_meta, 1203u, 0u);

tpa_op_t attention_softmax_recv(void);
tpa_op_t attention_softmax_send(void);
tpa_op_t attention_softmax_done(void);

tpa_op_t attention_softmax_recv(void)
{
    struct attention_softmax_ws *w = tpa_ws();
    struct tpa_channel *ch = tpa_chan(0);

    if (!w || !ch) {
        attention_fail();
        return tpa_stop();
    }

    return tpa_recv(ch, (void **)&w->in, &w->in_len, attention_softmax_send);
}

tpa_op_t attention_softmax_send(void)
{
    struct attention_softmax_ws *w = tpa_ws();
    struct tpa_channel *ch = tpa_chan(1);
    uint32_t head;

    if (!w || !ch || !w->in || w->in_len != sizeof(*w->in)) {
        attention_fail();
        return tpa_stop();
    }

    head = w->in->head;
    if (head >= ATTENTION_HEADS) {
        attention_fail();
        return tpa_stop();
    }

    attention_trace_head(ATTENTION_TRACE_SOFTMAX_BEGIN, head);
    attention_compute_softmax(w->in, &w->out);
    attention_trace_head(ATTENTION_TRACE_SOFTMAX_END, head);

    return tpa_send(ch, &w->out, sizeof(w->out), attention_softmax_done);
}

tpa_op_t attention_softmax_done(void)
{
    return tpa_stop();
}
