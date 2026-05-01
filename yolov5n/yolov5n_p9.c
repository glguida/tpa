#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <etsoc/isa/cacheops.h>
#include <etsoc/isa/tensors.h>

#include "tpa/tpa.h"
#include "yolov5n_common.h"

#include "generated/yolov5n_l51_tensor_weights.h"
#include "generated/yolov5n_l52_tensor_weights.h"
#include "generated/yolov5n_l53_tensor_weights.h"
#include "generated/yolov5n_l54_tensor_weights.h"
#include "generated/yolov5n_l55_tensor_weights.h"
#include "generated/yolov5n_l56_tensor_weights.h"
#include "yolov5n_luts.h"

#define P3_H       40u
#define P3_W       40u
#define P3_C       128u
#define SKIP_H     20u
#define SKIP_W     20u
#define SKIP_C     128u
#define INPUT_H    SKIP_H
#define INPUT_W    SKIP_W
#define NR_PIX     (INPUT_H * INPUT_W)
#define PIX_BLOCK  16u
#define ROW_BLOCK  8u
#define OC_BLOCK   16u
#define DOWN_C     128u
#define BRANCH_C   128u
#define CAT0_C     256u
#define CAT1_C     256u
#define OUTPUT_C   256u

#define SCP_A_START    0u
#define SCP_B_START   16u
#define SCP_META_LINE 32u
#define FG32B_CONF UINT64_C(0x398A418820)
#define YV5N_P9_PATCH_BYTES  (PIX_BLOCK * 1152u)
#define YV5N_P9_SCRATCH_PEAK_BYTES \
    (YV5N_ARENA_STEP(0u, NR_PIX * DOWN_C, 64u) + \
     (NR_PIX * CAT0_C) + \
     (NR_PIX * BRANCH_C) + \
     (NR_PIX * BRANCH_C) + \
     (NR_PIX * BRANCH_C) + \
     (NR_PIX * CAT1_C) + \
     YV5N_P9_PATCH_BYTES + \
     YV5N_P9_PATCH_BYTES)

typedef struct {
    const yolov5n_layer_meta_t *ld;
    const int8_t *tl8_b;
    const int32_t (*bias_blk)[OC_BLOCK];
    const float (*scale_blk)[OC_BLOCK];
    uint32_t oc_blocks;
    uint32_t k_inner;
    uint32_t k_padded;
    uint32_t k_block;
    uint32_t k_blocks;
    uint32_t tl8_row_stride;
} tensor_layer_t;

typedef struct {
    const int8_t *p4;
    uint32_t p4_len;
    const int8_t *skip;
    uint32_t skip_len;
} yolo_full_p9_ws_t;

tpa_op_t yolo_full_p9_start(void);
tpa_op_t yolo_full_p9_recv_skip(void);
tpa_op_t yolo_full_p9_run(void);
tpa_op_t yolo_full_p9_done(void);

static int8_t *down_buf;
static int8_t *cat0_buf;
static int8_t *mid_buf;
static int8_t *a_buf;
static int8_t *b_buf;
static int8_t *cat1_buf;
static int8_t *patch3x3_buf;
static int8_t *patch3x3s2_buf;
static volatile uint32_t tensor_ready;
static const int32_t byte_lane_idx[8] __attribute__((aligned(32))) = {
    0, 1, 2, 3, 4, 5, 6, 7,
};
static const int8_t zero_lane_bytes[8] __attribute__((aligned(8))) = {
    0, 0, 0, 0, 0, 0, 0, 0,
};

TPA_PROC_MEM_META(yolov5n_p9_mem_meta, 509u, YV5N_P9_SCRATCH_PEAK_BYTES);
static const int8_t zero32[32] __attribute__((aligned(32), unused)) = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};
static const yolov5n_layer_meta_t layer51_meta = {
    .lut = yv5n_model_21_lut,
    .c_in = 128u,
    .k_out = 128u,
    .k_h = 3u,
    .k_w = 3u,
    .act_in_scale = 1.74814585e-02f,
    .act_out_scale = 2.55827547e-02f,
};

static const yolov5n_layer_meta_t layer52_meta = {
    .lut = yv5n_model_23_cv1_lut,
    .c_in = 256u,
    .k_out = 128u,
    .k_h = 1u,
    .k_w = 1u,
    .act_in_scale = 2.64770590e-02f,
    .act_out_scale = 3.31388609e-02f,
};

static const yolov5n_layer_meta_t layer53_meta = {
    .lut = yv5n_model_23_m_0_cv1_lut,
    .c_in = 128u,
    .k_out = 128u,
    .k_h = 1u,
    .k_w = 1u,
    .act_in_scale = 3.31388609e-02f,
    .act_out_scale = 2.97191068e-02f,
};

static const yolov5n_layer_meta_t layer54_meta = {
    .lut = yv5n_model_23_m_0_cv2_lut,
    .c_in = 128u,
    .k_out = 128u,
    .k_h = 3u,
    .k_w = 3u,
    .act_in_scale = 2.97191068e-02f,
    .act_out_scale = 2.47505218e-02f,
};

static const yolov5n_layer_meta_t layer55_meta = {
    .lut = yv5n_model_23_cv2_lut,
    .c_in = 256u,
    .k_out = 128u,
    .k_h = 1u,
    .k_w = 1u,
    .act_in_scale = 2.64770590e-02f,
    .act_out_scale = 1.54050279e-02f,
};

static const yolov5n_layer_meta_t layer56_meta = {
    .lut = yv5n_model_23_cv3_lut,
    .c_in = 256u,
    .k_out = 256u,
    .k_h = 1u,
    .k_w = 1u,
    .act_in_scale = 2.47505218e-02f,
    .act_out_scale = 2.37435262e-02f,
};

static const tensor_layer_t layer51 = {
    .ld = &layer51_meta,
    .tl8_b = yolov5n_l51_tl8_b,
    .bias_blk = yolov5n_l51_bias_blk,
    .scale_blk = yolov5n_l51_scale_blk,
    .oc_blocks = YOLOV5N_L51_TENSOR_OC_BLOCKS,
    .k_inner = YOLOV5N_L51_TENSOR_K_INNER,
    .k_padded = YOLOV5N_L51_TENSOR_K_PADDED,
    .k_block = YOLOV5N_L51_TENSOR_K_BLOCK,
    .k_blocks = YOLOV5N_L51_TENSOR_K_BLOCKS,
    .tl8_row_stride = YOLOV5N_L51_TL8_ROW_STRIDE,
};

