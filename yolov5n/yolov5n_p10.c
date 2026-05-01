#include <stddef.h>
#include <stdint.h>

#include <etsoc/isa/cacheops.h>
#include <etsoc/isa/tensors.h>

#include "test.h"
#include "tpa/tpa.h"
#include "yolov5n_common.h"
#include "yolov5n_demo.h"

#include "generated/yolov5n_l61_tensor_weights.h"
#include "generated/yolov5n_l62_tensor_weights.h"
#include "generated/yolov5n_l71_tensor_weights.h"
#include "generated/yolov5n_l67_tensor_weights.h"
#include "generated/yolov5n_l68_tensor_weights.h"
#include "generated/yolov5n_l74_tensor_weights.h"
#include "yolov5n_luts.h"

#define INPUT_H    20u
#define INPUT_W    20u
#define INPUT_C    256u
#define NR_PIX     (INPUT_H * INPUT_W)
#define PIX_BLOCK  16u
#define OC_BLOCK   16u
#define BOX_C      64u
#define CLS_C      80u
#define YV5N_P10_ACC_BYTES    (sizeof(int32_t) * PIX_BLOCK * OC_BLOCK)
#define YV5N_P10_PACKED_BYTES (PIX_BLOCK * 128u)
#define YV5N_P10_PATCH_BYTES  (PIX_BLOCK * 768u)
#define YV5N_P10_SCRATCH_PEAK_BYTES \
    (YV5N_ARENA_STEP(0u, NR_PIX * CLS_C, 64u) + \
     (NR_PIX * CLS_C) + \
     YV5N_P10_ACC_BYTES + \
     YV5N_P10_PACKED_BYTES + \
     YV5N_P10_PATCH_BYTES)

#define SCP_A_START    0u
#define SCP_B_START   16u
#define SCP_META_LINE 32u

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
    const int8_t *in;
    uint32_t in_len;
    int8_t *demo_box_out;
    int8_t *demo_cls_out;
} yolo_full_p10_ws_t;

tpa_op_t yolo_full_p10_start(void);
tpa_op_t yolo_full_p10_run(void);
tpa_op_t yolo_full_p10_send_cls(void);
tpa_op_t yolo_full_p10_done(void);

static int8_t *det_stage0_buf;
static int8_t *det_stage1_buf;
/*
 * Keep large scratch buffers out of the hart stack. The TPA runtime loop and
 * the process body share the same small per-hart stack, so multi-kilobyte
 * locals here corrupt runtime state after the process yields back.
 */
static int32_t *acc_blk_scratch;
static int8_t *packed_scratch;
static int8_t *patch_scratch;
static volatile uint32_t tensor_ready;
static const int32_t byte_lane_idx[8] __attribute__((aligned(32))) = {
    0, 1, 2, 3, 4, 5, 6, 7,
};
static const int8_t zero_lane_bytes[8] __attribute__((aligned(8))) = {
    0, 0, 0, 0, 0, 0, 0, 0,
};
#define box1_buf det_stage0_buf
#define box2_buf det_stage1_buf
#define box_buf  det_stage0_buf
#define cls1_buf det_stage0_buf
#define cls2_buf det_stage1_buf
#define cls_buf  det_stage0_buf

TPA_PROC_MEM_META(yolov5n_p10_mem_meta, 510u, YV5N_P10_SCRATCH_PEAK_BYTES);

static const yolov5n_layer_meta_t layer61_meta = {
    .lut = yv5n_model_24_cv2_2_0_lut,
    .c_in = 256u,
    .k_out = 64u,
    .k_h = 3u,
    .k_w = 3u,
    .act_in_scale = 2.37435262e-02f,
    .act_out_scale = 4.73450863e-02f,
};

static const yolov5n_layer_meta_t layer62_meta = {
    .lut = yv5n_model_24_cv2_2_1_lut,
    .c_in = 64u,
    .k_out = 64u,
    .k_h = 3u,
    .k_w = 3u,
    .act_in_scale = 4.73450863e-02f,
    .act_out_scale = 1.64309014e-01f,
};

static const yolov5n_layer_meta_t layer71_meta = {
    .lut = NULL,
    .c_in = 64u,
    .k_out = 64u,
    .k_h = 1u,
    .k_w = 1u,
    .act_in_scale = 1.64309014e-01f,
    .act_out_scale = 6.26516455e-02f,
};

