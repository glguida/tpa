#include "tpa/tpa.h"

#define DEMO_INPUT      7ull
#define DEMO_EXPECT     31ull
#define DEMO_BEGIN      0xd3110000ull
#define DEMO_VALUE      0xd3110001ull
#define DEMO_PASS       0xd31100ffull
#define DEMO_FAIL       0xd31100eeull
#define DEMO_CHK_LIMIT  512u

struct demo_stage_ws {
    uint64_t *in;
    uint32_t in_len;
    uint64_t out;
};

static uint64_t *sink_buf;
static uint32_t sink_len;
static volatile uint64_t sink_val;
static volatile uint32_t src_done;
static volatile uint32_t sink_done;
static volatile uint32_t tries;

static void mark(uint64_t v)
{
    arch_trace((uint32_t)v);
}

static void flush_word(const volatile void *p)
{
    et_cache_flush_line((uint64_t)p);
    asm volatile("fence rw, rw");
}

tpa_op_t demo_src_start(void);
tpa_op_t demo_src_done(void);
tpa_op_t demo_stage_recv(void);
tpa_op_t demo_stage_send(void);
tpa_op_t demo_stage_done(void);
tpa_op_t demo_sink_start(void);
tpa_op_t demo_sink_done(void);
tpa_op_t demo_chk(void);

tpa_op_t demo_src_start(void)
{
    static uint64_t input = DEMO_INPUT;
    struct tpa_channel *ch = tpa_chan(0);

    mark(DEMO_BEGIN);

    if (!ch) {
        mark(DEMO_FAIL);
        return tpa_stop();
    }

    return tpa_send(ch, &input, sizeof(input), demo_src_done);
}

tpa_op_t demo_src_done(void)
{
    src_done = 1;
    flush_word(&src_done);
    return tpa_stop();
}

tpa_op_t demo_stage_recv(void)
{
    struct demo_stage_ws *w = tpa_ws();
    struct tpa_channel *ch = tpa_chan(0);

    if (!w || !ch) {
        mark(DEMO_FAIL);
        return tpa_stop();
    }

    return tpa_recv(ch, (void **)&w->in, &w->in_len, demo_stage_send);
}

tpa_op_t demo_stage_send(void)
{
    struct demo_stage_ws *w = tpa_ws();
    struct tpa_channel *ch = tpa_chan(1);

    if (!w || !ch) {
        mark(DEMO_FAIL);
        return tpa_stop();
    }

    if (!w->in || w->in_len != sizeof(*w->in)) {
        mark(DEMO_FAIL);
        return tpa_stop();
    }

    w->out = *w->in * 2ull + 1ull;
    return tpa_send(ch, &w->out, sizeof(w->out), demo_stage_done);
}

tpa_op_t demo_stage_done(void)
{
    return tpa_stop();
}

tpa_op_t demo_sink_start(void)
{
    struct tpa_channel *ch = tpa_chan(0);

    if (!ch) {
        mark(DEMO_FAIL);
        return tpa_stop();
    }

    return tpa_recv(ch, (void **)&sink_buf, &sink_len, demo_sink_done);
}

tpa_op_t demo_sink_done(void)
{
    if (!sink_buf || sink_len != sizeof(*sink_buf)) {
        mark(DEMO_FAIL);
        return tpa_stop();
    }

    sink_val = *sink_buf;
    flush_word(&sink_val);
    sink_done = 1;
    flush_word(&sink_done);
    mark(DEMO_VALUE);
    mark(sink_val);
    return tpa_stop();
}

tpa_op_t demo_chk(void)
{
    uint64_t got;
    uint32_t done;

    et_cache_evict_line((uint64_t)&sink_done);
    et_cache_evict_line((uint64_t)&sink_val);

    done = sink_done;
    got = sink_val;

    if (done && got == DEMO_EXPECT) {
        mark(DEMO_PASS);
        return tpa_stop();
    }

    if (tries++ > DEMO_CHK_LIMIT) {
        mark(DEMO_FAIL);
        return tpa_stop();
    }

    flush_word(&tries);
    return tpa_yield(demo_chk);
}
