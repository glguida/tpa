#include "yolov8n_detect_downstream.h"

#include <stdint.h>

#ifndef TPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER
#error "Configure with -DTPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER=/path/to/yolov8n_external_detect_c2f_weights.h"
#endif
#include TPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER

#define YV8N_P4_P5_NECK_TRACE_BEGIN 0xe8e40000u
#define YV8N_P4_P5_NECK_TRACE_MODEL9_SENT 0xe8e40001u
#define YV8N_P4_P5_NECK_TRACE_MODEL19_DONE 0xe8e40002u
#define YV8N_P4_P5_NECK_TRACE_CONCAT_DONE 0xe8e40003u
#define YV8N_P4_P5_NECK_TRACE_CHECK_PASS 0xe8e40004u
#define YV8N_P4_P5_NECK_TRACE_FAIL 0xe8e4feeu

#define YV8N_P4_DENSE_C2F_SOURCE_PID 880u
#define YV8N_P4_DENSE_C2F_PID 881u
#define YV8N_P4_P5_MODEL19_PID 910u
#define YV8N_P4_P5_MODEL9_SOURCE_PID 911u
#define YV8N_P4_P5_CONCAT_PID 912u
#define YV8N_P4_P5_CHECKER_PID 913u

#define YV8N_P4_DENSE_C2F_IN_H 40u
#define YV8N_P4_DENSE_C2F_IN_W 40u
#define YV8N_P4_DENSE_C2F_IN_C 192u
#define YV8N_P4_DENSE_C2F_OUT_C 128u
#define YV8N_P4_DENSE_C2F_HIDDEN_C 64u
#define YV8N_P4_DENSE_C2F_POINTS (YV8N_P4_DENSE_C2F_IN_H * YV8N_P4_DENSE_C2F_IN_W)
#define YV8N_P4_DENSE_C2F_INPUT_BYTES (YV8N_P4_DENSE_C2F_POINTS * YV8N_P4_DENSE_C2F_IN_C)
#define YV8N_P4_DENSE_C2F_OUTPUT_BYTES (YV8N_P4_DENSE_C2F_POINTS * YV8N_P4_DENSE_C2F_OUT_C)
#define YV8N_P4_DENSE_C2F_SCRATCH_PEAK_BYTES 524288u
#define YV8N_P4_DENSE_C2F_DETECT_SAMPLE_POINTS 8u
#define YV8N_P4_DENSE_C2F_SUMMARY_MAGIC UINT32_C(0x59384434) /* "Y8D4" */
#define YV8N_P4_DENSE_C2F_SUMMARY_VERSION 1u

#define YV8N_P4_P5_MODEL19_ID 31u
#define YV8N_P4_P5_MODEL19_MAGIC UINT32_C(0x59384e39) /* "Y8N9" */
#define YV8N_P4_P5_CONCAT_MAGIC UINT32_C(0x59384354)  /* "Y8CT" */
#define YV8N_P4_P5_SUMMARY_VERSION 1u
#define YV8N_P4_P5_MODEL19_IN_H 40u
#define YV8N_P4_P5_MODEL19_IN_W 40u
#define YV8N_P4_P5_MODEL19_IN_C 128u
#define YV8N_P4_P5_MODEL19_OUT_H 20u
#define YV8N_P4_P5_MODEL19_OUT_W 20u
#define YV8N_P4_P5_MODEL19_OUT_C 128u
#define YV8N_P4_P5_MODEL19_POINTS \
    (YV8N_P4_P5_MODEL19_OUT_H * YV8N_P4_P5_MODEL19_OUT_W)
#define YV8N_P4_P5_MODEL19_INPUT_BYTES \
    (YV8N_P4_P5_MODEL19_IN_H * YV8N_P4_P5_MODEL19_IN_W * \
     YV8N_P4_P5_MODEL19_IN_C)
#define YV8N_P4_P5_MODEL19_OUTPUT_BYTES \
    (YV8N_P4_P5_MODEL19_POINTS * YV8N_P4_P5_MODEL19_OUT_C)
#define YV8N_P4_P5_MODEL9_H 20u
#define YV8N_P4_P5_MODEL9_W 20u
#define YV8N_P4_P5_MODEL9_C 256u
#define YV8N_P4_P5_MODEL9_BYTES \
    (YV8N_P4_P5_MODEL9_H * YV8N_P4_P5_MODEL9_W * YV8N_P4_P5_MODEL9_C)
#define YV8N_P4_P5_CONCAT_C \
    (YV8N_P4_P5_MODEL19_OUT_C + YV8N_P4_P5_MODEL9_C)
#define YV8N_P4_P5_CONCAT_BYTES \
    (YV8N_P4_P5_MODEL19_POINTS * YV8N_P4_P5_CONCAT_C)
#define YV8N_P4_P5_MODEL19_SCRATCH_PEAK_BYTES 4096u
#define YV8N_P4_P5_CONCAT_SCRATCH_PEAK_BYTES 0u
#define YV8N_P4_P5_LAYOUT_SAMPLE_POINTS 4u

#define FNV1A64_INIT UINT64_C(0xcbf29ce484222325)
#define FNV1A64_PRIME UINT64_C(0x100000001b3)

#define YV8N_P4_DENSE_C2F_EXPECT_INPUT_HASH UINT64_C(0x6da53b6d0122b9a5)
#define YV8N_P4_DENSE_C2F_EXPECT_LAYER_HASH UINT64_C(0xc759e3e9b51565bc)
#define YV8N_P4_DENSE_C2F_EXPECT_OUTPUT_HASH UINT64_C(0x689e9690e25a7f2e)
#define YV8N_P4_DENSE_C2F_EXPECT_POINT_HASH UINT64_C(0x5a921301de678525)
#define YV8N_P4_DENSE_C2F_EXPECT_COMPUTED_POINTS 1600u
#define YV8N_P4_DENSE_C2F_EXPECT_FIRST0 -11
#define YV8N_P4_DENSE_C2F_EXPECT_FIRST1 -8
#define YV8N_P4_DENSE_C2F_EXPECT_FIRST2 -10
#define YV8N_P4_DENSE_C2F_EXPECT_FIRST3 -11

