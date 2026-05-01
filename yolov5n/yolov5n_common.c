#include "yolov5n_common.h"
#include "yolov5n_residual_luts.h"

#include <math.h>

#include <etsoc/isa/cacheops.h>
#include <etsoc/isa/tensors.h>

#include "tpa/tpa.h"

#ifdef YV5N_SCRATCH_CONFIG_HEADER
#include YV5N_SCRATCH_CONFIG_HEADER
#endif

#define SCP_A_START    0u
#define SCP_B_START   16u
#define SCP_META_LINE 32u

#ifndef YV5N_ARENA_STORAGE_BYTES
#define YV5N_ARENA_STORAGE_BYTES (8u * 64u)
#define YV5N_ARENA_STATE_INITIALIZERS                                      \
    [0] = { .base = yolov5n_arena_storage + 0u, .cap = 64u },              \
    [2] = { .base = yolov5n_arena_storage + 64u, .cap = 64u },             \
    [4] = { .base = yolov5n_arena_storage + 128u, .cap = 64u },            \
    [6] = { .base = yolov5n_arena_storage + 192u, .cap = 64u },            \
    [8] = { .base = yolov5n_arena_storage + 256u, .cap = 64u },            \
    [10] = { .base = yolov5n_arena_storage + 320u, .cap = 64u },           \
    [12] = { .base = yolov5n_arena_storage + 384u, .cap = 64u },           \
    [14] = { .base = yolov5n_arena_storage + 448u, .cap = 64u },
#endif

static const int32_t byte_lane_idx[8] __attribute__((aligned(32))) = {
    0, 1, 2, 3, 4, 5, 6, 7,
};
static const int8_t zero_lane_bytes[8] __attribute__((aligned(8))) = {
    0, 0, 0, 0, 0, 0, 0, 0,
};

struct yolov5n_arena_state {
    uint8_t *base;
    uint32_t cap;
    uint32_t brk;
    uint32_t hi;
    uint32_t declared_cap;
};

static uint8_t yolov5n_arena_storage[YV5N_ARENA_STORAGE_BYTES]
    __attribute__((aligned(64)));

static struct yolov5n_arena_state yolov5n_arena[ARCH_NR_HARTS] = {
    YV5N_ARENA_STATE_INITIALIZERS
};

static inline struct yolov5n_arena_state *arena_self(void)
{
    uint32_t hartid = arch_runtime_hartid();

    if (hartid >= ARCH_NR_HARTS)
        return 0;

    return &yolov5n_arena[hartid];
}

static inline void arena_fail(void)
{
    __builtin_trap();
}

void yolov5n_arena_begin(uint32_t declared_peak_bytes)
{
    struct yolov5n_arena_state *st = arena_self();

    if (!st || !st->base || !st->cap)
        arena_fail();

    st->brk = 0;
    st->hi = 0;
    st->declared_cap = declared_peak_bytes ? declared_peak_bytes : st->cap;

    if (st->declared_cap > st->cap)
        arena_fail();
}

void yolov5n_arena_reset(void)
{
    struct yolov5n_arena_state *st = arena_self();

    if (!st || !st->base || !st->cap)
        arena_fail();

    st->brk = 0;
    st->hi = 0;
    st->declared_cap = st->cap;
}

void *yolov5n_arena_alloc(size_t bytes, size_t align)
{
    struct yolov5n_arena_state *st = arena_self();
    uint32_t limit;
    uint32_t off;
    uint32_t end;
    uintptr_t ptr;

    if (!st || !st->base || !st->cap)
        arena_fail();

    if (align == 0)
        align = 1;

    limit = st->declared_cap ? st->declared_cap : st->cap;
    off = (st->brk + (uint32_t)align - 1u) & ~((uint32_t)align - 1u);
    end = off + (uint32_t)bytes;
    if (end < off || end > limit || end > st->cap)
        arena_fail();

    ptr = (uintptr_t)st->base + off;
    st->brk = end;
    if (end > st->hi)
        st->hi = end;
    return (void *)ptr;
}

uint32_t yolov5n_arena_high_water(void)
{
    struct yolov5n_arena_state *st = arena_self();

    if (!st)
        return 0;

    return st->hi;
}

static inline void clear_tensor_error(void)
{
    asm volatile("csrwi tensor_error, 0" : : : "memory");
}

void yolov5n_enable_tensor_scratchpad(uint32_t *tensor_ready)
{
    uint64_t pmask = 0xff;

    if (*tensor_ready)
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
    *tensor_ready = 1;
}

