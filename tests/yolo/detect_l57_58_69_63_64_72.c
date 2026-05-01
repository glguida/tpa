#include <stddef.h>
#include <stdint.h>

#include <etsoc/isa/cacheops.h>
#include <etsoc/isa/tensors.h>

#include "test.h"
#include "tpa/tpa.h"

#include "generated/yolov5n_l57_58_69_63_64_72_case0.h"
#include "generated/yolov5n_l57_58_69_63_64_72_weights.h"
#include "generated/yolov5n_l57_tensor_weights.h"
#include "generated/yolov5n_l58_tensor_weights.h"
#include "generated/yolov5n_l69_tensor_weights.h"
#include "generated/yolov5n_l63_tensor_weights.h"
#include "generated/yolov5n_l64_tensor_weights.h"
#include "generated/yolov5n_l72_tensor_weights.h"

#define YOLO_DET_BEGIN       0xe3570000u
#define YOLO_DET_GOOD_INPUT  0xe3570001u
#define YOLO_DET_GOOD_OUTPUT 0xe3570002u
#define YOLO_DET_FAIL        0xe35700eeu

#define INPUT_H    YOLOV5N_L57_58_69_63_64_72_CASE0_INPUT_H
#define INPUT_W    YOLOV5N_L57_58_69_63_64_72_CASE0_INPUT_W
#define INPUT_C    YOLOV5N_L57_58_69_63_64_72_CASE0_INPUT_C
#define NR_PIX     (INPUT_H * INPUT_W)
#define PIX_BLOCK  16u
#define OC_BLOCK   16u
#define BOX_C      YOLOV5N_L57_58_69_63_64_72_CASE0_BOX_C
#define CLS_C      YOLOV5N_L57_58_69_63_64_72_CASE0_CLS_C

#define SCP_A_START    0u
#define SCP_B_START   16u
#define SCP_META_LINE 32u

