#include "yolov8n_detect_downstream.h"

#include <stdint.h>

#ifndef TPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER
#error "Configure with -DTPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER=/path/to/yolov8n_external_detect_c2f_weights.h"
#endif
#include TPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER

#define YV8N_P5_DENSE_C2F_TRACE_BEGIN 0xe8c5f000u
#define YV8N_P5_DENSE_C2F_TRACE_SOURCE_SENT 0xe8c5f001u
#define YV8N_P5_DENSE_C2F_TRACE_C2F_DONE 0xe8c5f002u
#define YV8N_P5_DENSE_C2F_TRACE_DETECT_DONE 0xe8c5f003u
#define YV8N_P5_DENSE_C2F_TRACE_CHECK_PASS 0xe8c5f004u
#define YV8N_P5_DENSE_C2F_TRACE_FAIL 0xe8c5feeu

#define YV8N_P5_DENSE_C2F_SOURCE_PID 870u
#define YV8N_P5_DENSE_C2F_PID 871u
#define YV8N_P5_DENSE_C2F_DETECT_PID 872u
#define YV8N_P5_DENSE_C2F_CHECKER_PID 873u

#define YV8N_P5_DENSE_C2F_IN_H 20u
#define YV8N_P5_DENSE_C2F_IN_W 20u
#define YV8N_P5_DENSE_C2F_IN_C 384u
#define YV8N_P5_DENSE_C2F_OUT_C 256u
#define YV8N_P5_DENSE_C2F_HIDDEN_C 128u
#define YV8N_P5_DENSE_C2F_CAT_C (3u * YV8N_P5_DENSE_C2F_HIDDEN_C)
#define YV8N_P5_DENSE_C2F_POINTS (YV8N_P5_DENSE_C2F_IN_H * YV8N_P5_DENSE_C2F_IN_W)
#define YV8N_P5_DENSE_C2F_INPUT_BYTES (YV8N_P5_DENSE_C2F_POINTS * YV8N_P5_DENSE_C2F_IN_C)
#define YV8N_P5_DENSE_C2F_OUTPUT_BYTES (YV8N_P5_DENSE_C2F_POINTS * YV8N_P5_DENSE_C2F_OUT_C)
#define YV8N_P5_DENSE_C2F_SCRATCH_PEAK_BYTES 262144u
#define YV8N_P5_DENSE_C2F_DETECT_SAMPLE_POINTS 2u
#define YV8N_P5_DENSE_C2F_MAX_COMPUTED_POINTS YV8N_P5_DENSE_C2F_POINTS
#define YV8N_P5_DENSE_C2F_SUMMARY_MAGIC UINT32_C(0x59384435) /* "Y8D5" */
#define YV8N_P5_DENSE_C2F_SUMMARY_VERSION 1u

#define YV8N_P5_DENSE_C2F_CV1_ID 8u
#define YV8N_P5_DENSE_C2F_B0_CV1_ID 9u
#define YV8N_P5_DENSE_C2F_B0_CV2_ID 10u
#define YV8N_P5_DENSE_C2F_CV2_ID 11u
#define YV8N_P5_DENSE_C2F_BOX0_ID 18u
#define YV8N_P5_DENSE_C2F_BOX1_ID 19u
#define YV8N_P5_DENSE_C2F_BOX2_ID 20u
#define YV8N_P5_DENSE_C2F_CLS0_ID 27u
#define YV8N_P5_DENSE_C2F_CLS1_ID 28u
#define YV8N_P5_DENSE_C2F_CLS2_ID 29u
#define YV8N_P5_DENSE_C2F_DFL_ID 30u

#define FNV1A64_INIT UINT64_C(0xcbf29ce484222325)
#define FNV1A64_PRIME UINT64_C(0x100000001b3)

#define YV8N_P5_DENSE_C2F_EXPECT_INPUT_HASH UINT64_C(0xa32679ba0aa02025)
#define YV8N_P5_DENSE_C2F_EXPECT_LAYER_HASH UINT64_C(0x243583d1486cabda)
#define YV8N_P5_DENSE_C2F_EXPECT_OUTPUT_HASH UINT64_C(0x69c8c5f4f9827a30)
#define YV8N_P5_DENSE_C2F_EXPECT_SAMPLED_HASH UINT64_C(0x69c8c5f4f9827a30)
#define YV8N_P5_DENSE_C2F_EXPECT_POINT_HASH UINT64_C(0x3b8ec8550d64d7b5)
#define YV8N_P5_DENSE_C2F_EXPECT_COMPUTED_POINTS 400u
#define YV8N_P5_DENSE_C2F_EXPECT_FIRST0 -4
#define YV8N_P5_DENSE_C2F_EXPECT_FIRST1 -6
#define YV8N_P5_DENSE_C2F_EXPECT_FIRST2 68
#define YV8N_P5_DENSE_C2F_EXPECT_FIRST3 -4

#define YV8N_P5_DENSE_C2F_DETECT_EXPECT_RAW_HASH UINT64_C(0xb9df62ac0bb5acb9)
#define YV8N_P5_DENSE_C2F_DETECT_EXPECT_BOX_HASH UINT64_C(0xffbc8daced6cb5a4)
#define YV8N_P5_DENSE_C2F_DETECT_EXPECT_CLS_HASH UINT64_C(0xd5548314c55c5e65)
#define YV8N_P5_DENSE_C2F_DETECT_EXPECT_PUBLIC_HASH UINT64_C(0xa861189329b4ad40)
#define YV8N_P5_DENSE_C2F_DETECT_EXPECT_FIRST0 1056
#define YV8N_P5_DENSE_C2F_DETECT_EXPECT_FIRST1 11952
#define YV8N_P5_DENSE_C2F_DETECT_EXPECT_FIRST2 69184
#define YV8N_P5_DENSE_C2F_DETECT_EXPECT_FIRST3 88544

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
} yolov8n_p5_dense_c2f_summary_t;

TPA_STATIC_ASSERT(sizeof(yolov8n_p5_dense_c2f_summary_t) == 160u,
                  "YOLOv8n dense P5 C2f summary edge size must match .tpp");

typedef struct {
    const int8_t *input;
    uint32_t input_len;
    int8_t *output;
    yolov8n_p5_dense_c2f_summary_t summary;
} c2f_ws_t;

typedef struct {
    const int8_t *input;
    uint32_t input_len;
    yolov8n_detect_summary_t *summary;
} detect_ws_t;

typedef struct {
    const yolov8n_p5_dense_c2f_summary_t *c2f_summary;
    const yolov8n_detect_summary_t *detect_summary;
    uint32_t c2f_len;
    uint32_t detect_len;
} checker_ws_t;

typedef struct {
    int8_t chunk0_map[YV8N_P5_DENSE_C2F_POINTS * YV8N_P5_DENSE_C2F_HIDDEN_C];
    int8_t chunk1_map[YV8N_P5_DENSE_C2F_POINTS * YV8N_P5_DENSE_C2F_HIDDEN_C];
    int8_t b0_cv1_map[YV8N_P5_DENSE_C2F_POINTS * YV8N_P5_DENSE_C2F_HIDDEN_C];
    int8_t bottleneck_map[YV8N_P5_DENSE_C2F_POINTS * YV8N_P5_DENSE_C2F_HIDDEN_C];
    int8_t cv1_tmp[256];
    int8_t chunk1_patch5[25 * YV8N_P5_DENSE_C2F_HIDDEN_C];
    int8_t chunk0_center[YV8N_P5_DENSE_C2F_HIDDEN_C];
    int8_t chunk1_center[YV8N_P5_DENSE_C2F_HIDDEN_C];
    int8_t conv0_patch3[9 * YV8N_P5_DENSE_C2F_HIDDEN_C];
    int8_t conv1_out[YV8N_P5_DENSE_C2F_HIDDEN_C];
    int8_t bottleneck[YV8N_P5_DENSE_C2F_HIDDEN_C];
    int8_t cat[YV8N_P5_DENSE_C2F_CAT_C];
    uint8_t patch5_valid[25];
    uint8_t patch3_valid[9];
} c2f_scratch_t;