static inline const int8_t *tl8_wblk(const yolov5n_tensor_layer_t *layer,
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

static inline void apply_lut_block_rows(const yolov5n_tensor_layer_t *layer,
                                        int8_t *dst, uint32_t out_stride,
                                        uint32_t rows)
{
    if (!layer->lut)
        return;

    for (uint32_t r = 0; r < rows; ++r) {
        int8_t *row = dst + r * out_stride;

        for (uint32_t c = 0; c < YV5N_OC_BLOCK; c += 8) {
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
                  [lut] "r"(layer->lut),
                  [dst] "r"(ptr)
                : "f0", "f1", "f31", "memory");
        }
    }
}

static int tensor_finish_block_rows(const yolov5n_tensor_layer_t *layer,
                                    int8_t *out_blk, uint32_t out_stride,
                                    uint32_t oc_blk, uint32_t rows,
                                    int32_t *acc_scratch)
{
    (void)acc_scratch;

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

    evict_range_to_l2(out_blk, (size_t)rows * out_stride);
    apply_lut_block_rows(layer, out_blk, out_stride, rows);
    return 0;
}

static int run_tensor_block(const yolov5n_tensor_layer_t *layer,
                            const int8_t *in_blk, uint32_t in_stride,
                            int8_t *out_blk, uint32_t out_stride,
                            uint32_t oc_blk, int32_t *acc_scratch)
{
    uint32_t acols = layer->k_block / 4u - 1u;

    for (uint32_t k_blk = 0; k_blk < layer->k_blocks; ++k_blk) {
        const int8_t *bblk = tl8_wblk(layer, oc_blk, k_blk);
        int last_kblk = (k_blk + 1u == layer->k_blocks);

        tensor_load(0, 0, SCP_A_START, 0, 0,
                    (uint64_t)(in_blk + (size_t)k_blk * layer->k_block),
                    0, YV5N_PIX_BLOCK - 1u, in_stride, 0);
        tensor_load(0, 0, SCP_B_START, 1, 0, (uint64_t)bblk, 0,
                    YV5N_PIX_BLOCK - 1u, layer->tl8_row_stride, 1);
        tensor_wait(TENSOR_LOAD_WAIT_0);
        tensor_wait(TENSOR_LOAD_WAIT_1);
        tensor_fma(0, 3, YV5N_PIX_BLOCK - 1u, acols, 0,
                   last_kblk, 0, 0, 0, SCP_B_START, SCP_A_START, 3,
                   k_blk == 0u);
    }

    return tensor_finish_block_rows(layer, out_blk, out_stride,
                                    oc_blk, YV5N_PIX_BLOCK, acc_scratch);
}

static void pack_1x1_block(const int8_t *src, uint32_t src_c,
                           int8_t *dst_rowblk, uint32_t row_stride)
{
    for (uint32_t r = 0; r < YV5N_PIX_BLOCK; ++r) {
        int8_t *dst_row = dst_rowblk + (size_t)r * row_stride;

        vec_zero_bytes(dst_row, row_stride);
        vec_copy_bytes(dst_row, src + (size_t)r * src_c, src_c);
    }
}

int yolov5n_run_conv1x1_layer(const yolov5n_tensor_layer_t *layer,
                              const int8_t *src, uint32_t nr_pix, uint32_t src_c,
                              int8_t *dst, uint32_t dst_c,
                              int8_t *packed_scratch, size_t packed_bytes,
                              int32_t *acc_scratch)
{
    if (layer->k_h != 1u || layer->k_w != 1u || layer->c_in != src_c ||
        layer->k_inner != src_c)
        return -1;

    if (layer->k_padded > src_c &&
        packed_bytes < (size_t)YV5N_PIX_BLOCK * layer->k_padded)
        return -1;

    for (size_t pix = 0; pix < nr_pix; pix += YV5N_PIX_BLOCK) {
        const int8_t *run_blk = src + pix * src_c;
        uint32_t run_stride = src_c;

        if (layer->k_padded != src_c) {
            pack_1x1_block(src + pix * src_c, src_c, packed_scratch, layer->k_padded);
            run_blk = packed_scratch;
            run_stride = layer->k_padded;
        }

        for (uint32_t oc_blk = 0; oc_blk < layer->oc_blocks; ++oc_blk) {
            if (run_tensor_block(layer, run_blk, run_stride,
                                 dst + pix * dst_c + oc_blk * YV5N_OC_BLOCK,
                                 dst_c, oc_blk, acc_scratch))
                return -1;
        }
    }

    return 0;
}

