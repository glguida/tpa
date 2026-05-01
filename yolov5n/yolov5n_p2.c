#include <stdint.h>

#include "tpa/tpa.h"

#include "yolov5n_common.h"

#include "generated/yolov5n_l15_tensor_weights.h"
#include "generated/yolov5n_l16_tensor_weights.h"
#include "generated/yolov5n_l17_tensor_weights.h"
#include "generated/yolov5n_l18_tensor_weights.h"
#include "generated/yolov5n_l19_tensor_weights.h"
#include "generated/yolov5n_l20_tensor_weights.h"
#include "generated/yolov5n_l21_tensor_weights.h"
#include "generated/yolov5n_l22_tensor_weights.h"
#include "generated/yolov5n_l23_tensor_weights.h"
#include "generated/yolov5n_l24_tensor_weights.h"
#include "yolov5n_luts.h"

#define INPUT_H 80u
#define INPUT_W 80u
#define INPUT_C 64u

#define DOWN_H 40u
#define DOWN_W 40u
#define DOWN_C 128u

#define NR_PIX     (DOWN_H * DOWN_W)
#define BRANCH_C   64u
#define CAT_C      128u
#define OUTPUT_C   128u

typedef struct {
    const int8_t *in;
    uint32_t in_len;
} yolov5n_p2_ws_t;

tpa_op_t yolov5n_p2_start(void);
tpa_op_t yolov5n_p2_run(void);
tpa_op_t yolov5n_p2_done(void);

static int8_t *down_buf;
static int8_t *a_buf;
static int8_t *mid_buf;
static int8_t *b_buf;
static int8_t *branch_buf;
static int8_t *cat_buf;
static int8_t *packed_scratch;
static int8_t *patch_scratch;
static int32_t *acc_scratch;
static int8_t residual_lut0[256] __attribute__((aligned(64)));
static int8_t residual_lut1[256] __attribute__((aligned(64)));
static int8_t residual_lut2[256] __attribute__((aligned(64)));
static int residual_lut0_ready;
static int residual_lut1_ready;
static int residual_lut2_ready;
static uint32_t tensor_ready;

#define PACKED_SCRATCH_BYTES  (YV5N_PIX_BLOCK * 128u)
#define PATCH_SCRATCH_BYTES   (YV5N_PIX_BLOCK * 576u)
#define ACC_SCRATCH_WORDS     (YV5N_PIX_BLOCK * YV5N_OC_BLOCK)
#define YV5N_P2_SCRATCH_PEAK_BYTES \
    (YV5N_ARENA_STEP(0u, NR_PIX * DOWN_C, 64u) + \
     (NR_PIX * BRANCH_C) + \
     (NR_PIX * BRANCH_C) + \
     (NR_PIX * BRANCH_C) + \
     PACKED_SCRATCH_BYTES + \
     PATCH_SCRATCH_BYTES + \
     (sizeof(int32_t) * ACC_SCRATCH_WORDS))

TPA_PROC_MEM_META(yolov5n_p2_mem_meta, 522u, YV5N_P2_SCRATCH_PEAK_BYTES);

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

DEF_LAYER(layer15, 15, yv5n_model_5_lut, 64u, 128u, 3u, 3u, 2u, 2u, 1u, 1u,
          2.31987360e-02f, 2.69375842e-02f);
DEF_LAYER(layer16, 16, yv5n_model_6_cv1_lut, 128u, 64u, 1u, 1u, 1u, 1u, 0u, 0u,
          2.69375842e-02f, 7.14033514e-03f);
DEF_LAYER(layer17, 17, yv5n_model_6_m_0_cv1_lut, 64u, 64u, 1u, 1u, 1u, 1u, 0u, 0u,
          7.14033514e-03f, 3.01994113e-02f);
DEF_LAYER(layer18, 18, yv5n_model_6_m_0_cv2_lut, 64u, 64u, 3u, 3u, 1u, 1u, 1u, 1u,
          3.01994113e-02f, 1.75305971e-02f);
DEF_LAYER(layer19, 19, yv5n_model_6_m_1_cv1_lut, 64u, 64u, 1u, 1u, 1u, 1u, 0u, 0u,
          1.56664126e-02f, 1.93535080e-02f);
DEF_LAYER(layer20, 20, yv5n_model_6_m_1_cv2_lut, 64u, 64u, 3u, 3u, 1u, 1u, 1u, 1u,
          1.93535080e-02f, 1.76682810e-02f);
DEF_LAYER(layer21, 21, yv5n_model_6_m_2_cv1_lut, 64u, 64u, 1u, 1u, 1u, 1u, 0u, 0u,
          2.37588207e-02f, 1.72004568e-02f);
DEF_LAYER(layer22, 22, yv5n_model_6_m_2_cv2_lut, 64u, 64u, 3u, 3u, 1u, 1u, 1u, 1u,
          1.72004568e-02f, 2.92498803e-02f);
DEF_LAYER(layer23, 23, yv5n_model_6_cv2_lut, 128u, 64u, 1u, 1u, 1u, 1u, 0u, 0u,
          2.69375842e-02f, 3.02781026e-02f);
DEF_LAYER(layer24, 24, yv5n_model_6_cv3_lut, 128u, 128u, 1u, 1u, 1u, 1u, 0u, 0u,
          3.67047956e-02f, 1.87158453e-02f);

tpa_op_t yolov5n_p2_start(void)
{
    yolov5n_p2_ws_t *w = tpa_ws();
    return tpa_recv(tpa_chan(0), (void **)&w->in, &w->in_len, yolov5n_p2_run);
}