static const tensor_layer_t layer52 = {
    .ld = &layer52_meta,
    .tl8_b = yolov5n_l52_tl8_b,
    .bias_blk = yolov5n_l52_bias_blk,
    .scale_blk = yolov5n_l52_scale_blk,
    .oc_blocks = YOLOV5N_L52_TENSOR_OC_BLOCKS,
    .k_inner = YOLOV5N_L52_TENSOR_K_INNER,
    .k_padded = YOLOV5N_L52_TENSOR_K_PADDED,
    .k_block = YOLOV5N_L52_TENSOR_K_BLOCK,
    .k_blocks = YOLOV5N_L52_TENSOR_K_BLOCKS,
    .tl8_row_stride = YOLOV5N_L52_TL8_ROW_STRIDE,
};

static const tensor_layer_t layer53 = {
    .ld = &layer53_meta,
    .tl8_b = yolov5n_l53_tl8_b,
    .bias_blk = yolov5n_l53_bias_blk,
    .scale_blk = yolov5n_l53_scale_blk,
    .oc_blocks = YOLOV5N_L53_TENSOR_OC_BLOCKS,
    .k_inner = YOLOV5N_L53_TENSOR_K_INNER,
    .k_padded = YOLOV5N_L53_TENSOR_K_PADDED,
    .k_block = YOLOV5N_L53_TENSOR_K_BLOCK,
    .k_blocks = YOLOV5N_L53_TENSOR_K_BLOCKS,
    .tl8_row_stride = YOLOV5N_L53_TL8_ROW_STRIDE,
};

static const tensor_layer_t layer54 = {
    .ld = &layer54_meta,
    .tl8_b = yolov5n_l54_tl8_b,
    .bias_blk = yolov5n_l54_bias_blk,
    .scale_blk = yolov5n_l54_scale_blk,
    .oc_blocks = YOLOV5N_L54_TENSOR_OC_BLOCKS,
    .k_inner = YOLOV5N_L54_TENSOR_K_INNER,
    .k_padded = YOLOV5N_L54_TENSOR_K_PADDED,
    .k_block = YOLOV5N_L54_TENSOR_K_BLOCK,
    .k_blocks = YOLOV5N_L54_TENSOR_K_BLOCKS,
    .tl8_row_stride = YOLOV5N_L54_TL8_ROW_STRIDE,
};

static const tensor_layer_t layer55 = {
    .ld = &layer55_meta,
    .tl8_b = yolov5n_l55_tl8_b,
    .bias_blk = yolov5n_l55_bias_blk,
    .scale_blk = yolov5n_l55_scale_blk,
    .oc_blocks = YOLOV5N_L55_TENSOR_OC_BLOCKS,
    .k_inner = YOLOV5N_L55_TENSOR_K_INNER,
    .k_padded = YOLOV5N_L55_TENSOR_K_PADDED,
    .k_block = YOLOV5N_L55_TENSOR_K_BLOCK,
    .k_blocks = YOLOV5N_L55_TENSOR_K_BLOCKS,
    .tl8_row_stride = YOLOV5N_L55_TL8_ROW_STRIDE,
};

static const tensor_layer_t layer56 = {
    .ld = &layer56_meta,
    .tl8_b = yolov5n_l56_tl8_b,
    .bias_blk = yolov5n_l56_bias_blk,
    .scale_blk = yolov5n_l56_scale_blk,
    .oc_blocks = YOLOV5N_L56_TENSOR_OC_BLOCKS,
    .k_inner = YOLOV5N_L56_TENSOR_K_INNER,
    .k_padded = YOLOV5N_L56_TENSOR_K_PADDED,
    .k_block = YOLOV5N_L56_TENSOR_K_BLOCK,
    .k_blocks = YOLOV5N_L56_TENSOR_K_BLOCKS,
    .tl8_row_stride = YOLOV5N_L56_TL8_ROW_STRIDE,
};

static int8_t residual_lut[256] __attribute__((aligned(64)));
static int residual_lut_ready;

static int round_away_f32(float x)
{
    return (x >= 0.0f) ? (int)(x + 0.5f) : (int)(x - 0.5f);
}

static void ensure_residual_lut(void)
{
    if (residual_lut_ready)
        return;

    float src_scale = layer52.ld->act_out_scale;
    float dst_scale = layer54.ld->act_out_scale;
    float ratio = src_scale / dst_scale;

    for (int i = 0; i < 256; ++i) {
        int8_t v = (int8_t)(uint8_t)i;
        int q = round_away_f32((float)v * ratio);
        if (q < -128)
            q = -128;
        if (q > 127)
            q = 127;
        residual_lut[i] = (int8_t)q;
    }
    residual_lut_ready = 1;
}

static inline void clear_tensor_error(void)
{
    asm volatile("csrwi tensor_error, 0" : : : "memory");
}

static void enable_tensor_scratchpad(void)
{
    uint64_t pmask = 0xff;

    if (tensor_ready)
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
    clear_tensor_error();
    excl_mode(0);
    tensor_ready = 1;
}

static inline const int8_t *tl8_wblk(const tensor_layer_t *layer,
                                     uint32_t oc_blk, uint32_t k_blk)
{
    size_t blk_bytes = (size_t)layer->k_block * (size_t)layer->tl8_row_stride;

    return layer->tl8_b +
           ((size_t)oc_blk * (size_t)layer->k_blocks + (size_t)k_blk) * blk_bytes;
}

static inline void load_meta_line(uint64_t dst, const void *src)
{
    tensor_load(0, 0, dst, 0, 0, (uint64_t)src, 0, 0, 64, 0);
    tensor_wait(TENSOR_LOAD_WAIT_0);
}

static inline uint64_t hwc32_pair_base(const int8_t *src,
                                       uint32_t y, uint32_t x)
{
    uint32_t x_pair = x & ~1u;

    return (uint64_t)(uintptr_t)(src + (((size_t)y * INPUT_W + x_pair) * BRANCH_C));
}

static inline uint64_t hwc32_pair_base_signed(const int8_t *src,
                                              int32_t y, uint32_t x)
{
    intptr_t base = (intptr_t)(uintptr_t)src;
    intptr_t off = ((intptr_t)y * (intptr_t)INPUT_W + (intptr_t)(x & ~1u)) *
                   (intptr_t)BRANCH_C;

    return (uint64_t)(base + off);
}

static inline uint32_t hwc32_aoffset(uint32_t x)
{
    return (x & 1u) ? 8u : 0u;
}

static inline void vec_copy8(int8_t *dst, const int8_t *src)
{
    __asm__ __volatile__(
        "flw.ps    f31, %[idx]\n"
        "fgb.ps    f0, f31(%[src])\n"
        "fscb.ps   f0, f31(%[dst])\n"
        :
        : [idx] "m"(*(&byte_lane_idx)),
          [src] "r"(src),
          [dst] "r"(dst)
        : "f0", "f31", "memory");
}

