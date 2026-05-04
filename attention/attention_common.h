#ifndef ATTENTION_COMMON_H
#define ATTENTION_COMMON_H

#include <stddef.h>
#include <stdint.h>

#include "test.h"
#include "tpa/tpa.h"

#define ATTENTION_SEQ_LEN 16u
#define ATTENTION_EMBED_DIM 64u
#define ATTENTION_HEADS 4u
#define ATTENTION_HEAD_DIM 16u
#define ATTENTION_SCORE_SCALE 0.25f
#define ATTENTION_TOLERANCE 0.001f
#define ATTENTION_PACKET_HEADER_BYTES 64u
#define ATTENTION_PACKET_HEADER_PAD_BYTES \
    (ATTENTION_PACKET_HEADER_BYTES - sizeof(uint32_t))
#define ATTENTION_MATRIX_ROW_BYTES (ATTENTION_HEAD_DIM * sizeof(float))
#define ATTENTION_TENSOR_SCRATCH_BYTES (32u * TPA_CACHELINE_BYTES)

#define ATTENTION_HEAD_INPUT_BYTES 3136u
#define ATTENTION_SCORE_PACKET_BYTES 2112u
#define ATTENTION_SOFTMAX_PACKET_BYTES 2112u

#define ATTENTION_QKV_WS_BYTES 16384u
#define ATTENTION_SCORE_WS_BYTES 4096u
#define ATTENTION_SOFTMAX_WS_BYTES 4096u
#define ATTENTION_OUTPUT_WS_BYTES 8192u

#define ATTENTION_TRACE_PROGRAM_BEGIN 0xa7700000u
#define ATTENTION_TRACE_SCORE_BEGIN 0xa7710000u
#define ATTENTION_TRACE_SCORE_END 0xa7720000u
#define ATTENTION_TRACE_SOFTMAX_BEGIN 0xa7730000u
#define ATTENTION_TRACE_SOFTMAX_END 0xa7740000u
#define ATTENTION_TRACE_OUTPUT_BEGIN 0xa7750000u
#define ATTENTION_TRACE_OUTPUT_END 0xa7750001u
#define ATTENTION_TRACE_OUTPUT_ALL_RECEIVED 0xa7750002u
#define ATTENTION_TRACE_OUTPUT_PRODUCT_BEGIN 0xa7760000u
#define ATTENTION_TRACE_OUTPUT_PRODUCT_END 0xa7770000u
#define ATTENTION_TRACE_OUTPUT_VALIDATE_BEGIN 0xa7780000u
#define ATTENTION_TRACE_OUTPUT_VALIDATE_END 0xa7790000u
#define ATTENTION_TRACE_PASS 0xa77f00ffu
#define ATTENTION_TRACE_FAIL 0xa77f00eeu

struct attention_head_input {
    uint32_t head;
    uint8_t header_pad[ATTENTION_PACKET_HEADER_PAD_BYTES];
    float q[ATTENTION_SEQ_LEN][ATTENTION_HEAD_DIM];
    float k[ATTENTION_SEQ_LEN][ATTENTION_HEAD_DIM];
    float v[ATTENTION_SEQ_LEN][ATTENTION_HEAD_DIM];
} __attribute__((aligned(64)));

struct attention_score_packet {
    uint32_t head;
    uint8_t header_pad[ATTENTION_PACKET_HEADER_PAD_BYTES];
    float score[ATTENTION_SEQ_LEN][ATTENTION_SEQ_LEN];
    float v[ATTENTION_SEQ_LEN][ATTENTION_HEAD_DIM];
} __attribute__((aligned(64)));

struct attention_softmax_packet {
    uint32_t head;
    uint8_t header_pad[ATTENTION_PACKET_HEADER_PAD_BYTES];
    float weight[ATTENTION_SEQ_LEN][ATTENTION_SEQ_LEN];
    float v[ATTENTION_SEQ_LEN][ATTENTION_HEAD_DIM];
} __attribute__((aligned(64)));

TPA_STATIC_ASSERT(ATTENTION_HEADS * ATTENTION_HEAD_DIM == ATTENTION_EMBED_DIM,
                  "attention dimensions must concatenate to embedding dim");
TPA_STATIC_ASSERT(ATTENTION_MATRIX_ROW_BYTES == TPA_CACHELINE_BYTES,
                  "attention matrix rows must be one cacheline");
