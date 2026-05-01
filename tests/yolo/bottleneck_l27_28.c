#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <etsoc/isa/cacheops.h>
#include <etsoc/isa/tensors.h>

#include "test.h"
#include "tpa/tpa.h"

#include "generated/yolov5n_l27_28_case0.h"
#include "generated/yolov5n_l27_28_weights.h"
#include "generated/yolov5n_l27_tensor_weights.h"
#include "generated/yolov5n_l28_tensor_weights.h"

#define YOLO_BTL27_28_BEGIN       0xe1272800u
#define YOLO_BTL27_28_GOOD_INPUT  0xe1272801u
#define YOLO_BTL27_28_GOOD_OUTPUT 0xe1272802u
#define YOLO_BTL27_28_FAIL        0xe12728eeu

#define INPUT_H    YOLOV5N_L27_28_CASE0_INPUT_H
#define INPUT_W    YOLOV5N_L27_28_CASE0_INPUT_W
#define INPUT_C    YOLOV5N_L27_28_CASE0_INPUT_C
#define OUTPUT_C   YOLOV5N_L27_28_CASE0_OUTPUT_C
#define NR_PIX     (INPUT_H * INPUT_W)
#define PIX_BLOCK  16u
#define OC_BLOCK   YOLOV5N_L27_TENSOR_OC_BLOCK

#define SCP_A_START    0u
#define SCP_B_START   16u
#define SCP_META_LINE 32u

#define DIAG_PUTCHAR (1ull << 56)
#define DIAG_CYCLE   (7ull << 56)

tpa_op_t yolo_bottleneck_l27_28_start(void);

typedef struct {
    const yv5n_layer_t *ld;
    const int8_t *tl8_b;
    const int32_t (*bias_blk)[OC_BLOCK];
    const float (*scale_blk)[OC_BLOCK];
    uint32_t oc_blocks;
    uint32_t k_inner;
    uint32_t k_block;
    uint32_t k_blocks;
    uint32_t tl8_row_stride;
} tensor_layer_t;

static int8_t mid_buf[YOLOV5N_L27_28_CASE0_OUTPUT_ELEMS]
    __attribute__((aligned(64)));
static int8_t out_buf[YOLOV5N_L27_28_CASE0_OUTPUT_ELEMS]
    __attribute__((aligned(64)));
static int8_t patch_buf[PIX_BLOCK * YOLOV5N_L28_TENSOR_K_INNER]
    __attribute__((aligned(64)));
static volatile uint32_t tensor_ready;
static const int32_t byte_lane_idx[8] __attribute__((aligned(32))) = {
    0, 1, 2, 3, 4, 5, 6, 7,
};

static const tensor_layer_t layer27 = {
    .ld = &yv5n_layers[27],
    .tl8_b = yolov5n_l27_tl8_b,
    .bias_blk = yolov5n_l27_bias_blk,
    .scale_blk = yolov5n_l27_scale_blk,
    .oc_blocks = YOLOV5N_L27_TENSOR_OC_BLOCKS,
    .k_inner = YOLOV5N_L27_TENSOR_K_INNER,
    .k_block = YOLOV5N_L27_TENSOR_K_BLOCK,
    .k_blocks = YOLOV5N_L27_TENSOR_K_BLOCKS,
    .tl8_row_stride = YOLOV5N_L27_TL8_ROW_STRIDE,
};

static const tensor_layer_t layer28 = {
    .ld = &yv5n_layers[28],
    .tl8_b = yolov5n_l28_tl8_b,
    .bias_blk = yolov5n_l28_bias_blk,
    .scale_blk = yolov5n_l28_scale_blk,
    .oc_blocks = YOLOV5N_L28_TENSOR_OC_BLOCKS,
    .k_inner = YOLOV5N_L28_TENSOR_K_INNER,
    .k_block = YOLOV5N_L28_TENSOR_K_BLOCK,
    .k_blocks = YOLOV5N_L28_TENSOR_K_BLOCKS,
    .tl8_row_stride = YOLOV5N_L28_TL8_ROW_STRIDE,
};

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
    uint64_t v = DIAG_PUTCHAR | (uint8_t)c;

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

    asm volatile(
        "csrw validation1, %1\n\t"
        "csrr %0, validation1\n"
        : "=r"(v)
        : "r"(DIAG_CYCLE)
        : "memory");

    return v;
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

static inline void load_meta_line(uint64_t dst, const void *src)
{
    tensor_load(0, 0, dst, 0, 0, (uint64_t)src, 0, 0, 64, 0);
    tensor_wait(TENSOR_LOAD_WAIT_0);
}