TPA_STATIC_ASSERT(sizeof(c2f_scratch_t) <= YV8N_P5_DENSE_C2F_SCRATCH_PEAK_BYTES,
                  "YOLOv8n dense P5 C2f scratch declaration is too small");

tpa_op_t yolov8n_p5_dense_c2f_input_source_start(void);
tpa_op_t yolov8n_p5_dense_c2f_input_source_done(void);
tpa_op_t yolov8n_p5_dense_c2f_start(void);
tpa_op_t yolov8n_p5_dense_c2f_run(void);
tpa_op_t yolov8n_p5_dense_c2f_send_summary(void);
tpa_op_t yolov8n_p5_dense_c2f_done(void);
tpa_op_t yolov8n_p5_dense_c2f_detect_start(void);
tpa_op_t yolov8n_p5_dense_c2f_detect_run(void);
tpa_op_t yolov8n_p5_dense_c2f_detect_done(void);
tpa_op_t yolov8n_p5_dense_c2f_detect_checker_start(void);
tpa_op_t yolov8n_p5_dense_c2f_detect_checker_recv_detect(void);
tpa_op_t yolov8n_p5_dense_c2f_detect_checker_run(void);

TPA_PROC_MEM_META(yolov8n_p5_dense_c2f_input_source_meta,
                  YV8N_P5_DENSE_C2F_SOURCE_PID, 0u);
TPA_PROC_MEM_META(yolov8n_p5_dense_c2f_meta,
                  YV8N_P5_DENSE_C2F_PID, YV8N_P5_DENSE_C2F_SCRATCH_PEAK_BYTES);
TPA_PROC_MEM_META(yolov8n_p5_dense_c2f_detect_meta,
                  YV8N_P5_DENSE_C2F_DETECT_PID, YV8N_DETECT_SCRATCH_PEAK_BYTES);
TPA_PROC_MEM_META(yolov8n_p5_dense_c2f_detect_checker_meta,
                  YV8N_P5_DENSE_C2F_CHECKER_PID, 0u);

static const uint32_t sample_points[YV8N_P5_DENSE_C2F_DETECT_SAMPLE_POINTS] = {
    0u, 21u,
};

static const uint32_t exp_lut_q12[17] = {
    554u, 712u, 914u, 1174u, 1507u, 1935u, 2484u, 3190u,
    4096u, 5259u, 6753u, 8671u, 11134u, 14296u, 18357u, 23571u,
    30266u,
};