static inline void vec_zero8(int8_t *dst)
{
    __asm__ __volatile__(
        "flw.ps    f31, %[idx]\n"
        "fgb.ps    f0, f31(%[src])\n"
        "fscb.ps   f0, f31(%[dst])\n"
        :
        : [idx] "m"(*(&byte_lane_idx)),
          [src] "r"(zero_lane_bytes),
          [dst] "r"(dst)
        : "f0", "f31", "memory");
}

static inline void vec_copy32(int8_t *dst, const int8_t *src)
{
    __asm__ __volatile__(
        "li        t0, %[conf]\n"
        "mov.m.x   m0, zero, 0xff\n"
        "fg32b.ps  f0, t0(%[src])\n"
        "fsc32b.ps f0, t0(%[dst])\n"
        :
        : [conf] "i"(FG32B_CONF),
          [src] "r"(src),
          [dst] "r"(dst)
        : "t0", "f0", "memory");
}

static void vec_copy_bytes(int8_t *dst, const int8_t *src, uint32_t n)
{
    for (uint32_t off = 0; off < n; off += 8)
        vec_copy8(dst + off, src + off);
}

static void vec_zero_bytes(int8_t *dst, uint32_t n)
{
    for (uint32_t off = 0; off < n; off += 8)
        vec_zero8(dst + off);
}

