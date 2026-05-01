#include <stdint.h>

#include "tpa/tpa.h"

#include "yolov5n_common.h"

#include "generated/yolov5n_l0_tensor_weights.h"
#include "generated/yolov5n_l1_tensor_weights.h"
#include "generated/yolov5n_l2_tensor_weights.h"
#include "generated/yolov5n_l3_tensor_weights.h"
#include "generated/yolov5n_l4_tensor_weights.h"
#include "generated/yolov5n_l5_tensor_weights.h"
#include "generated/yolov5n_l6_tensor_weights.h"
#include "yolov5n_luts.h"

#define INPUT_H 640u
#define INPUT_W 640u
#define INPUT_C 3u

#define L0_H 320u
#define L0_W 320u
#define L0_C 16u

#define NR_PIX_160 (160u * 160u)
#define BRANCH_C   16u
#define CAT_C      32u
#define OUTPUT_C   32u

typedef struct {
    const int8_t *in;
    uint32_t in_len;
} yolov5n_p0_ws_t;

tpa_op_t yolov5n_p0_start(void);
tpa_op_t yolov5n_p0_run(void);
tpa_op_t yolov5n_p0_done(void);

static int8_t *buf0;
static int8_t *a_buf;
static int8_t *mid_buf;
static int8_t *packed_scratch;
static int8_t *patch_scratch;
static int32_t *acc_scratch;
static int8_t residual_lut0[256] __attribute__((aligned(64)));
static int residual_lut0_ready;
static uint32_t tensor_ready;

#define l0_buf      buf0
#define l1_buf      out_buf
#define b_buf       buf0
#define branch_buf  mid_buf
#define cat_buf     buf0

#define PACKED_SCRATCH_BYTES  (YV5N_PIX_BLOCK * 64u)
#define PATCH_SCRATCH_BYTES   (YV5N_PIX_BLOCK * 192u)
#define ACC_SCRATCH_WORDS     (YV5N_PIX_BLOCK * YV5N_OC_BLOCK)
#define YV5N_P0_SCRATCH_PEAK_BYTES \
    (YV5N_ARENA_STEP(0u, L0_H * L0_W * L0_C, 64u) + \
     (NR_PIX_160 * BRANCH_C) + \
     (NR_PIX_160 * BRANCH_C) + \
     PACKED_SCRATCH_BYTES + \
     PATCH_SCRATCH_BYTES + \
     (sizeof(int32_t) * ACC_SCRATCH_WORDS))

TPA_PROC_MEM_META(yolov5n_p0_mem_meta, 520u, YV5N_P0_SCRATCH_PEAK_BYTES);

#define DEF_LAYER(NAME, PFX, LUT, CIN, KOUT, KH, KW, SH, SW, PH, PW, AIN, AOUT) \
    static const yolov5n_tensor_layer_t NAME = {                                 \
        .tl8_b = yolov5n_l##PFX##_tl8_b,                                         \
        .bias_blk = yolov5n_l##PFX##_bias_blk,                                   \
        .scale_blk = yolov5n_l##PFX##_scale_blk,                                 \
        .lut = LUT,                                                              \
        .c_in = CIN,                                                             \
        .k_out = KOUT,                                                           \
        .k_h = KH,                                                               \
        .k_w = KW,                                                               \
        .stride_h = SH,                                                          \
        .stride_w = SW,                                                          \
        .pad_h = PH,                                                             \
        .pad_w = PW,                                                             \
        .k_inner = YOLOV5N_L##PFX##_TENSOR_K_INNER,                              \
        .k_padded = YOLOV5N_L##PFX##_TENSOR_K_PADDED,                            \
        .k_block = YOLOV5N_L##PFX##_TENSOR_K_BLOCK,                              \
        .k_blocks = YOLOV5N_L##PFX##_TENSOR_K_BLOCKS,                            \
        .tl8_row_stride = YOLOV5N_L##PFX##_TL8_ROW_STRIDE,                       \
        .oc_blocks = YOLOV5N_L##PFX##_TENSOR_OC_BLOCKS,                          \
        .act_in_scale = AIN,                                                     \
        .act_out_scale = AOUT,                                                   \
    }

DEF_LAYER(layer0, 0, yv5n_model_0_lut, 3u, 16u, 6u, 6u, 2u, 2u, 2u, 2u,
          7.87401152e-03f, 2.00544538e-01f);
DEF_LAYER(layer1, 1, yv5n_model_1_lut, 16u, 32u, 3u, 3u, 2u, 2u, 1u, 1u,
          2.00544538e-01f, 6.45417101e-01f);
DEF_LAYER(layer2, 2, yv5n_model_2_cv1_lut, 32u, 16u, 1u, 1u, 1u, 1u, 0u, 0u,
          6.45417101e-01f, 5.32445908e-02f);
DEF_LAYER(layer3, 3, yv5n_model_2_m_0_cv1_lut, 16u, 16u, 1u, 1u, 1u, 1u, 0u, 0u,
          5.32445908e-02f, 8.99802230e-02f);