#define YV8N_P4_P5_MODEL19_EXPECT_INPUT_HASH UINT64_C(0x689e9690e25a7f2e)
#define YV8N_P4_P5_MODEL19_EXPECT_LAYER_HASH UINT64_C(0x29ad1f3297a94e56)
#define YV8N_P4_P5_MODEL19_EXPECT_OUTPUT_HASH UINT64_C(0xa5c44f41068d6c43)
#define YV8N_P4_P5_MODEL19_EXPECT_POINT_HASH UINT64_C(0x3b8ec8550d64d7b5)
#define YV8N_P4_P5_MODEL19_EXPECT_FIRST0 52
#define YV8N_P4_P5_MODEL19_EXPECT_FIRST1 13
#define YV8N_P4_P5_MODEL19_EXPECT_FIRST2 -15
#define YV8N_P4_P5_MODEL19_EXPECT_FIRST3 3

#define YV8N_P4_P5_CONCAT_EXPECT_MODEL9_HASH UINT64_C(0x9b16c0453065b725)
#define YV8N_P4_P5_CONCAT_EXPECT_OUTPUT_HASH UINT64_C(0x4a6829162fa17343)
#define YV8N_P4_P5_CONCAT_EXPECT_LAYOUT_HASH UINT64_C(0xddbcba325f750cea)
#define YV8N_P4_P5_CONCAT_EXPECT_MODEL9_FIRST0 28
#define YV8N_P4_P5_CONCAT_EXPECT_MODEL9_FIRST1 49
#define YV8N_P4_P5_CONCAT_EXPECT_MODEL9_FIRST2 70
#define YV8N_P4_P5_CONCAT_EXPECT_MODEL9_FIRST3 91

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t input_bytes;
    uint32_t output_bytes;
    uint32_t input_h;
    uint32_t input_w;
    uint32_t input_c;
    uint32_t output_c;
    uint32_t hidden_c;
    uint32_t detect_sample_points;
    uint32_t scratch_peak_bytes;
    uint32_t sampled_output_points;
    uint64_t input_hash;
    uint64_t c2f_layer_hash;
    uint64_t output_hash;
    uint64_t sampled_output_hash;
    uint64_t computed_point_hash;
    uint32_t detect_sample_point_ids[YV8N_DETECT_SAMPLE_POINTS];
    uint32_t computed_point_count;
    int32_t first_output_q8[4];
    uint32_t reserved[5];
} yolov8n_p4_dense_c2f_summary_t;

TPA_STATIC_ASSERT(sizeof(yolov8n_p4_dense_c2f_summary_t) == 160u,
                  "YOLOv8n dense P4 C2f summary edge size must match .tpp");

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t input_bytes;
    uint32_t output_bytes;
    uint32_t input_h;
    uint32_t input_w;
    uint32_t input_c;
    uint32_t output_h;
    uint32_t output_w;
    uint32_t output_c;
    uint32_t stride_h;
    uint32_t stride_w;
    uint32_t pad_h;
    uint32_t pad_w;
    uint32_t scratch_peak_bytes;
    uint32_t computed_points;
    uint32_t layer_id;
    uint32_t reserved0;
    uint64_t input_hash;
    uint64_t layer_hash;
    uint64_t output_hash;
    uint64_t point_hash;
    int32_t first_output_q8[4];
    uint32_t reserved[10];
} yolov8n_p4_p5_model19_summary_t;

TPA_STATIC_ASSERT(sizeof(yolov8n_p4_p5_model19_summary_t) == 160u,
                  "YOLOv8n P4-to-P5 model.19 summary edge size must match .tpp");

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t model19_bytes;
    uint32_t model9_bytes;
    uint32_t output_bytes;
    uint32_t height;
    uint32_t width;
    uint32_t model19_channels;
    uint32_t model9_channels;
    uint32_t output_channels;
    uint32_t model19_first_channel;
    uint32_t model9_first_channel;
    uint32_t full_edge_bytes;
    uint32_t layout_sample_points;
    uint32_t scratch_peak_bytes;
    uint32_t reserved0;
    uint64_t model19_hash;
    uint64_t model9_hash;
    uint64_t output_hash;
    uint64_t layout_hash;
    uint64_t checker_edge_hash;
    int32_t first_model19_q8[4];
    int32_t first_model9_q8[4];
    uint32_t reserved[6];
} yolov8n_p4_p5_concat_summary_t;

TPA_STATIC_ASSERT(sizeof(yolov8n_p4_p5_concat_summary_t) == 160u,
                  "YOLOv8n P4-to-P5 concat summary edge size must match .tpp");

typedef struct {
    int8_t out_vec[YV8N_P4_P5_MODEL19_OUT_C];
} model19_scratch_t;

TPA_STATIC_ASSERT(sizeof(model19_scratch_t) <= YV8N_P4_P5_MODEL19_SCRATCH_PEAK_BYTES,
                  "YOLOv8n P4-to-P5 model.19 scratch declaration is too small");

typedef struct {
    const int8_t *input;
    uint32_t input_len;
    int8_t *output;
    yolov8n_p4_p5_model19_summary_t summary;
} model19_ws_t;

typedef struct {
    const int8_t *model19;
    const int8_t *model9;
    uint32_t model19_len;
    uint32_t model9_len;
    int8_t *output;
    yolov8n_p4_p5_concat_summary_t summary;
} concat_ws_t;

typedef struct {
    const yolov8n_p4_dense_c2f_summary_t *p4_summary;
    const yolov8n_p4_p5_model19_summary_t *model19_summary;
    const int8_t *concat_edge;
    const yolov8n_p4_p5_concat_summary_t *concat_summary;
    uint32_t p4_len;
    uint32_t model19_len;
    uint32_t concat_edge_len;
    uint32_t concat_summary_len;
} checker_ws_t;

TPA_STATIC_ASSERT(sizeof(model19_ws_t) <= 256u,
                  "YOLOv8n P4-to-P5 model.19 workspace exceeds .tpm declaration");
TPA_STATIC_ASSERT(sizeof(concat_ws_t) <= 256u,
                  "YOLOv8n P4-to-P5 concat workspace exceeds .tpm declaration");