static inline void dot_i8_pi4x4_accum(int32_t acc[16],
                                      const int8_t *act0,
                                      const int8_t *act1,
                                      const int8_t *act2,
                                      const int8_t *act3,
                                      const int8_t *wt0,
                                      const int8_t *wt1,
                                      const int8_t *wt2,
                                      const int8_t *wt3,
                                      uint32_t n)
{
    uintptr_t a0_addr = (uintptr_t)act0;
    uintptr_t a1_addr = (uintptr_t)act1;
    uintptr_t a2_addr = (uintptr_t)act2;
    uintptr_t a3_addr = (uintptr_t)act3;
    uintptr_t w0_addr = (uintptr_t)wt0;
    uintptr_t w1_addr = (uintptr_t)wt1;
    uintptr_t w2_addr = (uintptr_t)wt2;
    uintptr_t w3_addr = (uintptr_t)wt3;
    uintptr_t out_ptr = (uintptr_t)acc;

    __asm__ __volatile__(
        "mov.m.x   m0, zero, 0xff\n"
        "xor       t0, t0, t0\n"
        "flw.ps    f28, %[off]\n"
        "flw.ps    f27, %[off]\n"
        "fxor.pi   f8,  f8,  f8\n"
        "fxor.pi   f9,  f9,  f9\n"
        "fxor.pi   f10, f10, f10\n"
        "fxor.pi   f11, f11, f11\n"
        "fxor.pi   f12, f12, f12\n"
        "fxor.pi   f13, f13, f13\n"
        "fxor.pi   f14, f14, f14\n"
        "fxor.pi   f15, f15, f15\n"
        "fxor.pi   f16, f16, f16\n"
        "fxor.pi   f17, f17, f17\n"
        "fxor.pi   f18, f18, f18\n"
        "fxor.pi   f19, f19, f19\n"
        "fxor.pi   f20, f20, f20\n"
        "fxor.pi   f21, f21, f21\n"
        "fxor.pi   f22, f22, f22\n"
        "fxor.pi   f23, f23, f23\n"
        "1:\n"
        "addi      t0, t0, 8\n"
        "ble       %[count], t0, 2f\n"
        "fgb.ps    f0, f28(%[a0_addr])\n"
        "fgb.ps    f1, f28(%[a1_addr])\n"
        "fgb.ps    f2, f28(%[a2_addr])\n"
        "fgb.ps    f3, f28(%[a3_addr])\n"
        "fgb.ps    f4, f27(%[w0_addr])\n"
        "fgb.ps    f5, f27(%[w1_addr])\n"
        "fgb.ps    f6, f27(%[w2_addr])\n"
        "fgb.ps    f7, f27(%[w3_addr])\n"
        "fmul.pi   f24, f0, f4\n"
        "fadd.pi   f8,  f8,  f24\n"
        "fmul.pi   f24, f0, f5\n"
        "fadd.pi   f9,  f9,  f24\n"
        "fmul.pi   f24, f0, f6\n"
        "fadd.pi   f10, f10, f24\n"
        "fmul.pi   f24, f0, f7\n"
        "fadd.pi   f11, f11, f24\n"
        "fmul.pi   f24, f1, f4\n"
        "fadd.pi   f12, f12, f24\n"
        "fmul.pi   f24, f1, f5\n"
        "fadd.pi   f13, f13, f24\n"
        "fmul.pi   f24, f1, f6\n"
        "fadd.pi   f14, f14, f24\n"
        "fmul.pi   f24, f1, f7\n"
        "fadd.pi   f15, f15, f24\n"
        "fmul.pi   f24, f2, f4\n"
        "fadd.pi   f16, f16, f24\n"
        "fmul.pi   f24, f2, f5\n"
        "fadd.pi   f17, f17, f24\n"
        "fmul.pi   f24, f2, f6\n"
        "fadd.pi   f18, f18, f24\n"
        "fmul.pi   f24, f2, f7\n"
        "fadd.pi   f19, f19, f24\n"
        "fmul.pi   f24, f3, f4\n"
        "fadd.pi   f20, f20, f24\n"
        "fmul.pi   f24, f3, f5\n"
        "fadd.pi   f21, f21, f24\n"
        "fmul.pi   f24, f3, f6\n"
        "fadd.pi   f22, f22, f24\n"
        "fmul.pi   f24, f3, f7\n"
        "fadd.pi   f23, f23, f24\n"
        "addi      %[a0_addr], %[a0_addr], 8\n"
        "addi      %[a1_addr], %[a1_addr], 8\n"
        "addi      %[a2_addr], %[a2_addr], 8\n"
        "addi      %[a3_addr], %[a3_addr], 8\n"
        "addi      %[w0_addr], %[w0_addr], 8\n"
        "addi      %[w1_addr], %[w1_addr], 8\n"
        "addi      %[w2_addr], %[w2_addr], 8\n"
        "addi      %[w3_addr], %[w3_addr], 8\n"
        "j         1b\n"
        "2:\n"
        "addi      t0, t0, -8\n"
        "sub       t1, %[count], t0\n"
        "beq       t1, zero, 3f\n"
        "addi      t2, zero, 1\n"
        "sll       t2, t2, t1\n"
        "addi      t2, t2, -1\n"
        "mov.m.x   m0, t2, 0\n"
        "fgb.ps    f0, f28(%[a0_addr])\n"
        "fgb.ps    f1, f28(%[a1_addr])\n"
        "fgb.ps    f2, f28(%[a2_addr])\n"
        "fgb.ps    f3, f28(%[a3_addr])\n"
        "fgb.ps    f4, f27(%[w0_addr])\n"
        "fgb.ps    f5, f27(%[w1_addr])\n"
        "fgb.ps    f6, f27(%[w2_addr])\n"
        "fgb.ps    f7, f27(%[w3_addr])\n"
        "fmul.pi   f24, f0, f4\n"
        "fadd.pi   f8,  f8,  f24\n"
        "fmul.pi   f24, f0, f5\n"
        "fadd.pi   f9,  f9,  f24\n"
        "fmul.pi   f24, f0, f6\n"
        "fadd.pi   f10, f10, f24\n"
        "fmul.pi   f24, f0, f7\n"
        "fadd.pi   f11, f11, f24\n"
        "fmul.pi   f24, f1, f4\n"
        "fadd.pi   f12, f12, f24\n"
        "fmul.pi   f24, f1, f5\n"
        "fadd.pi   f13, f13, f24\n"
        "fmul.pi   f24, f1, f6\n"
        "fadd.pi   f14, f14, f24\n"
        "fmul.pi   f24, f1, f7\n"
        "fadd.pi   f15, f15, f24\n"
        "fmul.pi   f24, f2, f4\n"
        "fadd.pi   f16, f16, f24\n"
        "fmul.pi   f24, f2, f5\n"
        "fadd.pi   f17, f17, f24\n"
        "fmul.pi   f24, f2, f6\n"
        "fadd.pi   f18, f18, f24\n"
        "fmul.pi   f24, f2, f7\n"
        "fadd.pi   f19, f19, f24\n"
        "fmul.pi   f24, f3, f4\n"
        "fadd.pi   f20, f20, f24\n"
        "fmul.pi   f24, f3, f5\n"
        "fadd.pi   f21, f21, f24\n"
        "fmul.pi   f24, f3, f6\n"
        "fadd.pi   f22, f22, f24\n"
        "fmul.pi   f24, f3, f7\n"
        "fadd.pi   f23, f23, f24\n"
        "3:\n"
        "fswizz.ps f24, f8, 0xe\n"
        "fadd.pi   f8, f8, f24\n"
        "fswizz.ps f24, f8, 0x1\n"
        "fadd.pi   f8, f8, f24\n"
        "fmvs.x.ps t1, f8, 0x0\n"
        "fmvs.x.ps t2, f8, 0x4\n"
        "add       t1, t1, t2\n"
        "lw        t2, 0(%[out_ptr])\n"
        "add       t1, t1, t2\n"
        "sw        t1, 0(%[out_ptr])\n"
        "fswizz.ps f24, f9, 0xe\n"
        "fadd.pi   f9, f9, f24\n"
        "fswizz.ps f24, f9, 0x1\n"
        "fadd.pi   f9, f9, f24\n"
        "fmvs.x.ps t1, f9, 0x0\n"
        "fmvs.x.ps t2, f9, 0x4\n"
        "add       t1, t1, t2\n"
        "lw        t2, 4(%[out_ptr])\n"
        "add       t1, t1, t2\n"
        "sw        t1, 4(%[out_ptr])\n"
        "fswizz.ps f24, f10, 0xe\n"
        "fadd.pi   f10, f10, f24\n"
        "fswizz.ps f24, f10, 0x1\n"
        "fadd.pi   f10, f10, f24\n"
        "fmvs.x.ps t1, f10, 0x0\n"
        "fmvs.x.ps t2, f10, 0x4\n"
        "add       t1, t1, t2\n"
        "lw        t2, 8(%[out_ptr])\n"
        "add       t1, t1, t2\n"
        "sw        t1, 8(%[out_ptr])\n"
        "fswizz.ps f24, f11, 0xe\n"
        "fadd.pi   f11, f11, f24\n"
        "fswizz.ps f24, f11, 0x1\n"
        "fadd.pi   f11, f11, f24\n"
        "fmvs.x.ps t1, f11, 0x0\n"
        "fmvs.x.ps t2, f11, 0x4\n"
        "add       t1, t1, t2\n"
        "lw        t2, 12(%[out_ptr])\n"
        "add       t1, t1, t2\n"
        "sw        t1, 12(%[out_ptr])\n"
        "fswizz.ps f24, f12, 0xe\n"
        "fadd.pi   f12, f12, f24\n"
        "fswizz.ps f24, f12, 0x1\n"
        "fadd.pi   f12, f12, f24\n"
        "fmvs.x.ps t1, f12, 0x0\n"
        "fmvs.x.ps t2, f12, 0x4\n"
        "add       t1, t1, t2\n"
        "lw        t2, 16(%[out_ptr])\n"
        "add       t1, t1, t2\n"
        "sw        t1, 16(%[out_ptr])\n"
        "fswizz.ps f24, f13, 0xe\n"
        "fadd.pi   f13, f13, f24\n"
        "fswizz.ps f24, f13, 0x1\n"
        "fadd.pi   f13, f13, f24\n"
        "fmvs.x.ps t1, f13, 0x0\n"
        "fmvs.x.ps t2, f13, 0x4\n"
        "add       t1, t1, t2\n"
        "lw        t2, 20(%[out_ptr])\n"
        "add       t1, t1, t2\n"
        "sw        t1, 20(%[out_ptr])\n"
        "fswizz.ps f24, f14, 0xe\n"
        "fadd.pi   f14, f14, f24\n"
        "fswizz.ps f24, f14, 0x1\n"
        "fadd.pi   f14, f14, f24\n"
        "fmvs.x.ps t1, f14, 0x0\n"
        "fmvs.x.ps t2, f14, 0x4\n"
        "add       t1, t1, t2\n"
        "lw        t2, 24(%[out_ptr])\n"
        "add       t1, t1, t2\n"
        "sw        t1, 24(%[out_ptr])\n"
        "fswizz.ps f24, f15, 0xe\n"
        "fadd.pi   f15, f15, f24\n"
        "fswizz.ps f24, f15, 0x1\n"
        "fadd.pi   f15, f15, f24\n"
        "fmvs.x.ps t1, f15, 0x0\n"
        "fmvs.x.ps t2, f15, 0x4\n"
        "add       t1, t1, t2\n"
        "lw        t2, 28(%[out_ptr])\n"
        "add       t1, t1, t2\n"
        "sw        t1, 28(%[out_ptr])\n"
        "fswizz.ps f24, f16, 0xe\n"
        "fadd.pi   f16, f16, f24\n"
        "fswizz.ps f24, f16, 0x1\n"
        "fadd.pi   f16, f16, f24\n"
        "fmvs.x.ps t1, f16, 0x0\n"
        "fmvs.x.ps t2, f16, 0x4\n"
        "add       t1, t1, t2\n"
        "lw        t2, 32(%[out_ptr])\n"
        "add       t1, t1, t2\n"
        "sw        t1, 32(%[out_ptr])\n"
        "fswizz.ps f24, f17, 0xe\n"
        "fadd.pi   f17, f17, f24\n"
        "fswizz.ps f24, f17, 0x1\n"
        "fadd.pi   f17, f17, f24\n"
        "fmvs.x.ps t1, f17, 0x0\n"
        "fmvs.x.ps t2, f17, 0x4\n"
        "add       t1, t1, t2\n"
        "lw        t2, 36(%[out_ptr])\n"
        "add       t1, t1, t2\n"
        "sw        t1, 36(%[out_ptr])\n"
        "fswizz.ps f24, f18, 0xe\n"
        "fadd.pi   f18, f18, f24\n"
        "fswizz.ps f24, f18, 0x1\n"
        "fadd.pi   f18, f18, f24\n"
        "fmvs.x.ps t1, f18, 0x0\n"
        "fmvs.x.ps t2, f18, 0x4\n"
        "add       t1, t1, t2\n"
        "lw        t2, 40(%[out_ptr])\n"
        "add       t1, t1, t2\n"
        "sw        t1, 40(%[out_ptr])\n"
        "fswizz.ps f24, f19, 0xe\n"
        "fadd.pi   f19, f19, f24\n"
        "fswizz.ps f24, f19, 0x1\n"
        "fadd.pi   f19, f19, f24\n"
        "fmvs.x.ps t1, f19, 0x0\n"
        "fmvs.x.ps t2, f19, 0x4\n"
        "add       t1, t1, t2\n"
        "lw        t2, 44(%[out_ptr])\n"
        "add       t1, t1, t2\n"
        "sw        t1, 44(%[out_ptr])\n"
        "fswizz.ps f24, f20, 0xe\n"
        "fadd.pi   f20, f20, f24\n"
        "fswizz.ps f24, f20, 0x1\n"
        "fadd.pi   f20, f20, f24\n"
        "fmvs.x.ps t1, f20, 0x0\n"
        "fmvs.x.ps t2, f20, 0x4\n"
        "add       t1, t1, t2\n"
        "lw        t2, 48(%[out_ptr])\n"
        "add       t1, t1, t2\n"
        "sw        t1, 48(%[out_ptr])\n"
        "fswizz.ps f24, f21, 0xe\n"
        "fadd.pi   f21, f21, f24\n"
        "fswizz.ps f24, f21, 0x1\n"
        "fadd.pi   f21, f21, f24\n"
        "fmvs.x.ps t1, f21, 0x0\n"
        "fmvs.x.ps t2, f21, 0x4\n"
        "add       t1, t1, t2\n"
        "lw        t2, 52(%[out_ptr])\n"
        "add       t1, t1, t2\n"
        "sw        t1, 52(%[out_ptr])\n"
        "fswizz.ps f24, f22, 0xe\n"
        "fadd.pi   f22, f22, f24\n"
        "fswizz.ps f24, f22, 0x1\n"
        "fadd.pi   f22, f22, f24\n"
        "fmvs.x.ps t1, f22, 0x0\n"
        "fmvs.x.ps t2, f22, 0x4\n"
        "add       t1, t1, t2\n"
        "lw        t2, 56(%[out_ptr])\n"
        "add       t1, t1, t2\n"
        "sw        t1, 56(%[out_ptr])\n"
        "fswizz.ps f24, f23, 0xe\n"
        "fadd.pi   f23, f23, f24\n"
        "fswizz.ps f24, f23, 0x1\n"
        "fadd.pi   f23, f23, f24\n"
        "fmvs.x.ps t1, f23, 0x0\n"
        "fmvs.x.ps t2, f23, 0x4\n"
        "add       t1, t1, t2\n"
        "lw        t2, 60(%[out_ptr])\n"
        "add       t1, t1, t2\n"
        "sw        t1, 60(%[out_ptr])\n"
        :
          [a0_addr] "+&r" (a0_addr), [a1_addr] "+&r" (a1_addr),
          [a2_addr] "+&r" (a2_addr), [a3_addr] "+&r" (a3_addr),
          [w0_addr] "+&r" (w0_addr), [w1_addr] "+&r" (w1_addr),
          [w2_addr] "+&r" (w2_addr), [w3_addr] "+&r" (w3_addr)
        : [count] "r" (n),
          [out_ptr] "r" (out_ptr),
          [off] "m" (*(&byte_lane_idx))
        : "memory", "t0", "t1", "t2",
          "f0", "f1", "f2", "f3", "f4", "f5", "f6", "f7",
          "f8", "f9", "f10", "f11", "f12", "f13", "f14", "f15",
          "f16", "f17", "f18", "f19", "f20", "f21", "f22", "f23",
          "f24", "f27", "f28");
}

