#include <stdint.h>

#include "tpa/tpa.h"

#include "yolov5n_common.h"

#include "generated/yolov5n_l7_tensor_weights.h"
#include "generated/yolov5n_l8_tensor_weights.h"
#include "generated/yolov5n_l9_tensor_weights.h"
#include "generated/yolov5n_l10_tensor_weights.h"
#include "generated/yolov5n_l11_tensor_weights.h"
#include "generated/yolov5n_l12_tensor_weights.h"
#include "generated/yolov5n_l13_tensor_weights.h"
#include "generated/yolov5n_l14_tensor_weights.h"
#include "yolov5n_luts.h"

#define INPUT_H 160u
#define INPUT_W 160u
#define INPUT_C 32u

#define DOWN_H 80u
#define DOWN_W 80u
#define DOWN_C 64u

#define NR_PIX     (DOWN_H * DOWN_W)
#define BRANCH_C   32u
#define CAT_C      64u
#define OUTPUT_C   64u

typedef struct {
    const int8_t *in;
    uint32_t in_len;
} yolov5n_p1_ws_t;

tpa_op_t yolov5n_p1_start(void);
tpa_op_t yolov5n_p1_run(void);
tpa_op_t yolov5n_p1_done(void);

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
static int residual_lut0_ready;
static int residual_lut1_ready;
static uint32_t tensor_ready;

#define PACKED_SCRATCH_BYTES  (YV5N_PIX_BLOCK * 64u)
#define PATCH_SCRATCH_BYTES   (YV5N_PIX_BLOCK * 320u)
#define ACC_SCRATCH_WORDS     (YV5N_PIX_BLOCK * YV5N_OC_BLOCK)
#define YV5N_P1_SCRATCH_PEAK_BYTES \
    (YV5N_ARENA_STEP(0u, NR_PIX * DOWN_C, 64u) + \
     (NR_PIX * BRANCH_C) + \
     (NR_PIX * BRANCH_C) + \
     (NR_PIX * BRANCH_C) + \
     PACKED_SCRATCH_BYTES + \
     PATCH_SCRATCH_BYTES + \
     (sizeof(int32_t) * ACC_SCRATCH_WORDS))

TPA_PROC_MEM_META(yolov5n_p1_mem_meta, 521u, YV5N_P1_SCRATCH_PEAK_BYTES);

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

DEF_LAYER(layer7, 7, yv5n_model_3_lut, 32u, 64u, 3u, 3u, 2u, 2u, 1u, 1u,
          8.06587925e-02f, 3.89574531e-02f);
DEF_LAYER(layer8, 8, yv5n_model_4_cv1_lut, 64u, 32u, 1u, 1u, 1u, 1u, 0u, 0u,
          3.89574531e-02f, 1.71277335e-02f);
DEF_LAYER(layer9, 9, yv5n_model_4_m_0_cv1_lut, 32u, 32u, 1u, 1u, 1u, 1u, 0u, 0u,
          1.71277335e-02f, 2.69322827e-02f);
DEF_LAYER(layer10, 10, yv5n_model_4_m_0_cv2_lut, 32u, 32u, 3u, 3u, 1u, 1u, 1u, 1u,
          2.69322827e-02f, 2.04704578e-02f);
DEF_LAYER(layer11, 11, yv5n_model_4_m_1_cv1_lut, 32u, 32u, 1u, 1u, 1u, 1u, 0u, 0u,
          2.13448757e-02f, 2.32489034e-02f);
DEF_LAYER(layer12, 12, yv5n_model_4_m_1_cv2_lut, 32u, 32u, 3u, 3u, 1u, 1u, 1u, 1u,
          2.32489034e-02f, 3.96867850e-02f);
DEF_LAYER(layer13, 13, yv5n_model_4_cv2_lut, 64u, 32u, 1u, 1u, 1u, 1u, 0u, 0u,
          3.89574531e-02f, 3.58640641e-02f);
DEF_LAYER(layer14, 14, yv5n_model_4_cv3_lut, 64u, 64u, 1u, 1u, 1u, 1u, 0u, 0u,
          5.56989354e-02f, 2.31987360e-02f);