static const yolov5n_layer_meta_t layer67_meta = {
    .lut = yv5n_model_24_cv3_2_0_lut,
    .c_in = 256u,
    .k_out = 80u,
    .k_h = 3u,
    .k_w = 3u,
    .act_in_scale = 2.37435262e-02f,
    .act_out_scale = 2.80130867e-02f,
};

static const yolov5n_layer_meta_t layer68_meta = {
    .lut = yv5n_model_24_cv3_2_1_lut,
    .c_in = 80u,
    .k_out = 80u,
    .k_h = 3u,
    .k_w = 3u,
    .act_in_scale = 2.80130867e-02f,
    .act_out_scale = 1.25205198e-01f,
    .exact_post = 1u,
};

static const yolov5n_layer_meta_t layer74_meta = {
    .lut = NULL,
    .c_in = 80u,
    .k_out = 80u,
    .k_h = 1u,
    .k_w = 1u,
    .act_in_scale = 1.25205198e-01f,
    .act_out_scale = 1.44189159e-01f,
    .exact_post = 1u,
};

static const tensor_layer_t layer61 = {
    .ld = &layer61_meta,
    .tl8_b = yolov5n_l61_tl8_b,
    .bias_blk = yolov5n_l61_bias_blk,
    .scale_blk = yolov5n_l61_scale_blk,
    .oc_blocks = YOLOV5N_L61_TENSOR_OC_BLOCKS,
    .k_inner = YOLOV5N_L61_TENSOR_K_INNER,
    .k_padded = YOLOV5N_L61_TENSOR_K_PADDED,
    .k_block = YOLOV5N_L61_TENSOR_K_BLOCK,
    .k_blocks = YOLOV5N_L61_TENSOR_K_BLOCKS,
    .tl8_row_stride = YOLOV5N_L61_TL8_ROW_STRIDE,
};

static const tensor_layer_t layer62 = {
    .ld = &layer62_meta,
    .tl8_b = yolov5n_l62_tl8_b,
    .bias_blk = yolov5n_l62_bias_blk,
    .scale_blk = yolov5n_l62_scale_blk,
    .oc_blocks = YOLOV5N_L62_TENSOR_OC_BLOCKS,
    .k_inner = YOLOV5N_L62_TENSOR_K_INNER,
    .k_padded = YOLOV5N_L62_TENSOR_K_PADDED,
    .k_block = YOLOV5N_L62_TENSOR_K_BLOCK,
    .k_blocks = YOLOV5N_L62_TENSOR_K_BLOCKS,
    .tl8_row_stride = YOLOV5N_L62_TL8_ROW_STRIDE,
};

static const tensor_layer_t layer71 = {
    .ld = &layer71_meta,
    .tl8_b = yolov5n_l71_tl8_b,
    .bias_blk = yolov5n_l71_bias_blk,
    .scale_blk = yolov5n_l71_scale_blk,
    .oc_blocks = YOLOV5N_L71_TENSOR_OC_BLOCKS,
    .k_inner = YOLOV5N_L71_TENSOR_K_INNER,
    .k_padded = YOLOV5N_L71_TENSOR_K_PADDED,
    .k_block = YOLOV5N_L71_TENSOR_K_BLOCK,
    .k_blocks = YOLOV5N_L71_TENSOR_K_BLOCKS,
    .tl8_row_stride = YOLOV5N_L71_TL8_ROW_STRIDE,
};

static const tensor_layer_t layer67 = {
    .ld = &layer67_meta,
    .tl8_b = yolov5n_l67_tl8_b,
    .bias_blk = yolov5n_l67_bias_blk,
    .scale_blk = yolov5n_l67_scale_blk,
    .oc_blocks = YOLOV5N_L67_TENSOR_OC_BLOCKS,
    .k_inner = YOLOV5N_L67_TENSOR_K_INNER,
    .k_padded = YOLOV5N_L67_TENSOR_K_PADDED,
    .k_block = YOLOV5N_L67_TENSOR_K_BLOCK,
    .k_blocks = YOLOV5N_L67_TENSOR_K_BLOCKS,
    .tl8_row_stride = YOLOV5N_L67_TL8_ROW_STRIDE,
};

static const tensor_layer_t layer68 = {
    .ld = &layer68_meta,
    .tl8_b = yolov5n_l68_tl8_b,
    .bias_blk = yolov5n_l68_bias_blk,
    .scale_blk = yolov5n_l68_scale_blk,
    .oc_blocks = YOLOV5N_L68_TENSOR_OC_BLOCKS,
    .k_inner = YOLOV5N_L68_TENSOR_K_INNER,
    .k_padded = YOLOV5N_L68_TENSOR_K_PADDED,
    .k_block = YOLOV5N_L68_TENSOR_K_BLOCK,
    .k_blocks = YOLOV5N_L68_TENSOR_K_BLOCKS,
    .tl8_row_stride = YOLOV5N_L68_TL8_ROW_STRIDE,
};