static inline void requant_store4_lut(const int32_t *acc,
                                      const float *scale4,
                                      int8_t *dst,
                                      const uint8_t *lut)
{
    float scale8[8] __attribute__((aligned(32))) = {
        scale4[0], scale4[1], scale4[2], scale4[3],
        scale4[0], scale4[1], scale4[2], scale4[3],
    };
    uint32_t mask = 0x0fu;

    if (lut) {
        __asm__ __volatile__(
            "mov.m.x   m0, %[mask], 0\n"
            "flw.ps    f31, %[idx]\n"
            "flw.ps    f0, 0(%[acc])\n"
            "flw.ps    f1, 0(%[scale])\n"
            "fcvt.ps.pw f0, f0\n"
            "fmul.ps   f0, f0, f1\n"
            "fcvt.pw.ps f0, f0\n"
            "fsat8.pi  f0, f0\n"
            "fandi.pi  f0, f0, 0xff\n"
            "fgb.ps    f1, f0(%[lut])\n"
            "fscb.ps   f1, f31(%[dst])\n"
            :
            : [mask] "r"(mask),
              [idx] "m"(*(&byte_lane_idx)),
              [acc] "r"(acc),
              [scale] "r"(scale8),
              [lut] "r"(lut),
              [dst] "r"(dst)
            : "f0", "f1", "f31", "memory");
    } else {
        __asm__ __volatile__(
            "mov.m.x   m0, %[mask], 0\n"
            "flw.ps    f31, %[idx]\n"
            "flw.ps    f0, 0(%[acc])\n"
            "flw.ps    f1, 0(%[scale])\n"
            "fcvt.ps.pw f0, f0\n"
            "fmul.ps   f0, f0, f1\n"
            "fcvt.pw.ps f0, f0\n"
            "fsat8.pi  f0, f0\n"
            "fscb.ps   f0, f31(%[dst])\n"
            :
            : [mask] "r"(mask),
              [idx] "m"(*(&byte_lane_idx)),
              [acc] "r"(acc),
              [scale] "r"(scale8),
              [dst] "r"(dst)
            : "f0", "f1", "f31", "memory");
    }
}