TPA_STATIC_ASSERT(sizeof(checker_ws_t) <= 96u,
                  "YOLOv8n P4-to-P5 checker workspace exceeds .tpm declaration");

TPA_PROC_MEM_META(yolov8n_p4_p5_model19_meta,
                  YV8N_P4_P5_MODEL19_PID, YV8N_P4_P5_MODEL19_SCRATCH_PEAK_BYTES);
TPA_PROC_MEM_META(yolov8n_p4_p5_model9_source_meta,
                  YV8N_P4_P5_MODEL9_SOURCE_PID, 0u);
TPA_PROC_MEM_META(yolov8n_p4_p5_concat_meta,
                  YV8N_P4_P5_CONCAT_PID, YV8N_P4_P5_CONCAT_SCRATCH_PEAK_BYTES);
TPA_PROC_MEM_META(yolov8n_p4_p5_checker_meta,
                  YV8N_P4_P5_CHECKER_PID, 0u);

tpa_op_t yolov8n_p4_p5_model19_start(void);
tpa_op_t yolov8n_p4_p5_model19_run(void);
tpa_op_t yolov8n_p4_p5_model19_send_summary(void);
tpa_op_t yolov8n_p4_p5_model19_done(void);
tpa_op_t yolov8n_p4_p5_model9_source_start(void);
tpa_op_t yolov8n_p4_p5_model9_source_done(void);
tpa_op_t yolov8n_p4_p5_concat_start(void);
tpa_op_t yolov8n_p4_p5_concat_recv_model9(void);
tpa_op_t yolov8n_p4_p5_concat_run(void);
tpa_op_t yolov8n_p4_p5_concat_send_summary(void);
tpa_op_t yolov8n_p4_p5_concat_done(void);
tpa_op_t yolov8n_p4_p5_checker_start(void);
tpa_op_t yolov8n_p4_p5_checker_recv_model19(void);
tpa_op_t yolov8n_p4_p5_checker_recv_concat_edge(void);
tpa_op_t yolov8n_p4_p5_checker_recv_concat_summary(void);
tpa_op_t yolov8n_p4_p5_checker_run(void);

static const uint32_t p4_sample_points[YV8N_P4_DENSE_C2F_DETECT_SAMPLE_POINTS] = {
    0u, 1u, 39u, 40u, 333u, 800u, 1511u, 1599u,
};

static const uint32_t layout_sample_points[YV8N_P4_P5_LAYOUT_SAMPLE_POINTS] = {
    0u, 1u, 21u, 399u,
};

static void mark(uint32_t v)
{
    arch_trace(v);
}

static void diag_hex64(const char *label, uint64_t v)
{
    static const char hex[] = "0123456789abcdef";

    yolov8n_detect_diag_puts(label);
    for (uint32_t i = 0; i < 16u; ++i) {
        uint32_t shift = (15u - i) * 4u;

        yolov8n_detect_diag_putc(hex[(v >> shift) & 0xfu]);
    }
    yolov8n_detect_diag_putc('\n');
}

static uint64_t fnv1a64_step(uint64_t h, uint8_t v)
{
    h ^= (uint64_t)v;
    h *= FNV1A64_PRIME;
    return h;
}

static uint64_t hash_u32_value(uint64_t h, uint32_t v)
{
    h = fnv1a64_step(h, (uint8_t)(v & 0xffu));
    h = fnv1a64_step(h, (uint8_t)((v >> 8) & 0xffu));
    h = fnv1a64_step(h, (uint8_t)((v >> 16) & 0xffu));
    h = fnv1a64_step(h, (uint8_t)((v >> 24) & 0xffu));
    return h;
}

static uint64_t hash_i8_bytes(uint64_t h, const int8_t *buf, uint32_t n)
{
    for (uint32_t i = 0; i < n; ++i)
        h = fnv1a64_step(h, (uint8_t)buf[i]);
    return h;
}

static uint64_t hash_full_i8(const int8_t *buf, uint32_t bytes)
{
    return hash_i8_bytes(FNV1A64_INIT, buf, bytes);
}

static int8_t u8_to_i8(uint8_t v)
{
    if (v < 128u)
        return (int8_t)v;
    return (int8_t)((int32_t)v - 256);
}

static int32_t round_to_i32(float v)
{
    if (v >= 0.0f)
        return (int32_t)(v + 0.5f);
    return (int32_t)(v - 0.5f);
}

static int8_t clamp_i8(int32_t v)
{
    if (v < -128)
        return -128;
    if (v > 127)
        return 127;
    return (int8_t)v;
}

static int8_t requant_layer_value(
    const yolov8n_external_detect_c2f_layer_t *layer,
    uint32_t oc,
    int32_t acc)
{
    float scaled = ((float)acc + (float)layer->b[oc]) * layer->s[oc];
    int8_t q = clamp_i8(round_to_i32(scaled));

    if (layer->lut)
        q = u8_to_i8(layer->lut[(uint8_t)q]);

    return q;
}

static uint64_t hash_layer_set(const uint32_t *layer_ids, uint32_t n)
{
    uint64_t h = FNV1A64_INIT;

    for (uint32_t i = 0; i < n; ++i) {
        uint32_t id = layer_ids[i];
        const yolov8n_external_detect_c2f_layer_t *layer =
            &yolov8n_external_detect_c2f_layers[id];
        uint32_t has_lut = layer->lut ? 1u : 0u;

        h = hash_u32_value(h, id);
        h = hash_u32_value(h, layer->K_out);
        h = hash_u32_value(h, layer->C_in);
        h = hash_u32_value(h, layer->kH);
        h = hash_u32_value(h, layer->kW);
        h = hash_u32_value(h, layer->stride_h);
        h = hash_u32_value(h, layer->stride_w);
        h = hash_u32_value(h, layer->pad_h);
        h = hash_u32_value(h, layer->pad_w);
        h = hash_u32_value(h, layer->K_inner);
        h = hash_u32_value(h, layer->w_stride);
        h = hash_u32_value(h, layer->K_out_pad);
        h = hash_u32_value(h, has_lut);
        h = hash_i8_bytes(h, layer->w, layer->K_out * layer->w_stride);
        for (uint32_t b = 0; b < layer->K_out_pad; ++b)
            h = hash_u32_value(h, (uint32_t)layer->b[b]);
        if (layer->lut) {
            for (uint32_t j = 0; j < 256u; ++j)
                h = fnv1a64_step(h, layer->lut[j]);
        }
    }

    return h;
}