DEF_LAYER(layer4, 4, yv5n_model_2_m_0_cv2_lut, 16u, 16u, 3u, 3u, 1u, 1u, 1u, 1u,
          8.99802230e-02f, 1.06311430e-01f);
DEF_LAYER(layer5, 5, yv5n_model_2_cv2_lut, 32u, 16u, 1u, 1u, 1u, 1u, 0u, 0u,
          6.45417101e-01f, 2.44227237e-01f);
DEF_LAYER(layer6, 6, yv5n_model_2_cv3_lut, 32u, 32u, 1u, 1u, 1u, 1u, 0u, 0u,
          2.44227237e-01f, 8.06587925e-02f);

tpa_op_t yolov5n_p0_start(void)
{
    yolov5n_p0_ws_t *w = tpa_ws();
    return tpa_recv(tpa_chan(0), (void **)&w->in, &w->in_len, yolov5n_p0_run);
}

tpa_op_t yolov5n_p0_run(void)
{
    yolov5n_p0_ws_t *w = tpa_ws();
    int8_t *out_buf = yolov5n_send_buf(tpa_chan(1));

    YV5N_REQUIRE_STOP(w->in_len == INPUT_H * INPUT_W * INPUT_C);

    yolov5n_arena_begin(YV5N_P0_SCRATCH_PEAK_BYTES);
    buf0 = yolov5n_arena_alloc(L0_H * L0_W * L0_C, 64u);
    a_buf = yolov5n_arena_alloc(NR_PIX_160 * BRANCH_C, 64u);
    mid_buf = yolov5n_arena_alloc(NR_PIX_160 * BRANCH_C, 64u);
    packed_scratch = yolov5n_arena_alloc(PACKED_SCRATCH_BYTES, 64u);
    patch_scratch = yolov5n_arena_alloc(PATCH_SCRATCH_BYTES, 64u);
    acc_scratch = yolov5n_arena_alloc(sizeof(*acc_scratch) * ACC_SCRATCH_WORDS, 64u);
    YV5N_REQUIRE_STOP(buf0 && a_buf && mid_buf &&
                      packed_scratch && patch_scratch && acc_scratch);

    yolov5n_enable_tensor_scratchpad(&tensor_ready);
    yolov5n_build_residual_lut(&layer2, &layer4, residual_lut0, &residual_lut0_ready);

    if (yolov5n_run_convk_packed_layer(&layer0, w->in,
                                       INPUT_H, INPUT_W, INPUT_C,
                                       l0_buf, L0_H, L0_W, L0_C,
                                       patch_scratch, PATCH_SCRATCH_BYTES,
                                       acc_scratch))
        return tpa_stop();

    if (yolov5n_run_convk_packed_layer(&layer1, l0_buf,
                                       L0_H, L0_W, L0_C,
                                       l1_buf, 160u, 160u, OUTPUT_C,
                                       patch_scratch, PATCH_SCRATCH_BYTES,
                                       acc_scratch))
        return tpa_stop();

    if (yolov5n_run_conv1x1_layer(&layer2, l1_buf, NR_PIX_160, OUTPUT_C,
                                  a_buf, BRANCH_C,
                                  packed_scratch, PACKED_SCRATCH_BYTES,
                                  acc_scratch))
        return tpa_stop();

    if (yolov5n_run_conv1x1_layer(&layer3, a_buf, NR_PIX_160, BRANCH_C,
                                  mid_buf, BRANCH_C,
                                  packed_scratch, PACKED_SCRATCH_BYTES,
                                  acc_scratch))
        return tpa_stop();

    if (yolov5n_run_convk_packed_layer(&layer4, mid_buf,
                                       160u, 160u, BRANCH_C,
                                       b_buf, 160u, 160u, BRANCH_C,
                                       patch_scratch, PATCH_SCRATCH_BYTES,
                                       acc_scratch))
        return tpa_stop();

    yolov5n_add_residual_block(a_buf, b_buf, NR_PIX_160 * BRANCH_C, residual_lut0);

    if (yolov5n_run_conv1x1_layer(&layer5, l1_buf, NR_PIX_160, OUTPUT_C,
                                  branch_buf, BRANCH_C,
                                  packed_scratch, PACKED_SCRATCH_BYTES,
                                  acc_scratch))
        return tpa_stop();

    yolov5n_concat2(b_buf, BRANCH_C, branch_buf, BRANCH_C, NR_PIX_160, cat_buf);

    if (yolov5n_run_conv1x1_layer(&layer6, cat_buf, NR_PIX_160, CAT_C,
                                  out_buf, OUTPUT_C,
                                  packed_scratch, PACKED_SCRATCH_BYTES,
                                  acc_scratch))
        return tpa_stop();

    return tpa_send(tpa_chan(1), out_buf, NR_PIX_160 * OUTPUT_C, yolov5n_p0_done);
}

tpa_op_t yolov5n_p0_done(void)
{
    return tpa_stop();
}