static void apply_lut_block_rows(const yolov5n_layer_meta_t *ld,
                                 int8_t *dst, uint32_t out_stride,
                                 uint32_t rows)
{
    if (!ld->lut)
        return;

    for (uint32_t r = 0; r < rows; ++r) {
        int8_t *row = dst + r * out_stride;

        for (uint32_t c = 0; c < OC_BLOCK; c += 8) {
            int8_t *ptr = row + c;

            __asm__ __volatile__(
                "flw.ps    f31, %[idx]\n"
                "fgb.ps    f0, f31(%[src])\n"
                "fandi.pi  f0, f0, 0xff\n"
                "fgb.ps    f1, f0(%[lut])\n"
                "fscb.ps   f1, f31(%[dst])\n"
                :
                : [idx] "m"(*(&byte_lane_idx)),
                  [src] "r"(ptr),
                  [lut] "r"(ld->lut),
                  [dst] "r"(ptr)
                : "f0", "f1", "f31", "memory");
        }
    }
}

static int tensor_finish_block_rows(const tensor_layer_t *layer,
                                    int8_t *out_blk, uint32_t out_stride,
                                    uint32_t oc_blk, uint32_t rows)
{
    tensor_wait(TENSOR_FMA_WAIT);
    if (get_tensor_error()) {
        clear_tensor_error();
        return -1;
    }

    load_meta_line(SCP_META_LINE, layer->bias_blk[oc_blk]);
    tensor_quant(0, 3, rows - 1u, SCP_META_LINE,
                 QUANT_LAST_TRANS, QUANT_LAST_TRANS, QUANT_LAST_TRANS,
                 QUANT_LAST_TRANS, QUANT_LAST_TRANS, QUANT_LAST_TRANS,
                 QUANT_LAST_TRANS, QUANT_LAST_TRANS,
                 QUANT_INT32_TO_FP32, QUANT_INT32_ADD_ROW);
    tensor_wait(TENSOR_QUANT_WAIT);

    load_meta_line(SCP_META_LINE, layer->scale_blk[oc_blk]);
    tensor_quant(0, 3, rows - 1u, SCP_META_LINE,
                 QUANT_LAST_TRANS, QUANT_LAST_TRANS, QUANT_LAST_TRANS,
                 QUANT_LAST_TRANS, QUANT_LAST_TRANS, QUANT_LAST_TRANS,
                 QUANT_PACK_128B, QUANT_SATINT8,
                 QUANT_FP32_TO_INT32, QUANT_FP32_MUL_ROW);
    tensor_wait(TENSOR_QUANT_WAIT);

    if (get_tensor_error()) {
        clear_tensor_error();
        return -1;
    }

    tensor_store(1, 0, 0, rows - 1u, (uint64_t)out_blk, 0, out_stride);
    tensor_wait(TENSOR_STORE_WAIT);
    if (get_tensor_error()) {
        clear_tensor_error();
        return -1;
    }

    apply_lut_block_rows(layer->ld, out_blk, out_stride, rows);
    return 0;
}

static int run_tensor_block_rows(const tensor_layer_t *layer,
                                 const int8_t *in_blk, uint32_t in_stride,
                                 int8_t *out_blk, uint32_t out_stride,
                                 uint32_t oc_blk, uint32_t rows)
{
    uint32_t acols = layer->k_block / 4u - 1u;

    for (uint32_t k_blk = 0; k_blk < layer->k_blocks; ++k_blk) {
        const int8_t *bblk = tl8_wblk(layer, oc_blk, k_blk);
        const int last_kblk = (k_blk + 1u == layer->k_blocks);

        tensor_load(0, 0, SCP_A_START, 0, 0,
                    (uint64_t)(in_blk + (size_t)k_blk * layer->k_block),
                    0, rows - 1u, in_stride, 0);
        tensor_load(0, 0, SCP_B_START, 1, 0, (uint64_t)bblk, 0,
                    PIX_BLOCK - 1, layer->tl8_row_stride, 1);
        tensor_wait(TENSOR_LOAD_WAIT_0);
        tensor_wait(TENSOR_LOAD_WAIT_1);
        tensor_fma(0, 3, rows - 1u, acols, 0,
                   last_kblk, 0, 0, 0, SCP_B_START, SCP_A_START, 3,
                   k_blk == 0);
    }

    return tensor_finish_block_rows(layer, out_blk, out_stride, oc_blk, rows);
}

static int run_tensor_block(const tensor_layer_t *layer,
                            const int8_t *in_blk, uint32_t in_stride,
                            int8_t *out_blk, uint32_t out_stride,
                            uint32_t oc_blk)
{
    return run_tensor_block_rows(layer, in_blk, in_stride, out_blk,
                                 out_stride, oc_blk, PIX_BLOCK);
}

static __attribute__((unused))
int run_conv1x1_layer_exact32(const tensor_layer_t *layer,
                              const int8_t *src,
                              int8_t *dst, uint32_t dst_c)
{
    const yolov5n_layer_meta_t *ld = layer->ld;
    uint32_t in_stride = INPUT_W * BRANCH_C;
    uint32_t out_stride = INPUT_W * dst_c;
    uint32_t acols = layer->k_inner / 4u - 1u;

    if (ld->c_in != 32u || ld->k_out != 32u || ld->k_h != 1u ||
        ld->k_w != 1u || dst_c != 32u || !ld->lut)
        return -1;

    for (uint32_t x = 0; x < INPUT_W; ++x) {
        uint32_t aoffset = hwc32_aoffset(x);

        for (uint32_t y0 = 0; y0 < INPUT_H; y0 += PIX_BLOCK) {
            uint32_t rows = INPUT_H - y0;
            uint64_t abase = hwc32_pair_base(src, y0, x);

            if (rows > PIX_BLOCK)
                rows = PIX_BLOCK;

            for (uint32_t oc_blk = 0; oc_blk < layer->oc_blocks; ++oc_blk) {
                const int8_t *bblk = tl8_wblk(layer, oc_blk, 0);
                int8_t *out_blk =
                    dst + (((size_t)y0 * INPUT_W + x) * dst_c) + oc_blk * OC_BLOCK;

                tensor_load(0, 0, SCP_A_START, 0, 0, abase, 0, rows - 1u,
                            in_stride, 0);
                tensor_load(0, 0, SCP_B_START, 1, 0, (uint64_t)bblk, 0,
                            layer->k_block / 4u - 1u, layer->tl8_row_stride, 1);
                tensor_wait(TENSOR_LOAD_WAIT_0);
                tensor_wait(TENSOR_LOAD_WAIT_1);

                tensor_fma(0, 3, rows - 1u, acols, aoffset,
                           1, 0, 0, 0, SCP_B_START, SCP_A_START, 3, 1);

                if (tensor_finish_block_rows(layer, out_blk, out_stride,
                                             oc_blk, rows))
                    return -1;
            }
        }
    }

    return 0;
}

