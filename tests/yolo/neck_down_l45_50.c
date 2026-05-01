#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <etsoc/isa/cacheops.h>
#include <etsoc/isa/tensors.h>

#include "test.h"
#include "tpa/tpa.h"

#include "generated/yolov5n_l45_50_case0.h"
#include "generated/yolov5n_l45_50_weights.h"
#include "generated/yolov5n_l45_tensor_weights.h"
#include "generated/yolov5n_l46_tensor_weights.h"
#include "generated/yolov5n_l47_tensor_weights.h"
#include "generated/yolov5n_l48_tensor_weights.h"
#include "generated/yolov5n_l49_tensor_weights.h"
#include "generated/yolov5n_l50_tensor_weights.h"

#define YOLO_NECK_DOWN_45_50_BEGIN       0xe3455000u
#define YOLO_NECK_DOWN_45_50_GOOD_INPUT  0xe3455001u
#define YOLO_NECK_DOWN_45_50_GOOD_OUTPUT 0xe3455002u
#define YOLO_NECK_DOWN_45_50_FAIL        0xe34550eeu

#define P3_H       YOLOV5N_L45_50_CASE0_P3_H
#define P3_W       YOLOV5N_L45_50_CASE0_P3_W
#define P3_C       YOLOV5N_L45_50_CASE0_P3_C
#define SKIP_H     YOLOV5N_L45_50_CASE0_SKIP_H
#define SKIP_W     YOLOV5N_L45_50_CASE0_SKIP_W
#define SKIP_C     YOLOV5N_L45_50_CASE0_SKIP_C
#define INPUT_H    SKIP_H
#define INPUT_W    SKIP_W
#define NR_PIX     (INPUT_H * INPUT_W)
#define PIX_BLOCK  16u
#define ROW_BLOCK  8u
#define OC_BLOCK   16u
#define DOWN_C     64u
#define BRANCH_C   64u
#define CAT0_C     128u
#define CAT1_C     128u
#define OUTPUT_C   YOLOV5N_L45_50_CASE0_OUTPUT_C

#define SCP_A_START    0u
#define SCP_B_START   16u
#define SCP_META_LINE 32u
#define FG32B_CONF UINT64_C(0x398A418820)