TPA_STATIC_ASSERT(offsetof(struct attention_head_input, q) ==
                      ATTENTION_PACKET_HEADER_BYTES,
                  "attention q matrix must start after aligned header");
TPA_STATIC_ASSERT(offsetof(struct attention_head_input, k) %
                          TPA_CACHELINE_BYTES ==
                      0,
                  "attention k matrix must be cacheline aligned");
TPA_STATIC_ASSERT(offsetof(struct attention_head_input, v) %
                          TPA_CACHELINE_BYTES ==
                      0,
                  "attention v matrix must be cacheline aligned");
TPA_STATIC_ASSERT(offsetof(struct attention_score_packet, score) ==
                      ATTENTION_PACKET_HEADER_BYTES,
                  "attention score matrix must start after aligned header");
TPA_STATIC_ASSERT(offsetof(struct attention_score_packet, v) %
                          TPA_CACHELINE_BYTES ==
                      0,
                  "attention score packet v matrix must be cacheline aligned");
TPA_STATIC_ASSERT(offsetof(struct attention_softmax_packet, weight) ==
                      ATTENTION_PACKET_HEADER_BYTES,
                  "attention weight matrix must start after aligned header");
TPA_STATIC_ASSERT(offsetof(struct attention_softmax_packet, v) %
                          TPA_CACHELINE_BYTES ==
                      0,
                  "attention softmax packet v matrix must be cacheline aligned");
TPA_STATIC_ASSERT(sizeof(struct attention_head_input) ==
                      ATTENTION_HEAD_INPUT_BYTES,
                  "attention_head_input channel payload size drifted");
TPA_STATIC_ASSERT(sizeof(struct attention_score_packet) ==
                      ATTENTION_SCORE_PACKET_BYTES,
                  "attention_score_packet channel payload size drifted");
TPA_STATIC_ASSERT(sizeof(struct attention_softmax_packet) ==
                      ATTENTION_SOFTMAX_PACKET_BYTES,
                  "attention_softmax_packet channel payload size drifted");

static inline void attention_trace(uint32_t tag)
{
    arch_trace(tag);
}

static inline void attention_trace_head(uint32_t base, uint32_t head)
{
    attention_trace(base | (head & 0xffu));
}

static inline void attention_fail(void)
{
    attention_trace(ATTENTION_TRACE_FAIL);
    TEST_FAIL;
}

static inline float attention_absf(float x)
{
    return x < 0.0f ? -x : x;
}

static inline float attention_centered_value(uint32_t x)
{
    int32_t centered = (int32_t)(x % 17u) - 8;

    return (float)centered * 0.03125f;
}

static inline void attention_clear_header_padding(
    uint8_t pad[ATTENTION_PACKET_HEADER_PAD_BYTES])
{
    for (uint32_t i = 0; i < ATTENTION_PACKET_HEADER_PAD_BYTES; i++)
        pad[i] = 0u;
}

static inline float attention_q_value(uint32_t head, uint32_t row,
                                      uint32_t dim)
{
    return attention_centered_value((head + 1u) * 37u +
                                    (row + 1u) * 11u +
                                    (dim + 1u) * 3u);
}

static inline float attention_k_value(uint32_t head, uint32_t row,
                                      uint32_t dim)
{
    return attention_centered_value((head + 1u) * 41u +
                                    (row + 1u) * 5u +
                                    (dim + 1u) * 13u);
}

static inline float attention_v_value(uint32_t head, uint32_t row,
                                      uint32_t dim)
{
    return attention_centered_value((head + 1u) * 31u +
                                    (row + 1u) * 13u +
                                    (dim + 1u) * 7u) * 0.5f;
}

static inline void attention_fill_head_input(struct attention_head_input *pkt,
                                             uint32_t head)
{
    pkt->head = head;
    attention_clear_header_padding(pkt->header_pad);
    for (uint32_t row = 0; row < ATTENTION_SEQ_LEN; row++) {
        for (uint32_t dim = 0; dim < ATTENTION_HEAD_DIM; dim++) {
            pkt->q[row][dim] = attention_q_value(head, row, dim);
            pkt->k[row][dim] = attention_k_value(head, row, dim);
            pkt->v[row][dim] = attention_v_value(head, row, dim);
        }
    }
}

static inline float attention_dot_qk(const float q[ATTENTION_HEAD_DIM],
                                     const float k[ATTENTION_HEAD_DIM])
{
    float acc = 0.0f;

    for (uint32_t dim = 0; dim < ATTENTION_HEAD_DIM; dim++)
        acc += q[dim] * k[dim];

    return acc * ATTENTION_SCORE_SCALE;
}

