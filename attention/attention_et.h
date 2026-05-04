#ifndef ATTENTION_ET_H
#define ATTENTION_ET_H

#include <stdint.h>

#include <etsoc/isa/cacheops.h>
#include <etsoc/isa/tensors.h>

#include "attention_common.h"

#define ATTENTION_ET_TENSOR_A_START 0u
#define ATTENTION_ET_TENSOR_B_START 16u
#define ATTENTION_ET_TENSOR_ROWS (ATTENTION_SEQ_LEN - 1u)
#define ATTENTION_ET_TENSOR_COLS (ATTENTION_HEAD_DIM - 1u)
#define ATTENTION_ET_TENSOR_BCOLS ((ATTENTION_HEAD_DIM / 4u) - 1u)
#define ATTENTION_ET_TENSOR_FP32 0u
#define ATTENTION_ET_TENSOR_LOAD_NORMAL 0u
#define ATTENTION_ET_TENSOR_LOAD_TRANSPOSE32 7u

static inline void attention_et_clear_tensor_error(void)
{
    asm volatile("csrwi tensor_error, 0" : : : "memory");
}

static inline void attention_et_enable_tensor_scratchpad(void)
{
    static volatile uint32_t tensor_ready[ARCH_NR_HARTS]
        __attribute__((aligned(64)));
    uint32_t hart = arch_runtime_hartid();
    uint64_t pmask = 0xffu;

    if (hart < ARCH_NR_HARTS && tensor_ready[hart])
        return;

    excl_mode(1);
    et_cache_evict_l1d_to_l2();
    asm volatile("fence rw, rw" : : : "memory");
    mcache_control(1, 0, 0, 0);
    mcache_control(1, 1, 0, 0);
    asm volatile("csrwi tensor_mask, 0\n"
                 "csrwi tensor_coop, 0\n"
                 "mova.m.x %0\n"
                 :
                 : "r"(pmask)
                 : "memory");
    attention_et_clear_tensor_error();
    excl_mode(0);

    if (hart < ARCH_NR_HARTS)
        tensor_ready[hart] = 1u;
}

static inline void attention_et_store_rf_16x16(float dst[ATTENTION_SEQ_LEN]
                                                [ATTENTION_HEAD_DIM])
{
    char *p = (char *)dst;

    asm volatile(
        "fsw.ps f0, 0(%[p])\n"
        "fsw.ps f1, 32(%[p])\n"
        "add %[p], %[p], %[s]\n"
        "fsw.ps f2, 0(%[p])\n"
        "fsw.ps f3, 32(%[p])\n"
        "add %[p], %[p], %[s]\n"
        "fsw.ps f4, 0(%[p])\n"
        "fsw.ps f5, 32(%[p])\n"
        "add %[p], %[p], %[s]\n"
        "fsw.ps f6, 0(%[p])\n"
        "fsw.ps f7, 32(%[p])\n"
        "add %[p], %[p], %[s]\n"
        "fsw.ps f8, 0(%[p])\n"
        "fsw.ps f9, 32(%[p])\n"
        "add %[p], %[p], %[s]\n"
        "fsw.ps f10, 0(%[p])\n"
        "fsw.ps f11, 32(%[p])\n"
        "add %[p], %[p], %[s]\n"
        "fsw.ps f12, 0(%[p])\n"
        "fsw.ps f13, 32(%[p])\n"
        "add %[p], %[p], %[s]\n"
        "fsw.ps f14, 0(%[p])\n"
        "fsw.ps f15, 32(%[p])\n"
        "add %[p], %[p], %[s]\n"
        "fsw.ps f16, 0(%[p])\n"
        "fsw.ps f17, 32(%[p])\n"
        "add %[p], %[p], %[s]\n"
        "fsw.ps f18, 0(%[p])\n"
        "fsw.ps f19, 32(%[p])\n"
        "add %[p], %[p], %[s]\n"
        "fsw.ps f20, 0(%[p])\n"
        "fsw.ps f21, 32(%[p])\n"
        "add %[p], %[p], %[s]\n"
        "fsw.ps f22, 0(%[p])\n"
        "fsw.ps f23, 32(%[p])\n"
        "add %[p], %[p], %[s]\n"
        "fsw.ps f24, 0(%[p])\n"
        "fsw.ps f25, 32(%[p])\n"
        "add %[p], %[p], %[s]\n"
        "fsw.ps f26, 0(%[p])\n"
        "fsw.ps f27, 32(%[p])\n"
        "add %[p], %[p], %[s]\n"
        "fsw.ps f28, 0(%[p])\n"
        "fsw.ps f29, 32(%[p])\n"
        "add %[p], %[p], %[s]\n"
        "fsw.ps f30, 0(%[p])\n"
        "fsw.ps f31, 32(%[p])\n"
        : [p] "+&r"(p)
        : [s] "r"((uint64_t)ATTENTION_MATRIX_ROW_BYTES)
        : "memory", "f0", "f1", "f2", "f3", "f4", "f5", "f6", "f7",
          "f8", "f9", "f10", "f11", "f12", "f13", "f14", "f15",
          "f16", "f17", "f18", "f19", "f20", "f21", "f22", "f23",
          "f24", "f25", "f26", "f27", "f28", "f29", "f30", "f31");
}