static inline const int8_t *tl8_wblk(const tensor_layer_t *layer,
                                     uint32_t oc_blk, uint32_t k_blk)
{
    size_t blk_bytes = (size_t)layer->k_block * (size_t)layer->tl8_row_stride;

    return layer->tl8_b +
           ((size_t)oc_blk * (size_t)layer->k_blocks + (size_t)k_blk) * blk_bytes;
}

static void apply_lut_block(const yv5n_layer_t *ld, int8_t *dst, uint32_t out_stride)
{
    if (!ld->lut)
        return;

    for (uint32_t r = 0; r < PIX_BLOCK; ++r) {
        for (uint32_t c = 0; c < OC_BLOCK; ++c) {
            int8_t v = dst[r * out_stride + c];

            dst[r * out_stride + c] = (int8_t)ld->lut[(uint8_t)v];
        }
    }
}

static int run_tensor_block(const tensor_layer_t *layer,
                            const int8_t *in_blk, uint32_t in_stride,
                            int8_t *out_blk, uint32_t out_stride,
                            uint32_t oc_blk)
{
    for (uint32_t k_blk = 0; k_blk < layer->k_blocks; ++k_blk) {
        const int8_t *bblk = tl8_wblk(layer, oc_blk, k_blk);
        const int last_kblk = (k_blk + 1u == layer->k_blocks);

        tensor_load(0, 0, SCP_A_START, 0, 0,
                    (uint64_t)(in_blk + (size_t)k_blk * layer->k_block),
                    0, PIX_BLOCK - 1, in_stride, 0);
        tensor_load(0, 0, SCP_B_START, 1, 0, (uint64_t)bblk, 0,
                    PIX_BLOCK - 1, layer->tl8_row_stride, 1);
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

    load_meta_line(SCP_META_LINE, layer->bias_blk[oc_blk]);
    tensor_quant(0, 3, PIX_BLOCK - 1, SCP_META_LINE,
                 QUANT_LAST_TRANS, QUANT_LAST_TRANS, QUANT_LAST_TRANS,
                 QUANT_LAST_TRANS, QUANT_LAST_TRANS, QUANT_LAST_TRANS,
                 QUANT_LAST_TRANS, QUANT_LAST_TRANS,
                 QUANT_INT32_TO_FP32, QUANT_INT32_ADD_ROW);
    tensor_wait(TENSOR_QUANT_WAIT);

    load_meta_line(SCP_META_LINE, layer->scale_blk[oc_blk]);
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

    tensor_store(1, 0, 0, PIX_BLOCK - 1, (uint64_t)out_blk, 0, out_stride);
    tensor_wait(TENSOR_STORE_WAIT);
    if (get_tensor_error()) {
        clear_tensor_error();
        return -1;
    }

    apply_lut_block(layer->ld, out_blk, out_stride);
    return 0;
}

static void pack_3x3_block(const int8_t *src, size_t base_pix)
{
    for (uint32_t r = 0; r < PIX_BLOCK; ++r) {
        size_t pix = base_pix + r;
        uint32_t oy = (uint32_t)(pix / INPUT_W);
        uint32_t ox = (uint32_t)(pix % INPUT_W);
        int8_t *dst = patch_buf + (size_t)r * layer28.k_inner;
        uint32_t k = 0;

        for (uint32_t ky = 0; ky < 3; ++ky) {
            int32_t iy = (int32_t)oy + (int32_t)ky - 1;

            for (uint32_t kx = 0; kx < 3; ++kx) {
                int32_t ix = (int32_t)ox + (int32_t)kx - 1;

                if (iy < 0 || iy >= (int32_t)INPUT_H ||
                    ix < 0 || ix >= (int32_t)INPUT_W) {
                    memset(dst + k, 0, INPUT_C);
                } else {
                    const int8_t *src_px =
                        src + (((size_t)iy * INPUT_W + (size_t)ix) * INPUT_C);
                    memcpy(dst + k, src_px, INPUT_C);
                }
                k += INPUT_C;
            }
        }
    }
}

static void add_residual_block(const int8_t *in_px, int8_t *out_px)
{
    const size_t n = PIX_BLOCK * OUTPUT_C;
    const int8_t *lut = yolov5n_l27_28_res_lut;

    for (size_t off = 0; off < n; off += 8) {
        const int8_t *src = in_px + off;
        int8_t *dst = out_px + off;

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

tpa_op_t yolo_bottleneck_l27_28_start(void)
{
    const size_t nr_pix = NR_PIX;
    uint64_t got_in_hash = UINT64_C(0xcbf29ce484222325);
    uint64_t got_mid_hash = UINT64_C(0xcbf29ce484222325);
    uint64_t got_body_hash = UINT64_C(0xcbf29ce484222325);
    uint64_t got_out_hash = UINT64_C(0xcbf29ce484222325);
    uint64_t total_beg = 0;
    uint64_t total_end = 0;
    uint64_t cv1_cycles = 0;
    uint64_t midchk_cycles = 0;
    uint64_t cv2_cycles = 0;
    uint64_t bodychk_cycles = 0;
    uint64_t add_cycles = 0;
    uint64_t check_cycles = 0;

    mark(YOLO_BTL27_28_BEGIN);

    if (INPUT_C != 128u || OUTPUT_C != 128u ||
        YOLOV5N_L27_28_CASE0_INPUT_ELEMS != NR_PIX * INPUT_C ||
        YOLOV5N_L27_28_CASE0_OUTPUT_ELEMS != NR_PIX * OUTPUT_C ||
        PIX_BLOCK != OC_BLOCK ||
        layer27.k_block != layer28.k_block ||
        layer27.oc_blocks != layer28.oc_blocks ||
        layer27.ld->kH != 1u || layer27.ld->kW != 1u ||
        layer28.ld->kH != 3u || layer28.ld->kW != 3u) {
        mark(YOLO_BTL27_28_FAIL);
        mark(0x10u);
        TEST_FAIL;
        return tpa_stop();
    }

    for (size_t i = 0; i < YOLOV5N_L27_28_CASE0_INPUT_ELEMS; ++i)
        got_in_hash = fnv1a64_step(got_in_hash, yolov5n_l27_28_case0_in[i]);

    if (got_in_hash != YOLOV5N_L27_28_CASE0_INPUT_HASH) {
        mark(YOLO_BTL27_28_FAIL);
        mark(0x11u);
        mark((uint32_t)got_in_hash);
        mark((uint32_t)(got_in_hash >> 32));
        TEST_FAIL;
        return tpa_stop();
    }
    mark(YOLO_BTL27_28_GOOD_INPUT);

    enable_tensor_scratchpad();
    total_beg = emu_cycle();

    {
        uint64_t beg = emu_cycle();
    for (size_t pix = 0; pix < nr_pix; pix += PIX_BLOCK) {
        const int8_t *in_px = yolov5n_l27_28_case0_in + (pix * INPUT_C);
        int8_t *mid_px = mid_buf + (pix * OUTPUT_C);

        for (uint32_t oc_blk = 0; oc_blk < layer27.oc_blocks; ++oc_blk) {
            if (run_tensor_block(&layer27, in_px, INPUT_C,
                                 mid_px + oc_blk * OC_BLOCK, OUTPUT_C, oc_blk)) {
                mark(YOLO_BTL27_28_FAIL);
                mark(0x20u);
                mark((uint32_t)get_tensor_error());
                TEST_FAIL;
                return tpa_stop();
            }
        }
    }
        cv1_cycles = emu_cycle() - beg;
    }

    {
        uint64_t beg = emu_cycle();
    for (size_t i = 0; i < YOLOV5N_L27_28_CASE0_MID_ELEMS; ++i) {
        int8_t gotv = mid_buf[i];
        int8_t expv = yolov5n_l27_28_case0_mid[i];

        got_mid_hash = fnv1a64_step(got_mid_hash, gotv);
        if (gotv != expv) {
            size_t pix = i / OUTPUT_C;

            diag_puts("mid mismatch pix=");
            diag_puti((int)pix);
            diag_puts(" ch=");
            diag_puti((int)(i % OUTPUT_C));
            diag_puts(" got=");
            diag_puti(gotv);
            diag_puts(" exp=");
            diag_puti(expv);
            diag_putc('\n');
            mark(YOLO_BTL27_28_FAIL);
            mark(0x30u + (uint32_t)pix);
            mark((uint32_t)(i % OUTPUT_C));
            mark((uint8_t)gotv);
            TEST_FAIL;
            return tpa_stop();
        }
    }
        midchk_cycles = emu_cycle() - beg;
    }

    if (got_mid_hash != YOLOV5N_L27_28_CASE0_MID_HASH) {
        mark(YOLO_BTL27_28_FAIL);
        mark(0x13u);
        mark((uint32_t)got_mid_hash);
        mark((uint32_t)(got_mid_hash >> 32));
        TEST_FAIL;
        return tpa_stop();
    }

    {
        uint64_t beg = emu_cycle();
    for (size_t pix = 0; pix < nr_pix; pix += PIX_BLOCK) {
        const int8_t *in_px = yolov5n_l27_28_case0_in + (pix * INPUT_C);
        int8_t *out_px = out_buf + (pix * OUTPUT_C);

        pack_3x3_block(mid_buf, pix);
        for (uint32_t oc_blk = 0; oc_blk < layer28.oc_blocks; ++oc_blk) {
            if (run_tensor_block(&layer28, patch_buf, layer28.k_inner,
                                 out_px + oc_blk * OC_BLOCK, OUTPUT_C, oc_blk)) {
                mark(YOLO_BTL27_28_FAIL);
                mark(0x21u);
                mark((uint32_t)get_tensor_error());
                TEST_FAIL;
                return tpa_stop();
            }
        }

        for (uint32_t i = 0; i < PIX_BLOCK * OUTPUT_C; ++i) {
            int8_t gotv = out_px[i];
            int8_t expv = yolov5n_l27_28_case0_body[pix * OUTPUT_C + i];

            got_body_hash = fnv1a64_step(got_body_hash, gotv);
            if (gotv != expv) {
                size_t pix_idx = pix + (i / OUTPUT_C);

                diag_puts("body mismatch pix=");
                diag_puti((int)pix_idx);
                diag_puts(" ch=");
                diag_puti((int)(i % OUTPUT_C));
                diag_puts(" got=");
                diag_puti(gotv);
                diag_puts(" exp=");
                diag_puti(expv);
                diag_putc('\n');
                mark(YOLO_BTL27_28_FAIL);
                mark(0x40u + (uint32_t)pix_idx);
                mark((uint32_t)(i % OUTPUT_C));
                mark((uint8_t)gotv);
                TEST_FAIL;
                return tpa_stop();
            }
        }
        {
            uint64_t add_beg = emu_cycle();
        add_residual_block(in_px, out_px);
            add_cycles += emu_cycle() - add_beg;
        }
    }
        cv2_cycles = emu_cycle() - beg - add_cycles;
    }

    {
        uint64_t beg = emu_cycle();
    if (got_body_hash != YOLOV5N_L27_28_CASE0_BODY_HASH) {
        mark(YOLO_BTL27_28_FAIL);
        mark(0x14u);
        mark((uint32_t)got_body_hash);
        mark((uint32_t)(got_body_hash >> 32));
        TEST_FAIL;
        return tpa_stop();
    }
        bodychk_cycles = emu_cycle() - beg;
    }

    {
        uint64_t beg = emu_cycle();
    for (size_t i = 0; i < YOLOV5N_L27_28_CASE0_OUTPUT_ELEMS; ++i) {
        int8_t outv = out_buf[i];
        int8_t expv = yolov5n_l27_28_case0_out[i];

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
                diag_puti(yolov5n_l27_28_case0_out[pix * OUTPUT_C + c]);
            }
            diag_putc('\n');
            mark(YOLO_BTL27_28_FAIL);
            mark(0x100u + (uint32_t)pix);
            mark((uint32_t)(i % OUTPUT_C));
            mark((uint8_t)outv);
            TEST_FAIL;
            return tpa_stop();
        }
    }
        check_cycles = emu_cycle() - beg;
    }

    if (got_out_hash != YOLOV5N_L27_28_CASE0_OUTPUT_HASH) {
        mark(YOLO_BTL27_28_FAIL);
        mark(0x12u);
        mark((uint32_t)got_out_hash);
        mark((uint32_t)(got_out_hash >> 32));
        TEST_FAIL;
        return tpa_stop();
    }

    total_end = emu_cycle();
    diag_line("BTL27_28_TOTAL", total_end - total_beg);
    diag_line("BTL27_28_CV1", cv1_cycles);
    diag_line("BTL27_28_MIDCHK", midchk_cycles);
    diag_line("BTL27_28_CV2", cv2_cycles);
    diag_line("BTL27_28_BODYCHK", bodychk_cycles);
    diag_line("BTL27_28_ADD", add_cycles);
    diag_line("BTL27_28_CHECK", check_cycles);
    diag_line("BTL27_28_WORK", cv1_cycles + cv2_cycles + add_cycles);

    mark(YOLO_BTL27_28_GOOD_OUTPUT);
    TEST_PASS;
    return tpa_stop();
}