static inline void attention_compute_scores(
    const struct attention_head_input *in,
    struct attention_score_packet *out)
{
    out->head = in->head;
    attention_clear_header_padding(out->header_pad);

    for (uint32_t row = 0; row < ATTENTION_SEQ_LEN; row++) {
        for (uint32_t dim = 0; dim < ATTENTION_HEAD_DIM; dim++)
            out->v[row][dim] = in->v[row][dim];
    }

    for (uint32_t row = 0; row < ATTENTION_SEQ_LEN; row++) {
        for (uint32_t col = 0; col < ATTENTION_SEQ_LEN; col++)
            out->score[row][col] = attention_dot_qk(in->q[row], in->k[col]);
    }
}

static inline float attention_exp_approx(float x)
{
    float term = x;
    float sum = 1.0f + term;

    if (x < -8.0f)
        return 0.0003354626f;

    term *= x * 0.5f;
    sum += term;
    term *= x * 0.3333333433f;
    sum += term;
    term *= x * 0.25f;
    sum += term;
    term *= x * 0.2f;
    sum += term;
    term *= x * 0.1666666716f;
    sum += term;
    term *= x * 0.1428571492f;
    sum += term;
    term *= x * 0.125f;
    sum += term;

    if (sum < 0.0001f)
        return 0.0001f;

    return sum;
}

static inline float attention_reciprocal_sum(float sum)
{
    float x = 0.0625f;

    x = x * (2.0f - sum * x);
    x = x * (2.0f - sum * x);
    x = x * (2.0f - sum * x);
    x = x * (2.0f - sum * x);
    return x;
}

static inline void attention_softmax_row(
    const float score[ATTENTION_SEQ_LEN],
    float weight[ATTENTION_SEQ_LEN])
{
    float max_score = score[0];
    float sum = 0.0f;

    for (uint32_t col = 1u; col < ATTENTION_SEQ_LEN; col++) {
        if (score[col] > max_score)
            max_score = score[col];
    }

    for (uint32_t col = 0; col < ATTENTION_SEQ_LEN; col++) {
        weight[col] = attention_exp_approx(score[col] - max_score);
        sum += weight[col];
    }

    {
        float inv_sum = attention_reciprocal_sum(sum);

        for (uint32_t col = 0; col < ATTENTION_SEQ_LEN; col++)
            weight[col] = weight[col] * inv_sum;
    }
}

static inline void attention_compute_softmax(
    const struct attention_score_packet *in,
    struct attention_softmax_packet *out)
{
    out->head = in->head;
    attention_clear_header_padding(out->header_pad);

    for (uint32_t row = 0; row < ATTENTION_SEQ_LEN; row++) {
        attention_softmax_row(in->score[row], out->weight[row]);
        for (uint32_t dim = 0; dim < ATTENTION_HEAD_DIM; dim++)
            out->v[row][dim] = in->v[row][dim];
    }
}

static inline float attention_packet_output_value(
    const struct attention_softmax_packet *pkt, uint32_t row, uint32_t dim)
{
    float acc = 0.0f;

    for (uint32_t col = 0; col < ATTENTION_SEQ_LEN; col++)
        acc += pkt->weight[row][col] * pkt->v[col][dim];

    return acc;
}

static inline float attention_reference_score(uint32_t head, uint32_t row,
                                              uint32_t col)
{
    float acc = 0.0f;

    for (uint32_t dim = 0; dim < ATTENTION_HEAD_DIM; dim++)
        acc += attention_q_value(head, row, dim) *
               attention_k_value(head, col, dim);

    return acc * ATTENTION_SCORE_SCALE;
}

static inline float attention_reference_output_value(uint32_t head,
                                                     uint32_t row,
                                                     uint32_t dim)
{
    float score[ATTENTION_SEQ_LEN];
    float weight[ATTENTION_SEQ_LEN];
    float acc = 0.0f;

    for (uint32_t col = 0; col < ATTENTION_SEQ_LEN; col++)
        score[col] = attention_reference_score(head, row, col);

    attention_softmax_row(score, weight);

    for (uint32_t col = 0; col < ATTENTION_SEQ_LEN; col++)
        acc += weight[col] * attention_v_value(head, col, dim);

    return acc;
}

#endif /* ATTENTION_COMMON_H */
