#ifndef TPA_YOLOV5N_COMMON_H
#define TPA_YOLOV5N_COMMON_H

#include <stddef.h>
#include <stdint.h>

#include "test.h"
#include "tpa/tpa.h"

#define YV5N_PIX_BLOCK 16u
#define YV5N_OC_BLOCK  16u
#define YV5N_ALIGN_UP(bytes, align) \
    (((uint32_t)(bytes) + ((uint32_t)(align) - 1u)) & ~((uint32_t)(align) - 1u))
#define YV5N_ARENA_STEP(base, bytes, align) \
    (YV5N_ALIGN_UP((base), (align)) + (uint32_t)(bytes))

typedef struct {
    const int8_t *tl8_b;
    const int32_t (*bias_blk)[YV5N_OC_BLOCK];
    const float (*scale_blk)[YV5N_OC_BLOCK];
    const uint8_t *lut;
    uint32_t c_in;
    uint32_t k_out;
    uint32_t k_h;
    uint32_t k_w;
    uint32_t stride_h;
    uint32_t stride_w;
    uint32_t pad_h;
    uint32_t pad_w;
    uint32_t k_inner;
    uint32_t k_padded;
    uint32_t k_block;
    uint32_t k_blocks;
    uint32_t tl8_row_stride;
    uint32_t oc_blocks;
    float act_in_scale;
    float act_out_scale;
} yolov5n_tensor_layer_t;

typedef struct {
    const uint8_t *lut;
    uint32_t c_in;
    uint32_t k_out;
    uint32_t k_h;
    uint32_t k_w;
    float act_in_scale;
    float act_out_scale;
    uint8_t exact_post;
} yolov5n_layer_meta_t;

void yolov5n_enable_tensor_scratchpad(uint32_t *tensor_ready);

static inline void yolov5n_diag_putc(char c)
{
    arch_diag_putc(c);
}

static inline void yolov5n_diag_puts(const char *s)
{
    while (*s)
        yolov5n_diag_putc(*s++);
}

#define YV5N_FAIL_STOP() do { \
    TEST_FAIL; \
    return tpa_stop(); \
} while (0)

#define YV5N_FAIL_MSG_STOP(msg) do { \
    yolov5n_diag_puts(msg); \
    TEST_FAIL; \
    return tpa_stop(); \
} while (0)

#define YV5N_REQUIRE_STOP(cond) do { \
    if (!(cond)) \
        YV5N_FAIL_STOP(); \
} while (0)

#define YV5N_REQUIRE_MSG_STOP(msg, cond) do { \
    if (!(cond)) \
        YV5N_FAIL_MSG_STOP(msg); \
} while (0)

#define YV5N_TRY_STOP(expr) do { \
    if ((expr)) \
        YV5N_FAIL_STOP(); \
} while (0)

#define YV5N_TRY_MSG_STOP(msg, expr) do { \
    if ((expr)) \
        YV5N_FAIL_MSG_STOP(msg); \
} while (0)

static inline int8_t *yolov5n_send_buf(struct tpa_chan *ch)
{
    void *buf = tpa_send_buf(ch);

    if (!buf)
        __builtin_trap();

    return (int8_t *)buf;
}

int yolov5n_run_conv1x1_layer(const yolov5n_tensor_layer_t *layer,
                              const int8_t *src, uint32_t nr_pix, uint32_t src_c,
                              int8_t *dst, uint32_t dst_c,
                              int8_t *packed_scratch, size_t packed_bytes,
                              int32_t *acc_scratch);

int yolov5n_run_convk_packed_layer(const yolov5n_tensor_layer_t *layer,
                                   const int8_t *src,
                                   uint32_t src_h, uint32_t src_w, uint32_t src_c,
                                   int8_t *dst,
                                   uint32_t dst_h, uint32_t dst_w, uint32_t dst_c,
                                   int8_t *patch_scratch, size_t patch_bytes,
                                   int32_t *acc_scratch);

void yolov5n_build_residual_lut(const yolov5n_tensor_layer_t *src_layer,
                                const yolov5n_tensor_layer_t *dst_layer,
                                int8_t *lut, int *ready);

void yolov5n_build_silu_lut(float scale, int8_t *lut, int *ready);

void yolov5n_arena_begin(uint32_t declared_peak_bytes);
void yolov5n_arena_reset(void);
void *yolov5n_arena_alloc(size_t bytes, size_t align);
uint32_t yolov5n_arena_high_water(void);

void yolov5n_add_residual_block(const int8_t *skip_px, int8_t *body_px,
                                size_t n, const int8_t *residual_lut);

void yolov5n_concat2(const int8_t *a, uint32_t a_c,
                     const int8_t *b, uint32_t b_c,
                     size_t nr_pix, int8_t *dst);

void yolov5n_concat4(const int8_t *a, const int8_t *b,
                     const int8_t *c, const int8_t *d,
                     uint32_t c_each, size_t nr_pix, int8_t *dst);

void yolov5n_maxpool5x5_s1_p2(const int8_t *src,
                              uint32_t h, uint32_t w, uint32_t c,
                              int8_t *dst, int8_t *tmp);

#endif