static uint64_t hash_model19_layer(void)
{
    static const uint32_t layer_ids[] = { YV8N_P4_P5_MODEL19_ID };

    return hash_layer_set(layer_ids,
                          (uint32_t)(sizeof(layer_ids) / sizeof(layer_ids[0])));
}

static int verify_model19_layer(void)
{
    const yolov8n_external_detect_c2f_layer_t *layer =
        &yolov8n_external_detect_c2f_layers[YV8N_P4_P5_MODEL19_ID];

    if (YOLOV8N_EXTERNAL_DETECT_C2F_N_LAYERS != 32u)
        return -1;
    if (layer->K_out != YV8N_P4_P5_MODEL19_OUT_C ||
        layer->C_in != YV8N_P4_P5_MODEL19_IN_C ||
        layer->kH != 3u || layer->kW != 3u ||
        layer->stride_h != 2u || layer->stride_w != 2u ||
        layer->pad_h != 1u || layer->pad_w != 1u ||
        layer->K_inner != 9u * YV8N_P4_P5_MODEL19_IN_C ||
        layer->w_stride != 9u * YV8N_P4_P5_MODEL19_IN_C ||
        layer->K_out_pad < YV8N_P4_P5_MODEL19_OUT_C || !layer->lut)
        return -1;
    return 0;
}

static int8_t model9_sppf_value(uint32_t y, uint32_t x, uint32_t c)
{
    uint32_t v = (y * 53u + x * 31u + c * 7u +
                  (y + 11u) * (c + 5u) + ((x ^ c) * 3u) + 101u) & 0xffu;

    return (int8_t)((int32_t)v - 128);
}

static void fill_model9_source_edge(int8_t *out)
{
    for (uint32_t y = 0; y < YV8N_P4_P5_MODEL9_H; ++y) {
        for (uint32_t x = 0; x < YV8N_P4_P5_MODEL9_W; ++x) {
            for (uint32_t c = 0; c < YV8N_P4_P5_MODEL9_C; ++c) {
                size_t idx = (((size_t)y * YV8N_P4_P5_MODEL9_W + x) *
                              YV8N_P4_P5_MODEL9_C) + c;

                out[idx] = model9_sppf_value(y, x, c);
            }
        }
    }
}

static void model19_conv_at(const yolov8n_external_detect_c2f_layer_t *layer,
                            const int8_t *input,
                            uint32_t oy,
                            uint32_t ox,
                            int8_t *out)
{
    for (uint32_t oc = 0; oc < layer->K_out; ++oc) {
        int32_t acc = 0;

        for (uint32_t ky = 0; ky < layer->kH; ++ky) {
            int32_t iy = (int32_t)(oy * layer->stride_h + ky) -
                         (int32_t)layer->pad_h;

            for (uint32_t kx = 0; kx < layer->kW; ++kx) {
                int32_t ix = (int32_t)(ox * layer->stride_w + kx) -
                             (int32_t)layer->pad_w;
                size_t w_base = (size_t)oc * layer->w_stride +
                                ((size_t)ky * layer->kW + kx) * layer->C_in;

                if (iy < 0 || iy >= (int32_t)YV8N_P4_P5_MODEL19_IN_H ||
                    ix < 0 || ix >= (int32_t)YV8N_P4_P5_MODEL19_IN_W)
                    continue;

                const int8_t *in = input +
                    (((size_t)(uint32_t)iy * YV8N_P4_P5_MODEL19_IN_W +
                      (uint32_t)ix) * YV8N_P4_P5_MODEL19_IN_C);
                for (uint32_t ic = 0; ic < layer->C_in; ++ic)
                    acc += (int32_t)in[ic] * (int32_t)layer->w[w_base + ic];
            }
        }
        out[oc] = requant_layer_value(layer, oc, acc);
    }
}

static uint64_t hash_layout_samples(const int8_t *concat)
{
    uint64_t h = FNV1A64_INIT;

    for (uint32_t i = 0; i < YV8N_P4_P5_LAYOUT_SAMPLE_POINTS; ++i) {
        uint32_t point = layout_sample_points[i];
        const int8_t *base = concat + (size_t)point * YV8N_P4_P5_CONCAT_C;

        h = hash_u32_value(h, point);
        h = fnv1a64_step(h, (uint8_t)base[0]);
        h = fnv1a64_step(h, (uint8_t)base[YV8N_P4_P5_MODEL19_OUT_C - 1u]);
        h = fnv1a64_step(h, (uint8_t)base[YV8N_P4_P5_MODEL19_OUT_C]);
        h = fnv1a64_step(h, (uint8_t)base[YV8N_P4_P5_CONCAT_C - 1u]);
    }

    return h;
}

