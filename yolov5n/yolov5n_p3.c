#include <stdint.h>

#include "tpa/tpa.h"

#include "yolov5n_common.h"

#include "generated/yolov5n_l25_tensor_weights.h"
#include "generated/yolov5n_l26_tensor_weights.h"
#include "generated/yolov5n_l27_tensor_weights.h"
#include "generated/yolov5n_l28_tensor_weights.h"
#include "generated/yolov5n_l29_tensor_weights.h"
#include "generated/yolov5n_l30_tensor_weights.h"
#include "generated/yolov5n_l31_tensor_weights.h"
#include "generated/yolov5n_l32_tensor_weights.h"
#include "generated/yolov5n_l33_tensor_weights.h"
#include "yolov5n_luts.h"

#define INPUT_H 40u
#define INPUT_W 40u
#define INPUT_C 128u

#define DOWN_H 20u
#define DOWN_W 20u
#define DOWN_C 256u

#define NR_PIX       (DOWN_H * DOWN_W)
#define BRANCH_C     128u
#define CAT1_C       256u
#define CAT4_C       512u
#define OUTPUT_C     128u

typedef struct {
    const int8_t *in;
    uint32_t in_len;
} yolov5n_p3_ws_t;

tpa_op_t yolov5n_p3_start(void);
tpa_op_t yolov5n_p3_run(void);
tpa_op_t yolov5n_p3_done(void);

static int8_t *down_buf;
static int8_t *a_buf;
static int8_t *mid_buf;
static int8_t *b_buf;
static int8_t *branch_buf;
static int8_t *cat1_buf;
static int8_t *c3_buf;
static int8_t *cat4_buf;
static int8_t *sppf_buf;
static int8_t *p1_buf;
static int8_t *p2_buf;
static int8_t *p3_buf;
static int8_t *packed_scratch;
static int8_t *patch_scratch;
static int8_t *pool_tmp;
static int32_t *acc_scratch;
static int8_t residual_lut0[256] __attribute__((aligned(64)));
static int residual_lut0_ready;
static uint32_t tensor_ready;

#define PACKED_SCRATCH_BYTES  (YV5N_PIX_BLOCK * 512u)
#define PATCH_SCRATCH_BYTES   (YV5N_PIX_BLOCK * 1152u)
#define ACC_SCRATCH_WORDS     (YV5N_PIX_BLOCK * YV5N_OC_BLOCK)
#define YV5N_P3_SCRATCH_PEAK_BYTES \
    (YV5N_ARENA_STEP(0u, NR_PIX * CAT4_C, 64u) + \
     (NR_PIX * DOWN_C) + \
     (NR_PIX * BRANCH_C) + \
     (NR_PIX * BRANCH_C) + \
     (NR_PIX * BRANCH_C) + \
     (NR_PIX * BRANCH_C) + \
     (NR_PIX * BRANCH_C) + \
     (NR_PIX * BRANCH_C) + \
     PACKED_SCRATCH_BYTES + \
     PATCH_SCRATCH_BYTES + \
     (NR_PIX * BRANCH_C) + \
     (sizeof(int32_t) * ACC_SCRATCH_WORDS))

TPA_PROC_MEM_META(yolov5n_p3_mem_meta, 523u, YV5N_P3_SCRATCH_PEAK_BYTES);

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

DEF_LAYER(layer25, 25, yv5n_model_7_lut, 128u, 256u, 3u, 3u, 2u, 2u, 1u, 1u,
          1.87158453e-02f, 2.16285732e-02f);
DEF_LAYER(layer26, 26, yv5n_model_8_cv1_lut, 256u, 128u, 1u, 1u, 1u, 1u, 0u, 0u,
          2.16285732e-02f, 4.80553766e-03f);
DEF_LAYER(layer27, 27, yv5n_model_8_m_0_cv1_lut, 128u, 128u, 1u, 1u, 1u, 1u, 0u, 0u,
          4.80553766e-03f, 2.06935687e-02f);
DEF_LAYER(layer28, 28, yv5n_model_8_m_0_cv2_lut, 128u, 128u, 3u, 3u, 1u, 1u, 1u, 1u,
          2.06935687e-02f, 2.86584914e-02f);
DEF_LAYER(layer29, 29, yv5n_model_8_cv2_lut, 256u, 128u, 1u, 1u, 1u, 1u, 0u, 0u,
          2.16285732e-02f, 3.15175094e-02f);
DEF_LAYER(layer30, 30, yv5n_model_8_cv3_lut, 256u, 256u, 1u, 1u, 1u, 1u, 0u, 0u,
          3.15175094e-02f, 2.75250870e-02f);
DEF_LAYER(layer31, 31, yv5n_model_9_cv1_lut, 256u, 128u, 1u, 1u, 1u, 1u, 0u, 0u,
          2.75250870e-02f, 3.16683777e-02f);
DEF_LAYER(layer32, 32, yv5n_model_9_cv2_lut, 512u, 256u, 1u, 1u, 1u, 1u, 0u, 0u,
          3.22858292e-02f, 1.59258730e-02f);
DEF_LAYER(layer33, 33, yv5n_model_10_lut, 256u, 128u, 1u, 1u, 1u, 1u, 0u, 0u,
          1.59258730e-02f, 2.61424414e-02f);

tpa_op_t yolov5n_p3_start(void)
{
    yolov5n_p3_ws_t *w = tpa_ws();
    return tpa_recv(tpa_chan(0), (void **)&w->in, &w->in_len, yolov5n_p3_run);
}

