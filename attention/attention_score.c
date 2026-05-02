#include "test.h"
#include "tpa/tpa.h"

#include "attention_common.h"

struct attention_score_ws {
    struct attention_score_packet out __attribute__((aligned(64)));
    struct attention_head_input *in;
    uint32_t in_len;
};

TPA_STATIC_ASSERT(sizeof(struct attention_score_ws) <= ATTENTION_SCORE_WS_BYTES,
                  "attention score manifest workspace too small");
TPA_PROC_MEM_META(attention_score_meta, 1202u, 0u);

tpa_op_t attention_score_recv(void);
tpa_op_t attention_score_send(void);
tpa_op_t attention_score_done(void);

tpa_op_t attention_score_recv(void)
{
    struct attention_score_ws *w = tpa_ws();
    struct tpa_channel *ch = tpa_chan(0);

    if (!w || !ch) {
        attention_fail();
        return tpa_stop();
    }

    return tpa_recv(ch, (void **)&w->in, &w->in_len, attention_score_send);
}

tpa_op_t attention_score_send(void)
{
    struct attention_score_ws *w = tpa_ws();
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

    attention_trace_head(ATTENTION_TRACE_SCORE_BEGIN, head);
    attention_compute_scores(w->in, &w->out);
    attention_trace_head(ATTENTION_TRACE_SCORE_END, head);

    return tpa_send(ch, &w->out, sizeof(w->out), attention_score_done);
}

tpa_op_t attention_score_done(void)
{
    return tpa_stop();
}
