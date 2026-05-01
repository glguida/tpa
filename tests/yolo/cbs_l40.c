#include <stddef.h>
#include <stdint.h>

#include <etsoc/isa/cacheops.h>
#include <etsoc/isa/tensors.h>

#include "test.h"
#include "tpa/tpa.h"

#include "generated/yolov5n_l40_case0.h"
#include "generated/yolov5n_l40_weights.h"
#include "generated/yolov5n_l40_tensor_weights.h"

#define YOLO_CBS_L40_BEGIN       0xe0400000u
#define YOLO_CBS_L40_GOOD_INPUT  0xe0400001u
#define YOLO_CBS_L40_GOOD_OUTPUT 0xe0400002u
#define YOLO_CBS_L40_FAIL        0xe04000eeu

#define INPUT_C   128u
#define OUTPUT_C  32u
#define PIX_BLOCK 16u

#define SCP_A_START    0u
#define SCP_B_START   16u
#define SCP_META_LINE 32u

tpa_op_t yolo_cbs_l40_start(void);

static int8_t out_buf[YOLOV5N_L40_CASE0_OUTPUT_ELEMS]
    __attribute__((aligned(64)));
static volatile uint32_t tensor_ready;

static void mark(uint32_t v)
{
    arch_trace(v);
}

static uint64_t fnv1a64_step(uint64_t h, int8_t v)
{
    h ^= (uint8_t)v;
    h *= UINT64_C(0x100000001b3);
    return h;
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

static void diag_puti(int v)
{
    char buf[24];
    unsigned n = 0;
    unsigned u;

    if (v < 0) {
        diag_putc('-');
        u = (unsigned)(-v);
    } else {
        u = (unsigned)v;
    }

    if (!u) {
        diag_putc('0');
        return;
    }

    while (u) {
        buf[n++] = (char)('0' + (u % 10u));
        u /= 10u;
    }
    while (n)
        diag_putc(buf[--n]);
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

static inline const int8_t *tl8_wblk(uint32_t oc_blk, uint32_t k_blk)
{
    const size_t blk_bytes = YOLOV5N_L40_TENSOR_K_BLOCK *
                             YOLOV5N_L40_TL8_ROW_STRIDE;

    return yolov5n_l40_tl8_b +
           ((size_t)oc_blk * YOLOV5N_L40_TENSOR_K_BLOCKS + k_blk) * blk_bytes;
}

static inline void load_meta_line(uint64_t dst, const void *src)
{
    tensor_load(0, 0, dst, 0, 0, (uint64_t)src, 0, 0, 64, 0);
    tensor_wait(TENSOR_LOAD_WAIT_0);
}

static void apply_lut_block(int8_t *dst)
{
    const yv5n_layer_t *ld = &yv5n_layers[40];

    if (!ld->lut)
        return;

    for (uint32_t r = 0; r < PIX_BLOCK; ++r) {
        for (uint32_t c = 0; c < YOLOV5N_L40_TENSOR_OC_BLOCK; ++c) {
            int8_t v = dst[r * OUTPUT_C + c];

            dst[r * OUTPUT_C + c] = (int8_t)ld->lut[(uint8_t)v];
        }
    }
}

static int run_tensor_block(const int8_t *in_blk, int8_t *out_blk, uint32_t oc_blk)
{
    for (uint32_t k_blk = 0; k_blk < YOLOV5N_L40_TENSOR_K_BLOCKS; ++k_blk) {
        const int8_t *bblk = tl8_wblk(oc_blk, k_blk);
        const int last_kblk =
            (k_blk + 1u == YOLOV5N_L40_TENSOR_K_BLOCKS);

        tensor_load(0, 0, SCP_A_START, 0, 0,
                    (uint64_t)(in_blk + k_blk * YOLOV5N_L40_TENSOR_K_BLOCK),
                    0, PIX_BLOCK - 1, INPUT_C, 0);
        tensor_load(0, 0, SCP_B_START, 1, 0, (uint64_t)bblk, 0,
                    PIX_BLOCK - 1, YOLOV5N_L40_TL8_ROW_STRIDE, 1);
        tensor_wait(TENSOR_LOAD_WAIT_0);
        tensor_wait(TENSOR_LOAD_WAIT_1);
        tensor_fma(0, 3, PIX_BLOCK - 1, 15, 0,
                   last_kblk, 0, 0, 0, SCP_B_START, SCP_A_START, 3,
                   k_blk == 0);
    }

    tensor_wait(TENSOR_FMA_WAIT);
    if (get_tensor_error()) {
        clear_tensor_error();
        return -1;
    }

    load_meta_line(SCP_META_LINE, yolov5n_l40_bias_blk[oc_blk]);
    tensor_quant(0, 3, PIX_BLOCK - 1, SCP_META_LINE,
                 QUANT_LAST_TRANS, QUANT_LAST_TRANS, QUANT_LAST_TRANS,
                 QUANT_LAST_TRANS, QUANT_LAST_TRANS, QUANT_LAST_TRANS,
                 QUANT_LAST_TRANS, QUANT_LAST_TRANS,
                 QUANT_INT32_TO_FP32, QUANT_INT32_ADD_ROW);
    tensor_wait(TENSOR_QUANT_WAIT);

    load_meta_line(SCP_META_LINE, yolov5n_l40_scale_blk[oc_blk]);
    tensor_quant(0, 3, PIX_BLOCK - 1, SCP_META_LINE,
                 QUANT_LAST_TRANS, QUANT_LAST_TRANS, QUANT_LAST_TRANS,
                 QUANT_LAST_TRANS, QUANT_LAST_TRANS, QUANT_LAST_TRANS,
                 QUANT_PACK_128B, QUANT_SATINT8,
                 QUANT_FP32_TO_INT32, QUANT_FP32_MUL_ROW);
    tensor_wait(TENSOR_QUANT_WAIT);

    if (get_tensor_error()) {
        clear_tensor_error();
        return -1;
    }

    tensor_store(1, 0, 0, PIX_BLOCK - 1, (uint64_t)out_blk, 0, OUTPUT_C);
    tensor_wait(TENSOR_STORE_WAIT);
    if (get_tensor_error()) {
        clear_tensor_error();
        return -1;
    }

    apply_lut_block(out_blk);
    return 0;
}

tpa_op_t yolo_cbs_l40_start(void)
{
    const yv5n_layer_t *ld = &yv5n_layers[40];
    const size_t nr_pix =
        (size_t)YOLOV5N_L40_CASE0_INPUT_H * (size_t)YOLOV5N_L40_CASE0_INPUT_W;
    uint64_t got_in_hash = UINT64_C(0xcbf29ce484222325);
    uint64_t got_out_hash = UINT64_C(0xcbf29ce484222325);

    mark(YOLO_CBS_L40_BEGIN);

    if (ld->C_in != INPUT_C || ld->K_out != OUTPUT_C ||
        YOLOV5N_L40_CASE0_INPUT_C != INPUT_C ||
        ld->kH != 1u || ld->kW != 1u ||
        ld->stride_h != 1u || ld->stride_w != 1u ||
        ld->pad_h != 0u || ld->pad_w != 0u ||
        YOLOV5N_L40_CASE0_INPUT_ELEMS !=
            YOLOV5N_L40_CASE0_INPUT_H * YOLOV5N_L40_CASE0_INPUT_W * INPUT_C ||
        YOLOV5N_L40_CASE0_OUTPUT_ELEMS !=
            YOLOV5N_L40_CASE0_OUTPUT_H * YOLOV5N_L40_CASE0_OUTPUT_W * OUTPUT_C) {
        mark(YOLO_CBS_L40_FAIL);
        mark(0x10u);
        TEST_FAIL;
        return tpa_stop();
    }

    for (size_t i = 0; i < YOLOV5N_L40_CASE0_INPUT_ELEMS; ++i)
        got_in_hash = fnv1a64_step(got_in_hash, yolov5n_l40_case0_in[i]);

    if (got_in_hash != YOLOV5N_L40_CASE0_INPUT_HASH) {
        mark(YOLO_CBS_L40_FAIL);
        mark(0x11u);
        mark((uint32_t)got_in_hash);
        mark((uint32_t)(got_in_hash >> 32));
        TEST_FAIL;
        return tpa_stop();
    }
    mark(YOLO_CBS_L40_GOOD_INPUT);

    enable_tensor_scratchpad();

    for (size_t pix = 0; pix < nr_pix; pix += PIX_BLOCK) {
        const int8_t *in_px = yolov5n_l40_case0_in + (pix * INPUT_C);
        int8_t *out_px = out_buf + (pix * OUTPUT_C);

        for (uint32_t oc_blk = 0; oc_blk < YOLOV5N_L40_TENSOR_OC_BLOCKS; ++oc_blk) {
            if (run_tensor_block(in_px,
                                 out_px + oc_blk * YOLOV5N_L40_TENSOR_OC_BLOCK,
                                 oc_blk)) {
                mark(YOLO_CBS_L40_FAIL);
                mark(0x20u);
                mark((uint32_t)get_tensor_error());
                TEST_FAIL;
                return tpa_stop();
            }
        }
    }

    for (size_t i = 0; i < YOLOV5N_L40_CASE0_OUTPUT_ELEMS; ++i) {
        int8_t outv = out_buf[i];
        int8_t expv = yolov5n_l40_case0_out[i];

        got_out_hash = fnv1a64_step(got_out_hash, outv);
        if (outv != expv) {
            size_t pix = i / OUTPUT_C;

            diag_puts("mismatch pix=");
            diag_puti((int)pix);
            diag_puts(" ch=");
            diag_puti((int)(i % OUTPUT_C));
            diag_puts(" got=");
            diag_puti(outv);
            diag_puts(" exp=");
            diag_puti(expv);
            diag_putc('\n');
            diag_puts("got_row:");
            for (uint32_t c = 0; c < 16; ++c) {
                diag_putc(' ');
                diag_puti(out_buf[pix * OUTPUT_C + c]);
            }
            diag_putc('\n');
            diag_puts("exp_row:");
            for (uint32_t c = 0; c < 16; ++c) {
                diag_putc(' ');
                diag_puti(yolov5n_l40_case0_out[pix * OUTPUT_C + c]);
            }
            diag_putc('\n');
            mark(YOLO_CBS_L40_FAIL);
            mark(0x100u + (uint32_t)pix);
            mark((uint32_t)(i % OUTPUT_C));
            mark((uint8_t)outv);
            TEST_FAIL;
            return tpa_stop();
        }
    }

    if (got_out_hash != YOLOV5N_L40_CASE0_OUTPUT_HASH) {
        mark(YOLO_CBS_L40_FAIL);
        mark(0x12u);
        mark((uint32_t)got_out_hash);
        mark((uint32_t)(got_out_hash >> 32));
        TEST_FAIL;
        return tpa_stop();
    }

    mark(YOLO_CBS_L40_GOOD_OUTPUT);
    TEST_PASS;
    return tpa_stop();
}