static __attribute__((unused))
int run_conv3x3_layer_exact32(const tensor_layer_t *layer,
                              const int8_t *src,
                              int8_t *dst, uint32_t dst_c)
{
    const yolov5n_layer_meta_t *ld = layer->ld;
    uint32_t in_stride = INPUT_W * BRANCH_C;
    uint32_t out_stride = INPUT_W * dst_c;
    uint32_t acols = layer->k_block / 4u - 1u;

    if (ld->c_in != 32u || ld->k_out != 32u || ld->k_h != 3u ||
        ld->k_w != 3u || dst_c != 32u || !ld->lut)
        return -1;

    convolution_size(1, INPUT_H, 0, 1);

    for (uint32_t x = 0; x < INPUT_W; ++x) {
        int last_tap = -1;

        for (int tap = 0; tap < 9; ++tap) {
            int kx = tap % 3 - 1;
            int px = (int)x + kx;

            if (px >= 0 && px < (int)INPUT_W)
                last_tap = tap;
        }

        for (uint32_t y0 = 0; y0 < INPUT_H; y0 += PIX_BLOCK) {
            uint32_t rows = INPUT_H - y0;

            if (rows > PIX_BLOCK)
                rows = PIX_BLOCK;

            for (uint32_t oc_blk = 0; oc_blk < layer->oc_blocks; ++oc_blk) {
                int first_pass = 1;
                int8_t *out_blk =
                    dst + (((size_t)y0 * INPUT_W + x) * dst_c) + oc_blk * OC_BLOCK;

                for (int tap = 0; tap < 9; ++tap) {
                    int ky = tap / 3 - 1;
                    int kx = tap % 3 - 1;
                    int px = (int)x + kx;
                    int row_start = (int)y0 + ky;
                    const int8_t *bblk;
                    uint64_t abase;
                    uint32_t aoffset;

                    if (px < 0 || px >= (int)INPUT_W)
                        continue;

                    bblk = tl8_wblk(layer, oc_blk, (uint32_t)tap);
                    abase = hwc32_pair_base_signed(src, row_start, (uint32_t)px);
                    aoffset = hwc32_aoffset((uint32_t)px);

                    convolution_ctrl((uint64_t)(uint16_t)row_start, 0);
                    tensor_load(1, 0, SCP_A_START, 0, 0, abase, 0, rows - 1u,
                                in_stride, 0);
                    tensor_load(0, 0, SCP_B_START, 1, 0, (uint64_t)bblk, 0,
                                layer->k_block / 4u - 1u, layer->tl8_row_stride, 1);
                    tensor_wait(TENSOR_LOAD_WAIT_0);
                    tensor_wait(TENSOR_LOAD_WAIT_1);

                    tensor_fma(1, 3, rows - 1u, acols, aoffset,
                               tap == last_tap, 0, 0, 0,
                               SCP_B_START, SCP_A_START, 3, first_pass);
                    first_pass = 0;
                }

                if (tensor_finish_block_rows(layer, out_blk, out_stride,
                                             oc_blk, rows))
                    return -1;
            }
        }
    }

    return 0;
}

static int run_conv1x1_layer(const tensor_layer_t *layer,
                             const int8_t *src, uint32_t src_c,
                             int8_t *dst, uint32_t dst_c)
{
    for (size_t pix = 0; pix < NR_PIX; pix += PIX_BLOCK) {
        const int8_t *in_blk = src + pix * src_c;
        uint32_t in_stride = src_c;

        for (uint32_t oc_blk = 0; oc_blk < layer->oc_blocks; ++oc_blk) {
            if (run_tensor_block(layer, in_blk, in_stride,
                                 dst + pix * dst_c + oc_blk * OC_BLOCK,
                                 dst_c, oc_blk))
                return -1;
        }
    }
    return 0;
}

static void pack_3x3_block(const int8_t *src, uint32_t src_c,
                           size_t pix0, int8_t *dst_rowblk,
                           uint32_t row_stride)
{
    for (uint32_t r = 0; r < PIX_BLOCK; ++r) {
        size_t pix = pix0 + r;
        uint32_t y = (uint32_t)(pix / INPUT_W);
        uint32_t x = (uint32_t)(pix % INPUT_W);
        int8_t *dst_row = dst_rowblk + (size_t)r * row_stride;

        vec_zero_bytes(dst_row, row_stride);

        for (int32_t ky = -1; ky <= 1; ++ky) {
            int32_t iy = (int32_t)y + ky;

            for (int32_t kx = -1; kx <= 1; ++kx) {
                int32_t ix = (int32_t)x + kx;
                uint32_t tap = (uint32_t)((ky + 1) * 3 + (kx + 1));
                int8_t *tap_dst = dst_row + tap * src_c;

                if (iy < 0 || iy >= (int32_t)INPUT_H ||
                    ix < 0 || ix >= (int32_t)INPUT_W) {
                    continue;
                } else {
                    const int8_t *src_px =
                        src + (((size_t)(uint32_t)iy * INPUT_W + (uint32_t)ix) * src_c);
                    vec_copy_bytes(tap_dst, src_px, src_c);
                }
            }
        }
    }
}

static int run_conv3x3_layer(const tensor_layer_t *layer,
                             const int8_t *src, uint32_t src_c,
                             int8_t *dst, uint32_t dst_c)
{
    if (layer->k_inner != 9u * src_c || layer->k_padded < layer->k_inner)
        return -1;

    for (size_t pix = 0; pix < NR_PIX; pix += PIX_BLOCK) {
        pack_3x3_block(src, src_c, pix, patch3x3_buf, layer->k_padded);

        for (uint32_t oc_blk = 0; oc_blk < layer->oc_blocks; ++oc_blk) {
            if (run_tensor_block(layer, patch3x3_buf, layer->k_padded,
                                 dst + pix * dst_c + oc_blk * OC_BLOCK,
                                 dst_c, oc_blk))
                return -1;
        }
    }
    return 0;
}

static void pack_3x3s2_block(const int8_t *src, uint32_t src_c,
                             size_t pix0, int8_t *dst_rowblk,
                             uint32_t row_stride)
{
    for (uint32_t r = 0; r < PIX_BLOCK; ++r) {
        size_t pix = pix0 + r;
        uint32_t y = (uint32_t)(pix / INPUT_W);
        uint32_t x = (uint32_t)(pix % INPUT_W);
        int8_t *dst_row = dst_rowblk + (size_t)r * row_stride;

        vec_zero_bytes(dst_row, row_stride);

        for (int32_t ky = -1; ky <= 1; ++ky) {
            int32_t iy = (int32_t)(y * 2u) + ky;

            for (int32_t kx = -1; kx <= 1; ++kx) {
                int32_t ix = (int32_t)(x * 2u) + kx;
                uint32_t tap = (uint32_t)((ky + 1) * 3 + (kx + 1));
                int8_t *tap_dst = dst_row + tap * src_c;

                if (iy < 0 || iy >= (int32_t)P3_H ||
                    ix < 0 || ix >= (int32_t)P3_W) {
                    continue;
                } else {
                    const int8_t *src_px =
                        src + (((size_t)(uint32_t)iy * P3_W + (uint32_t)ix) * src_c);
                    vec_copy_bytes(tap_dst, src_px, src_c);
                }
            }
        }
    }
}