static int run_model19(const int8_t *input,
                       int8_t *output,
                       yolov8n_p4_p5_model19_summary_t *summary)
{
    const yolov8n_external_detect_c2f_layer_t *layer =
        &yolov8n_external_detect_c2f_layers[YV8N_P4_P5_MODEL19_ID];
    model19_scratch_t *scratch;
    uint64_t point_hash = FNV1A64_INIT;

    yolov8n_p5_arena_begin(YV8N_P4_P5_MODEL19_SCRATCH_PEAK_BYTES);
    scratch = yolov8n_p5_arena_alloc(sizeof(*scratch), 64u);
    if (!scratch)
        return -1;

    for (uint32_t point = 0; point < YV8N_P4_P5_MODEL19_POINTS; ++point) {
        uint32_t y = point / YV8N_P4_P5_MODEL19_OUT_W;
        uint32_t x = point % YV8N_P4_P5_MODEL19_OUT_W;
        int8_t *out = output + (size_t)point * YV8N_P4_P5_MODEL19_OUT_C;

        model19_conv_at(layer, input, y, x, scratch->out_vec);
        for (uint32_t c = 0; c < YV8N_P4_P5_MODEL19_OUT_C; ++c)
            out[c] = scratch->out_vec[c];
        point_hash = hash_u32_value(point_hash, point);
    }

    summary->magic = YV8N_P4_P5_MODEL19_MAGIC;
    summary->version = YV8N_P4_P5_SUMMARY_VERSION;
    summary->input_bytes = YV8N_P4_P5_MODEL19_INPUT_BYTES;
    summary->output_bytes = YV8N_P4_P5_MODEL19_OUTPUT_BYTES;
    summary->input_h = YV8N_P4_P5_MODEL19_IN_H;
    summary->input_w = YV8N_P4_P5_MODEL19_IN_W;
    summary->input_c = YV8N_P4_P5_MODEL19_IN_C;
    summary->output_h = YV8N_P4_P5_MODEL19_OUT_H;
    summary->output_w = YV8N_P4_P5_MODEL19_OUT_W;
    summary->output_c = YV8N_P4_P5_MODEL19_OUT_C;
    summary->stride_h = layer->stride_h;
    summary->stride_w = layer->stride_w;
    summary->pad_h = layer->pad_h;
    summary->pad_w = layer->pad_w;
    summary->scratch_peak_bytes = YV8N_P4_P5_MODEL19_SCRATCH_PEAK_BYTES;
    summary->computed_points = YV8N_P4_P5_MODEL19_POINTS;
    summary->layer_id = YV8N_P4_P5_MODEL19_ID;
    summary->reserved0 = 0u;
    summary->input_hash = hash_full_i8(input, YV8N_P4_P5_MODEL19_INPUT_BYTES);
    summary->layer_hash = hash_model19_layer();
    summary->output_hash = hash_full_i8(output, YV8N_P4_P5_MODEL19_OUTPUT_BYTES);
    summary->point_hash = point_hash;
    for (uint32_t i = 0; i < 4u; ++i)
        summary->first_output_q8[i] = output[i];
    for (uint32_t i = 0; i < (uint32_t)(sizeof(summary->reserved) /
                                        sizeof(summary->reserved[0])); ++i)
        summary->reserved[i] = 0u;

    return (yolov8n_p5_arena_high_water() <=
            YV8N_P4_P5_MODEL19_SCRATCH_PEAK_BYTES) ? 0 : -1;
}

static void run_concat(const int8_t *model19,
                       const int8_t *model9,
                       int8_t *output,
                       yolov8n_p4_p5_concat_summary_t *summary)
{
    for (uint32_t point = 0; point < YV8N_P4_P5_MODEL19_POINTS; ++point) {
        const int8_t *m19 = model19 + (size_t)point * YV8N_P4_P5_MODEL19_OUT_C;
        const int8_t *m9 = model9 + (size_t)point * YV8N_P4_P5_MODEL9_C;
        int8_t *out = output + (size_t)point * YV8N_P4_P5_CONCAT_C;

        for (uint32_t c = 0; c < YV8N_P4_P5_MODEL19_OUT_C; ++c)
            out[c] = m19[c];
        for (uint32_t c = 0; c < YV8N_P4_P5_MODEL9_C; ++c)
            out[YV8N_P4_P5_MODEL19_OUT_C + c] = m9[c];
    }

    summary->magic = YV8N_P4_P5_CONCAT_MAGIC;
    summary->version = YV8N_P4_P5_SUMMARY_VERSION;
    summary->model19_bytes = YV8N_P4_P5_MODEL19_OUTPUT_BYTES;
    summary->model9_bytes = YV8N_P4_P5_MODEL9_BYTES;
    summary->output_bytes = YV8N_P4_P5_CONCAT_BYTES;
    summary->height = YV8N_P4_P5_MODEL19_OUT_H;
    summary->width = YV8N_P4_P5_MODEL19_OUT_W;
    summary->model19_channels = YV8N_P4_P5_MODEL19_OUT_C;
    summary->model9_channels = YV8N_P4_P5_MODEL9_C;
    summary->output_channels = YV8N_P4_P5_CONCAT_C;
    summary->model19_first_channel = 0u;
    summary->model9_first_channel = YV8N_P4_P5_MODEL19_OUT_C;
    summary->full_edge_bytes = YV8N_P4_P5_CONCAT_BYTES;
    summary->layout_sample_points = YV8N_P4_P5_LAYOUT_SAMPLE_POINTS;
    summary->scratch_peak_bytes = YV8N_P4_P5_CONCAT_SCRATCH_PEAK_BYTES;
    summary->reserved0 = 0u;
    summary->model19_hash = hash_full_i8(model19, YV8N_P4_P5_MODEL19_OUTPUT_BYTES);
    summary->model9_hash = hash_full_i8(model9, YV8N_P4_P5_MODEL9_BYTES);
    summary->output_hash = hash_full_i8(output, YV8N_P4_P5_CONCAT_BYTES);
    summary->layout_hash = hash_layout_samples(output);
    summary->checker_edge_hash = summary->output_hash;
    for (uint32_t i = 0; i < 4u; ++i) {
        summary->first_model19_q8[i] = model19[i];
        summary->first_model9_q8[i] = model9[i];
    }
    for (uint32_t i = 0; i < (uint32_t)(sizeof(summary->reserved) /
                                        sizeof(summary->reserved[0])); ++i)
        summary->reserved[i] = 0u;
}

