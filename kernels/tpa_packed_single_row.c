#include <stdint.h>

#include "test.h"
#include "tpa/tpa.h"

#define PSR_ROW_FLOATS 16u
#define PSR_PACKED_LANES 8u
#define PSR_ROW_BYTES (PSR_ROW_FLOATS * (uint32_t)sizeof(float))
#define PSR_PACKET_ALIGN 64u

#define PSR_TRACE_BEGIN         0x70510000u
#define PSR_TRACE_COMPUTE_BEGIN 0x70510001u
#define PSR_TRACE_COMPUTE_END   0x70510002u
#define PSR_TRACE_PASS          0x705100ffu
#define PSR_TRACE_FAIL          0x705100eeu

#define PSR_SCALE_VALUE 2.0f
#define PSR_BIAS_VALUE  1.0f

struct psr_row_packet {
    float lane[PSR_ROW_FLOATS];
} __attribute__((aligned(PSR_PACKET_ALIGN)));

struct psr_compute_ws {
    struct psr_row_packet *in;
    uint32_t in_len;
};

struct psr_check_ws {
    struct psr_row_packet *out;
    uint32_t out_len;
};

TPA_STATIC_ASSERT(PSR_ROW_BYTES == 64u,
                  "packed-single row packet must carry one 64-byte row");
TPA_STATIC_ASSERT(sizeof(struct psr_row_packet) == PSR_ROW_BYTES,
                  "packed-single row packet has unexpected padding");
TPA_STATIC_ASSERT(_Alignof(struct psr_row_packet) == PSR_PACKET_ALIGN,
                  "packed-single row packet must be 64-byte aligned");
TPA_STATIC_ASSERT(sizeof(struct psr_compute_ws) <= 16u,
                  "packed-single compute manifest workspace too small");
TPA_STATIC_ASSERT(sizeof(struct psr_check_ws) <= 16u,
                  "packed-single checker manifest workspace too small");

TPA_PROC_MEM_META(packed_single_row_source_meta, 701u, 0u);
TPA_PROC_MEM_META(packed_single_row_compute_meta, 702u, 0u);
TPA_PROC_MEM_META(packed_single_row_check_meta, 703u, 0u);

static const float psr_scale_vec[PSR_PACKED_LANES]
    __attribute__((aligned(PSR_PACKET_ALIGN))) = {
        PSR_SCALE_VALUE, PSR_SCALE_VALUE, PSR_SCALE_VALUE, PSR_SCALE_VALUE,
        PSR_SCALE_VALUE, PSR_SCALE_VALUE, PSR_SCALE_VALUE, PSR_SCALE_VALUE,
    };

static const float psr_bias_vec[PSR_PACKED_LANES]
    __attribute__((aligned(PSR_PACKET_ALIGN))) = {
        PSR_BIAS_VALUE, PSR_BIAS_VALUE, PSR_BIAS_VALUE, PSR_BIAS_VALUE,
        PSR_BIAS_VALUE, PSR_BIAS_VALUE, PSR_BIAS_VALUE, PSR_BIAS_VALUE,
    };

tpa_op_t packed_single_row_source_start(void);
tpa_op_t packed_single_row_source_done(void);
tpa_op_t packed_single_row_compute_recv(void);
tpa_op_t packed_single_row_compute_send(void);
tpa_op_t packed_single_row_compute_done(void);
tpa_op_t packed_single_row_check_recv(void);
tpa_op_t packed_single_row_check_done(void);

static void psr_mark(uint32_t tag)
{
    arch_trace(tag);
}

static void psr_fail(void)
{
    psr_mark(PSR_TRACE_FAIL);
    TEST_FAIL;
}

static int psr_is_aligned(const void *ptr)
{
    return (((uintptr_t)ptr) & (uintptr_t)(PSR_PACKET_ALIGN - 1u)) == 0u;
}

static float psr_input_value(uint32_t lane)
{
    return (float)((int32_t)lane - 8);
}

static float psr_expected_value(uint32_t lane)
{
    return psr_input_value(lane) * PSR_SCALE_VALUE + PSR_BIAS_VALUE;
}