tpa_op_t yolov5n_p1_start(void)
{
    yolov5n_p1_ws_t *w = tpa_ws();
    return tpa_recv(tpa_chan(0), (void **)&w->in, &w->in_len, yolov5n_p1_run);
}

tpa_op_t yolov5n_p1_run(void)
{
    yolov5n_p1_ws_t *w = tpa_ws();
    int8_t *out_buf = yolov5n_send_buf(tpa_chan(1));

    YV5N_REQUIRE_STOP(w->in_len == INPUT_H * INPUT_W * INPUT_C);

    yolov5n_arena_begin(YV5N_P1_SCRATCH_PEAK_BYTES);
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
    yolov5n_build_residual_lut(&layer8, &layer10, residual_lut0, &residual_lut0_ready);
    yolov5n_build_residual_lut(&layer10, &layer12, residual_lut1, &residual_lut1_ready);

    if (yolov5n_run_convk_packed_layer(&layer7, w->in,
                                       INPUT_H, INPUT_W, INPUT_C,
                                       down_buf, DOWN_H, DOWN_W, DOWN_C,
                                       patch_scratch, PATCH_SCRATCH_BYTES,
                                       acc_scratch))
        return tpa_stop();

    if (yolov5n_run_conv1x1_layer(&layer8, down_buf, NR_PIX, DOWN_C,
                                  a_buf, BRANCH_C,
                                  packed_scratch, PACKED_SCRATCH_BYTES,
                                  acc_scratch))
        return tpa_stop();

    if (yolov5n_run_conv1x1_layer(&layer9, a_buf, NR_PIX, BRANCH_C,
                                  mid_buf, BRANCH_C,
                                  packed_scratch, PACKED_SCRATCH_BYTES,
                                  acc_scratch))
        return tpa_stop();

    if (yolov5n_run_convk_packed_layer(&layer10, mid_buf,
                                       DOWN_H, DOWN_W, BRANCH_C,
                                       b_buf, DOWN_H, DOWN_W, BRANCH_C,
                                       patch_scratch, PATCH_SCRATCH_BYTES,
                                       acc_scratch))
        return tpa_stop();

    yolov5n_add_residual_block(a_buf, b_buf, NR_PIX * BRANCH_C, residual_lut0);

    if (yolov5n_run_conv1x1_layer(&layer11, b_buf, NR_PIX, BRANCH_C,
                                  mid_buf, BRANCH_C,
                                  packed_scratch, PACKED_SCRATCH_BYTES,
                                  acc_scratch))
        return tpa_stop();

    if (yolov5n_run_convk_packed_layer(&layer12, mid_buf,
                                       DOWN_H, DOWN_W, BRANCH_C,
                                       a_buf, DOWN_H, DOWN_W, BRANCH_C,
                                       patch_scratch, PATCH_SCRATCH_BYTES,
                                       acc_scratch))
        return tpa_stop();

    yolov5n_add_residual_block(b_buf, a_buf, NR_PIX * BRANCH_C, residual_lut1);

    if (yolov5n_run_conv1x1_layer(&layer13, down_buf, NR_PIX, DOWN_C,
                                  branch_buf, BRANCH_C,
                                  packed_scratch, PACKED_SCRATCH_BYTES,
                                  acc_scratch))
        return tpa_stop();

    yolov5n_concat2(a_buf, BRANCH_C, branch_buf, BRANCH_C, NR_PIX, cat_buf);

    if (yolov5n_run_conv1x1_layer(&layer14, cat_buf, NR_PIX, CAT_C,
                                  out_buf, OUTPUT_C,
                                  packed_scratch, PACKED_SCRATCH_BYTES,
                                  acc_scratch))
        return tpa_stop();

    return tpa_send(tpa_chan(1), out_buf, NR_PIX * OUTPUT_C, yolov5n_p1_done);
}

tpa_op_t yolov5n_p1_done(void)
{
    return tpa_stop();
}