typedef struct {
    const yv5n_l45_50_layer_t *ld;
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

tpa_op_t yolo_neck_down_l45_50_start(void);

static int8_t down_buf[NR_PIX * DOWN_C] __attribute__((aligned(64)));
static int8_t cat0_buf[NR_PIX * CAT0_C] __attribute__((aligned(64)));
static int8_t a0_buf[NR_PIX * BRANCH_C] __attribute__((aligned(64)));
static int8_t mid_buf[NR_PIX * BRANCH_C] __attribute__((aligned(64)));
static int8_t a_buf[NR_PIX * BRANCH_C] __attribute__((aligned(64)));
static int8_t b_buf[NR_PIX * BRANCH_C] __attribute__((aligned(64)));
static int8_t cat1_buf[NR_PIX * CAT1_C] __attribute__((aligned(64)));
static int8_t out_buf[YOLOV5N_L45_50_CASE0_OUTPUT_ELEMS]
    __attribute__((aligned(64)));
static volatile uint32_t tensor_ready;
static const int32_t byte_lane_idx[8] __attribute__((aligned(32))) = {
    0, 1, 2, 3, 4, 5, 6, 7,
};
static const int8_t zero_lane_bytes[8] __attribute__((aligned(8))) = {
    0, 0, 0, 0, 0, 0, 0, 0,
};
static const int8_t zero32[32] __attribute__((aligned(32), unused)) = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

static const tensor_layer_t layer45 = {
    .ld = &yv5n_l45_50_layers[45],
    .tl8_b = yolov5n_l45_tl8_b,
    .bias_blk = yolov5n_l45_bias_blk,
    .scale_blk = yolov5n_l45_scale_blk,
    .oc_blocks = YOLOV5N_L45_TENSOR_OC_BLOCKS,
    .k_inner = YOLOV5N_L45_TENSOR_K_INNER,
    .k_padded = YOLOV5N_L45_TENSOR_K_PADDED,
    .k_block = YOLOV5N_L45_TENSOR_K_BLOCK,
    .k_blocks = YOLOV5N_L45_TENSOR_K_BLOCKS,
    .tl8_row_stride = YOLOV5N_L45_TL8_ROW_STRIDE,
};

static const tensor_layer_t layer46 = {
    .ld = &yv5n_l45_50_layers[46],
    .tl8_b = yolov5n_l46_tl8_b,
    .bias_blk = yolov5n_l46_bias_blk,
    .scale_blk = yolov5n_l46_scale_blk,
    .oc_blocks = YOLOV5N_L46_TENSOR_OC_BLOCKS,
    .k_inner = YOLOV5N_L46_TENSOR_K_INNER,
    .k_padded = YOLOV5N_L46_TENSOR_K_PADDED,
    .k_block = YOLOV5N_L46_TENSOR_K_BLOCK,
    .k_blocks = YOLOV5N_L46_TENSOR_K_BLOCKS,
    .tl8_row_stride = YOLOV5N_L46_TL8_ROW_STRIDE,
};

static const tensor_layer_t layer47 = {
    .ld = &yv5n_l45_50_layers[47],
    .tl8_b = yolov5n_l47_tl8_b,
    .bias_blk = yolov5n_l47_bias_blk,
    .scale_blk = yolov5n_l47_scale_blk,
    .oc_blocks = YOLOV5N_L47_TENSOR_OC_BLOCKS,
    .k_inner = YOLOV5N_L47_TENSOR_K_INNER,
    .k_padded = YOLOV5N_L47_TENSOR_K_PADDED,
    .k_block = YOLOV5N_L47_TENSOR_K_BLOCK,
    .k_blocks = YOLOV5N_L47_TENSOR_K_BLOCKS,
    .tl8_row_stride = YOLOV5N_L47_TL8_ROW_STRIDE,
};

static const tensor_layer_t layer48 = {
    .ld = &yv5n_l45_50_layers[48],
    .tl8_b = yolov5n_l48_tl8_b,
    .bias_blk = yolov5n_l48_bias_blk,
    .scale_blk = yolov5n_l48_scale_blk,
    .oc_blocks = YOLOV5N_L48_TENSOR_OC_BLOCKS,
    .k_inner = YOLOV5N_L48_TENSOR_K_INNER,
    .k_padded = YOLOV5N_L48_TENSOR_K_PADDED,
    .k_block = YOLOV5N_L48_TENSOR_K_BLOCK,
    .k_blocks = YOLOV5N_L48_TENSOR_K_BLOCKS,
    .tl8_row_stride = YOLOV5N_L48_TL8_ROW_STRIDE,
};

static const tensor_layer_t layer49 = {
    .ld = &yv5n_l45_50_layers[49],
    .tl8_b = yolov5n_l49_tl8_b,
    .bias_blk = yolov5n_l49_bias_blk,
    .scale_blk = yolov5n_l49_scale_blk,
    .oc_blocks = YOLOV5N_L49_TENSOR_OC_BLOCKS,
    .k_inner = YOLOV5N_L49_TENSOR_K_INNER,
    .k_padded = YOLOV5N_L49_TENSOR_K_PADDED,
    .k_block = YOLOV5N_L49_TENSOR_K_BLOCK,
    .k_blocks = YOLOV5N_L49_TENSOR_K_BLOCKS,
    .tl8_row_stride = YOLOV5N_L49_TL8_ROW_STRIDE,
};

static const tensor_layer_t layer50 = {
    .ld = &yv5n_l45_50_layers[50],
    .tl8_b = yolov5n_l50_tl8_b,
    .bias_blk = yolov5n_l50_bias_blk,
    .scale_blk = yolov5n_l50_scale_blk,
    .oc_blocks = YOLOV5N_L50_TENSOR_OC_BLOCKS,
    .k_inner = YOLOV5N_L50_TENSOR_K_INNER,
    .k_padded = YOLOV5N_L50_TENSOR_K_PADDED,
    .k_block = YOLOV5N_L50_TENSOR_K_BLOCK,
    .k_blocks = YOLOV5N_L50_TENSOR_K_BLOCKS,
    .tl8_row_stride = YOLOV5N_L50_TL8_ROW_STRIDE,
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

static uint64_t hash_buf(const int8_t *buf, size_t n)
{
    uint64_t h = UINT64_C(0xcbf29ce484222325);

    for (size_t i = 0; i < n; ++i)
        h = fnv1a64_step(h, buf[i]);
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

static __attribute__((unused)) void diag_puti(int v)
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

static void apply_lut_block_rows(const yv5n_l45_50_layer_t *ld,
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
        diag_puts("tensor_error_fma=");
        diag_putu64(get_tensor_error());
        diag_putc('\n');
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
        diag_puts("tensor_error_quant=");
        diag_putu64(get_tensor_error());
        diag_putc('\n');
        clear_tensor_error();
        return -1;
    }

    tensor_store(1, 0, 0, rows - 1u, (uint64_t)out_blk, 0, out_stride);
    tensor_wait(TENSOR_STORE_WAIT);
    if (get_tensor_error()) {
        diag_puts("tensor_error_store=");
        diag_putu64(get_tensor_error());
        diag_putc('\n');
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
    const yv5n_l45_50_layer_t *ld = layer->ld;
    uint32_t in_stride = INPUT_W * BRANCH_C;
    uint32_t out_stride = INPUT_W * dst_c;
    uint32_t acols = layer->k_inner / 4u - 1u;

    if (ld->C_in != 32u || ld->K_out != 32u || ld->kH != 1u ||
        ld->kW != 1u || dst_c != 32u || !ld->lut)
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
    const yv5n_l45_50_layer_t *ld = layer->ld;
    uint32_t in_stride = INPUT_W * BRANCH_C;
    uint32_t out_stride = INPUT_W * dst_c;
    uint32_t acols = layer->k_block / 4u - 1u;

    if (ld->C_in != 32u || ld->K_out != 32u || ld->kH != 3u ||
        ld->kW != 3u || dst_c != 32u || !ld->lut)
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
    int8_t patch[PIX_BLOCK * 576u] __attribute__((aligned(64)));

    if (layer->k_inner != 9u * src_c || layer->k_padded != layer->k_inner)
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
    int8_t patch[PIX_BLOCK * 576u] __attribute__((aligned(64)));

    if (layer->k_inner != 9u * src_c || layer->k_padded != layer->k_inner)
        return -1;

    for (size_t pix = 0; pix < NR_PIX; pix += PIX_BLOCK) {
        pack_3x3s2_block(src, src_c, pix, patch, layer->k_padded);

        for (uint32_t oc_blk = 0; oc_blk < layer->oc_blocks; ++oc_blk) {
            if (run_tensor_block(layer, patch, layer->k_padded,
                                 dst + pix * dst_c + oc_blk * OC_BLOCK,
                                 dst_c, oc_blk))
                return -1;
        }
    }
    return 0;
}

static void add_residual_block(const int8_t *skip_px, int8_t *body_px, size_t n)
{
    const int8_t *lut = yolov5n_l45_50_res_lut;

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

tpa_op_t yolo_neck_down_l45_50_start(void)
{
    uint64_t p3_hash = hash_buf(yolov5n_l45_50_case0_p3,
                                YOLOV5N_L45_50_CASE0_P3_ELEMS);
    uint64_t skip_hash = hash_buf(yolov5n_l45_50_case0_skip,
                                  YOLOV5N_L45_50_CASE0_SKIP_ELEMS);
    uint64_t got_hash;
    uint64_t total_beg = 0;
    uint64_t total_end = 0;
    uint64_t down_cycles = 0;
    uint64_t downchk_cycles = 0;
    uint64_t cat0_cycles = 0;
    uint64_t cat0chk_cycles = 0;
    uint64_t l46_cycles = 0;
    uint64_t a0chk_cycles = 0;
    uint64_t l47_cycles = 0;
    uint64_t midchk_cycles = 0;
    uint64_t l48_cycles = 0;
    uint64_t bodychk_cycles = 0;
    uint64_t add_cycles = 0;
    uint64_t achk_cycles = 0;
    uint64_t l49_cycles = 0;
    uint64_t bchk_cycles = 0;
    uint64_t cat1_cycles = 0;
    uint64_t cat1chk_cycles = 0;
    uint64_t l50_cycles = 0;
    uint64_t check_cycles = 0;

    mark(YOLO_NECK_DOWN_45_50_BEGIN);

    if (P3_C != 64u || SKIP_C != 64u || DOWN_C != 64u ||
        BRANCH_C != 64u || CAT0_C != 128u || CAT1_C != 128u ||
        OUTPUT_C != 128u ||
        YOLOV5N_L45_50_CASE0_OUTPUT_ELEMS != NR_PIX * OUTPUT_C) {
        mark(YOLO_NECK_DOWN_45_50_FAIL);
        mark(0x10u);
        TEST_FAIL;
        return tpa_stop();
    }

    if (p3_hash != YOLOV5N_L45_50_CASE0_P3_HASH) {
        fail_hash("p3_in", p3_hash, YOLOV5N_L45_50_CASE0_P3_HASH);
        mark(YOLO_NECK_DOWN_45_50_FAIL);
        mark(0x11u);
        TEST_FAIL;
        return tpa_stop();
    }

    if (skip_hash != YOLOV5N_L45_50_CASE0_SKIP_HASH) {
        fail_hash("skip_in", skip_hash, YOLOV5N_L45_50_CASE0_SKIP_HASH);
        mark(YOLO_NECK_DOWN_45_50_FAIL);
        mark(0x12u);
        TEST_FAIL;
        return tpa_stop();
    }
    mark(YOLO_NECK_DOWN_45_50_GOOD_INPUT);

    enable_tensor_scratchpad();
    total_beg = emu_cycle();

    {
        uint64_t beg = emu_cycle();
        if (run_conv3x3s2_layer(&layer45, yolov5n_l45_50_case0_p3,
                                P3_C, down_buf, DOWN_C)) {
            mark(YOLO_NECK_DOWN_45_50_FAIL);
            mark(0x20u);
            TEST_FAIL;
            return tpa_stop();
        }
        down_cycles = emu_cycle() - beg;
    }
    {
        uint64_t beg = emu_cycle();
        got_hash = hash_buf(down_buf, sizeof(down_buf));
        downchk_cycles = emu_cycle() - beg;
        if (got_hash != YOLOV5N_L45_50_CASE0_DOWN_HASH) {
            fail_hash("down", got_hash, YOLOV5N_L45_50_CASE0_DOWN_HASH);
            mark(YOLO_NECK_DOWN_45_50_FAIL);
            mark(0x21u);
            TEST_FAIL;
            return tpa_stop();
        }
    }

    {
        uint64_t beg = emu_cycle();
        concat_input(down_buf, yolov5n_l45_50_case0_skip, cat0_buf);
        cat0_cycles = emu_cycle() - beg;
    }
    {
        uint64_t beg = emu_cycle();
        got_hash = hash_buf(cat0_buf, sizeof(cat0_buf));
        cat0chk_cycles = emu_cycle() - beg;
        if (got_hash != YOLOV5N_L45_50_CASE0_CAT0_HASH) {
            fail_hash("cat0", got_hash, YOLOV5N_L45_50_CASE0_CAT0_HASH);
            mark(YOLO_NECK_DOWN_45_50_FAIL);
            mark(0x22u);
            TEST_FAIL;
            return tpa_stop();
        }
    }

    {
        uint64_t beg = emu_cycle();
        if (run_conv1x1_layer(&layer46, cat0_buf, CAT0_C, a0_buf, BRANCH_C)) {
            mark(YOLO_NECK_DOWN_45_50_FAIL);
            mark(0x23u);
            TEST_FAIL;
            return tpa_stop();
        }
        l46_cycles = emu_cycle() - beg;
    }
    {
        uint64_t beg = emu_cycle();
        got_hash = hash_buf(a0_buf, sizeof(a0_buf));
        a0chk_cycles = emu_cycle() - beg;
        if (got_hash != YOLOV5N_L45_50_CASE0_A0_HASH) {
            fail_hash("a0", got_hash, YOLOV5N_L45_50_CASE0_A0_HASH);
            mark(YOLO_NECK_DOWN_45_50_FAIL);
            mark(0x24u);
            TEST_FAIL;
            return tpa_stop();
        }
    }

    {
        uint64_t beg = emu_cycle();
        if (run_conv1x1_layer(&layer47, a0_buf, BRANCH_C, mid_buf, BRANCH_C)) {
            mark(YOLO_NECK_DOWN_45_50_FAIL);
            mark(0x25u);
            TEST_FAIL;
            return tpa_stop();
        }
        l47_cycles = emu_cycle() - beg;
    }
    {
        uint64_t beg = emu_cycle();
        got_hash = hash_buf(mid_buf, sizeof(mid_buf));
        midchk_cycles = emu_cycle() - beg;
        if (got_hash != YOLOV5N_L45_50_CASE0_MID_HASH) {
            fail_hash("mid", got_hash, YOLOV5N_L45_50_CASE0_MID_HASH);
            mark(YOLO_NECK_DOWN_45_50_FAIL);
            mark(0x26u);
            TEST_FAIL;
            return tpa_stop();
        }
    }

    {
        uint64_t beg = emu_cycle();
        if (run_conv3x3_layer(&layer48, mid_buf, BRANCH_C, a_buf, BRANCH_C)) {
            mark(YOLO_NECK_DOWN_45_50_FAIL);
            mark(0x27u);
            TEST_FAIL;
            return tpa_stop();
        }
        l48_cycles = emu_cycle() - beg;
    }
    {
        uint64_t beg = emu_cycle();
        got_hash = hash_buf(a_buf, sizeof(a_buf));
        bodychk_cycles = emu_cycle() - beg;
        if (got_hash != YOLOV5N_L45_50_CASE0_BODY_HASH) {
            fail_hash("body", got_hash, YOLOV5N_L45_50_CASE0_BODY_HASH);
            mark(YOLO_NECK_DOWN_45_50_FAIL);
            mark(0x28u);
            TEST_FAIL;
            return tpa_stop();
        }
    }

    {
        uint64_t beg = emu_cycle();
        add_residual_block(a0_buf, a_buf, sizeof(a_buf));
        add_cycles = emu_cycle() - beg;
    }
    {
        uint64_t beg = emu_cycle();
        got_hash = hash_buf(a_buf, sizeof(a_buf));
        achk_cycles = emu_cycle() - beg;
        if (got_hash != YOLOV5N_L45_50_CASE0_A_HASH) {
            fail_hash("a", got_hash, YOLOV5N_L45_50_CASE0_A_HASH);
            mark(YOLO_NECK_DOWN_45_50_FAIL);
            mark(0x29u);
            TEST_FAIL;
            return tpa_stop();
        }
    }

    {
        uint64_t beg = emu_cycle();
        if (run_conv1x1_layer(&layer49, cat0_buf, CAT0_C, b_buf, BRANCH_C)) {
            mark(YOLO_NECK_DOWN_45_50_FAIL);
            mark(0x2au);
            TEST_FAIL;
            return tpa_stop();
        }
        l49_cycles = emu_cycle() - beg;
    }
    {
        uint64_t beg = emu_cycle();
        got_hash = hash_buf(b_buf, sizeof(b_buf));
        bchk_cycles = emu_cycle() - beg;
        if (got_hash != YOLOV5N_L45_50_CASE0_B_HASH) {
            fail_hash("b", got_hash, YOLOV5N_L45_50_CASE0_B_HASH);
            mark(YOLO_NECK_DOWN_45_50_FAIL);
            mark(0x2bu);
            TEST_FAIL;
            return tpa_stop();
        }
    }

    {
        uint64_t beg = emu_cycle();
        concat_branch(a_buf, b_buf, cat1_buf);
        cat1_cycles = emu_cycle() - beg;
    }
    {
        uint64_t beg = emu_cycle();
        got_hash = hash_buf(cat1_buf, sizeof(cat1_buf));
        cat1chk_cycles = emu_cycle() - beg;
        if (got_hash != YOLOV5N_L45_50_CASE0_CAT1_HASH) {
            fail_hash("cat1", got_hash, YOLOV5N_L45_50_CASE0_CAT1_HASH);
            mark(YOLO_NECK_DOWN_45_50_FAIL);
            mark(0x2cu);
            TEST_FAIL;
            return tpa_stop();
        }
    }

    {
        uint64_t beg = emu_cycle();
        if (run_conv1x1_layer(&layer50, cat1_buf, CAT1_C, out_buf, OUTPUT_C)) {
            mark(YOLO_NECK_DOWN_45_50_FAIL);
            mark(0x2du);
            TEST_FAIL;
            return tpa_stop();
        }
        l50_cycles = emu_cycle() - beg;
    }
    {
        uint64_t beg = emu_cycle();
        got_hash = hash_buf(out_buf, YOLOV5N_L45_50_CASE0_OUTPUT_ELEMS);
        if (got_hash != YOLOV5N_L45_50_CASE0_OUTPUT_HASH) {
            fail_hash("out", got_hash, YOLOV5N_L45_50_CASE0_OUTPUT_HASH);
            mark(YOLO_NECK_DOWN_45_50_FAIL);
            mark(0x2eu);
            TEST_FAIL;
            return tpa_stop();
        }

        for (size_t i = 0; i < YOLOV5N_L45_50_CASE0_OUTPUT_ELEMS; ++i) {
            if (out_buf[i] != yolov5n_l45_50_case0_out[i]) {
                fail_first_diff("out", out_buf, yolov5n_l45_50_case0_out,
                                YOLOV5N_L45_50_CASE0_OUTPUT_ELEMS);
                mark(YOLO_NECK_DOWN_45_50_FAIL);
                mark(0x2fu);
                TEST_FAIL;
                return tpa_stop();
            }
        }
        check_cycles = emu_cycle() - beg;
    }

    total_end = emu_cycle();
    diag_line("NECK_DOWN_45_50_TOTAL", total_end - total_beg);
    diag_line("NECK_DOWN_45_50_DOWN", down_cycles);
    diag_line("NECK_DOWN_45_50_DOWNCHK", downchk_cycles);
    diag_line("NECK_DOWN_45_50_CAT0", cat0_cycles);
    diag_line("NECK_DOWN_45_50_CAT0CHK", cat0chk_cycles);
    diag_line("NECK_DOWN_45_50_CV46", l46_cycles);
    diag_line("NECK_DOWN_45_50_A0CHK", a0chk_cycles);
    diag_line("NECK_DOWN_45_50_CV47", l47_cycles);
    diag_line("NECK_DOWN_45_50_MIDCHK", midchk_cycles);
    diag_line("NECK_DOWN_45_50_CV48", l48_cycles);
    diag_line("NECK_DOWN_45_50_BODYCHK", bodychk_cycles);
    diag_line("NECK_DOWN_45_50_ADD", add_cycles);
    diag_line("NECK_DOWN_45_50_ACHK", achk_cycles);
    diag_line("NECK_DOWN_45_50_CV49", l49_cycles);
    diag_line("NECK_DOWN_45_50_BCHK", bchk_cycles);
    diag_line("NECK_DOWN_45_50_CAT1", cat1_cycles);
    diag_line("NECK_DOWN_45_50_CAT1CHK", cat1chk_cycles);
    diag_line("NECK_DOWN_45_50_CV50", l50_cycles);
    diag_line("NECK_DOWN_45_50_CHECK", check_cycles);
    diag_line("NECK_DOWN_45_50_TENSOR",
              down_cycles + l46_cycles + l47_cycles + l48_cycles +
              l49_cycles + l50_cycles);
    diag_line("NECK_DOWN_45_50_WORK",
              down_cycles + cat0_cycles + l46_cycles + l47_cycles +
              l48_cycles + add_cycles + l49_cycles + cat1_cycles +
              l50_cycles);

    mark(YOLO_NECK_DOWN_45_50_GOOD_OUTPUT);
    TEST_PASS;
    return tpa_stop();
}