static void psr_fill_input(struct psr_row_packet *pkt)
{
    for (uint32_t lane = 0; lane < PSR_ROW_FLOATS; lane++)
        pkt->lane[lane] = psr_input_value(lane);
}

static void psr_scale_bias_row_ps(float dst[PSR_ROW_FLOATS],
                                  const float src[PSR_ROW_FLOATS])
{
    uint32_t mask = 0xffu;

    asm volatile(
        "mov.m.x m0, %[mask], 0\n"
        "flw.ps f0, 0(%[src])\n"
        "flw.ps f1, 32(%[src])\n"
        "flw.ps f2, 0(%[scale])\n"
        "flw.ps f3, 0(%[bias])\n"
        "fmul.ps f0, f0, f2\n"
        "fmul.ps f1, f1, f2\n"
        "fadd.ps f0, f0, f3\n"
        "fadd.ps f1, f1, f3\n"
        "fsw.ps f0, 0(%[dst])\n"
        "fsw.ps f1, 32(%[dst])\n"
        :
        : [mask] "r"(mask), [dst] "r"(dst), [src] "r"(src),
          [scale] "r"(psr_scale_vec), [bias] "r"(psr_bias_vec)
        : "f0", "f1", "f2", "f3", "memory");
}

tpa_op_t packed_single_row_source_start(void)
{
    struct tpa_channel *ch = tpa_chan(0);
    struct psr_row_packet *pkt;

    psr_mark(PSR_TRACE_BEGIN);

    if (!ch) {
        psr_fail();
        return tpa_stop();
    }

    pkt = (struct psr_row_packet *)tpa_send_buf(ch);
    if (!pkt || !psr_is_aligned(pkt)) {
        psr_fail();
        return tpa_stop();
    }

    psr_fill_input(pkt);
    return tpa_send(ch, pkt, sizeof(*pkt), packed_single_row_source_done);
}

tpa_op_t packed_single_row_source_done(void)
{
    return tpa_stop();
}

tpa_op_t packed_single_row_compute_recv(void)
{
    struct psr_compute_ws *w = tpa_ws();
    struct tpa_channel *ch = tpa_chan(0);

    if (!w || !ch) {
        psr_fail();
        return tpa_stop();
    }

    return tpa_recv(ch, (void **)&w->in, &w->in_len,
                    packed_single_row_compute_send);
}

tpa_op_t packed_single_row_compute_send(void)
{
    struct psr_compute_ws *w = tpa_ws();
    struct tpa_channel *ch = tpa_chan(1);
    struct psr_row_packet *out;

    if (!w || !ch || !w->in || w->in_len != sizeof(*w->in) ||
        !psr_is_aligned(w->in)) {
        psr_fail();
        return tpa_stop();
    }

    out = (struct psr_row_packet *)tpa_send_buf(ch);
    if (!out || !psr_is_aligned(out)) {
        psr_fail();
        return tpa_stop();
    }

    psr_mark(PSR_TRACE_COMPUTE_BEGIN);
    psr_scale_bias_row_ps(out->lane, w->in->lane);
    psr_mark(PSR_TRACE_COMPUTE_END);

    return tpa_send(ch, out, sizeof(*out), packed_single_row_compute_done);
}

tpa_op_t packed_single_row_compute_done(void)
{
    return tpa_stop();
}

tpa_op_t packed_single_row_check_recv(void)
{
    struct psr_check_ws *w = tpa_ws();
    struct tpa_channel *ch = tpa_chan(0);

    if (!w || !ch) {
        psr_fail();
        return tpa_stop();
    }

    return tpa_recv(ch, (void **)&w->out, &w->out_len,
                    packed_single_row_check_done);
}

tpa_op_t packed_single_row_check_done(void)
{
    struct psr_check_ws *w = tpa_ws();

    if (!w || !w->out || w->out_len != sizeof(*w->out) ||
        !psr_is_aligned(w->out)) {
        psr_fail();
        return tpa_stop();
    }

    for (uint32_t lane = 0; lane < PSR_ROW_FLOATS; lane++) {
        if (w->out->lane[lane] != psr_expected_value(lane)) {
            psr_fail();
            return tpa_stop();
        }
    }

    psr_mark(PSR_TRACE_PASS);
    TEST_PASS;
    return tpa_stop();
}