static const uint16_t sigmoid_lut_q15[17] = {
    3906u, 4851u, 5978u, 7297u, 8812u, 10512u, 12371u, 14346u,
    16384u, 18421u, 20396u, 22255u, 23955u, 25470u, 26789u, 27916u,
    28861u,
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

static int8_t c2f_input_value(uint32_t y, uint32_t x, uint32_t c)
{
    uint32_t v = (y * 41u + x * 23u + c * 7u +
                  (y + 5u) * (x + 3u) + ((c & 15u) * 13u) + 19u) & 0xffu;

    return (int8_t)((int32_t)v - 128);
}

static int8_t c2f_output_sentinel(uint32_t y, uint32_t x, uint32_t c)
{
    uint32_t v = (y * 11u + x * 29u + c * 5u +
                  ((y ^ x) * 17u) + 73u) & 0xffu;

    return (int8_t)((int32_t)v - 128);
}

static uint64_t hash_full_i8(const int8_t *buf, uint32_t bytes)
{
    return hash_i8_bytes(FNV1A64_INIT, buf, bytes);
}

static void conv1x1_hwc_at(
    const yolov8n_external_detect_c2f_layer_t *layer,
    const int8_t *input,
    uint32_t h,
    uint32_t w,
    uint32_t in_c,
    uint32_t y,
    uint32_t x,
    int8_t *out)
{
    (void)h;
    for (uint32_t oc = 0; oc < layer->K_out; ++oc) {
        int32_t acc = 0;
        size_t w_base = (size_t)oc * layer->w_stride;
        size_t in_base = (((size_t)y * w + x) * in_c);

        for (uint32_t ic = 0; ic < layer->C_in; ++ic)
            acc += (int32_t)input[in_base + ic] * (int32_t)layer->w[w_base + ic];

        out[oc] = requant_layer_value(layer, oc, acc);
    }
}

static void conv1x1_vec(const yolov8n_external_detect_c2f_layer_t *layer,
                        const int8_t *in,
                        int8_t *out)
{
    for (uint32_t oc = 0; oc < layer->K_out; ++oc) {
        int32_t acc = 0;
        size_t w_base = (size_t)oc * layer->w_stride;

        for (uint32_t ic = 0; ic < layer->C_in; ++ic)
            acc += (int32_t)in[ic] * (int32_t)layer->w[w_base + ic];

        out[oc] = requant_layer_value(layer, oc, acc);
    }
}

static void conv3x3_from_patch(
    const yolov8n_external_detect_c2f_layer_t *layer,
    const int8_t *patch,
    uint32_t patch_side,
    uint32_t patch_c,
    const uint8_t *valid,
    uint32_t center_y,
    uint32_t center_x,
    int8_t *out)
{
    for (uint32_t oc = 0; oc < layer->K_out; ++oc) {
        int32_t acc = 0;

        for (uint32_t ky = 0; ky < layer->kH; ++ky) {
            uint32_t py = center_y + ky - layer->pad_h;

            for (uint32_t kx = 0; kx < layer->kW; ++kx) {
                uint32_t px = center_x + kx - layer->pad_w;
                uint32_t patch_idx = py * patch_side + px;
                const int8_t *in = patch + (size_t)patch_idx * patch_c;
                size_t w_base = (size_t)oc * layer->w_stride +
                                ((size_t)ky * layer->kW + kx) * layer->C_in;

                if (py >= patch_side || px >= patch_side || !valid[patch_idx])
                    continue;

                for (uint32_t ic = 0; ic < layer->C_in; ++ic)
                    acc += (int32_t)in[ic] * (int32_t)layer->w[w_base + ic];
            }
        }
        out[oc] = requant_layer_value(layer, oc, acc);
    }
}

static uint32_t clipped_lut_index(int8_t v)
{
    int32_t idx = (int32_t)v + 8;

    if (idx < 0)
        return 0u;
    if (idx > 16)
        return 16u;
    return (uint32_t)idx;
}

static uint32_t dfl_project_q8(const int8_t *box_raw, uint32_t coord)
{
    uint32_t total = 0u;
    uint32_t weighted = 0u;
    uint32_t base = coord * YV8N_DETECT_REG_MAX;

    for (uint32_t bin = 0; bin < YV8N_DETECT_REG_MAX; ++bin) {
        uint32_t weight = exp_lut_q12[clipped_lut_index(box_raw[base + bin])];

        total += weight;
        weighted += bin * weight;
    }

    return (weighted * 256u + total / 2u) / total;
}

static uint16_t sigmoid_q15(int8_t logit)
{
    return sigmoid_lut_q15[clipped_lut_index(logit)];
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

static uint64_t hash_c2f_layers(void)
{
    static const uint32_t layer_ids[] = {
        YV8N_P5_DENSE_C2F_CV1_ID,
        YV8N_P5_DENSE_C2F_B0_CV1_ID,
        YV8N_P5_DENSE_C2F_B0_CV2_ID,
        YV8N_P5_DENSE_C2F_CV2_ID,
    };

    return hash_layer_set(layer_ids,
                          (uint32_t)(sizeof(layer_ids) / sizeof(layer_ids[0])));
}

static uint64_t hash_detect_layers(void)
{
    static const uint32_t layer_ids[] = {
        YV8N_P5_DENSE_C2F_BOX0_ID, YV8N_P5_DENSE_C2F_BOX1_ID, YV8N_P5_DENSE_C2F_BOX2_ID,
        YV8N_P5_DENSE_C2F_CLS0_ID, YV8N_P5_DENSE_C2F_CLS1_ID, YV8N_P5_DENSE_C2F_CLS2_ID,
        YV8N_P5_DENSE_C2F_DFL_ID,
    };

    return hash_layer_set(layer_ids,
                          (uint32_t)(sizeof(layer_ids) / sizeof(layer_ids[0])));
}

static int verify_c2f_layers(void)
{
    const yolov8n_external_detect_c2f_layer_t *cv1 =
        &yolov8n_external_detect_c2f_layers[YV8N_P5_DENSE_C2F_CV1_ID];
    const yolov8n_external_detect_c2f_layer_t *b0_cv1 =
        &yolov8n_external_detect_c2f_layers[YV8N_P5_DENSE_C2F_B0_CV1_ID];
    const yolov8n_external_detect_c2f_layer_t *b0_cv2 =
        &yolov8n_external_detect_c2f_layers[YV8N_P5_DENSE_C2F_B0_CV2_ID];
    const yolov8n_external_detect_c2f_layer_t *cv2 =
        &yolov8n_external_detect_c2f_layers[YV8N_P5_DENSE_C2F_CV2_ID];

    if (YOLOV8N_EXTERNAL_DETECT_C2F_N_LAYERS != 32u)
        return -1;
    if (cv1->K_out != 256u || cv1->C_in != 384u || cv1->kH != 1u ||
        cv1->kW != 1u)
        return -1;
    if (b0_cv1->K_out != 128u || b0_cv1->C_in != 128u ||
        b0_cv1->kH != 3u || b0_cv1->kW != 3u ||
        b0_cv1->pad_h != 1u || b0_cv1->pad_w != 1u)
        return -1;
    if (b0_cv2->K_out != 128u || b0_cv2->C_in != 128u ||
        b0_cv2->kH != 3u || b0_cv2->kW != 3u ||
        b0_cv2->pad_h != 1u || b0_cv2->pad_w != 1u)
        return -1;
    if (cv2->K_out != 256u || cv2->C_in != 384u || cv2->kH != 1u ||
        cv2->kW != 1u)
        return -1;
    return 0;
}

static int verify_detect_layers(void)
{
    const yolov8n_external_detect_c2f_layer_t *box0 =
        &yolov8n_external_detect_c2f_layers[YV8N_P5_DENSE_C2F_BOX0_ID];
    const yolov8n_external_detect_c2f_layer_t *box1 =
        &yolov8n_external_detect_c2f_layers[YV8N_P5_DENSE_C2F_BOX1_ID];
    const yolov8n_external_detect_c2f_layer_t *box2 =
        &yolov8n_external_detect_c2f_layers[YV8N_P5_DENSE_C2F_BOX2_ID];
    const yolov8n_external_detect_c2f_layer_t *cls0 =
        &yolov8n_external_detect_c2f_layers[YV8N_P5_DENSE_C2F_CLS0_ID];
    const yolov8n_external_detect_c2f_layer_t *cls1 =
        &yolov8n_external_detect_c2f_layers[YV8N_P5_DENSE_C2F_CLS1_ID];
    const yolov8n_external_detect_c2f_layer_t *cls2 =
        &yolov8n_external_detect_c2f_layers[YV8N_P5_DENSE_C2F_CLS2_ID];
    const yolov8n_external_detect_c2f_layer_t *dfl =
        &yolov8n_external_detect_c2f_layers[YV8N_P5_DENSE_C2F_DFL_ID];

    if (YOLOV8N_EXTERNAL_DETECT_C2F_N_LAYERS != 32u)
        return -1;
    if (box0->K_out != 64u || box0->C_in != 256u || box0->kH != 3u ||
        box0->kW != 3u || box0->pad_h != 1u || box0->pad_w != 1u)
        return -1;
    if (box1->K_out != 64u || box1->C_in != 64u || box1->kH != 3u ||
        box1->kW != 3u || box1->pad_h != 1u || box1->pad_w != 1u)
        return -1;
    if (box2->K_out != 64u || box2->C_in != 64u || box2->kH != 1u ||
        box2->kW != 1u)
        return -1;
    if (cls0->K_out != 80u || cls0->C_in != 256u || cls0->kH != 3u ||
        cls0->kW != 3u || cls0->pad_h != 1u || cls0->pad_w != 1u)
        return -1;
    if (cls1->K_out != 80u || cls1->C_in != 80u || cls1->kH != 3u ||
        cls1->kW != 3u || cls1->pad_h != 1u || cls1->pad_w != 1u)
        return -1;
    if (cls2->K_out != 80u || cls2->C_in != 80u || cls2->kH != 1u ||
        cls2->kW != 1u)
        return -1;
    if (dfl->K_out != 1u || dfl->C_in != YV8N_DETECT_REG_MAX ||
        dfl->kH != 1u || dfl->kW != 1u)
        return -1;
    return 0;
}

static void fill_c2f_source_edge(int8_t *out)
{
    for (uint32_t y = 0; y < YV8N_P5_DENSE_C2F_IN_H; ++y) {
        for (uint32_t x = 0; x < YV8N_P5_DENSE_C2F_IN_W; ++x) {
            for (uint32_t c = 0; c < YV8N_P5_DENSE_C2F_IN_C; ++c) {
                size_t idx = (((size_t)y * YV8N_P5_DENSE_C2F_IN_W + x) *
                              YV8N_P5_DENSE_C2F_IN_C) + c;

                out[idx] = c2f_input_value(y, x, c);
            }
        }
    }
}

static void fill_c2f_output_sentinel(int8_t *out)
{
    for (uint32_t y = 0; y < YV8N_P5_DENSE_C2F_IN_H; ++y) {
        for (uint32_t x = 0; x < YV8N_P5_DENSE_C2F_IN_W; ++x) {
            for (uint32_t c = 0; c < YV8N_P5_DENSE_C2F_OUT_C; ++c) {
                size_t idx = (((size_t)y * YV8N_P5_DENSE_C2F_IN_W + x) *
                              YV8N_P5_DENSE_C2F_OUT_C) + c;

                out[idx] = c2f_output_sentinel(y, x, c);
            }
        }
    }
}

static uint32_t add_unique_point(uint32_t *points, uint32_t count, uint32_t point)
{
    for (uint32_t i = 0; i < count; ++i) {
        if (points[i] == point)
            return count;
    }
    points[count] = point;
    return count + 1u;
}

static uint32_t c2f_needed_points(uint32_t *points)
{
    uint32_t count = 0u;

    for (uint32_t i = 0; i < YV8N_P5_DENSE_C2F_DETECT_SAMPLE_POINTS; ++i) {
        int32_t cy = (int32_t)(sample_points[i] / YV8N_P5_DENSE_C2F_IN_W);
        int32_t cx = (int32_t)(sample_points[i] % YV8N_P5_DENSE_C2F_IN_W);

        for (int32_t dy = -1; dy <= 1; ++dy) {
            for (int32_t dx = -1; dx <= 1; ++dx) {
                int32_t y = cy + dy;
                int32_t x = cx + dx;

                if (y < 0 || y >= (int32_t)YV8N_P5_DENSE_C2F_IN_H ||
                    x < 0 || x >= (int32_t)YV8N_P5_DENSE_C2F_IN_W)
                    continue;
                count = add_unique_point(
                    points, count, (uint32_t)y * YV8N_P5_DENSE_C2F_IN_W + (uint32_t)x);
            }
        }
    }

    return count;
}

static void compute_c2f_point(const int8_t *input,
                              uint32_t point,
                              c2f_scratch_t *s,
                              int8_t *out)
{
    const yolov8n_external_detect_c2f_layer_t *cv1 =
        &yolov8n_external_detect_c2f_layers[YV8N_P5_DENSE_C2F_CV1_ID];
    const yolov8n_external_detect_c2f_layer_t *b0_cv1 =
        &yolov8n_external_detect_c2f_layers[YV8N_P5_DENSE_C2F_B0_CV1_ID];
    const yolov8n_external_detect_c2f_layer_t *b0_cv2 =
        &yolov8n_external_detect_c2f_layers[YV8N_P5_DENSE_C2F_B0_CV2_ID];
    const yolov8n_external_detect_c2f_layer_t *cv2 =
        &yolov8n_external_detect_c2f_layers[YV8N_P5_DENSE_C2F_CV2_ID];
    int32_t cy = (int32_t)(point / YV8N_P5_DENSE_C2F_IN_W);
    int32_t cx = (int32_t)(point % YV8N_P5_DENSE_C2F_IN_W);

    for (uint32_t i = 0; i < 25u; ++i)
        s->patch5_valid[i] = 0u;
    for (int32_t dy = -2; dy <= 2; ++dy) {
        for (int32_t dx = -2; dx <= 2; ++dx) {
            int32_t y = cy + dy;
            int32_t x = cx + dx;
            uint32_t patch_idx = (uint32_t)(dy + 2) * 5u + (uint32_t)(dx + 2);
            int8_t *chunk1 = s->chunk1_patch5 +
                             (size_t)patch_idx * YV8N_P5_DENSE_C2F_HIDDEN_C;

            if (y < 0 || y >= (int32_t)YV8N_P5_DENSE_C2F_IN_H ||
                x < 0 || x >= (int32_t)YV8N_P5_DENSE_C2F_IN_W)
                continue;

            conv1x1_hwc_at(cv1, input, YV8N_P5_DENSE_C2F_IN_H,
                           YV8N_P5_DENSE_C2F_IN_W, YV8N_P5_DENSE_C2F_IN_C,
                           (uint32_t)y, (uint32_t)x, s->cv1_tmp);
            for (uint32_t c = 0; c < YV8N_P5_DENSE_C2F_HIDDEN_C; ++c) {
                if (dy == 0 && dx == 0) {
                    s->chunk0_center[c] = s->cv1_tmp[c];
                    s->chunk1_center[c] = s->cv1_tmp[YV8N_P5_DENSE_C2F_HIDDEN_C + c];
                }
                chunk1[c] = s->cv1_tmp[YV8N_P5_DENSE_C2F_HIDDEN_C + c];
            }
            s->patch5_valid[patch_idx] = 1u;
        }
    }

    for (uint32_t i = 0; i < 9u; ++i)
        s->patch3_valid[i] = 0u;
    for (int32_t dy = -1; dy <= 1; ++dy) {
        for (int32_t dx = -1; dx <= 1; ++dx) {
            int32_t y = cy + dy;
            int32_t x = cx + dx;
            uint32_t patch3_idx = (uint32_t)(dy + 1) * 3u + (uint32_t)(dx + 1);
            int8_t *conv0 = s->conv0_patch3 +
                            (size_t)patch3_idx * YV8N_P5_DENSE_C2F_HIDDEN_C;

            if (y < 0 || y >= (int32_t)YV8N_P5_DENSE_C2F_IN_H ||
                x < 0 || x >= (int32_t)YV8N_P5_DENSE_C2F_IN_W)
                continue;
            conv3x3_from_patch(b0_cv1, s->chunk1_patch5, 5u,
                               YV8N_P5_DENSE_C2F_HIDDEN_C, s->patch5_valid,
                               (uint32_t)(dy + 2), (uint32_t)(dx + 2), conv0);
            s->patch3_valid[patch3_idx] = 1u;
        }
    }

    conv3x3_from_patch(b0_cv2, s->conv0_patch3, 3u,
                       YV8N_P5_DENSE_C2F_HIDDEN_C, s->patch3_valid,
                       1u, 1u, s->conv1_out);

    for (uint32_t c = 0; c < YV8N_P5_DENSE_C2F_HIDDEN_C; ++c)
        s->bottleneck[c] = clamp_i8((int32_t)s->conv1_out[c] +
                                    (int32_t)s->chunk1_center[c]);

    for (uint32_t c = 0; c < YV8N_P5_DENSE_C2F_HIDDEN_C; ++c) {
        s->cat[c] = s->chunk0_center[c];
        s->cat[YV8N_P5_DENSE_C2F_HIDDEN_C + c] = s->chunk1_center[c];
        s->cat[2u * YV8N_P5_DENSE_C2F_HIDDEN_C + c] = s->bottleneck[c];
    }

    conv1x1_vec(cv2, s->cat, out);
}

static void fill_map_patch3(const int8_t *map,
                            uint32_t h,
                            uint32_t w,
                            uint32_t c,
                            uint32_t cy,
                            uint32_t cx,
                            int8_t *patch,
                            uint8_t *valid)
{
    for (uint32_t i = 0; i < 9u; ++i)
        valid[i] = 0u;

    for (int32_t dy = -1; dy <= 1; ++dy) {
        for (int32_t dx = -1; dx <= 1; ++dx) {
            int32_t y = (int32_t)cy + dy;
            int32_t x = (int32_t)cx + dx;
            uint32_t patch_idx = (uint32_t)(dy + 1) * 3u + (uint32_t)(dx + 1);
            int8_t *dst = patch + (size_t)patch_idx * c;

            if (y < 0 || y >= (int32_t)h || x < 0 || x >= (int32_t)w)
                continue;

            const int8_t *src = map +
                ((size_t)(uint32_t)y * w + (uint32_t)x) * c;
            for (uint32_t ch = 0; ch < c; ++ch)
                dst[ch] = src[ch];
            valid[patch_idx] = 1u;
        }
    }
}

static int run_c2f(const int8_t *input,
                   int8_t *output,
                   yolov8n_p5_dense_c2f_summary_t *summary)
{
    uint32_t point_count = 0u;
    c2f_scratch_t *scratch;
    uint64_t sampled_hash = FNV1A64_INIT;
    uint64_t point_hash = FNV1A64_INIT;

    yolov8n_p5_arena_begin(YV8N_P5_DENSE_C2F_SCRATCH_PEAK_BYTES);
    scratch = yolov8n_p5_arena_alloc(sizeof(*scratch), 64u);
    if (!scratch)
        return -1;

    const yolov8n_external_detect_c2f_layer_t *cv1 =
        &yolov8n_external_detect_c2f_layers[YV8N_P5_DENSE_C2F_CV1_ID];
    const yolov8n_external_detect_c2f_layer_t *b0_cv1 =
        &yolov8n_external_detect_c2f_layers[YV8N_P5_DENSE_C2F_B0_CV1_ID];
    const yolov8n_external_detect_c2f_layer_t *b0_cv2 =
        &yolov8n_external_detect_c2f_layers[YV8N_P5_DENSE_C2F_B0_CV2_ID];
    const yolov8n_external_detect_c2f_layer_t *cv2 =
        &yolov8n_external_detect_c2f_layers[YV8N_P5_DENSE_C2F_CV2_ID];

    for (uint32_t point = 0; point < YV8N_P5_DENSE_C2F_POINTS; ++point) {
        uint32_t y = point / YV8N_P5_DENSE_C2F_IN_W;
        uint32_t x = point % YV8N_P5_DENSE_C2F_IN_W;
        int8_t *chunk0 = scratch->chunk0_map +
            (size_t)point * YV8N_P5_DENSE_C2F_HIDDEN_C;
        int8_t *chunk1 = scratch->chunk1_map +
            (size_t)point * YV8N_P5_DENSE_C2F_HIDDEN_C;

        conv1x1_hwc_at(cv1, input, YV8N_P5_DENSE_C2F_IN_H,
                       YV8N_P5_DENSE_C2F_IN_W, YV8N_P5_DENSE_C2F_IN_C,
                       y, x, scratch->cv1_tmp);
        for (uint32_t c = 0; c < YV8N_P5_DENSE_C2F_HIDDEN_C; ++c) {
            chunk0[c] = scratch->cv1_tmp[c];
            chunk1[c] = scratch->cv1_tmp[YV8N_P5_DENSE_C2F_HIDDEN_C + c];
        }
    }

    for (uint32_t point = 0; point < YV8N_P5_DENSE_C2F_POINTS; ++point) {
        uint32_t y = point / YV8N_P5_DENSE_C2F_IN_W;
        uint32_t x = point % YV8N_P5_DENSE_C2F_IN_W;
        int8_t *b0_cv1_out = scratch->b0_cv1_map +
            (size_t)point * YV8N_P5_DENSE_C2F_HIDDEN_C;

        fill_map_patch3(scratch->chunk1_map, YV8N_P5_DENSE_C2F_IN_H,
                        YV8N_P5_DENSE_C2F_IN_W, YV8N_P5_DENSE_C2F_HIDDEN_C,
                        y, x, scratch->conv0_patch3, scratch->patch3_valid);
        conv3x3_from_patch(b0_cv1, scratch->conv0_patch3, 3u,
                           YV8N_P5_DENSE_C2F_HIDDEN_C, scratch->patch3_valid,
                           1u, 1u, b0_cv1_out);
    }

    for (uint32_t point = 0; point < YV8N_P5_DENSE_C2F_POINTS; ++point) {
        uint32_t y = point / YV8N_P5_DENSE_C2F_IN_W;
        uint32_t x = point % YV8N_P5_DENSE_C2F_IN_W;
        int8_t *bottleneck = scratch->bottleneck_map +
            (size_t)point * YV8N_P5_DENSE_C2F_HIDDEN_C;
        const int8_t *chunk1 = scratch->chunk1_map +
            (size_t)point * YV8N_P5_DENSE_C2F_HIDDEN_C;

        fill_map_patch3(scratch->b0_cv1_map, YV8N_P5_DENSE_C2F_IN_H,
                        YV8N_P5_DENSE_C2F_IN_W, YV8N_P5_DENSE_C2F_HIDDEN_C,
                        y, x, scratch->conv0_patch3, scratch->patch3_valid);
        conv3x3_from_patch(b0_cv2, scratch->conv0_patch3, 3u,
                           YV8N_P5_DENSE_C2F_HIDDEN_C, scratch->patch3_valid,
                           1u, 1u, scratch->conv1_out);
        for (uint32_t c = 0; c < YV8N_P5_DENSE_C2F_HIDDEN_C; ++c)
            bottleneck[c] = clamp_i8((int32_t)scratch->conv1_out[c] +
                                     (int32_t)chunk1[c]);
    }

    for (uint32_t point = 0; point < YV8N_P5_DENSE_C2F_POINTS; ++point) {
        int8_t *out = output + (size_t)point * YV8N_P5_DENSE_C2F_OUT_C;
        const int8_t *chunk0 = scratch->chunk0_map +
            (size_t)point * YV8N_P5_DENSE_C2F_HIDDEN_C;
        const int8_t *chunk1 = scratch->chunk1_map +
            (size_t)point * YV8N_P5_DENSE_C2F_HIDDEN_C;
        const int8_t *bottleneck = scratch->bottleneck_map +
            (size_t)point * YV8N_P5_DENSE_C2F_HIDDEN_C;

        for (uint32_t c = 0; c < YV8N_P5_DENSE_C2F_HIDDEN_C; ++c) {
            scratch->cat[c] = chunk0[c];
            scratch->cat[YV8N_P5_DENSE_C2F_HIDDEN_C + c] = chunk1[c];
            scratch->cat[2u * YV8N_P5_DENSE_C2F_HIDDEN_C + c] = bottleneck[c];
        }

        conv1x1_vec(cv2, scratch->cat, out);
        point_hash = hash_u32_value(point_hash, point);
        sampled_hash = hash_i8_bytes(sampled_hash, out, YV8N_P5_DENSE_C2F_OUT_C);
        ++point_count;
    }

    summary->magic = YV8N_P5_DENSE_C2F_SUMMARY_MAGIC;
    summary->version = YV8N_P5_DENSE_C2F_SUMMARY_VERSION;
    summary->input_bytes = YV8N_P5_DENSE_C2F_INPUT_BYTES;
    summary->output_bytes = YV8N_P5_DENSE_C2F_OUTPUT_BYTES;
    summary->input_h = YV8N_P5_DENSE_C2F_IN_H;
    summary->input_w = YV8N_P5_DENSE_C2F_IN_W;
    summary->input_c = YV8N_P5_DENSE_C2F_IN_C;
    summary->output_c = YV8N_P5_DENSE_C2F_OUT_C;
    summary->hidden_c = YV8N_P5_DENSE_C2F_HIDDEN_C;
    summary->detect_sample_points = YV8N_P5_DENSE_C2F_DETECT_SAMPLE_POINTS;
    summary->scratch_peak_bytes = YV8N_P5_DENSE_C2F_SCRATCH_PEAK_BYTES;
    summary->sampled_output_points = point_count;
    summary->input_hash = hash_full_i8(input, YV8N_P5_DENSE_C2F_INPUT_BYTES);
    summary->c2f_layer_hash = hash_c2f_layers();
    summary->output_hash = hash_full_i8(output, YV8N_P5_DENSE_C2F_OUTPUT_BYTES);
    summary->sampled_output_hash = sampled_hash;
    summary->computed_point_hash = point_hash;
    for (uint32_t i = 0; i < YV8N_P5_DENSE_C2F_DETECT_SAMPLE_POINTS; ++i)
        summary->detect_sample_point_ids[i] = sample_points[i];
    summary->computed_point_count = point_count;
    for (uint32_t i = 0; i < 4u; ++i)
        summary->first_output_q8[i] = output[i];
    for (uint32_t i = 0; i < (uint32_t)(sizeof(summary->reserved) /
                                        sizeof(summary->reserved[0])); ++i)
        summary->reserved[i] = 0u;

    return (yolov8n_p5_arena_high_water() <= YV8N_P5_DENSE_C2F_SCRATCH_PEAK_BYTES) ? 0 : -1;
}

static void update_detect_point_outputs(yolov8n_detect_summary_t *summary,
                                        uint32_t sample_idx,
                                        uint32_t point,
                                        const int8_t *box_raw,
                                        const int8_t *cls_raw,
                                        int32_t *box_public,
                                        uint16_t *cls_public,
                                        uint64_t *raw_hash,
                                        uint64_t *box_hash,
                                        uint64_t *cls_hash)
{
    int32_t box[4];
    uint16_t best_score = 0u;
    uint16_t best_cls = 0u;
    uint32_t y = point / YV8N_P5_DENSE_C2F_IN_W;
    uint32_t x = point % YV8N_P5_DENSE_C2F_IN_W;
    uint32_t left = dfl_project_q8(box_raw, 0u);
    uint32_t top = dfl_project_q8(box_raw, 1u);
    uint32_t right = dfl_project_q8(box_raw, 2u);
    uint32_t bottom = dfl_project_q8(box_raw, 3u);
    int32_t cx_grid_q8 = (int32_t)(x * 256u + 128u);
    int32_t cy_grid_q8 = (int32_t)(y * 256u + 128u);
    int32_t x1 = (cx_grid_q8 - (int32_t)left) * 32;
    int32_t y1 = (cy_grid_q8 - (int32_t)top) * 32;
    int32_t x2 = (cx_grid_q8 + (int32_t)right) * 32;
    int32_t y2 = (cy_grid_q8 + (int32_t)bottom) * 32;

    *raw_hash = hash_i8_bytes(*raw_hash, box_raw, YV8N_DETECT_DFL_CHANNELS);
    *raw_hash = hash_i8_bytes(*raw_hash, cls_raw, YV8N_DETECT_CLASS_CHANNELS);

    box[0] = (x1 + x2) / 2;
    box[1] = (y1 + y2) / 2;
    box[2] = x2 - x1;
    box[3] = y2 - y1;

    if (sample_idx == 0u) {
        for (uint32_t c = 0; c < 4u; ++c)
            summary->first_box_q8[c] = box[c];
    }

    for (uint32_t c = 0; c < 4u; ++c) {
        box_public[(size_t)sample_idx * 4u + c] = box[c];
        *box_hash = hash_u32_value(*box_hash, (uint32_t)box[c]);
    }

    for (uint32_t cls = 0; cls < YV8N_DETECT_CLASS_CHANNELS; ++cls) {
        uint16_t score = sigmoid_q15(cls_raw[cls]);

        if (score > best_score) {
            best_score = score;
            best_cls = (uint16_t)cls;
        }
        cls_public[(size_t)sample_idx * YV8N_DETECT_CLASS_CHANNELS + cls] = score;
        *cls_hash = hash_u32_value(*cls_hash, (uint32_t)score);
    }

    summary->sample_point_ids[sample_idx] = point;
    summary->best_classes[sample_idx] = best_cls;
}

static void conv3x3_input_at(const yolov8n_external_detect_c2f_layer_t *layer,
                             const int8_t *input,
                             int32_t y,
                             int32_t x,
                             int8_t *out)
{
    for (uint32_t oc = 0; oc < layer->K_out; ++oc) {
        int32_t acc = 0;

        for (uint32_t ky = 0; ky < layer->kH; ++ky) {
            int32_t iy = y + (int32_t)ky - (int32_t)layer->pad_h;

            for (uint32_t kx = 0; kx < layer->kW; ++kx) {
                int32_t ix = x + (int32_t)kx - (int32_t)layer->pad_w;
                size_t w_base = (size_t)oc * layer->w_stride +
                                ((size_t)ky * layer->kW + kx) * layer->C_in;

                if (iy < 0 || iy >= (int32_t)YV8N_P5_DENSE_C2F_IN_H ||
                    ix < 0 || ix >= (int32_t)YV8N_P5_DENSE_C2F_IN_W)
                    continue;

                for (uint32_t ic = 0; ic < layer->C_in; ++ic) {
                    size_t in_idx = (((size_t)(uint32_t)iy * YV8N_P5_DENSE_C2F_IN_W +
                                      (uint32_t)ix) * YV8N_P5_DENSE_C2F_OUT_C) + ic;

                    acc += (int32_t)input[in_idx] * (int32_t)layer->w[w_base + ic];
                }
            }
        }
        out[oc] = requant_layer_value(layer, oc, acc);
    }
}

static void fill_detect_taps(
    const yolov8n_external_detect_c2f_layer_t *layer,
    const int8_t *input,
    uint32_t point,
    int8_t *taps,
    uint32_t tap_stride,
    uint8_t *valid)
{
    int32_t y = (int32_t)(point / YV8N_P5_DENSE_C2F_IN_W);
    int32_t x = (int32_t)(point % YV8N_P5_DENSE_C2F_IN_W);

    for (uint32_t tap = 0; tap < 9u; ++tap) {
        int32_t iy = y + (int32_t)(tap / 3u) - 1;
        int32_t ix = x + (int32_t)(tap % 3u) - 1;

        valid[tap] = (iy >= 0 && iy < (int32_t)YV8N_P5_DENSE_C2F_IN_H &&
                      ix >= 0 && ix < (int32_t)YV8N_P5_DENSE_C2F_IN_W) ? 1u : 0u;
        if (valid[tap]) {
            conv3x3_input_at(layer, input, iy, ix,
                             taps + (size_t)tap * tap_stride);
        }
    }
}

static int build_detect_summary(const int8_t *input,
                                yolov8n_detect_summary_t *summary)
{
    const yolov8n_external_detect_c2f_layer_t *box0 =
        &yolov8n_external_detect_c2f_layers[YV8N_P5_DENSE_C2F_BOX0_ID];
    const yolov8n_external_detect_c2f_layer_t *box1 =
        &yolov8n_external_detect_c2f_layers[YV8N_P5_DENSE_C2F_BOX1_ID];
    const yolov8n_external_detect_c2f_layer_t *box2 =
        &yolov8n_external_detect_c2f_layers[YV8N_P5_DENSE_C2F_BOX2_ID];
    const yolov8n_external_detect_c2f_layer_t *cls0 =
        &yolov8n_external_detect_c2f_layers[YV8N_P5_DENSE_C2F_CLS0_ID];
    const yolov8n_external_detect_c2f_layer_t *cls1 =
        &yolov8n_external_detect_c2f_layers[YV8N_P5_DENSE_C2F_CLS1_ID];
    const yolov8n_external_detect_c2f_layer_t *cls2 =
        &yolov8n_external_detect_c2f_layers[YV8N_P5_DENSE_C2F_CLS2_ID];
    int8_t *box_taps;
    int8_t *box1_out;
    int8_t *box_raw;
    int8_t *cls_taps;
    int8_t *cls1_out;
    int8_t *cls_raw;
    int32_t *box_public;
    uint16_t *cls_public;
    uint8_t tap_valid[9];
    uint64_t raw_hash = FNV1A64_INIT;
    uint64_t box_hash = FNV1A64_INIT;
    uint64_t cls_hash = FNV1A64_INIT;
    uint64_t public_hash = FNV1A64_INIT;

    yolov8n_p5_arena_begin(YV8N_DETECT_SCRATCH_PEAK_BYTES);
    box_taps = yolov8n_p5_arena_alloc(9u * 64u, 64u);
    box1_out = yolov8n_p5_arena_alloc(64u, 64u);
    box_raw = yolov8n_p5_arena_alloc(YV8N_DETECT_DFL_CHANNELS, 64u);
    cls_taps = yolov8n_p5_arena_alloc(9u * 80u, 64u);
    cls1_out = yolov8n_p5_arena_alloc(80u, 64u);
    cls_raw = yolov8n_p5_arena_alloc(YV8N_DETECT_CLASS_CHANNELS, 64u);
    box_public = yolov8n_p5_arena_alloc(
        YV8N_P5_DENSE_C2F_DETECT_SAMPLE_POINTS * 4u * sizeof(*box_public), 64u);
    cls_public = yolov8n_p5_arena_alloc(
        YV8N_P5_DENSE_C2F_DETECT_SAMPLE_POINTS * YV8N_DETECT_CLASS_CHANNELS *
        sizeof(*cls_public), 64u);
    if (!box_taps || !box1_out || !box_raw || !cls_taps || !cls1_out ||
        !cls_raw || !box_public || !cls_public)
        return -1;

    summary->magic = YV8N_DETECT_SUMMARY_MAGIC;
    summary->version = YV8N_DETECT_SUMMARY_VERSION;
    summary->scale_id = 2u;
    summary->input_bytes = YV8N_P5_DENSE_C2F_OUTPUT_BYTES;
    summary->input_h = YV8N_P5_DENSE_C2F_IN_H;
    summary->input_w = YV8N_P5_DENSE_C2F_IN_W;
    summary->input_c = YV8N_P5_DENSE_C2F_OUT_C;
    summary->stride = 32u;
    summary->raw_channels = YV8N_DETECT_RAW_CHANNELS;
    summary->public_channels = YV8N_DETECT_PUBLIC_CHANNELS;
    summary->sample_points = YV8N_P5_DENSE_C2F_DETECT_SAMPLE_POINTS;
    summary->scratch_peak_bytes = YV8N_DETECT_SCRATCH_PEAK_BYTES;
    summary->input_hash = hash_full_i8(input, YV8N_P5_DENSE_C2F_OUTPUT_BYTES);
    summary->layer_hash = hash_detect_layers();
    for (uint32_t i = 0; i < (uint32_t)(sizeof(summary->reserved) /
                                        sizeof(summary->reserved[0])); ++i)
        summary->reserved[i] = 0u;

    for (uint32_t i = 0; i < YV8N_P5_DENSE_C2F_DETECT_SAMPLE_POINTS; ++i) {
        uint32_t point = sample_points[i];

        fill_detect_taps(box0, input, point, box_taps, 64u, tap_valid);
        conv3x3_from_patch(box1, box_taps, 3u, 64u, tap_valid, 1u, 1u, box1_out);
        conv1x1_vec(box2, box1_out, box_raw);

        fill_detect_taps(cls0, input, point, cls_taps, 80u, tap_valid);
        conv3x3_from_patch(cls1, cls_taps, 3u, 80u, tap_valid, 1u, 1u, cls1_out);
        conv1x1_vec(cls2, cls1_out, cls_raw);

        update_detect_point_outputs(summary, i, point, box_raw, cls_raw,
                                    box_public, cls_public,
                                    &raw_hash, &box_hash, &cls_hash);
    }

    for (uint32_t c = 0; c < 4u; ++c) {
        for (uint32_t i = 0; i < YV8N_P5_DENSE_C2F_DETECT_SAMPLE_POINTS; ++i) {
            public_hash = hash_u32_value(
                public_hash, (uint32_t)box_public[(size_t)i * 4u + c]);
        }
    }
    for (uint32_t cls = 0; cls < YV8N_DETECT_CLASS_CHANNELS; ++cls) {
        for (uint32_t i = 0; i < YV8N_P5_DENSE_C2F_DETECT_SAMPLE_POINTS; ++i) {
            public_hash = hash_u32_value(
                public_hash,
                (uint32_t)cls_public[(size_t)i * YV8N_DETECT_CLASS_CHANNELS + cls]);
        }
    }

    summary->raw_hash = raw_hash;
    summary->box_hash = box_hash;
    summary->cls_hash = cls_hash;
    summary->public_hash = public_hash;

    return (yolov8n_p5_arena_high_water() <= YV8N_DETECT_SCRATCH_PEAK_BYTES) ? 0 : -1;
}

static int validate_c2f_summary(const yolov8n_p5_dense_c2f_summary_t *summary)
{
    if (summary->magic != YV8N_P5_DENSE_C2F_SUMMARY_MAGIC ||
        summary->version != YV8N_P5_DENSE_C2F_SUMMARY_VERSION ||
        summary->input_bytes != YV8N_P5_DENSE_C2F_INPUT_BYTES ||
        summary->output_bytes != YV8N_P5_DENSE_C2F_OUTPUT_BYTES ||
        summary->input_h != YV8N_P5_DENSE_C2F_IN_H ||
        summary->input_w != YV8N_P5_DENSE_C2F_IN_W ||
        summary->input_c != YV8N_P5_DENSE_C2F_IN_C ||
        summary->output_c != YV8N_P5_DENSE_C2F_OUT_C ||
        summary->hidden_c != YV8N_P5_DENSE_C2F_HIDDEN_C ||
        summary->detect_sample_points != YV8N_P5_DENSE_C2F_DETECT_SAMPLE_POINTS ||
        summary->scratch_peak_bytes != YV8N_P5_DENSE_C2F_SCRATCH_PEAK_BYTES ||
        summary->sampled_output_points != YV8N_P5_DENSE_C2F_EXPECT_COMPUTED_POINTS ||
        summary->computed_point_count != YV8N_P5_DENSE_C2F_EXPECT_COMPUTED_POINTS)
        return -1;
    if (summary->input_hash != YV8N_P5_DENSE_C2F_EXPECT_INPUT_HASH ||
        summary->c2f_layer_hash != YV8N_P5_DENSE_C2F_EXPECT_LAYER_HASH ||
        summary->output_hash != YV8N_P5_DENSE_C2F_EXPECT_OUTPUT_HASH ||
        summary->sampled_output_hash != YV8N_P5_DENSE_C2F_EXPECT_SAMPLED_HASH ||
        summary->computed_point_hash != YV8N_P5_DENSE_C2F_EXPECT_POINT_HASH)
        return -1;
    for (uint32_t i = 0; i < YV8N_P5_DENSE_C2F_DETECT_SAMPLE_POINTS; ++i) {
        if (summary->detect_sample_point_ids[i] != sample_points[i])
            return -1;
    }
    if (summary->first_output_q8[0] != YV8N_P5_DENSE_C2F_EXPECT_FIRST0 ||
        summary->first_output_q8[1] != YV8N_P5_DENSE_C2F_EXPECT_FIRST1 ||
        summary->first_output_q8[2] != YV8N_P5_DENSE_C2F_EXPECT_FIRST2 ||
        summary->first_output_q8[3] != YV8N_P5_DENSE_C2F_EXPECT_FIRST3)
        return -1;
    for (uint32_t i = 0; i < (uint32_t)(sizeof(summary->reserved) /
                                        sizeof(summary->reserved[0])); ++i) {
        if (summary->reserved[i] != 0u)
            return -1;
    }
    return 0;
}

static int validate_detect_summary(const yolov8n_detect_summary_t *summary)
{
    if (summary->magic != YV8N_DETECT_SUMMARY_MAGIC ||
        summary->version != YV8N_DETECT_SUMMARY_VERSION ||
        summary->scale_id != 2u ||
        summary->input_bytes != YV8N_P5_DENSE_C2F_OUTPUT_BYTES ||
        summary->input_h != YV8N_P5_DENSE_C2F_IN_H ||
        summary->input_w != YV8N_P5_DENSE_C2F_IN_W ||
        summary->input_c != YV8N_P5_DENSE_C2F_OUT_C ||
        summary->stride != 32u ||
        summary->raw_channels != YV8N_DETECT_RAW_CHANNELS ||
        summary->public_channels != YV8N_DETECT_PUBLIC_CHANNELS ||
        summary->sample_points != YV8N_P5_DENSE_C2F_DETECT_SAMPLE_POINTS ||
        summary->scratch_peak_bytes != YV8N_DETECT_SCRATCH_PEAK_BYTES)
        return -1;
    if (summary->input_hash != YV8N_P5_DENSE_C2F_EXPECT_OUTPUT_HASH ||
        summary->layer_hash != UINT64_C(0x7fee3de2b008ded2) ||
        summary->raw_hash != YV8N_P5_DENSE_C2F_DETECT_EXPECT_RAW_HASH ||
        summary->box_hash != YV8N_P5_DENSE_C2F_DETECT_EXPECT_BOX_HASH ||
        summary->cls_hash != YV8N_P5_DENSE_C2F_DETECT_EXPECT_CLS_HASH ||
        summary->public_hash != YV8N_P5_DENSE_C2F_DETECT_EXPECT_PUBLIC_HASH)
        return -1;
    for (uint32_t i = 0; i < YV8N_P5_DENSE_C2F_DETECT_SAMPLE_POINTS; ++i) {
        if (summary->sample_point_ids[i] != sample_points[i] ||
            summary->best_classes[i] != 0u)
            return -1;
    }
    if (summary->first_box_q8[0] != YV8N_P5_DENSE_C2F_DETECT_EXPECT_FIRST0 ||
        summary->first_box_q8[1] != YV8N_P5_DENSE_C2F_DETECT_EXPECT_FIRST1 ||
        summary->first_box_q8[2] != YV8N_P5_DENSE_C2F_DETECT_EXPECT_FIRST2 ||
        summary->first_box_q8[3] != YV8N_P5_DENSE_C2F_DETECT_EXPECT_FIRST3)
        return -1;
    for (uint32_t i = 0; i < (uint32_t)(sizeof(summary->reserved) /
                                        sizeof(summary->reserved[0])); ++i) {
        if (summary->reserved[i] != 0u)
            return -1;
    }
    return 0;
}

tpa_op_t yolov8n_p5_dense_c2f_input_source_start(void)
{
    struct tpa_channel *ch = tpa_chan(0);
    int8_t *out = tpa_send_buf(ch);

    mark(YV8N_P5_DENSE_C2F_TRACE_BEGIN);
    YV8N_DETECT_REQUIRE_MSG_STOP("Y8D5:SRCBUF\n", out != 0);
    fill_c2f_source_edge(out);
    mark(YV8N_P5_DENSE_C2F_TRACE_SOURCE_SENT);
    return tpa_send(ch, out, YV8N_P5_DENSE_C2F_INPUT_BYTES,
                    yolov8n_p5_dense_c2f_input_source_done);
}

tpa_op_t yolov8n_p5_dense_c2f_input_source_done(void)
{
    return tpa_stop();
}

tpa_op_t yolov8n_p5_dense_c2f_start(void)
{
    c2f_ws_t *w = tpa_ws();

    return tpa_recv(tpa_chan(0), (void **)&w->input, &w->input_len,
                    yolov8n_p5_dense_c2f_run);
}

tpa_op_t yolov8n_p5_dense_c2f_run(void)
{
    c2f_ws_t *w = tpa_ws();
    struct tpa_channel *out_ch = tpa_chan(1);

    YV8N_DETECT_REQUIRE_MSG_STOP("Y8D5:LEN\n",
                                 w->input_len == YV8N_P5_DENSE_C2F_INPUT_BYTES);
    YV8N_DETECT_REQUIRE_MSG_STOP("Y8D5:LAYERS\n", verify_c2f_layers() == 0);
    w->output = (int8_t *)tpa_send_buf(out_ch);
    YV8N_DETECT_REQUIRE_MSG_STOP("Y8D5:OUTBUF\n", w->output != 0);
    YV8N_DETECT_REQUIRE_MSG_STOP("Y8D5:RUN\n",
                                 run_c2f(w->input, w->output, &w->summary) == 0);
    mark(YV8N_P5_DENSE_C2F_TRACE_C2F_DONE);
    return tpa_send(out_ch, w->output, YV8N_P5_DENSE_C2F_OUTPUT_BYTES,
                    yolov8n_p5_dense_c2f_send_summary);
}

tpa_op_t yolov8n_p5_dense_c2f_send_summary(void)
{
    c2f_ws_t *w = tpa_ws();
    struct tpa_channel *summary_ch = tpa_chan(2);
    yolov8n_p5_dense_c2f_summary_t *summary =
        (yolov8n_p5_dense_c2f_summary_t *)tpa_send_buf(summary_ch);

    YV8N_DETECT_REQUIRE_MSG_STOP("Y8D5:SUMBUF\n", summary != 0);
    *summary = w->summary;
    return tpa_send(summary_ch, summary, sizeof(*summary), yolov8n_p5_dense_c2f_done);
}

tpa_op_t yolov8n_p5_dense_c2f_done(void)
{
    return tpa_stop();
}

tpa_op_t yolov8n_p5_dense_c2f_detect_start(void)
{
    detect_ws_t *w = tpa_ws();

    return tpa_recv(tpa_chan(0), (void **)&w->input, &w->input_len,
                    yolov8n_p5_dense_c2f_detect_run);
}

tpa_op_t yolov8n_p5_dense_c2f_detect_run(void)
{
    detect_ws_t *w = tpa_ws();
    struct tpa_channel *out_ch = tpa_chan(1);

    YV8N_DETECT_REQUIRE_MSG_STOP("Y8D5DET:LEN\n",
                                 w->input_len == YV8N_P5_DENSE_C2F_OUTPUT_BYTES);
    YV8N_DETECT_REQUIRE_MSG_STOP("Y8D5DET:LAYERS\n", verify_detect_layers() == 0);
    w->summary = (yolov8n_detect_summary_t *)tpa_send_buf(out_ch);
    YV8N_DETECT_REQUIRE_MSG_STOP("Y8D5DET:SUMBUF\n", w->summary != 0);
    YV8N_DETECT_REQUIRE_MSG_STOP("Y8D5DET:RUN\n",
                                 build_detect_summary(w->input, w->summary) == 0);
    mark(YV8N_P5_DENSE_C2F_TRACE_DETECT_DONE);
    return tpa_send(out_ch, w->summary, sizeof(*w->summary),
                    yolov8n_p5_dense_c2f_detect_done);
}

tpa_op_t yolov8n_p5_dense_c2f_detect_done(void)
{
    return tpa_stop();
}

tpa_op_t yolov8n_p5_dense_c2f_detect_checker_start(void)
{
    checker_ws_t *w = tpa_ws();

    return tpa_recv(tpa_chan(0), (void **)&w->c2f_summary, &w->c2f_len,
                    yolov8n_p5_dense_c2f_detect_checker_recv_detect);
}

tpa_op_t yolov8n_p5_dense_c2f_detect_checker_recv_detect(void)
{
    checker_ws_t *w = tpa_ws();

    return tpa_recv(tpa_chan(1), (void **)&w->detect_summary, &w->detect_len,
                    yolov8n_p5_dense_c2f_detect_checker_run);
}

tpa_op_t yolov8n_p5_dense_c2f_detect_checker_run(void)
{
    checker_ws_t *w = tpa_ws();

    if (w->c2f_len != sizeof(*w->c2f_summary)) {
        yolov8n_detect_diag_puts("Y8D5:CSUMLEN\n");
        mark(YV8N_P5_DENSE_C2F_TRACE_FAIL);
        TEST_FAIL;
        return tpa_stop();
    }
    if (w->detect_len != sizeof(*w->detect_summary)) {
        yolov8n_detect_diag_puts("Y8D5:DSUMLEN\n");
        mark(YV8N_P5_DENSE_C2F_TRACE_FAIL);
        TEST_FAIL;
        return tpa_stop();
    }
    if (validate_c2f_summary(w->c2f_summary)) {
        yolov8n_detect_diag_puts("Y8D5:C2FSUM\n");
        diag_hex64("IN=", w->c2f_summary->input_hash);
        diag_hex64("LAYER=", w->c2f_summary->c2f_layer_hash);
        diag_hex64("OUT=", w->c2f_summary->output_hash);
        diag_hex64("SAMP=", w->c2f_summary->sampled_output_hash);
        diag_hex64("POINT=", w->c2f_summary->computed_point_hash);
        diag_hex64("COUNT=", w->c2f_summary->computed_point_count);
        mark(YV8N_P5_DENSE_C2F_TRACE_FAIL);
        TEST_FAIL;
        return tpa_stop();
    }
    if (validate_detect_summary(w->detect_summary)) {
        yolov8n_detect_diag_puts("Y8D5:DETSUM\n");
        diag_hex64("DIN=", w->detect_summary->input_hash);
        diag_hex64("DRAW=", w->detect_summary->raw_hash);
        diag_hex64("DBOX=", w->detect_summary->box_hash);
        diag_hex64("DCLS=", w->detect_summary->cls_hash);
        diag_hex64("DPUB=", w->detect_summary->public_hash);
        diag_hex64("DF0=", (uint32_t)w->detect_summary->first_box_q8[0]);
        diag_hex64("DF1=", (uint32_t)w->detect_summary->first_box_q8[1]);
        diag_hex64("DF2=", (uint32_t)w->detect_summary->first_box_q8[2]);
        diag_hex64("DF3=", (uint32_t)w->detect_summary->first_box_q8[3]);
        mark(YV8N_P5_DENSE_C2F_TRACE_FAIL);
        TEST_FAIL;
        return tpa_stop();
    }

    mark(YV8N_P5_DENSE_C2F_TRACE_CHECK_PASS);
    TEST_PASS;
    return tpa_stop();
}