tpa_op_t yolov5n_p3_run(void)
{
    yolov5n_p3_ws_t *w = tpa_ws();
    int8_t *out_buf = yolov5n_send_buf(tpa_chan(1));

    YV5N_REQUIRE_STOP(w->in_len == INPUT_H * INPUT_W * INPUT_C);

    yolov5n_arena_begin(YV5N_P3_SCRATCH_PEAK_BYTES);
    cat1_buf = yolov5n_arena_alloc(NR_PIX * CAT4_C, 64u);
    down_buf = yolov5n_arena_alloc(NR_PIX * DOWN_C, 64u);
    a_buf = yolov5n_arena_alloc(NR_PIX * BRANCH_C, 64u);
    mid_buf = yolov5n_arena_alloc(NR_PIX * BRANCH_C, 64u);
    b_buf = yolov5n_arena_alloc(NR_PIX * BRANCH_C, 64u);
    p1_buf = yolov5n_arena_alloc(NR_PIX * BRANCH_C, 64u);
    p2_buf = yolov5n_arena_alloc(NR_PIX * BRANCH_C, 64u);
    p3_buf = yolov5n_arena_alloc(NR_PIX * BRANCH_C, 64u);
    packed_scratch = yolov5n_arena_alloc(PACKED_SCRATCH_BYTES, 64u);
    patch_scratch = yolov5n_arena_alloc(PATCH_SCRATCH_BYTES, 64u);
    pool_tmp = yolov5n_arena_alloc(NR_PIX * BRANCH_C, 64u);
    acc_scratch = yolov5n_arena_alloc(sizeof(*acc_scratch) * ACC_SCRATCH_WORDS, 64u);
    branch_buf = mid_buf;
    cat4_buf = cat1_buf;
    c3_buf = down_buf;
    sppf_buf = down_buf;
    YV5N_REQUIRE_STOP(cat1_buf && down_buf && a_buf && mid_buf && b_buf &&
                      p1_buf && p2_buf && p3_buf && packed_scratch &&
                      patch_scratch && pool_tmp && acc_scratch);

    yolov5n_enable_tensor_scratchpad(&tensor_ready);
    yolov5n_build_residual_lut(&layer26, &layer28, residual_lut0, &residual_lut0_ready);

    if (yolov5n_run_convk_packed_layer(&layer25, w->in,
                                       INPUT_H, INPUT_W, INPUT_C,
                                       down_buf, DOWN_H, DOWN_W, DOWN_C,
                                       patch_scratch, PATCH_SCRATCH_BYTES,
                                       acc_scratch))
        return tpa_stop();

    if (yolov5n_run_conv1x1_layer(&layer26, down_buf, NR_PIX, DOWN_C,
                                  a_buf, BRANCH_C,
                                  packed_scratch, PACKED_SCRATCH_BYTES,
                                  acc_scratch))
        return tpa_stop();

    if (yolov5n_run_conv1x1_layer(&layer27, a_buf, NR_PIX, BRANCH_C,
                                  mid_buf, BRANCH_C,
                                  packed_scratch, PACKED_SCRATCH_BYTES,
                                  acc_scratch))
        return tpa_stop();

    if (yolov5n_run_convk_packed_layer(&layer28, mid_buf,
                                       DOWN_H, DOWN_W, BRANCH_C,
                                       b_buf, DOWN_H, DOWN_W, BRANCH_C,
                                       patch_scratch, PATCH_SCRATCH_BYTES,
                                       acc_scratch))
        return tpa_stop();

    yolov5n_add_residual_block(a_buf, b_buf, NR_PIX * BRANCH_C, residual_lut0);

    if (yolov5n_run_conv1x1_layer(&layer29, down_buf, NR_PIX, DOWN_C,
                                  branch_buf, BRANCH_C,
                                  packed_scratch, PACKED_SCRATCH_BYTES,
                                  acc_scratch))
        return tpa_stop();

    yolov5n_concat2(b_buf, BRANCH_C, branch_buf, BRANCH_C, NR_PIX, cat1_buf);

    if (yolov5n_run_conv1x1_layer(&layer30, cat1_buf, NR_PIX, CAT1_C,
                                  c3_buf, DOWN_C,
                                  packed_scratch, PACKED_SCRATCH_BYTES,
                                  acc_scratch))
        return tpa_stop();

    if (yolov5n_run_conv1x1_layer(&layer31, c3_buf, NR_PIX, DOWN_C,
                                  a_buf, BRANCH_C,
                                  packed_scratch, PACKED_SCRATCH_BYTES,
                                  acc_scratch))
        return tpa_stop();

    yolov5n_maxpool5x5_s1_p2(a_buf, DOWN_H, DOWN_W, BRANCH_C, p1_buf, pool_tmp);
    yolov5n_maxpool5x5_s1_p2(p1_buf, DOWN_H, DOWN_W, BRANCH_C, p2_buf, pool_tmp);
    yolov5n_maxpool5x5_s1_p2(p2_buf, DOWN_H, DOWN_W, BRANCH_C, p3_buf, pool_tmp);

    yolov5n_concat4(p3_buf, p2_buf, p1_buf, a_buf, BRANCH_C, NR_PIX, cat4_buf);

    if (yolov5n_run_conv1x1_layer(&layer32, cat4_buf, NR_PIX, CAT4_C,
                                  sppf_buf, DOWN_C,
                                  packed_scratch, PACKED_SCRATCH_BYTES,
                                  acc_scratch))
        return tpa_stop();

    if (yolov5n_run_conv1x1_layer(&layer33, sppf_buf, NR_PIX, DOWN_C,
                                  out_buf, OUTPUT_C,
                                  packed_scratch, PACKED_SCRATCH_BYTES,
                                  acc_scratch))
        return tpa_stop();

    return tpa_send(tpa_chan(1), out_buf, NR_PIX * OUTPUT_C, yolov5n_p3_done);
}

tpa_op_t yolov5n_p3_done(void)
{
    return tpa_stop();
}