static void pack_conv_block(const yolov5n_tensor_layer_t *layer,
                            const int8_t *src,
                            uint32_t src_h, uint32_t src_w, uint32_t src_c,
                            uint32_t dst_w, size_t pix0,
                            int8_t *dst_rowblk, uint32_t row_stride)
{
    for (uint32_t r = 0; r < YV5N_PIX_BLOCK; ++r) {
        size_t out_pix = pix0 + r;
        uint32_t oy = (uint32_t)(out_pix / dst_w);
        uint32_t ox = (uint32_t)(out_pix % dst_w);
        int8_t *dst_row = dst_rowblk + (size_t)r * row_stride;

        vec_zero_bytes(dst_row, row_stride);

        for (uint32_t ky = 0; ky < layer->k_h; ++ky) {
            int32_t iy =
                (int32_t)(oy * layer->stride_h + ky) - (int32_t)layer->pad_h;

            for (uint32_t kx = 0; kx < layer->k_w; ++kx) {
                int32_t ix =
                    (int32_t)(ox * layer->stride_w + kx) - (int32_t)layer->pad_w;
                uint32_t tap = (ky * layer->k_w + kx) * src_c;
                int8_t *tap_dst = dst_row + tap;

                if (iy < 0 || iy >= (int32_t)src_h ||
                    ix < 0 || ix >= (int32_t)src_w) {
                    continue;
                }

                vec_copy_bytes(
                    tap_dst,
                    src + (((size_t)(uint32_t)iy * src_w + (uint32_t)ix) * src_c),
                    src_c);
            }
        }
    }
}

int yolov5n_run_convk_packed_layer(const yolov5n_tensor_layer_t *layer,
                                   const int8_t *src,
                                   uint32_t src_h, uint32_t src_w, uint32_t src_c,
                                   int8_t *dst,
                                   uint32_t dst_h, uint32_t dst_w, uint32_t dst_c,
                                   int8_t *patch_scratch, size_t patch_bytes,
                                   int32_t *acc_scratch)
{
    size_t nr_pix = (size_t)dst_h * dst_w;

    if (layer->c_in != src_c ||
        layer->k_inner != layer->k_h * layer->k_w * src_c ||
        patch_bytes < (size_t)YV5N_PIX_BLOCK * layer->k_padded)
        return -1;

    for (size_t pix = 0; pix < nr_pix; pix += YV5N_PIX_BLOCK) {
        pack_conv_block(layer, src, src_h, src_w, src_c, dst_w, pix,
                        patch_scratch, layer->k_padded);

        for (uint32_t oc_blk = 0; oc_blk < layer->oc_blocks; ++oc_blk) {
            if (run_tensor_block(layer, patch_scratch, layer->k_padded,
                                 dst + pix * dst_c + oc_blk * YV5N_OC_BLOCK,
                                 dst_c, oc_blk, acc_scratch))
                return -1;
        }
    }

    return 0;
}

static int round_away_f32(float x)
{
    return (x >= 0.0f) ? (int)(x + 0.5f) : (int)(x - 0.5f);
}

static uint32_t yolov5n_f32_bits(float x)
{
    union {
        float f;
        uint32_t u;
    } cvt = { .f = x };

    return cvt.u;
}

static const int8_t *yolov5n_lookup_residual_lut(uint32_t src_bits, uint32_t dst_bits)
{
    if (src_bits == 0x3d5a1700u && dst_bits == 0x3dd9b9cfu)
        return yv5n_p0_residual_lut0;
    if (src_bits == 0x3c8c4f76u && dst_bits == 0x3ca7b1a9u)
        return yv5n_p1_residual_lut0;
    if (src_bits == 0x3ca7b1a9u && dst_bits == 0x3d228e9cu)
        return yv5n_p1_residual_lut1;
    if (src_bits == 0x3be9f979u && dst_bits == 0x3c8f9c54u)
        return yv5n_p2_residual_lut0;
    if (src_bits == 0x3c8f9c54u && dst_bits == 0x3c90bd12u)
        return yv5n_p2_residual_lut1;
    if (src_bits == 0x3c90bd12u && dst_bits == 0x3cef9d72u)
        return yv5n_p2_residual_lut2;
    if (src_bits == 0x3b9d77c6u && dst_bits == 0x3ceac536u)
        return yv5n_p3_residual_lut0;
    return 0;
}