static int run_conv3x3s2_layer(const tensor_layer_t *layer,
                               const int8_t *src, uint32_t src_c,
                               int8_t *dst, uint32_t dst_c)
{
    if (layer->k_inner != 9u * src_c || layer->k_padded < layer->k_inner)
        return -1;

    for (size_t pix = 0; pix < NR_PIX; pix += PIX_BLOCK) {
        pack_3x3s2_block(src, src_c, pix, patch3x3s2_buf, layer->k_padded);

        for (uint32_t oc_blk = 0; oc_blk < layer->oc_blocks; ++oc_blk) {
            if (run_tensor_block(layer, patch3x3s2_buf, layer->k_padded,
                                 dst + pix * dst_c + oc_blk * OC_BLOCK,
                                 dst_c, oc_blk))
                return -1;
        }
    }
    return 0;
}

static void add_residual_block(const int8_t *skip_px, int8_t *body_px, size_t n)
{
    ensure_residual_lut();
    const int8_t *lut = residual_lut;

    for (size_t off = 0; off < n; off += 8) {
        const int8_t *src = skip_px + off;
        int8_t *dst = body_px + off;

        __asm__ __volatile__(
            "flw.ps    f31, %[idx]\n"
            "fgb.ps    f0, f31(%[src])\n"
            "fandi.pi  f0, f0, 0xff\n"
            "fgb.ps    f1, f0(%[lut])\n"
            "fgb.ps    f2, f31(%[dst])\n"
            "fadd.pi   f1, f1, f2\n"
            "fsat8.pi  f1, f1\n"
            "fscb.ps   f1, f31(%[dst])\n"
            :
            : [idx] "m"(*(&byte_lane_idx)),
              [src] "r"(src),
              [lut] "r"(lut),
              [dst] "r"(dst)
            : "f0", "f1", "f2", "f31", "memory");
    }
}

static void concat_input(const int8_t *down, const int8_t *skip, int8_t *dst)
{
    for (size_t pix = 0; pix < NR_PIX; ++pix) {
        int8_t *dst_px = dst + pix * CAT0_C;
        const int8_t *down_px = down + pix * DOWN_C;
        const int8_t *skip_px = skip + pix * SKIP_C;

        vec_copy_bytes(dst_px, down_px, DOWN_C);
        vec_copy_bytes(dst_px + DOWN_C, skip_px, SKIP_C);
    }
}

static void concat_branch(const int8_t *a, const int8_t *b, int8_t *dst)
{
    for (size_t pix = 0; pix < NR_PIX; ++pix) {
        int8_t *dst_px = dst + pix * CAT1_C;
        const int8_t *a_px = a + pix * BRANCH_C;
        const int8_t *b_px = b + pix * BRANCH_C;

        vec_copy_bytes(dst_px, a_px, BRANCH_C);
        vec_copy_bytes(dst_px + BRANCH_C, b_px, BRANCH_C);
    }
}

tpa_op_t yolo_full_p9_start(void)
{
    yolo_full_p9_ws_t *w = tpa_ws();

    return tpa_recv(tpa_chan(0), (void **)&w->p4, &w->p4_len, yolo_full_p9_recv_skip);
}

tpa_op_t yolo_full_p9_recv_skip(void)
{
    yolo_full_p9_ws_t *w = tpa_ws();

    return tpa_recv(tpa_chan(1), (void **)&w->skip, &w->skip_len, yolo_full_p9_run);
}

tpa_op_t yolo_full_p9_run(void)
{
    yolo_full_p9_ws_t *w = tpa_ws();
    int8_t *out_buf = yolov5n_send_buf(tpa_chan(2));

    YV5N_REQUIRE_STOP(w->p4_len == P3_H * P3_W * P3_C);
    YV5N_REQUIRE_STOP(w->skip_len == SKIP_H * SKIP_W * SKIP_C);

    yolov5n_arena_begin(YV5N_P9_SCRATCH_PEAK_BYTES);
    down_buf = yolov5n_arena_alloc(NR_PIX * DOWN_C, 64u);
    cat0_buf = yolov5n_arena_alloc(NR_PIX * CAT0_C, 64u);
    mid_buf = yolov5n_arena_alloc(NR_PIX * BRANCH_C, 64u);
    a_buf = yolov5n_arena_alloc(NR_PIX * BRANCH_C, 64u);
    b_buf = yolov5n_arena_alloc(NR_PIX * BRANCH_C, 64u);
    cat1_buf = yolov5n_arena_alloc(NR_PIX * CAT1_C, 64u);
    patch3x3_buf = yolov5n_arena_alloc(PIX_BLOCK * 1152u, 64u);
    patch3x3s2_buf = yolov5n_arena_alloc(PIX_BLOCK * 1152u, 64u);
    YV5N_REQUIRE_STOP(down_buf && cat0_buf && mid_buf && a_buf &&
                      b_buf && cat1_buf && patch3x3_buf && patch3x3s2_buf);

    enable_tensor_scratchpad();
    YV5N_TRY_STOP(run_conv3x3s2_layer(&layer51, w->p4, P3_C, down_buf, DOWN_C));
    concat_input(down_buf, w->skip, cat0_buf);
    YV5N_TRY_STOP(run_conv1x1_layer(&layer52, cat0_buf, CAT0_C, a_buf, BRANCH_C));
    YV5N_TRY_STOP(run_conv1x1_layer(&layer53, a_buf, BRANCH_C, mid_buf, BRANCH_C));
    YV5N_TRY_STOP(run_conv3x3_layer(&layer54, mid_buf, BRANCH_C, b_buf, BRANCH_C));
    add_residual_block(a_buf, b_buf, NR_PIX * BRANCH_C);
    YV5N_TRY_STOP(run_conv1x1_layer(&layer55, cat0_buf, CAT0_C, a_buf, BRANCH_C));
    concat_branch(b_buf, a_buf, cat1_buf);
    YV5N_TRY_STOP(run_conv1x1_layer(&layer56, cat1_buf, CAT1_C, out_buf, OUTPUT_C));

    return tpa_send(tpa_chan(2), out_buf, NR_PIX * OUTPUT_C, yolo_full_p9_done);
}

tpa_op_t yolo_full_p9_done(void)
{
    return tpa_stop();
}