static inline int attention_et_matmul_16x16(
    float dst[ATTENTION_SEQ_LEN][ATTENTION_HEAD_DIM],
    const float lhs[ATTENTION_SEQ_LEN][ATTENTION_HEAD_DIM],
    const float rhs[ATTENTION_SEQ_LEN][ATTENTION_HEAD_DIM],
    uint64_t rhs_load_transform)
{
    attention_et_enable_tensor_scratchpad();
    attention_et_clear_tensor_error();

    tensor_load(0, 0, ATTENTION_ET_TENSOR_A_START,
                ATTENTION_ET_TENSOR_LOAD_NORMAL, 0, (uint64_t)lhs, 0,
                ATTENTION_ET_TENSOR_ROWS, ATTENTION_MATRIX_ROW_BYTES, 0);
    tensor_load(0, 0, ATTENTION_ET_TENSOR_B_START, rhs_load_transform, 0,
                (uint64_t)rhs, 0, ATTENTION_ET_TENSOR_ROWS,
                ATTENTION_MATRIX_ROW_BYTES, 1);
    tensor_wait(TENSOR_LOAD_WAIT_0);
    tensor_wait(TENSOR_LOAD_WAIT_1);
    tensor_fma(0, ATTENTION_ET_TENSOR_BCOLS, ATTENTION_ET_TENSOR_ROWS,
               ATTENTION_ET_TENSOR_COLS, 0, 0, 0, 0, 0,
               ATTENTION_ET_TENSOR_B_START, ATTENTION_ET_TENSOR_A_START,
               ATTENTION_ET_TENSOR_FP32, 1);
    tensor_wait(TENSOR_FMA_WAIT);

    if (get_tensor_error() != 0)
        return -1;

    attention_et_store_rf_16x16(dst);
    return 0;
}

static inline void attention_et_copy_row_ps(
    float dst[ATTENTION_HEAD_DIM], const float src[ATTENTION_HEAD_DIM])
{
    uint32_t mask = 0xffu;

    asm volatile(
        "mov.m.x m0, %[mask], 0\n"
        "flw.ps f0, 0(%[src])\n"
        "flw.ps f1, 32(%[src])\n"
        "fsw.ps f0, 0(%[dst])\n"
        "fsw.ps f1, 32(%[dst])\n"
        :
        : [mask] "r"(mask), [dst] "r"(dst), [src] "r"(src)
        : "f0", "f1", "memory");
}

static inline void attention_et_copy_matrix_ps(
    float dst[ATTENTION_SEQ_LEN][ATTENTION_HEAD_DIM],
    const float src[ATTENTION_SEQ_LEN][ATTENTION_HEAD_DIM])
{
    for (uint32_t row = 0; row < ATTENTION_SEQ_LEN; row++)
        attention_et_copy_row_ps(dst[row], src[row]);
}

static inline void attention_et_mul_row_scalar_ps(
    float row[ATTENTION_HEAD_DIM], float scalar)
{
    float scale[8] __attribute__((aligned(64)));
    uint32_t mask = 0xffu;

    for (uint32_t lane = 0; lane < 8u; lane++)
        scale[lane] = scalar;

    asm volatile(
        "mov.m.x m0, %[mask], 0\n"
        "flw.ps f2, 0(%[scale])\n"
        "flw.ps f0, 0(%[row])\n"
        "flw.ps f1, 32(%[row])\n"
        "fmul.ps f0, f0, f2\n"
        "fmul.ps f1, f1, f2\n"
        "fsw.ps f0, 0(%[row])\n"
        "fsw.ps f1, 32(%[row])\n"
        :
        : [mask] "r"(mask), [row] "r"(row), [scale] "r"(scale)
        : "f0", "f1", "f2", "memory");
}

static inline void attention_et_scale_matrix_ps(
    float matrix[ATTENTION_SEQ_LEN][ATTENTION_HEAD_DIM], float scalar)
{
    for (uint32_t row = 0; row < ATTENTION_SEQ_LEN; row++)
        attention_et_mul_row_scalar_ps(matrix[row], scalar);
}

static inline int attention_compute_scores_tensor(
    const struct attention_head_input *in, struct attention_score_packet *out)
{
    out->head = in->head;
    attention_clear_header_padding(out->header_pad);
    attention_et_copy_matrix_ps(out->v, in->v);

    if (attention_et_matmul_16x16(out->score, in->q, in->k,
                                  ATTENTION_ET_TENSOR_LOAD_TRANSPOSE32))
        return -1;

    attention_et_scale_matrix_ps(out->score, ATTENTION_SCORE_SCALE);
    return 0;
}

static inline void attention_softmax_row_ps(
    const float score[ATTENTION_SEQ_LEN], float weight[ATTENTION_SEQ_LEN])
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

    if (sum > 0.0f)
        attention_et_mul_row_scalar_ps(weight, attention_reciprocal_sum(sum));
}

static inline void attention_compute_softmax_ps(
    const struct attention_score_packet *in, struct attention_softmax_packet *out)
{
    out->head = in->head;
    attention_clear_header_padding(out->header_pad);

    for (uint32_t row = 0; row < ATTENTION_SEQ_LEN; row++) {
        attention_softmax_row_ps(in->score[row], out->weight[row]);
        attention_et_copy_row_ps(out->v[row], in->v[row]);
    }
}

#endif /* ATTENTION_ET_H */