static const tensor_layer_t layer74 = {
    .ld = &layer74_meta,
    .tl8_b = yolov5n_l74_tl8_b,
    .bias_blk = yolov5n_l74_bias_blk,
    .scale_blk = yolov5n_l74_scale_blk,
    .oc_blocks = YOLOV5N_L74_TENSOR_OC_BLOCKS,
    .k_inner = YOLOV5N_L74_TENSOR_K_INNER,
    .k_padded = YOLOV5N_L74_TENSOR_K_PADDED,
    .k_block = YOLOV5N_L74_TENSOR_K_BLOCK,
    .k_blocks = YOLOV5N_L74_TENSOR_K_BLOCKS,
    .tl8_row_stride = YOLOV5N_L74_TL8_ROW_STRIDE,
};

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

static inline void apply_lut_block_rows(const yolov5n_layer_meta_t *ld,
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

static inline void exact_requant_store8_lut(const int32_t *acc,
                                            const int32_t *bias,
                                            const float *scale,
                                            int8_t *dst,
                                            const uint8_t *lut)
{
    uint32_t mask = 0xffu;

    if (lut) {
        __asm__ __volatile__(
            "mov.m.x    m0, %[mask], 0\n"
            "flw.ps     f31, %[idx]\n"
            "flw.ps     f0, 0(%[acc])\n"
            "flw.ps     f1, 0(%[bias])\n"
            "fadd.pi    f0, f0, f1\n"
            "fcvt.ps.pw f0, f0\n"
            "flw.ps     f1, 0(%[scale])\n"
            "fmul.ps    f0, f0, f1\n"
            "fcvt.pw.ps f0, f0, rmm\n"
            "fsat8.pi   f0, f0\n"
            "fandi.pi   f0, f0, 0xff\n"
            "fgb.ps     f1, f0(%[lut])\n"
            "fscb.ps    f1, f31(%[dst])\n"
            :
            : [mask] "r"(mask),
              [idx] "m"(*(&byte_lane_idx)),
              [acc] "r"(acc),
              [bias] "r"(bias),
              [scale] "r"(scale),
              [lut] "r"(lut),
              [dst] "r"(dst)
            : "f0", "f1", "f31", "memory");
    } else {
        __asm__ __volatile__(
            "mov.m.x    m0, %[mask], 0\n"
            "flw.ps     f31, %[idx]\n"
            "flw.ps     f0, 0(%[acc])\n"
            "flw.ps     f1, 0(%[bias])\n"
            "fadd.pi    f0, f0, f1\n"
            "fcvt.ps.pw f0, f0\n"
            "flw.ps     f1, 0(%[scale])\n"
            "fmul.ps    f0, f0, f1\n"
            "fcvt.pw.ps f0, f0, rmm\n"
            "fsat8.pi   f0, f0\n"
            "fscb.ps    f0, f31(%[dst])\n"
            :
            : [mask] "r"(mask),
              [idx] "m"(*(&byte_lane_idx)),
              [acc] "r"(acc),
              [bias] "r"(bias),
              [scale] "r"(scale),
              [dst] "r"(dst)
            : "f0", "f1", "f31", "memory");
    }
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

static void evict_range_to_l2(const void *ptr, size_t n)
{
    uintptr_t base = (uintptr_t)ptr & ~(uintptr_t)63u;
    uintptr_t end = ((uintptr_t)ptr + n + 63u) & ~(uintptr_t)63u;

    asm volatile("fence rw, rw" : : : "memory");
    for (uintptr_t p = base; p < end; p += 64u)
        et_cache_evict_line((uint64_t)p);
    asm volatile("fence rw, rw" : : : "memory");
}

static void evict_range_to_mem(const void *ptr, size_t n)
{
    uintptr_t base = (uintptr_t)ptr & ~(uintptr_t)63u;
    uintptr_t end = ((uintptr_t)ptr + n + 63u) & ~(uintptr_t)63u;

    asm volatile("fence rw, rw" : : : "memory");
    evict(to_Mem, (volatile const void *)base, end - base);
    asm volatile("fence rw, rw" : : : "memory");
}

static void pack_1x1_block(const int8_t *src, uint32_t src_c,
                           int8_t *dst_rowblk, uint32_t row_stride)
{
    for (uint32_t r = 0; r < PIX_BLOCK; ++r) {
        int8_t *dst_row = dst_rowblk + (size_t)r * row_stride;

        vec_zero_bytes(dst_row, row_stride);
        vec_copy_bytes(dst_row, src + (size_t)r * src_c, src_c);
    }
}

static int tensor_finish_block_rows(const tensor_layer_t *layer,
                                    int8_t *out_blk, uint32_t out_stride,
                                    uint32_t oc_blk, uint32_t rows)
{
    int32_t *acc_blk = acc_blk_scratch;

    tensor_wait(TENSOR_FMA_WAIT);
    if (get_tensor_error()) {
        clear_tensor_error();
        return -1;
    }

    if (layer->ld->exact_post) {
        const int32_t *bias = layer->bias_blk[oc_blk];
        const float *scale = layer->scale_blk[oc_blk];

        // tensor_store bypasses L1/L2. If acc_blk is resident and dirty in L1,
        // evicting it after the store can overwrite the fresh tensor result.
        evict_range_to_mem(acc_blk, (size_t)rows * 64u);
        tensor_store(0, 0, 3, rows - 1u, (uint64_t)acc_blk, 0, 64);
        tensor_wait(TENSOR_STORE_WAIT);
        if (get_tensor_error()) {
            clear_tensor_error();
            return -1;
        }

        for (uint32_t r = 0; r < rows; ++r) {
            int8_t *dst_row = out_blk + (size_t)r * out_stride;
            const int32_t *src_row = acc_blk + (size_t)r * OC_BLOCK;

            exact_requant_store8_lut(src_row + 0u, bias + 0u, scale + 0u,
                                     dst_row + 0u, layer->ld->lut);
            exact_requant_store8_lut(src_row + 8u, bias + 8u, scale + 8u,
                                     dst_row + 8u, layer->ld->lut);
        }

        return 0;
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

    evict_range_to_l2(out_blk, (size_t)rows * out_stride);
    apply_lut_block_rows(layer->ld, out_blk, out_stride, rows);
    return 0;
}

static int run_tensor_block(const tensor_layer_t *layer,
                            const int8_t *in_blk, uint32_t in_stride,
                            int8_t *out_blk, uint32_t out_stride,
                            uint32_t oc_blk)
{
    uint32_t acols = layer->k_block / 4u - 1u;

    for (uint32_t k_blk = 0; k_blk < layer->k_blocks; ++k_blk) {
        const int8_t *bblk = tl8_wblk(layer, oc_blk, k_blk);
        const int last_kblk = (k_blk + 1u == layer->k_blocks);

        if (k_blk)
            tensor_wait(TENSOR_FMA_WAIT);

        tensor_load(0, 0, SCP_A_START, 0, 0,
                    (uint64_t)(in_blk + (size_t)k_blk * layer->k_block),
                    0, PIX_BLOCK - 1u, in_stride, 0);
        tensor_load(0, 0, SCP_B_START, 1, 0, (uint64_t)bblk, 0,
                    PIX_BLOCK - 1u, layer->tl8_row_stride, 1);
        tensor_wait(TENSOR_LOAD_WAIT_0);
        tensor_wait(TENSOR_LOAD_WAIT_1);
        tensor_fma(0, 3, PIX_BLOCK - 1u, acols, 0,
                   last_kblk, 0, 0, 0, SCP_B_START, SCP_A_START, 3,
                   k_blk == 0u);
    }

    return tensor_finish_block_rows(layer, out_blk, out_stride, oc_blk, PIX_BLOCK);
}

static int run_conv1x1_layer(const tensor_layer_t *layer,
                             const int8_t *src, uint32_t src_c,
                             int8_t *dst, uint32_t dst_c)
{
    int8_t *packed = packed_scratch;

    if (layer->k_inner != src_c || layer->k_padded > 128u)
        return -1;

    for (size_t pix = 0; pix < NR_PIX; pix += PIX_BLOCK) {
        const int8_t *in_blk = src + pix * src_c;
        const int8_t *run_blk = in_blk;
        uint32_t run_stride = src_c;

        if (layer->k_padded != src_c) {
            pack_1x1_block(in_blk, src_c, packed, layer->k_padded);
            run_blk = packed;
            run_stride = layer->k_padded;
        }

        for (uint32_t oc_blk = 0; oc_blk < layer->oc_blocks; ++oc_blk) {
            if (run_tensor_block(layer, run_blk, run_stride,
                                 dst + pix * dst_c + oc_blk * OC_BLOCK,
                                 dst_c, oc_blk))
                return -1;
        }
    }

    return 0;
}

static inline uint64_t hwc_exact_base_signed(const int8_t *src,
                                             int32_t y, uint32_t x,
                                             uint32_t c_exact)
{
    intptr_t base = (intptr_t)(uintptr_t)src;
    intptr_t off = ((intptr_t)y * (intptr_t)INPUT_W + (intptr_t)x) *
                   (intptr_t)c_exact;
    return (uint64_t)(base + off);
}

static __attribute__((unused))
int run_conv3x3_layer_exact(const tensor_layer_t *layer,
                            const int8_t *src, uint32_t src_c,
                            int8_t *dst, uint32_t dst_c)
{
    const yolov5n_layer_meta_t *ld = layer->ld;
    uint32_t in_stride = INPUT_W * src_c;
    uint32_t out_stride = INPUT_W * dst_c;
    uint32_t acols = layer->k_block / 4u - 1u;
    uint32_t blocks_per_tap;

    if (ld->k_h != 3u || ld->k_w != 3u || ld->c_in != src_c ||
        (layer->k_blocks % 9u) != 0u || (src_c % layer->k_block) != 0u)
        return -1;

    blocks_per_tap = layer->k_blocks / 9u;
    if (blocks_per_tap != src_c / layer->k_block)
        return -1;

    convolution_size(1, INPUT_H, 0, 1);

    for (uint32_t x = 0; x < INPUT_W; ++x) {
        int last_valid_tap = -1;

        for (int tap = 0; tap < 9; ++tap) {
            int kx = tap % 3 - 1;
            int px = (int)x + kx;

            if (px >= 0 && px < (int)INPUT_W)
                last_valid_tap = tap;
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
                    if (px < 0 || px >= (int)INPUT_W)
                        continue;

                    convolution_ctrl((uint64_t)(uint16_t)row_start, 0);

                    for (uint32_t sub_blk = 0; sub_blk < blocks_per_tap; ++sub_blk) {
                        const int8_t *bblk =
                            tl8_wblk(layer, oc_blk,
                                     (uint32_t)tap * blocks_per_tap + sub_blk);
                        uint64_t abase = hwc_exact_base_signed(
                            src, row_start, (uint32_t)px, src_c) +
                                         (uint64_t)(sub_blk * layer->k_block);
                        int last_pass =
                            (tap == last_valid_tap) &&
                            (sub_blk + 1u == blocks_per_tap);

                        tensor_load(1, 0, SCP_A_START, 0, 0, abase, 0, rows - 1u,
                                    in_stride, 0);
                        tensor_load(0, 0, SCP_B_START, 1, 0, (uint64_t)bblk, 0,
                                    layer->k_block / 4u - 1u, layer->tl8_row_stride,
                                    1);
                        tensor_wait(TENSOR_LOAD_WAIT_0);
                        tensor_wait(TENSOR_LOAD_WAIT_1);

                        tensor_fma(1, 3, rows - 1u, acols, 0,
                                   last_pass, 0, 0, 0,
                                   SCP_B_START, SCP_A_START, 3, first_pass);
                        first_pass = 0;
                    }
                }

                if (tensor_finish_block_rows(layer, out_blk, out_stride,
                                             oc_blk, rows))
                    return -1;
            }
        }
    }

    // Leave the tensor path in a neutral state before subsequent non-conv
    // kernels. convolution_size/control update tensor_mask indirectly.
    convolution_ctrl(0, 0);
    convolution_size(1, 1, 0, 1);
    tensor_mask(0, 0xffffu);
    (void)get_tensor_mask();

    return 0;
}

static void pack_3x3_block(const int8_t *src, uint32_t src_c,
                           size_t pix0, int8_t *dst_rowblk,
                           uint32_t row_stride)
{
    uint32_t y = (uint32_t)(pix0 / INPUT_W);
    uint32_t x0 = (uint32_t)(pix0 % INPUT_W);

    for (uint32_t r = 0; r < PIX_BLOCK; ++r) {
        uint32_t x = x0 + r;
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
                } else {
                    const int8_t *src_px =
                        src + (((size_t)(uint32_t)iy * INPUT_W + (uint32_t)ix) * src_c);
                    vec_copy_bytes(tap_dst, src_px, src_c);
                }
            }
        }
    }
}

