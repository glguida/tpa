#include "test.h"
#include "tpa/tpa.h"

#include "attention_common.h"

struct attention_qkv_gen_ws {
    struct attention_head_input head0 __attribute__((aligned(64)));
    struct attention_head_input head1 __attribute__((aligned(64)));
    struct attention_head_input head2 __attribute__((aligned(64)));
    struct attention_head_input head3 __attribute__((aligned(64)));
    uint32_t next_head;
};

TPA_STATIC_ASSERT(sizeof(struct attention_qkv_gen_ws) <= ATTENTION_QKV_WS_BYTES,
                  "attention qkv manifest workspace too small");
TPA_PROC_MEM_META(attention_qkv_gen_meta, 1201u, 0u);

static struct attention_head_input *attention_qkv_packet(
    struct attention_qkv_gen_ws *w, uint32_t head)
{
    switch (head) {
    case 0u:
        return &w->head0;
    case 1u:
        return &w->head1;
    case 2u:
        return &w->head2;
    case 3u:
        return &w->head3;
    default:
        return 0;
    }
}

tpa_op_t attention_qkv_gen_start(void);
tpa_op_t attention_qkv_gen_send(void);
tpa_op_t attention_qkv_gen_after_send(void);

tpa_op_t attention_qkv_gen_start(void)
{
    struct attention_qkv_gen_ws *w = tpa_ws();

    attention_trace(ATTENTION_TRACE_PROGRAM_BEGIN);

    if (!w) {
        attention_fail();
        return tpa_stop();
    }

    attention_fill_head_input(&w->head0, 0u);
    attention_fill_head_input(&w->head1, 1u);
    attention_fill_head_input(&w->head2, 2u);
    attention_fill_head_input(&w->head3, 3u);
    w->next_head = 0u;

    return attention_qkv_gen_send();
}

tpa_op_t attention_qkv_gen_send(void)
{
    struct attention_qkv_gen_ws *w = tpa_ws();
    struct attention_head_input *pkt;
    struct tpa_channel *ch;

    if (!w || w->next_head >= ATTENTION_HEADS) {
        attention_fail();
        return tpa_stop();
    }

    ch = tpa_chan((uint16_t)w->next_head);
    pkt = attention_qkv_packet(w, w->next_head);
    if (!ch || !pkt) {
        attention_fail();
        return tpa_stop();
    }

    return tpa_send(ch, pkt, sizeof(*pkt), attention_qkv_gen_after_send);
}

tpa_op_t attention_qkv_gen_after_send(void)
{
    struct attention_qkv_gen_ws *w = tpa_ws();

    if (!w) {
        attention_fail();
        return tpa_stop();
    }

    w->next_head++;
    if (w->next_head < ATTENTION_HEADS)
        return attention_qkv_gen_send();

    return tpa_stop();
}