static int validate_p4_summary(const yolov8n_p4_dense_c2f_summary_t *summary)
{
    if (summary->magic != YV8N_P4_DENSE_C2F_SUMMARY_MAGIC ||
        summary->version != YV8N_P4_DENSE_C2F_SUMMARY_VERSION ||
        summary->input_bytes != YV8N_P4_DENSE_C2F_INPUT_BYTES ||
        summary->output_bytes != YV8N_P4_DENSE_C2F_OUTPUT_BYTES ||
        summary->input_h != YV8N_P4_DENSE_C2F_IN_H ||
        summary->input_w != YV8N_P4_DENSE_C2F_IN_W ||
        summary->input_c != YV8N_P4_DENSE_C2F_IN_C ||
        summary->output_c != YV8N_P4_DENSE_C2F_OUT_C ||
        summary->hidden_c != YV8N_P4_DENSE_C2F_HIDDEN_C ||
        summary->scratch_peak_bytes != YV8N_P4_DENSE_C2F_SCRATCH_PEAK_BYTES ||
        summary->sampled_output_points != YV8N_P4_DENSE_C2F_EXPECT_COMPUTED_POINTS ||
        summary->computed_point_count != YV8N_P4_DENSE_C2F_EXPECT_COMPUTED_POINTS)
        return -1;
    if (summary->input_hash != YV8N_P4_DENSE_C2F_EXPECT_INPUT_HASH ||
        summary->c2f_layer_hash != YV8N_P4_DENSE_C2F_EXPECT_LAYER_HASH ||
        summary->output_hash != YV8N_P4_DENSE_C2F_EXPECT_OUTPUT_HASH ||
        summary->sampled_output_hash != YV8N_P4_DENSE_C2F_EXPECT_OUTPUT_HASH ||
        summary->computed_point_hash != YV8N_P4_DENSE_C2F_EXPECT_POINT_HASH)
        return -1;
    for (uint32_t i = 0; i < YV8N_P4_DENSE_C2F_DETECT_SAMPLE_POINTS; ++i) {
        if (summary->detect_sample_point_ids[i] != p4_sample_points[i])
            return -1;
    }
    if (summary->first_output_q8[0] != YV8N_P4_DENSE_C2F_EXPECT_FIRST0 ||
        summary->first_output_q8[1] != YV8N_P4_DENSE_C2F_EXPECT_FIRST1 ||
        summary->first_output_q8[2] != YV8N_P4_DENSE_C2F_EXPECT_FIRST2 ||
        summary->first_output_q8[3] != YV8N_P4_DENSE_C2F_EXPECT_FIRST3)
        return -1;
    for (uint32_t i = 0; i < (uint32_t)(sizeof(summary->reserved) /
                                        sizeof(summary->reserved[0])); ++i) {
        if (summary->reserved[i] != 0u)
            return -1;
    }
    return 0;
}

static int validate_model19_summary(const yolov8n_p4_p5_model19_summary_t *summary)
{
    if (summary->magic != YV8N_P4_P5_MODEL19_MAGIC ||
        summary->version != YV8N_P4_P5_SUMMARY_VERSION ||
        summary->input_bytes != YV8N_P4_P5_MODEL19_INPUT_BYTES ||
        summary->output_bytes != YV8N_P4_P5_MODEL19_OUTPUT_BYTES ||
        summary->input_h != YV8N_P4_P5_MODEL19_IN_H ||
        summary->input_w != YV8N_P4_P5_MODEL19_IN_W ||
        summary->input_c != YV8N_P4_P5_MODEL19_IN_C ||
        summary->output_h != YV8N_P4_P5_MODEL19_OUT_H ||
        summary->output_w != YV8N_P4_P5_MODEL19_OUT_W ||
        summary->output_c != YV8N_P4_P5_MODEL19_OUT_C ||
        summary->stride_h != 2u || summary->stride_w != 2u ||
        summary->pad_h != 1u || summary->pad_w != 1u ||
        summary->scratch_peak_bytes != YV8N_P4_P5_MODEL19_SCRATCH_PEAK_BYTES ||
        summary->computed_points != YV8N_P4_P5_MODEL19_POINTS ||
        summary->layer_id != YV8N_P4_P5_MODEL19_ID || summary->reserved0 != 0u)
        return -1;
    if (summary->input_hash != YV8N_P4_P5_MODEL19_EXPECT_INPUT_HASH ||
        summary->layer_hash != YV8N_P4_P5_MODEL19_EXPECT_LAYER_HASH ||
        summary->output_hash != YV8N_P4_P5_MODEL19_EXPECT_OUTPUT_HASH ||
        summary->point_hash != YV8N_P4_P5_MODEL19_EXPECT_POINT_HASH)
        return -1;
    if (summary->first_output_q8[0] != YV8N_P4_P5_MODEL19_EXPECT_FIRST0 ||
        summary->first_output_q8[1] != YV8N_P4_P5_MODEL19_EXPECT_FIRST1 ||
        summary->first_output_q8[2] != YV8N_P4_P5_MODEL19_EXPECT_FIRST2 ||
        summary->first_output_q8[3] != YV8N_P4_P5_MODEL19_EXPECT_FIRST3)
        return -1;
    for (uint32_t i = 0; i < (uint32_t)(sizeof(summary->reserved) /
                                        sizeof(summary->reserved[0])); ++i) {
        if (summary->reserved[i] != 0u)
            return -1;
    }
    return 0;
}