typedef struct {
    const yv5n_l57_58_69_63_64_72_layer_t *ld;
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

tpa_op_t yolo_detect_l57_58_69_63_64_72_start(void);

static int8_t box1_buf[NR_PIX * 64u] __attribute__((aligned(64)));
static int8_t box2_buf[NR_PIX * 64u] __attribute__((aligned(64)));
static int8_t box_buf[YOLOV5N_L57_58_69_63_64_72_CASE0_BOX_ELEMS] __attribute__((aligned(64)));
static int8_t cls1_buf[NR_PIX * 80u] __attribute__((aligned(64)));
static int8_t cls2_buf[NR_PIX * 80u] __attribute__((aligned(64)));
static int8_t cls_buf[YOLOV5N_L57_58_69_63_64_72_CASE0_CLS_ELEMS] __attribute__((aligned(64)));
static volatile uint32_t tensor_ready;
static const int32_t byte_lane_idx[8] __attribute__((aligned(32))) = {
    0, 1, 2, 3, 4, 5, 6, 7,
};
static const int8_t zero_lane_bytes[8] __attribute__((aligned(8))) = {
    0, 0, 0, 0, 0, 0, 0, 0,
};

static const tensor_layer_t layer57 = {
    .ld = &yv5n_l57_58_69_63_64_72_layers[57],
    .tl8_b = yolov5n_l57_tl8_b,
    .bias_blk = yolov5n_l57_bias_blk,
    .scale_blk = yolov5n_l57_scale_blk,
    .oc_blocks = YOLOV5N_L57_TENSOR_OC_BLOCKS,
    .k_inner = YOLOV5N_L57_TENSOR_K_INNER,
    .k_padded = YOLOV5N_L57_TENSOR_K_PADDED,
    .k_block = YOLOV5N_L57_TENSOR_K_BLOCK,
    .k_blocks = YOLOV5N_L57_TENSOR_K_BLOCKS,
    .tl8_row_stride = YOLOV5N_L57_TL8_ROW_STRIDE,
};

static const tensor_layer_t layer58 = {
    .ld = &yv5n_l57_58_69_63_64_72_layers[58],
    .tl8_b = yolov5n_l58_tl8_b,
    .bias_blk = yolov5n_l58_bias_blk,
    .scale_blk = yolov5n_l58_scale_blk,
    .oc_blocks = YOLOV5N_L58_TENSOR_OC_BLOCKS,
    .k_inner = YOLOV5N_L58_TENSOR_K_INNER,
    .k_padded = YOLOV5N_L58_TENSOR_K_PADDED,
    .k_block = YOLOV5N_L58_TENSOR_K_BLOCK,
    .k_blocks = YOLOV5N_L58_TENSOR_K_BLOCKS,
    .tl8_row_stride = YOLOV5N_L58_TL8_ROW_STRIDE,
};

static const tensor_layer_t layer69 = {
    .ld = &yv5n_l57_58_69_63_64_72_layers[69],
    .tl8_b = yolov5n_l69_tl8_b,
    .bias_blk = yolov5n_l69_bias_blk,
    .scale_blk = yolov5n_l69_scale_blk,
    .oc_blocks = YOLOV5N_L69_TENSOR_OC_BLOCKS,
    .k_inner = YOLOV5N_L69_TENSOR_K_INNER,
    .k_padded = YOLOV5N_L69_TENSOR_K_PADDED,
    .k_block = YOLOV5N_L69_TENSOR_K_BLOCK,
    .k_blocks = YOLOV5N_L69_TENSOR_K_BLOCKS,
    .tl8_row_stride = YOLOV5N_L69_TL8_ROW_STRIDE,
};

static const tensor_layer_t layer63 = {
    .ld = &yv5n_l57_58_69_63_64_72_layers[63],
    .tl8_b = yolov5n_l63_tl8_b,
    .bias_blk = yolov5n_l63_bias_blk,
    .scale_blk = yolov5n_l63_scale_blk,
    .oc_blocks = YOLOV5N_L63_TENSOR_OC_BLOCKS,
    .k_inner = YOLOV5N_L63_TENSOR_K_INNER,
    .k_padded = YOLOV5N_L63_TENSOR_K_PADDED,
    .k_block = YOLOV5N_L63_TENSOR_K_BLOCK,
    .k_blocks = YOLOV5N_L63_TENSOR_K_BLOCKS,
    .tl8_row_stride = YOLOV5N_L63_TL8_ROW_STRIDE,
};

static const tensor_layer_t layer64 = {
    .ld = &yv5n_l57_58_69_63_64_72_layers[64],
    .tl8_b = yolov5n_l64_tl8_b,
    .bias_blk = yolov5n_l64_bias_blk,
    .scale_blk = yolov5n_l64_scale_blk,
    .oc_blocks = YOLOV5N_L64_TENSOR_OC_BLOCKS,
    .k_inner = YOLOV5N_L64_TENSOR_K_INNER,
    .k_padded = YOLOV5N_L64_TENSOR_K_PADDED,
    .k_block = YOLOV5N_L64_TENSOR_K_BLOCK,
    .k_blocks = YOLOV5N_L64_TENSOR_K_BLOCKS,
    .tl8_row_stride = YOLOV5N_L64_TL8_ROW_STRIDE,
};

static const tensor_layer_t layer72 = {
    .ld = &yv5n_l57_58_69_63_64_72_layers[72],
    .tl8_b = yolov5n_l72_tl8_b,
    .bias_blk = yolov5n_l72_bias_blk,
    .scale_blk = yolov5n_l72_scale_blk,
    .oc_blocks = YOLOV5N_L72_TENSOR_OC_BLOCKS,
    .k_inner = YOLOV5N_L72_TENSOR_K_INNER,
    .k_padded = YOLOV5N_L72_TENSOR_K_PADDED,
    .k_block = YOLOV5N_L72_TENSOR_K_BLOCK,
    .k_blocks = YOLOV5N_L72_TENSOR_K_BLOCKS,
    .tl8_row_stride = YOLOV5N_L72_TL8_ROW_STRIDE,
};

static void mark(uint32_t v)
{
    arch_trace(v);
}

static inline void diag_putc(char c)
{
    uint64_t v = (1ull << 56) | (uint8_t)c;
    asm volatile("csrw validation1, %0" :: "r"(v) : "memory");
}

static void diag_puts(const char *s)
{
    while (*s)
        diag_putc(*s++);
}

static void diag_putu64(uint64_t v)
{
    char buf[32];
    uint32_t n = 0;

    if (!v) {
        diag_putc('0');
        return;
    }

    while (v) {
        buf[n++] = (char)('0' + (v % 10ull));
        v /= 10ull;
    }
    while (n)
        diag_putc(buf[--n]);
}

static void diag_line(const char *name, uint64_t v)
{
    diag_puts(name);
    diag_putc('=');
    diag_putu64(v);
    diag_putc('\n');
}

static void fail_hash(const char *name, uint64_t got, uint64_t exp)
{
    diag_puts("FAIL_");
    diag_puts(name);
    diag_putc('\n');
    diag_line("got", got);
    diag_line("exp", exp);
}

static void fail_first_diff(const char *name, const int8_t *got,
                            const int8_t *exp, size_t n)
{
    for (size_t i = 0; i < n; ++i) {
        if (got[i] != exp[i]) {
            diag_puts("DIFF_");
            diag_puts(name);
            diag_putc('\n');
            diag_line("idx", i);
            diag_line("gotv", (uint8_t)got[i]);
            diag_line("expv", (uint8_t)exp[i]);
            return;
        }
    }
}

static inline uint64_t emu_cycle(void)
{
    uint64_t v;
    uint64_t mode = (7ull << 56);

    asm volatile(
        "csrw validation1, %1\n\t"
        "csrr %0, validation1\n"
        : "=r"(v)
        : "r"(mode)
        : "memory");

    return v;
}

static uint64_t fnv1a64_step(uint64_t h, int8_t v)
{
    h ^= (uint8_t)v;
    h *= UINT64_C(0x100000001b3);
    return h;
}

static uint64_t hash_buf(const int8_t *buf, size_t n)
{
    uint64_t h = UINT64_C(0xcbf29ce484222325);

    for (size_t i = 0; i < n; ++i)
        h = fnv1a64_step(h, buf[i]);
    return h;
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

static inline void apply_lut_block_rows(const yv5n_l57_58_69_63_64_72_layer_t *ld,
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
    int32_t acc_blk[PIX_BLOCK * OC_BLOCK] __attribute__((aligned(64)));

    tensor_wait(TENSOR_FMA_WAIT);
    if (get_tensor_error()) {
        clear_tensor_error();
        return -1;
    }

    if (layer->ld == &yv5n_l57_58_69_63_64_72_layers[64] ||
        layer->ld == &yv5n_l57_58_69_63_64_72_layers[72]) {
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
    int8_t packed[PIX_BLOCK * 128u] __attribute__((aligned(64)));

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
    const yv5n_l57_58_69_63_64_72_layer_t *ld = layer->ld;
    uint32_t in_stride = INPUT_W * src_c;
    uint32_t out_stride = INPUT_W * dst_c;
    uint32_t acols = layer->k_block / 4u - 1u;

    if (ld->kH != 3u || ld->kW != 3u || ld->C_in != src_c ||
        layer->k_blocks != 9u)
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

                    if (px < 0 || px >= (int)INPUT_W)
                        continue;

                    bblk = tl8_wblk(layer, oc_blk, (uint32_t)tap);
                    abase = hwc_exact_base_signed(src, row_start, (uint32_t)px, src_c);

                    convolution_ctrl((uint64_t)(uint16_t)row_start, 0);
                    tensor_load(1, 0, SCP_A_START, 0, 0, abase, 0, rows - 1u,
                                in_stride, 0);
                    tensor_load(0, 0, SCP_B_START, 1, 0, (uint64_t)bblk, 0,
                                layer->k_block / 4u - 1u, layer->tl8_row_stride, 1);
                    tensor_wait(TENSOR_LOAD_WAIT_0);
                    tensor_wait(TENSOR_LOAD_WAIT_1);

                    tensor_fma(1, 3, rows - 1u, acols, 0,
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
    int8_t patch[PIX_BLOCK * 768u] __attribute__((aligned(64)));

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

tpa_op_t yolo_detect_l57_58_69_63_64_72_start(void)
{
    uint64_t total_beg = 0, total_end = 0;
    uint64_t b57_beg = 0, b57_end = 0;
    uint64_t b58_beg = 0, b58_end = 0;
    uint64_t b69_beg = 0, b69_end = 0;
    uint64_t c63_beg = 0, c63_end = 0;
    uint64_t c64_beg = 0, c64_end = 0;
    uint64_t c72_beg = 0, c72_end = 0;

    mark(YOLO_DET_BEGIN);

    if (hash_buf(yolov5n_l57_58_69_63_64_72_case0_in,
                 INPUT_H * INPUT_W * INPUT_C) !=
        YOLOV5N_L57_58_69_63_64_72_CASE0_INPUT_HASH) {
        fail_hash("input",
                  hash_buf(yolov5n_l57_58_69_63_64_72_case0_in,
                           INPUT_H * INPUT_W * INPUT_C),
                  YOLOV5N_L57_58_69_63_64_72_CASE0_INPUT_HASH);
        mark(YOLO_DET_FAIL);
        mark(0x10u);
        TEST_FAIL;
        return tpa_stop();
    }
    mark(YOLO_DET_GOOD_INPUT);

    enable_tensor_scratchpad();

    total_beg = emu_cycle();

    b57_beg = emu_cycle();
    if (run_conv3x3_layer_packed(&layer57,
                                 yolov5n_l57_58_69_63_64_72_case0_in, 64u,
                                 box1_buf, 64u)) {
        mark(YOLO_DET_FAIL);
        mark(0x20u);
        TEST_FAIL;
        return tpa_stop();
    }
    b57_end = emu_cycle();
    {
        uint64_t h = hash_buf(box1_buf, NR_PIX * 64u);
        if (h != YOLOV5N_L57_58_69_63_64_72_CASE0_BOX1_HASH) {
            fail_hash("box1", h, YOLOV5N_L57_58_69_63_64_72_CASE0_BOX1_HASH);
            mark(YOLO_DET_FAIL);
            mark(0x21u);
            TEST_FAIL;
            return tpa_stop();
        }
    }

    b58_beg = emu_cycle();
    if (run_conv3x3_layer_packed(&layer58, box1_buf, 64u, box2_buf, 64u)) {
        mark(YOLO_DET_FAIL);
        mark(0x22u);
        TEST_FAIL;
        return tpa_stop();
    }
    b58_end = emu_cycle();
    {
        uint64_t h = hash_buf(box2_buf, NR_PIX * 64u);
        if (h != YOLOV5N_L57_58_69_63_64_72_CASE0_BOX2_HASH) {
            fail_hash("box2", h, YOLOV5N_L57_58_69_63_64_72_CASE0_BOX2_HASH);
            fail_first_diff("box2", box2_buf,
                            yolov5n_l57_58_69_63_64_72_case0_box2,
                            NR_PIX * 64u);
            mark(YOLO_DET_FAIL);
            mark(0x23u);
            TEST_FAIL;
            return tpa_stop();
        }
    }

    b69_beg = emu_cycle();
    if (run_conv1x1_layer(&layer69, box2_buf, 64u, box_buf, 64u)) {
        mark(YOLO_DET_FAIL);
        mark(0x24u);
        TEST_FAIL;
        return tpa_stop();
    }
    b69_end = emu_cycle();
    {
        uint64_t h = hash_buf(box_buf, YOLOV5N_L57_58_69_63_64_72_CASE0_BOX_ELEMS);
        if (h != YOLOV5N_L57_58_69_63_64_72_CASE0_BOX_HASH) {
            fail_hash("box", h, YOLOV5N_L57_58_69_63_64_72_CASE0_BOX_HASH);
            mark(YOLO_DET_FAIL);
            mark(0x25u);
            TEST_FAIL;
            return tpa_stop();
        }
    }

    c63_beg = emu_cycle();
    if (run_conv3x3_layer_packed(&layer63,
                                 yolov5n_l57_58_69_63_64_72_case0_in, 64u,
                                 cls1_buf, 80u)) {
        mark(YOLO_DET_FAIL);
        mark(0x26u);
        TEST_FAIL;
        return tpa_stop();
    }
    c63_end = emu_cycle();
    {
        uint64_t h = hash_buf(cls1_buf, NR_PIX * 80u);
        if (h != YOLOV5N_L57_58_69_63_64_72_CASE0_CLS1_HASH) {
            fail_hash("cls1", h, YOLOV5N_L57_58_69_63_64_72_CASE0_CLS1_HASH);
            mark(YOLO_DET_FAIL);
            mark(0x27u);
            TEST_FAIL;
            return tpa_stop();
        }
    }

    c64_beg = emu_cycle();
    if (run_conv3x3_layer_packed(&layer64, cls1_buf, 80u, cls2_buf, 80u)) {
        mark(YOLO_DET_FAIL);
        mark(0x28u);
        TEST_FAIL;
        return tpa_stop();
    }
    c64_end = emu_cycle();
    {
        uint64_t h = hash_buf(cls2_buf, NR_PIX * 80u);
        if (h != YOLOV5N_L57_58_69_63_64_72_CASE0_CLS2_HASH) {
            fail_hash("cls2", h, YOLOV5N_L57_58_69_63_64_72_CASE0_CLS2_HASH);
            fail_first_diff("cls2", cls2_buf,
                            yolov5n_l57_58_69_63_64_72_case0_cls2,
                            NR_PIX * 80u);
            mark(YOLO_DET_FAIL);
            mark(0x29u);
            TEST_FAIL;
            return tpa_stop();
        }
    }

    c72_beg = emu_cycle();
    if (run_conv1x1_layer(&layer72, cls2_buf, 80u, cls_buf, 80u)) {
        mark(YOLO_DET_FAIL);
        mark(0x2au);
        TEST_FAIL;
        return tpa_stop();
    }
    c72_end = emu_cycle();
    {
        uint64_t h = hash_buf(cls_buf, YOLOV5N_L57_58_69_63_64_72_CASE0_CLS_ELEMS);
        if (h != YOLOV5N_L57_58_69_63_64_72_CASE0_CLS_HASH) {
            fail_hash("cls", h, YOLOV5N_L57_58_69_63_64_72_CASE0_CLS_HASH);
            mark(YOLO_DET_FAIL);
            mark(0x2bu);
            TEST_FAIL;
            return tpa_stop();
        }
    }

    for (size_t i = 0; i < YOLOV5N_L57_58_69_63_64_72_CASE0_BOX_ELEMS; ++i) {
        if (box_buf[i] != yolov5n_l57_58_69_63_64_72_case0_box[i]) {
            mark(YOLO_DET_FAIL);
            mark(0x2cu);
            mark((uint32_t)i);
            mark((uint32_t)(uint8_t)box_buf[i]);
            mark((uint32_t)(uint8_t)yolov5n_l57_58_69_63_64_72_case0_box[i]);
            TEST_FAIL;
            return tpa_stop();
        }
    }

    for (size_t i = 0; i < YOLOV5N_L57_58_69_63_64_72_CASE0_CLS_ELEMS; ++i) {
        if (cls_buf[i] != yolov5n_l57_58_69_63_64_72_case0_cls[i]) {
            mark(YOLO_DET_FAIL);
            mark(0x2du);
            mark((uint32_t)i);
            mark((uint32_t)(uint8_t)cls_buf[i]);
            mark((uint32_t)(uint8_t)yolov5n_l57_58_69_63_64_72_case0_cls[i]);
            TEST_FAIL;
            return tpa_stop();
        }
    }

    total_end = emu_cycle();

    diag_line("DET_57_58_69_63_64_72_TOTAL", total_end - total_beg);
    diag_line("DET_57", b57_end - b57_beg);
    diag_line("DET_58", b58_end - b58_beg);
    diag_line("DET_69", b69_end - b69_beg);
    diag_line("DET_63", c63_end - c63_beg);
    diag_line("DET_64", c64_end - c64_beg);
    diag_line("DET_72", c72_end - c72_beg);
    diag_line("DET_WORK",
              (b57_end - b57_beg) + (b58_end - b58_beg) + (b69_end - b69_beg) +
              (c63_end - c63_beg) + (c64_end - c64_beg) + (c72_end - c72_beg));

    mark(YOLO_DET_GOOD_OUTPUT);
    TEST_PASS;
    return tpa_stop();
}