void yolov5n_build_residual_lut(const yolov5n_tensor_layer_t *src_layer,
                                const yolov5n_tensor_layer_t *dst_layer,
                                int8_t *lut, int *ready)
{
    const int8_t *precomputed;
    uint32_t src_bits;
    uint32_t dst_bits;

    if (*ready)
        return;

    src_bits = yolov5n_f32_bits(src_layer->act_out_scale);
    dst_bits = yolov5n_f32_bits(dst_layer->act_out_scale);
    precomputed = yolov5n_lookup_residual_lut(src_bits, dst_bits);
    if (precomputed) {
        for (int i = 0; i < 256; ++i)
            lut[i] = precomputed[i];
        *ready = 1;
        return;
    }

    float ratio = src_layer->act_out_scale / dst_layer->act_out_scale;

    for (int i = 0; i < 256; ++i) {
        int8_t v = (int8_t)(uint8_t)i;
        int q = round_away_f32((float)v * ratio);
        if (q < -128)
            q = -128;
        if (q > 127)
            q = 127;
        lut[i] = (int8_t)q;
    }

    *ready = 1;
}

void yolov5n_build_silu_lut(float scale, int8_t *lut, int *ready)
{
    if (*ready)
        return;

    for (int i = 0; i < 256; ++i) {
        int8_t q = (int8_t)(uint8_t)i;
        float x = (float)q * scale;
        float y = x / (1.0f + expf(-x));
        int v = round_away_f32(y / scale);

        if (v < -128)
            v = -128;
        if (v > 127)
            v = 127;
        lut[i] = (int8_t)v;
    }

    *ready = 1;
}

void yolov5n_add_residual_block(const int8_t *skip_px, int8_t *body_px,
                                size_t n, const int8_t *residual_lut)
{
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
              [lut] "r"(residual_lut),
              [dst] "r"(dst)
            : "f0", "f1", "f2", "f31", "memory");
    }
}

void yolov5n_concat2(const int8_t *a, uint32_t a_c,
                     const int8_t *b, uint32_t b_c,
                     size_t nr_pix, int8_t *dst)
{
    uint32_t out_c = a_c + b_c;

    for (size_t pix = 0; pix < nr_pix; ++pix) {
        int8_t *dst_px = dst + pix * out_c;

        vec_copy_bytes(dst_px, a + pix * a_c, a_c);
        vec_copy_bytes(dst_px + a_c, b + pix * b_c, b_c);
    }
}

void yolov5n_concat4(const int8_t *a, const int8_t *b,
                     const int8_t *c, const int8_t *d,
                     uint32_t c_each, size_t nr_pix, int8_t *dst)
{
    uint32_t out_c = c_each * 4u;

    for (size_t pix = 0; pix < nr_pix; ++pix) {
        int8_t *dst_px = dst + pix * out_c;

        vec_copy_bytes(dst_px, a + pix * c_each, c_each);
        vec_copy_bytes(dst_px + c_each, b + pix * c_each, c_each);
        vec_copy_bytes(dst_px + 2u * c_each, c + pix * c_each, c_each);
        vec_copy_bytes(dst_px + 3u * c_each, d + pix * c_each, c_each);
    }
}

void yolov5n_maxpool5x5_s1_p2(const int8_t *src,
                              uint32_t h, uint32_t w, uint32_t c,
                              int8_t *dst, int8_t *tmp)
{
    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            int8_t *tmp_px = tmp + (((size_t)y * w + x) * c);

            for (uint32_t ch = 0; ch < c; ch += 8) {
                uint32_t x0 = (x > 2u) ? (x - 2u) : 0u;
                uint32_t x1 = x + 2u;
                const int8_t *src_px;

                if (x1 >= w)
                    x1 = w - 1u;

                src_px = src + (((size_t)y * w + x0) * c) + ch;
                vec_copy8(tmp_px + ch, src_px);

                for (uint32_t xx = x0 + 1u; xx <= x1; ++xx) {
                    src_px = src + (((size_t)y * w + xx) * c) + ch;
                    vec_max_inplace8(tmp_px + ch, src_px);
                }
            }
        }
    }

    for (uint32_t y = 0; y < h; ++y) {
        uint32_t y0 = (y > 2u) ? (y - 2u) : 0u;
        uint32_t y1 = y + 2u;

        if (y1 >= h)
            y1 = h - 1u;

        for (uint32_t x = 0; x < w; ++x) {
            int8_t *dst_px = dst + (((size_t)y * w + x) * c);

            for (uint32_t ch = 0; ch < c; ch += 8) {
                const int8_t *tmp_px =
                    tmp + (((size_t)y0 * w + x) * c) + ch;

                vec_copy8(dst_px + ch, tmp_px);

                for (uint32_t yy = y0 + 1u; yy <= y1; ++yy) {
                    tmp_px = tmp + (((size_t)yy * w + x) * c) + ch;
                    vec_max_inplace8(dst_px + ch, tmp_px);
                }
            }
        }
    }
}