static int validate_concat_summary(const yolov8n_p4_p5_concat_summary_t *summary)
{
    if (summary->magic != YV8N_P4_P5_CONCAT_MAGIC ||
        summary->version != YV8N_P4_P5_SUMMARY_VERSION ||
        summary->model19_bytes != YV8N_P4_P5_MODEL19_OUTPUT_BYTES ||
        summary->model9_bytes != YV8N_P4_P5_MODEL9_BYTES ||
        summary->output_bytes != YV8N_P4_P5_CONCAT_BYTES ||
        summary->height != YV8N_P4_P5_MODEL19_OUT_H ||
        summary->width != YV8N_P4_P5_MODEL19_OUT_W ||
        summary->model19_channels != YV8N_P4_P5_MODEL19_OUT_C ||
        summary->model9_channels != YV8N_P4_P5_MODEL9_C ||
        summary->output_channels != YV8N_P4_P5_CONCAT_C ||
        summary->model19_first_channel != 0u ||
        summary->model9_first_channel != YV8N_P4_P5_MODEL19_OUT_C ||
        summary->full_edge_bytes != YV8N_P4_P5_CONCAT_BYTES ||
        summary->layout_sample_points != YV8N_P4_P5_LAYOUT_SAMPLE_POINTS ||
        summary->scratch_peak_bytes != YV8N_P4_P5_CONCAT_SCRATCH_PEAK_BYTES ||
        summary->reserved0 != 0u)
        return -1;
    if (summary->model19_hash != YV8N_P4_P5_MODEL19_EXPECT_OUTPUT_HASH ||
        summary->model9_hash != YV8N_P4_P5_CONCAT_EXPECT_MODEL9_HASH ||
        summary->output_hash != YV8N_P4_P5_CONCAT_EXPECT_OUTPUT_HASH ||
        summary->layout_hash != YV8N_P4_P5_CONCAT_EXPECT_LAYOUT_HASH ||
        summary->checker_edge_hash != YV8N_P4_P5_CONCAT_EXPECT_OUTPUT_HASH)
        return -1;
    if (summary->first_model19_q8[0] != YV8N_P4_P5_MODEL19_EXPECT_FIRST0 ||
        summary->first_model19_q8[1] != YV8N_P4_P5_MODEL19_EXPECT_FIRST1 ||
        summary->first_model19_q8[2] != YV8N_P4_P5_MODEL19_EXPECT_FIRST2 ||
        summary->first_model19_q8[3] != YV8N_P4_P5_MODEL19_EXPECT_FIRST3 ||
        summary->first_model9_q8[0] != YV8N_P4_P5_CONCAT_EXPECT_MODEL9_FIRST0 ||
        summary->first_model9_q8[1] != YV8N_P4_P5_CONCAT_EXPECT_MODEL9_FIRST1 ||
        summary->first_model9_q8[2] != YV8N_P4_P5_CONCAT_EXPECT_MODEL9_FIRST2 ||
        summary->first_model9_q8[3] != YV8N_P4_P5_CONCAT_EXPECT_MODEL9_FIRST3)
        return -1;
    for (uint32_t i = 0; i < (uint32_t)(sizeof(summary->reserved) /
                                        sizeof(summary->reserved[0])); ++i) {
        if (summary->reserved[i] != 0u)
            return -1;
    }
    return 0;
}

tpa_op_t yolov8n_p4_p5_model9_source_start(void)
{
    struct tpa_channel *ch = tpa_chan(0);
    int8_t *out = tpa_send_buf(ch);

    mark(YV8N_P4_P5_NECK_TRACE_BEGIN);
    YV8N_DETECT_REQUIRE_MSG_STOP("Y8P4P5:M9BUF\n", out != 0);
    fill_model9_source_edge(out);
    mark(YV8N_P4_P5_NECK_TRACE_MODEL9_SENT);
    return tpa_send(ch, out, YV8N_P4_P5_MODEL9_BYTES,
                    yolov8n_p4_p5_model9_source_done);
}

tpa_op_t yolov8n_p4_p5_model9_source_done(void)
{
    return tpa_stop();
}

tpa_op_t yolov8n_p4_p5_model19_start(void)
{
    model19_ws_t *w = tpa_ws();

    return tpa_recv(tpa_chan(0), (void **)&w->input, &w->input_len,
                    yolov8n_p4_p5_model19_run);
}

tpa_op_t yolov8n_p4_p5_model19_run(void)
{
    model19_ws_t *w = tpa_ws();
    struct tpa_channel *out_ch = tpa_chan(1);

    YV8N_DETECT_REQUIRE_MSG_STOP("Y8P4P5:M19LEN\n",
                                 w->input_len == YV8N_P4_P5_MODEL19_INPUT_BYTES);
    YV8N_DETECT_REQUIRE_MSG_STOP("Y8P4P5:M19LAYER\n", verify_model19_layer() == 0);
    w->output = (int8_t *)tpa_send_buf(out_ch);
    YV8N_DETECT_REQUIRE_MSG_STOP("Y8P4P5:M19BUF\n", w->output != 0);
    YV8N_DETECT_REQUIRE_MSG_STOP("Y8P4P5:M19RUN\n",
                                 run_model19(w->input, w->output, &w->summary) == 0);
    mark(YV8N_P4_P5_NECK_TRACE_MODEL19_DONE);
    return tpa_send(out_ch, w->output, YV8N_P4_P5_MODEL19_OUTPUT_BYTES,
                    yolov8n_p4_p5_model19_send_summary);
}

tpa_op_t yolov8n_p4_p5_model19_send_summary(void)
{
    model19_ws_t *w = tpa_ws();
    struct tpa_channel *summary_ch = tpa_chan(2);
    yolov8n_p4_p5_model19_summary_t *summary =
        (yolov8n_p4_p5_model19_summary_t *)tpa_send_buf(summary_ch);

    YV8N_DETECT_REQUIRE_MSG_STOP("Y8P4P5:M19SUMBUF\n", summary != 0);
    *summary = w->summary;
    return tpa_send(summary_ch, summary, sizeof(*summary), yolov8n_p4_p5_model19_done);
}

tpa_op_t yolov8n_p4_p5_model19_done(void)
{
    return tpa_stop();
}

tpa_op_t yolov8n_p4_p5_concat_start(void)
{
    concat_ws_t *w = tpa_ws();

    return tpa_recv(tpa_chan(0), (void **)&w->model19, &w->model19_len,
                    yolov8n_p4_p5_concat_recv_model9);
}

tpa_op_t yolov8n_p4_p5_concat_recv_model9(void)
{
    concat_ws_t *w = tpa_ws();

    return tpa_recv(tpa_chan(1), (void **)&w->model9, &w->model9_len,
                    yolov8n_p4_p5_concat_run);
}

tpa_op_t yolov8n_p4_p5_concat_run(void)
{
    concat_ws_t *w = tpa_ws();
    struct tpa_channel *out_ch = tpa_chan(2);

    YV8N_DETECT_REQUIRE_MSG_STOP("Y8P4P5:CTM19LEN\n",
                                 w->model19_len == YV8N_P4_P5_MODEL19_OUTPUT_BYTES);
    YV8N_DETECT_REQUIRE_MSG_STOP("Y8P4P5:CTM9LEN\n",
                                 w->model9_len == YV8N_P4_P5_MODEL9_BYTES);
    w->output = (int8_t *)tpa_send_buf(out_ch);
    YV8N_DETECT_REQUIRE_MSG_STOP("Y8P4P5:CTBUF\n", w->output != 0);
    run_concat(w->model19, w->model9, w->output, &w->summary);
    mark(YV8N_P4_P5_NECK_TRACE_CONCAT_DONE);
    return tpa_send(out_ch, w->output, YV8N_P4_P5_CONCAT_BYTES,
                    yolov8n_p4_p5_concat_send_summary);
}

