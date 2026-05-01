#include <stddef.h>
#include <stdint.h>

#include <etsoc/isa/cacheops.h>
#include <etsoc/isa/tensors.h>

#include "test.h"
#include "tpa/tpa.h"

#include "generated/yolov5n_l31_32_case0.h"
#include "generated/yolov5n_l31_32_weights.h"
#include "generated/yolov5n_l31_tensor_weights.h"
#include "generated/yolov5n_l32_tensor_weights.h"

#define YOLO_SPPF_31_32_BEGIN       0xe3313200u
#define YOLO_SPPF_31_32_GOOD_INPUT  0xe3313201u
#define YOLO_SPPF_31_32_GOOD_OUTPUT 0xe3313202u
#define YOLO_SPPF_31_32_FAIL        0xe33132eeu

#define INPUT_H    YOLOV5N_L31_32_CASE0_INPUT_H
#define INPUT_W    YOLOV5N_L31_32_CASE0_INPUT_W
#define INPUT_C    YOLOV5N_L31_32_CASE0_INPUT_C
#define CV1_C      YOLOV5N_L31_32_CASE0_CV1_C
#define CAT_C      YOLOV5N_L31_32_CASE0_CAT_C
#define OUTPUT_C   YOLOV5N_L31_32_CASE0_OUTPUT_C
#define NR_PIX     (INPUT_H * INPUT_W)
#define PIX_BLOCK  16u
#define OC_BLOCK   16u

#define SCP_A_START    0u
#define SCP_B_START   16u
#define SCP_META_LINE 32u