tpa_op_t yolov5n_p2_run(void)
{
    yolov5n_p2_ws_t *w = tpa_ws();
    int8_t *out_buf = yolov5n_send_buf(tpa_chan(1));

    YV5N_REQUIRE_STOP(w->in_len == INPUT_H * INPUT_W * INPUT_C);

    yolov5n_arena_begin(YV5N_P2_SCRATCH_PEAK_BYTES);
    down_buf = yolov5n_arena_alloc(NR_PIX * DOWN_C, 64u);
    a_buf = yolov5n_arena_alloc(NR_PIX * BRANCH_C, 64u);
    mid_buf = yolov5n_arena_alloc(NR_PIX * BRANCH_C, 64u);
    b_buf = yolov5n_arena_alloc(NR_PIX * BRANCH_C, 64u);
    packed_scratch = yolov5n_arena_alloc(PACKED_SCRATCH_BYTES, 64u);
    patch_scratch = yolov5n_arena_alloc(PATCH_SCRATCH_BYTES, 64u);
    acc_scratch = yolov5n_arena_alloc(sizeof(*acc_scratch) * ACC_SCRATCH_WORDS, 64u);
    branch_buf = mid_buf;
    cat_buf = down_buf;
    YV5N_REQUIRE_STOP(down_buf && a_buf && mid_buf && b_buf &&
                      packed_scratch && patch_scratch && acc_scratch);

    yolov5n_enable_tensor_scratchpad(&tensor_ready);
    yolov5n_build_residual_lut(&layer16, &layer18, residual_lut0, &residual_lut0_ready);
    yolov5n_build_residual_lut(&layer18, &layer20, residual_lut1, &residual_lut1_ready);
    yolov5n_build_residual_lut(&layer20, &layer22, residual_lut2, &residual_lut2_ready);

    if (yolov5n_run_convk_packed_layer(&layer15, w->in,
                                       INPUT_H, INPUT_W, INPUT_C,
                                       down_buf, DOWN_H, DOWN_W, DOWN_C,
                                       patch_scratch, PATCH_SCRATCH_BYTES,
                                       acc_scratch))
        return tpa_stop();

    if (yolov5n_run_conv1x1_layer(&layer16, down_buf, NR_PIX, DOWN_C,
                                  a_buf, BRANCH_C,
                                  packed_scratch, PACKED_SCRATCH_BYTES,
                                  acc_scratch))
        return tpa_stop();

    if (yolov5n_run_conv1x1_layer(&layer17, a_buf, NR_PIX, BRANCH_C,
                                  mid_buf, BRANCH_C,
                                  packed_scratch, PACKED_SCRATCH_BYTES,
                                  acc_scratch))
        return tpa_stop();

    if (yolov5n_run_convk_packed_layer(&layer18, mid_buf,
                                       DOWN_H, DOWN_W, BRANCH_C,
                                       b_buf, DOWN_H, DOWN_W, BRANCH_C,
                                       patch_scratch, PATCH_SCRATCH_BYTES,
                                       acc_scratch))
        return tpa_stop();

    yolov5n_add_residual_block(a_buf, b_buf, NR_PIX * BRANCH_C, residual_lut0);

    if (yolov5n_run_conv1x1_layer(&layer19, b_buf, NR_PIX, BRANCH_C,
                                  mid_buf, BRANCH_C,
                                  packed_scratch, PACKED_SCRATCH_BYTES,
                                  acc_scratch))
        return tpa_stop();

    if (yolov5n_run_convk_packed_layer(&layer20, mid_buf,
                                       DOWN_H, DOWN_W, BRANCH_C,
                                       a_buf, DOWN_H, DOWN_W, BRANCH_C,
                                       patch_scratch, PATCH_SCRATCH_BYTES,
                                       acc_scratch))
        return tpa_stop();

    yolov5n_add_residual_block(b_buf, a_buf, NR_PIX * BRANCH_C, residual_lut1);

    if (yolov5n_run_conv1x1_layer(&layer21, a_buf, NR_PIX, BRANCH_C,
                                  mid_buf, BRANCH_C,
                                  packed_scratch, PACKED_SCRATCH_BYTES,
                                  acc_scratch))
        return tpa_stop();

    if (yolov5n_run_convk_packed_layer(&layer22, mid_buf,
                                       DOWN_H, DOWN_W, BRANCH_C,
                                       b_buf, DOWN_H, DOWN_W, BRANCH_C,
                                       patch_scratch, PATCH_SCRATCH_BYTES,
                                       acc_scratch))
        return tpa_stop();

    yolov5n_add_residual_block(a_buf, b_buf, NR_PIX * BRANCH_C, residual_lut2);

    if (yolov5n_run_conv1x1_layer(&layer23, down_buf, NR_PIX, DOWN_C,
                                  branch_buf, BRANCH_C,
                                  packed_scratch, PACKED_SCRATCH_BYTES,
                                  acc_scratch))
        return tpa_stop();

    yolov5n_concat2(b_buf, BRANCH_C, branch_buf, BRANCH_C, NR_PIX, cat_buf);

    if (yolov5n_run_conv1x1_layer(&layer24, cat_buf, NR_PIX, CAT_C,
                                  out_buf, OUTPUT_C,
                                  packed_scratch, PACKED_SCRATCH_BYTES,
                                  acc_scratch))
        return tpa_stop();

    return tpa_send(tpa_chan(1), out_buf, NR_PIX * OUTPUT_C, yolov5n_p2_done);
}

tpa_op_t yolov5n_p2_done(void)
{
    return tpa_stop();
}