static int run_conv3x3_layer_packed(const tensor_layer_t *layer,
                                    const int8_t *src, uint32_t src_c,
                                    int8_t *dst, uint32_t dst_c)
{
    int8_t *patch = patch_scratch;

    if (layer->k_inner != 9u * src_c || layer->k_padded > 768u)
        return -1;

    for (size_t pix = 0; pix < NR_PIX; pix += PIX_BLOCK) {
        pack_3x3_block(src, src_c, pix, patch, layer->k_padded);

        for (uint32_t oc_blk = 0; oc_blk < layer->oc_blocks; ++oc_blk) {
            if (run_tensor_block(layer, patch, layer->k_padded,
                                 dst + pix * dst_c + oc_blk * OC_BLOCK,
                                 dst_c, oc_blk))
                return -1;
        }
    }

    return 0;
}

tpa_op_t yolo_full_p10_start(void)
{
    yolo_full_p10_ws_t *w = tpa_ws();
    return tpa_recv(tpa_chan(0), (void **)&w->in, &w->in_len, yolo_full_p10_run);
}

tpa_op_t yolo_full_p10_run(void)
{
    yolo_full_p10_ws_t *w = tpa_ws();
    int demo_outputs = tpa_chan(1) && tpa_chan(2);

    YV5N_REQUIRE_MSG_STOP("P10:LEN\n", w->in_len == INPUT_H * INPUT_W * INPUT_C);

    yolov5n_arena_begin(YV5N_P10_SCRATCH_PEAK_BYTES);
    det_stage0_buf = yolov5n_arena_alloc(NR_PIX * CLS_C, 64u);
    det_stage1_buf = yolov5n_arena_alloc(NR_PIX * CLS_C, 64u);
    acc_blk_scratch = yolov5n_arena_alloc(sizeof(*acc_blk_scratch) * PIX_BLOCK * OC_BLOCK, 64u);
    packed_scratch = yolov5n_arena_alloc(PIX_BLOCK * 128u, 64u);
    patch_scratch = yolov5n_arena_alloc(PIX_BLOCK * 768u, 64u);
    YV5N_REQUIRE_MSG_STOP("P10:ALLOC\n", det_stage0_buf && det_stage1_buf &&
                          acc_blk_scratch && packed_scratch && patch_scratch);

    enable_tensor_scratchpad();
    YV5N_TRY_MSG_STOP("P10:L61\n", run_conv3x3_layer_exact(&layer61, w->in, 256u, box1_buf, 64u));

    YV5N_TRY_MSG_STOP("P10:L62\n", run_conv3x3_layer_packed(&layer62, box1_buf, 64u, box2_buf, 64u));

    if (demo_outputs) {
        w->demo_box_out = yolov5n_send_buf(tpa_chan(1));
        YV5N_TRY_MSG_STOP("P10:L71D\n", run_conv1x1_layer(&layer71, box2_buf, 64u, w->demo_box_out, 64u));
    } else {
        YV5N_TRY_MSG_STOP("P10:L71\n", run_conv1x1_layer(&layer71, box2_buf, 64u, box_buf, 64u));
    }

    YV5N_TRY_MSG_STOP("P10:L67\n", run_conv3x3_layer_exact(&layer67, w->in, 256u, cls1_buf, 80u));

    YV5N_TRY_MSG_STOP("P10:L68\n", run_conv3x3_layer_packed(&layer68, cls1_buf, 80u, cls2_buf, 80u));

    if (demo_outputs) {
        w->demo_cls_out = yolov5n_send_buf(tpa_chan(2));
        YV5N_TRY_MSG_STOP("P10:L74D\n", run_conv1x1_layer(&layer74, cls2_buf, 80u, w->demo_cls_out, 80u));
    } else {
        YV5N_TRY_MSG_STOP("P10:L74\n", run_conv1x1_layer(&layer74, cls2_buf, 80u, cls_buf, 80u));
    }

    if (demo_outputs) {
        return tpa_send(tpa_chan(1), w->demo_box_out, YV5N_DEMO_P10_BOX_BYTES,
                        yolo_full_p10_send_cls);
    }

    TEST_PASS;
    return tpa_stop();
}

tpa_op_t yolo_full_p10_send_cls(void)
{
    yolo_full_p10_ws_t *w = tpa_ws();

    if (!w)
        return tpa_stop();

    return tpa_send(tpa_chan(2), w->demo_cls_out, YV5N_DEMO_P10_CLS_BYTES,
                    yolo_full_p10_done);
}

tpa_op_t yolo_full_p10_done(void)
{
    return tpa_stop();
}