typedef struct {
    const yv5n_l31_32_layer_t *ld;
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

tpa_op_t yolo_sppf_l31_32_start(void);

static int8_t cv1_buf[NR_PIX * CV1_C] __attribute__((aligned(64)));
static int8_t p1_buf[NR_PIX * CV1_C] __attribute__((aligned(64)));
static int8_t p2_buf[NR_PIX * CV1_C] __attribute__((aligned(64)));
static int8_t p3_buf[NR_PIX * CV1_C] __attribute__((aligned(64)));
static int8_t cat_buf[NR_PIX * CAT_C] __attribute__((aligned(64)));
static int8_t out_buf[YOLOV5N_L31_32_CASE0_OUTPUT_ELEMS] __attribute__((aligned(64)));
static volatile uint32_t tensor_ready;
static const int32_t byte_lane_idx[8] __attribute__((aligned(32))) = {
    0, 1, 2, 3, 4, 5, 6, 7,
};

static const tensor_layer_t layer31 = {
    .ld = &yv5n_l31_32_layers[31],
    .tl8_b = yolov5n_l31_tl8_b,
    .bias_blk = yolov5n_l31_bias_blk,
    .scale_blk = yolov5n_l31_scale_blk,
    .oc_blocks = YOLOV5N_L31_TENSOR_OC_BLOCKS,
    .k_inner = YOLOV5N_L31_TENSOR_K_INNER,
    .k_padded = YOLOV5N_L31_TENSOR_K_PADDED,
    .k_block = YOLOV5N_L31_TENSOR_K_BLOCK,
    .k_blocks = YOLOV5N_L31_TENSOR_K_BLOCKS,
    .tl8_row_stride = YOLOV5N_L31_TL8_ROW_STRIDE,
};

static const tensor_layer_t layer32 = {
    .ld = &yv5n_l31_32_layers[32],
    .tl8_b = yolov5n_l32_tl8_b,
    .bias_blk = yolov5n_l32_bias_blk,
    .scale_blk = yolov5n_l32_scale_blk,
    .oc_blocks = YOLOV5N_L32_TENSOR_OC_BLOCKS,
    .k_inner = YOLOV5N_L32_TENSOR_K_INNER,
    .k_padded = YOLOV5N_L32_TENSOR_K_PADDED,
    .k_block = YOLOV5N_L32_TENSOR_K_BLOCK,
    .k_blocks = YOLOV5N_L32_TENSOR_K_BLOCKS,
    .tl8_row_stride = YOLOV5N_L32_TL8_ROW_STRIDE,
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

static inline void vec_max_inplace8(int8_t *dst, const int8_t *src)
{
    __asm__ __volatile__(
        "flw.ps    f31, %[idx]\n"
        "fgb.ps    f0, f31(%[dst])\n"
        "fgb.ps    f1, f31(%[src])\n"
        "fmax.pi   f0, f0, f1\n"
        "fscb.ps   f0, f31(%[dst])\n"
        :
        : [idx] "m"(*(&byte_lane_idx)),
          [dst] "r"(dst),
          [src] "r"(src)
        : "f0", "f1", "f31", "memory");
}

static void vec_copy_bytes(int8_t *dst, const int8_t *src, uint32_t n)
{
    for (uint32_t off = 0; off < n; off += 8)
        vec_copy8(dst + off, src + off);
}

static void apply_lut_block_rows(const yv5n_l31_32_layer_t *ld,
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

static int run_tensor_block(const tensor_layer_t *layer,
                            const int8_t *in_blk, uint32_t in_stride,
                            int8_t *out_blk, uint32_t out_stride,
                            uint32_t oc_blk)
{
    uint32_t acols = layer->k_block / 4u - 1u;

    for (uint32_t k_blk = 0; k_blk < layer->k_blocks; ++k_blk) {
        const int8_t *bblk = tl8_wblk(layer, oc_blk, k_blk);
        const int last_kblk = (k_blk + 1u == layer->k_blocks);

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
    for (size_t pix = 0; pix < NR_PIX; pix += PIX_BLOCK) {
        const int8_t *in_blk = src + pix * src_c;

        for (uint32_t oc_blk = 0; oc_blk < layer->oc_blocks; ++oc_blk) {
            if (run_tensor_block(layer, in_blk, src_c,
                                 dst + pix * dst_c + oc_blk * OC_BLOCK,
                                 dst_c, oc_blk))
                return -1;
        }
    }

    return 0;
}

static void maxpool5x5_s1_p2_c128(const int8_t *src, int8_t *dst, int8_t *tmp)
{
    /* First pass: horizontal 5-wide max into scratch. */
    for (uint32_t y = 0; y < INPUT_H; ++y) {
        for (uint32_t x = 0; x < INPUT_W; ++x) {
            int8_t *tmp_px = tmp + (((size_t)y * INPUT_W + x) * CV1_C);

            for (uint32_t c = 0; c < CV1_C; c += 8) {
                uint32_t x0 = (x > 2u) ? (x - 2u) : 0u;
                uint32_t x1 = x + 2u;
                const int8_t *src_px;

                if (x1 >= INPUT_W)
                    x1 = INPUT_W - 1u;

                src_px = src + (((size_t)y * INPUT_W + x0) * CV1_C) + c;
                vec_copy8(tmp_px + c, src_px);

                for (uint32_t xx = x0 + 1u; xx <= x1; ++xx) {
                    src_px = src + (((size_t)y * INPUT_W + xx) * CV1_C) + c;
                    vec_max_inplace8(tmp_px + c, src_px);
                }
            }
        }
    }

    /* Second pass: vertical 5-high max from scratch. */
    for (uint32_t y = 0; y < INPUT_H; ++y) {
        uint32_t y0 = (y > 2u) ? (y - 2u) : 0u;
        uint32_t y1 = y + 2u;

        if (y1 >= INPUT_H)
            y1 = INPUT_H - 1u;

        for (uint32_t x = 0; x < INPUT_W; ++x) {
            int8_t *dst_px = dst + (((size_t)y * INPUT_W + x) * CV1_C);

            for (uint32_t c = 0; c < CV1_C; c += 8) {
                const int8_t *tmp_px =
                    tmp + (((size_t)y0 * INPUT_W + x) * CV1_C) + c;

                vec_copy8(dst_px + c, tmp_px);

                for (uint32_t yy = y0 + 1u; yy <= y1; ++yy) {
                    tmp_px = tmp + (((size_t)yy * INPUT_W + x) * CV1_C) + c;
                    vec_max_inplace8(dst_px + c, tmp_px);
                }
            }
        }
    }
}

static void concat_c_int8_4way(const int8_t *a, const int8_t *b,
                               const int8_t *c, const int8_t *d,
                               int8_t *dst)
{
    for (uint32_t pix = 0; pix < NR_PIX; ++pix) {
        const int8_t *a_px = a + pix * CV1_C;
        const int8_t *b_px = b + pix * CV1_C;
        const int8_t *c_px = c + pix * CV1_C;
        const int8_t *d_px = d + pix * CV1_C;
        int8_t *dst_px = dst + pix * CAT_C;

        vec_copy_bytes(dst_px, a_px, CV1_C);
        vec_copy_bytes(dst_px + CV1_C, b_px, CV1_C);
        vec_copy_bytes(dst_px + (2u * CV1_C), c_px, CV1_C);
        vec_copy_bytes(dst_px + (3u * CV1_C), d_px, CV1_C);
    }
}

tpa_op_t yolo_sppf_l31_32_start(void)
{
    const size_t cv1_elems = NR_PIX * CV1_C;
    uint64_t total_beg = 0, total_end = 0;
    uint64_t cv31_beg = 0, cv31_end = 0;
    uint64_t p1_beg = 0, p1_end = 0;
    uint64_t p2_beg = 0, p2_end = 0;
    uint64_t p3_beg = 0, p3_end = 0;
    uint64_t cat_beg = 0, cat_end = 0;
    uint64_t cv32_beg = 0, cv32_end = 0;

    mark(YOLO_SPPF_31_32_BEGIN);

    if (yv5n_l31_32_layers[31].C_in != INPUT_C ||
        yv5n_l31_32_layers[31].K_out != CV1_C ||
        yv5n_l31_32_layers[32].C_in != CAT_C ||
        yv5n_l31_32_layers[32].K_out != OUTPUT_C ||
        YOLOV5N_L31_32_CASE0_INPUT_ELEMS != NR_PIX * INPUT_C ||
        YOLOV5N_L31_32_CASE0_OUTPUT_ELEMS != NR_PIX * OUTPUT_C) {
        mark(YOLO_SPPF_31_32_FAIL);
        mark(0x10u);
        TEST_FAIL;
        return tpa_stop();
    }

    if (hash_buf(yolov5n_l31_32_case0_in, YOLOV5N_L31_32_CASE0_INPUT_ELEMS) !=
        YOLOV5N_L31_32_CASE0_INPUT_HASH) {
        mark(YOLO_SPPF_31_32_FAIL);
        mark(0x11u);
        TEST_FAIL;
        return tpa_stop();
    }
    mark(YOLO_SPPF_31_32_GOOD_INPUT);

    enable_tensor_scratchpad();

    total_beg = emu_cycle();

    cv31_beg = emu_cycle();
    if (run_conv1x1_layer(&layer31, yolov5n_l31_32_case0_in, INPUT_C, cv1_buf, CV1_C)) {
        mark(YOLO_SPPF_31_32_FAIL);
        mark(0x20u);
        TEST_FAIL;
        return tpa_stop();
    }
    cv31_end = emu_cycle();
    if (hash_buf(cv1_buf, cv1_elems) != YOLOV5N_L31_32_CASE0_CV1_HASH) {
        mark(YOLO_SPPF_31_32_FAIL);
        mark(0x21u);
        TEST_FAIL;
        return tpa_stop();
    }

    p1_beg = emu_cycle();
    maxpool5x5_s1_p2_c128(cv1_buf, p1_buf, cat_buf);
    p1_end = emu_cycle();
    if (hash_buf(p1_buf, cv1_elems) != YOLOV5N_L31_32_CASE0_P1_HASH) {
        mark(YOLO_SPPF_31_32_FAIL);
        mark(0x22u);
        TEST_FAIL;
        return tpa_stop();
    }

    p2_beg = emu_cycle();
    maxpool5x5_s1_p2_c128(p1_buf, p2_buf, cat_buf);
    p2_end = emu_cycle();
    if (hash_buf(p2_buf, cv1_elems) != YOLOV5N_L31_32_CASE0_P2_HASH) {
        mark(YOLO_SPPF_31_32_FAIL);
        mark(0x23u);
        TEST_FAIL;
        return tpa_stop();
    }

    p3_beg = emu_cycle();
    maxpool5x5_s1_p2_c128(p2_buf, p3_buf, cat_buf);
    p3_end = emu_cycle();
    if (hash_buf(p3_buf, cv1_elems) != YOLOV5N_L31_32_CASE0_P3_HASH) {
        mark(YOLO_SPPF_31_32_FAIL);
        mark(0x24u);
        TEST_FAIL;
        return tpa_stop();
    }

    cat_beg = emu_cycle();
    concat_c_int8_4way(p3_buf, p2_buf, p1_buf, cv1_buf, cat_buf);
    cat_end = emu_cycle();
    if (hash_buf(cat_buf, NR_PIX * CAT_C) != YOLOV5N_L31_32_CASE0_CAT_HASH) {
        mark(YOLO_SPPF_31_32_FAIL);
        mark(0x25u);
        TEST_FAIL;
        return tpa_stop();
    }

    cv32_beg = emu_cycle();
    if (run_conv1x1_layer(&layer32, cat_buf, CAT_C, out_buf, OUTPUT_C)) {
        mark(YOLO_SPPF_31_32_FAIL);
        mark(0x26u);
        TEST_FAIL;
        return tpa_stop();
    }
    cv32_end = emu_cycle();

    if (hash_buf(out_buf, YOLOV5N_L31_32_CASE0_OUTPUT_ELEMS) !=
        YOLOV5N_L31_32_CASE0_OUTPUT_HASH) {
        mark(YOLO_SPPF_31_32_FAIL);
        mark(0x27u);
        TEST_FAIL;
        return tpa_stop();
    }

    for (size_t i = 0; i < YOLOV5N_L31_32_CASE0_OUTPUT_ELEMS; ++i) {
        if (out_buf[i] != yolov5n_l31_32_case0_out[i]) {
            mark(YOLO_SPPF_31_32_FAIL);
            mark(0x28u);
            mark((uint32_t)i);
            mark((uint32_t)(uint8_t)out_buf[i]);
            mark((uint32_t)(uint8_t)yolov5n_l31_32_case0_out[i]);
            TEST_FAIL;
            return tpa_stop();
        }
    }

    total_end = emu_cycle();

    diag_line("SPPF_31_32_TOTAL", total_end - total_beg);
    diag_line("SPPF_31_32_CV31", cv31_end - cv31_beg);
    diag_line("SPPF_31_32_P1", p1_end - p1_beg);
    diag_line("SPPF_31_32_P2", p2_end - p2_beg);
    diag_line("SPPF_31_32_P3", p3_end - p3_beg);
    diag_line("SPPF_31_32_CAT", cat_end - cat_beg);
    diag_line("SPPF_31_32_CV32", cv32_end - cv32_beg);
    diag_line("SPPF_31_32_WORK",
              (cv31_end - cv31_beg) + (p1_end - p1_beg) + (p2_end - p2_beg) +
              (p3_end - p3_beg) + (cat_end - cat_beg) + (cv32_end - cv32_beg));

    mark(YOLO_SPPF_31_32_GOOD_OUTPUT);
    TEST_PASS;
    return tpa_stop();
}