tpa_op_t yolov8n_p4_p5_concat_send_summary(void)
{
    concat_ws_t *w = tpa_ws();
    struct tpa_channel *summary_ch = tpa_chan(3);
    yolov8n_p4_p5_concat_summary_t *summary =
        (yolov8n_p4_p5_concat_summary_t *)tpa_send_buf(summary_ch);

    YV8N_DETECT_REQUIRE_MSG_STOP("Y8P4P5:CTSUMBUF\n", summary != 0);
    *summary = w->summary;
    return tpa_send(summary_ch, summary, sizeof(*summary), yolov8n_p4_p5_concat_done);
}

tpa_op_t yolov8n_p4_p5_concat_done(void)
{
    return tpa_stop();
}

tpa_op_t yolov8n_p4_p5_checker_start(void)
{
    checker_ws_t *w = tpa_ws();

    return tpa_recv(tpa_chan(0), (void **)&w->p4_summary, &w->p4_len,
                    yolov8n_p4_p5_checker_recv_model19);
}

tpa_op_t yolov8n_p4_p5_checker_recv_model19(void)
{
    checker_ws_t *w = tpa_ws();

    return tpa_recv(tpa_chan(1), (void **)&w->model19_summary, &w->model19_len,
                    yolov8n_p4_p5_checker_recv_concat_edge);
}

tpa_op_t yolov8n_p4_p5_checker_recv_concat_edge(void)
{
    checker_ws_t *w = tpa_ws();

    return tpa_recv(tpa_chan(2), (void **)&w->concat_edge, &w->concat_edge_len,
                    yolov8n_p4_p5_checker_recv_concat_summary);
}

tpa_op_t yolov8n_p4_p5_checker_recv_concat_summary(void)
{
    checker_ws_t *w = tpa_ws();

    return tpa_recv(tpa_chan(3), (void **)&w->concat_summary,
                    &w->concat_summary_len, yolov8n_p4_p5_checker_run);
}

tpa_op_t yolov8n_p4_p5_checker_run(void)
{
    checker_ws_t *w = tpa_ws();
    uint64_t edge_hash;
    uint64_t layout_hash;

    if (w->p4_len != sizeof(*w->p4_summary)) {
        yolov8n_detect_diag_puts("Y8P4P5:P4LEN\n");
        mark(YV8N_P4_P5_NECK_TRACE_FAIL);
        TEST_FAIL;
        return tpa_stop();
    }
    if (w->model19_len != sizeof(*w->model19_summary)) {
        yolov8n_detect_diag_puts("Y8P4P5:M19LEN\n");
        mark(YV8N_P4_P5_NECK_TRACE_FAIL);
        TEST_FAIL;
        return tpa_stop();
    }
    if (w->concat_edge_len != YV8N_P4_P5_CONCAT_BYTES) {
        yolov8n_detect_diag_puts("Y8P4P5:CTEDGELEN\n");
        mark(YV8N_P4_P5_NECK_TRACE_FAIL);
        TEST_FAIL;
        return tpa_stop();
    }
    if (w->concat_summary_len != sizeof(*w->concat_summary)) {
        yolov8n_detect_diag_puts("Y8P4P5:CTSUMLEN\n");
        mark(YV8N_P4_P5_NECK_TRACE_FAIL);
        TEST_FAIL;
        return tpa_stop();
    }
    if (validate_p4_summary(w->p4_summary)) {
        yolov8n_detect_diag_puts("Y8P4P5:P4SUM\n");
        diag_hex64("P4IN=", w->p4_summary->input_hash);
        diag_hex64("P4OUT=", w->p4_summary->output_hash);
        diag_hex64("P4POINT=", w->p4_summary->computed_point_hash);
        mark(YV8N_P4_P5_NECK_TRACE_FAIL);
        TEST_FAIL;
        return tpa_stop();
    }
    if (validate_model19_summary(w->model19_summary)) {
        yolov8n_detect_diag_puts("Y8P4P5:M19SUM\n");
        diag_hex64("M19IN=", w->model19_summary->input_hash);
        diag_hex64("M19LAYER=", w->model19_summary->layer_hash);
        diag_hex64("M19OUT=", w->model19_summary->output_hash);
        diag_hex64("M19POINT=", w->model19_summary->point_hash);
        mark(YV8N_P4_P5_NECK_TRACE_FAIL);
        TEST_FAIL;
        return tpa_stop();
    }
    if (validate_concat_summary(w->concat_summary)) {
        yolov8n_detect_diag_puts("Y8P4P5:CTSUM\n");
        diag_hex64("CTM19=", w->concat_summary->model19_hash);
        diag_hex64("CTM9=", w->concat_summary->model9_hash);
        diag_hex64("CTOUT=", w->concat_summary->output_hash);
        diag_hex64("CTLAY=", w->concat_summary->layout_hash);
        mark(YV8N_P4_P5_NECK_TRACE_FAIL);
        TEST_FAIL;
        return tpa_stop();
    }

    edge_hash = hash_full_i8(w->concat_edge, YV8N_P4_P5_CONCAT_BYTES);
    layout_hash = hash_layout_samples(w->concat_edge);
    if (edge_hash != YV8N_P4_P5_CONCAT_EXPECT_OUTPUT_HASH ||
        edge_hash != w->concat_summary->checker_edge_hash ||
        layout_hash != YV8N_P4_P5_CONCAT_EXPECT_LAYOUT_HASH) {
        yolov8n_detect_diag_puts("Y8P4P5:CTEDGE\n");
        diag_hex64("EDGE=", edge_hash);
        diag_hex64("LAY=", layout_hash);
        mark(YV8N_P4_P5_NECK_TRACE_FAIL);
        TEST_FAIL;
        return tpa_stop();
    }

    mark(YV8N_P4_P5_NECK_TRACE_CHECK_PASS);
    TEST_PASS;
    return tpa_stop();
}
