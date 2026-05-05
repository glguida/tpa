#include "yolov8n_p5_common.h"

#include <stdint.h>

#ifndef TPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER
#error "Configure with -DTPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER=/path/to/yolov8n_external_detect_c2f_weights.h"
#endif
#include TPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER

#define YV8N_P5_TRACE_BEGIN 0xe8a50000u
#define YV8N_P5_TRACE_SOURCE_SENT 0xe8a50001u
#define YV8N_P5_TRACE_DETECT_DONE 0xe8a50002u
#define YV8N_P5_TRACE_CHECK_PASS 0xe8a50003u
#define YV8N_P5_TRACE_FAIL 0xe8a50eeu

#define YV8N_P5_BOX0_ID 18u
#define YV8N_P5_BOX1_ID 19u
#define YV8N_P5_BOX2_ID 20u
#define YV8N_P5_CLS0_ID 27u
#define YV8N_P5_CLS1_ID 28u
#define YV8N_P5_CLS2_ID 29u
#define YV8N_P5_DFL_ID 30u

#define FNV1A64_INIT UINT64_C(0xcbf29ce484222325)
#define FNV1A64_PRIME UINT64_C(0x100000001b3)

typedef struct {
    const int8_t *p5;
    uint32_t p5_len;
    yolov8n_p5_summary_t *summary;
} yolov8n_p5_detect_ws_t;

typedef struct {
    const yolov8n_p5_summary_t *summary;
    uint32_t summary_len;
} yolov8n_p5_checker_ws_t;

tpa_op_t yolov8n_p5_source_start(void);
tpa_op_t yolov8n_p5_source_done(void);
tpa_op_t yolov8n_p5_detect_start(void);
tpa_op_t yolov8n_p5_detect_run(void);
tpa_op_t yolov8n_p5_detect_done(void);
tpa_op_t yolov8n_p5_checker_start(void);
tpa_op_t yolov8n_p5_checker_run(void);

TPA_PROC_MEM_META(yolov8n_p5_source_meta, 810u, 0u);
TPA_PROC_MEM_META(yolov8n_p5_detect_meta, 811u,
                  YV8N_P5_DETECT_SCRATCH_PEAK_BYTES);
TPA_PROC_MEM_META(yolov8n_p5_checker_meta, 812u, 0u);

static const uint32_t sample_points[YV8N_P5_SAMPLE_POINTS] = {
    0u, 1u, 19u, 20u, 123u, 210u, 381u, 399u,
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

static int8_t p5_synthetic_value(uint32_t y, uint32_t x, uint32_t c)
{
    uint32_t v = (y * 37u + x * 17u + c * 13u +
                  (y + 1u) * (x + 3u) + ((c & 7u) * 11u)) & 0xffu;

    return (int8_t)((int32_t)v - 128);
}

static uint64_t hash_full_input(const int8_t *p5)
{
    uint64_t h = FNV1A64_INIT;

    for (uint32_t i = 0; i < YV8N_P5_INPUT_BYTES; ++i)
        h = fnv1a64_step(h, (uint8_t)p5[i]);

    return h;
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

static void conv3x3_p5_at(const yolov8n_external_detect_c2f_layer_t *layer,
                          const int8_t *p5,
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

                if (iy < 0 || iy >= (int32_t)YV8N_P5_H ||
                    ix < 0 || ix >= (int32_t)YV8N_P5_W)
                    continue;

                for (uint32_t ic = 0; ic < layer->C_in; ++ic) {
                    size_t in_idx = (((size_t)(uint32_t)iy * YV8N_P5_W +
                                      (uint32_t)ix) * YV8N_P5_C) + ic;

                    acc += (int32_t)p5[in_idx] * (int32_t)layer->w[w_base + ic];
                }
            }
        }
        out[oc] = requant_layer_value(layer, oc, acc);
    }
}

static void fill_first_layer_taps(
    const yolov8n_external_detect_c2f_layer_t *layer,
    const int8_t *p5,
    uint32_t point,
    int8_t *taps,
    uint32_t tap_stride,
    uint8_t *valid)
{
    int32_t y = (int32_t)(point / YV8N_P5_W);
    int32_t x = (int32_t)(point % YV8N_P5_W);

    for (uint32_t tap = 0; tap < 9u; ++tap) {
        int32_t iy = y + (int32_t)(tap / 3u) - 1;
        int32_t ix = x + (int32_t)(tap % 3u) - 1;

        valid[tap] = (iy >= 0 && iy < (int32_t)YV8N_P5_H &&
                      ix >= 0 && ix < (int32_t)YV8N_P5_W) ? 1u : 0u;
        if (valid[tap])
            conv3x3_p5_at(layer, p5, iy, ix, taps + (size_t)tap * tap_stride);
    }
}

static void conv3x3_taps_at(const yolov8n_external_detect_c2f_layer_t *layer,
                            const int8_t *taps,
                            uint32_t tap_stride,
                            const uint8_t *valid,
                            int8_t *out)
{
    for (uint32_t oc = 0; oc < layer->K_out; ++oc) {
        int32_t acc = 0;

        for (uint32_t ky = 0; ky < layer->kH; ++ky) {
            for (uint32_t kx = 0; kx < layer->kW; ++kx) {
                uint32_t tap = ky * layer->kW + kx;
                const int8_t *in = taps + (size_t)tap * tap_stride;
                size_t w_base = (size_t)oc * layer->w_stride +
                                ((size_t)ky * layer->kW + kx) * layer->C_in;

                if (!valid[tap])
                    continue;

                for (uint32_t ic = 0; ic < layer->C_in; ++ic)
                    acc += (int32_t)in[ic] * (int32_t)layer->w[w_base + ic];
            }
        }
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
    uint32_t base = coord * YV8N_P5_REG_MAX;

    for (uint32_t bin = 0; bin < YV8N_P5_REG_MAX; ++bin) {
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

static uint64_t hash_selected_layers(void)
{
    static const uint32_t layer_ids[] = {
        YV8N_P5_BOX0_ID, YV8N_P5_BOX1_ID, YV8N_P5_BOX2_ID,
        YV8N_P5_CLS0_ID, YV8N_P5_CLS1_ID, YV8N_P5_CLS2_ID,
        YV8N_P5_DFL_ID,
    };
    uint64_t h = FNV1A64_INIT;

    for (uint32_t i = 0; i < (uint32_t)(sizeof(layer_ids) / sizeof(layer_ids[0])); ++i) {
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

static int verify_selected_layer_shapes(void)
{
    const yolov8n_external_detect_c2f_layer_t *box0 =
        &yolov8n_external_detect_c2f_layers[YV8N_P5_BOX0_ID];
    const yolov8n_external_detect_c2f_layer_t *box1 =
        &yolov8n_external_detect_c2f_layers[YV8N_P5_BOX1_ID];
    const yolov8n_external_detect_c2f_layer_t *box2 =
        &yolov8n_external_detect_c2f_layers[YV8N_P5_BOX2_ID];
    const yolov8n_external_detect_c2f_layer_t *cls0 =
        &yolov8n_external_detect_c2f_layers[YV8N_P5_CLS0_ID];
    const yolov8n_external_detect_c2f_layer_t *cls1 =
        &yolov8n_external_detect_c2f_layers[YV8N_P5_CLS1_ID];
    const yolov8n_external_detect_c2f_layer_t *cls2 =
        &yolov8n_external_detect_c2f_layers[YV8N_P5_CLS2_ID];
    const yolov8n_external_detect_c2f_layer_t *dfl =
        &yolov8n_external_detect_c2f_layers[YV8N_P5_DFL_ID];

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
    if (dfl->K_out != 1u || dfl->C_in != YV8N_P5_REG_MAX ||
        dfl->kH != 1u || dfl->kW != 1u)
        return -1;

    return 0;
}

static void update_point_outputs(yolov8n_p5_summary_t *summary,
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
    uint32_t y = point / YV8N_P5_W;
    uint32_t x = point % YV8N_P5_W;
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

    *raw_hash = hash_i8_bytes(*raw_hash, box_raw, YV8N_P5_DFL_CHANNELS);
    *raw_hash = hash_i8_bytes(*raw_hash, cls_raw, YV8N_P5_CLASS_CHANNELS);

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

    for (uint32_t cls = 0; cls < YV8N_P5_CLASS_CHANNELS; ++cls) {
        uint16_t score = sigmoid_q15(cls_raw[cls]);

        if (score > best_score) {
            best_score = score;
            best_cls = (uint16_t)cls;
        }
        cls_public[(size_t)sample_idx * YV8N_P5_CLASS_CHANNELS + cls] = score;
        *cls_hash = hash_u32_value(*cls_hash, (uint32_t)score);
    }

    summary->sample_point_ids[sample_idx] = point;
    summary->best_classes[sample_idx] = best_cls;
}

static int build_summary(const int8_t *p5, yolov8n_p5_summary_t *summary)
{
    const yolov8n_external_detect_c2f_layer_t *box0 =
        &yolov8n_external_detect_c2f_layers[YV8N_P5_BOX0_ID];
    const yolov8n_external_detect_c2f_layer_t *box1 =
        &yolov8n_external_detect_c2f_layers[YV8N_P5_BOX1_ID];
    const yolov8n_external_detect_c2f_layer_t *box2 =
        &yolov8n_external_detect_c2f_layers[YV8N_P5_BOX2_ID];
    const yolov8n_external_detect_c2f_layer_t *cls0 =
        &yolov8n_external_detect_c2f_layers[YV8N_P5_CLS0_ID];
    const yolov8n_external_detect_c2f_layer_t *cls1 =
        &yolov8n_external_detect_c2f_layers[YV8N_P5_CLS1_ID];
    const yolov8n_external_detect_c2f_layer_t *cls2 =
        &yolov8n_external_detect_c2f_layers[YV8N_P5_CLS2_ID];
    int8_t (*box_taps)[64];
    int8_t *box1_out;
    int8_t *box_raw;
    int8_t (*cls_taps)[80];
    int8_t *cls1_out;
    int8_t *cls_raw;
    int32_t *box_public;
    uint16_t *cls_public;
    uint8_t tap_valid[9];
    uint64_t raw_hash = FNV1A64_INIT;
    uint64_t box_hash = FNV1A64_INIT;
    uint64_t cls_hash = FNV1A64_INIT;
    uint64_t public_hash = FNV1A64_INIT;

    yolov8n_p5_arena_begin(YV8N_P5_DETECT_SCRATCH_PEAK_BYTES);
    box_taps = yolov8n_p5_arena_alloc(9u * 64u, 64u);
    box1_out = yolov8n_p5_arena_alloc(64u, 64u);
    box_raw = yolov8n_p5_arena_alloc(64u, 64u);
    cls_taps = yolov8n_p5_arena_alloc(9u * 80u, 64u);
    cls1_out = yolov8n_p5_arena_alloc(80u, 64u);
    cls_raw = yolov8n_p5_arena_alloc(80u, 64u);
    box_public = yolov8n_p5_arena_alloc(YV8N_P5_SAMPLE_POINTS * 4u * sizeof(*box_public), 64u);
    cls_public = yolov8n_p5_arena_alloc(YV8N_P5_SAMPLE_POINTS * YV8N_P5_CLASS_CHANNELS * sizeof(*cls_public), 64u);
    if (!box_taps || !box1_out || !box_raw || !cls_taps || !cls1_out ||
        !cls_raw || !box_public || !cls_public)
        return -1;

    summary->magic = YV8N_P5_SUMMARY_MAGIC;
    summary->version = YV8N_P5_SUMMARY_VERSION;
    summary->p5_input_bytes = YV8N_P5_INPUT_BYTES;
    summary->sample_points = YV8N_P5_SAMPLE_POINTS;
    summary->raw_channels = YV8N_P5_RAW_CHANNELS;
    summary->public_channels = YV8N_P5_PUBLIC_CHANNELS;
    summary->input_hash = hash_full_input(p5);
    summary->layer_hash = hash_selected_layers();
    for (uint32_t i = 0; i < (uint32_t)(sizeof(summary->reserved) /
                                        sizeof(summary->reserved[0])); ++i)
        summary->reserved[i] = 0u;

    for (uint32_t i = 0; i < YV8N_P5_SAMPLE_POINTS; ++i) {
        uint32_t point = sample_points[i];

        fill_first_layer_taps(box0, p5, point, &box_taps[0][0], 64u, tap_valid);
        conv3x3_taps_at(box1, &box_taps[0][0], 64u, tap_valid, box1_out);
        conv1x1_vec(box2, box1_out, box_raw);

        fill_first_layer_taps(cls0, p5, point, &cls_taps[0][0], 80u, tap_valid);
        conv3x3_taps_at(cls1, &cls_taps[0][0], 80u, tap_valid, cls1_out);
        conv1x1_vec(cls2, cls1_out, cls_raw);

        update_point_outputs(summary, i, point, box_raw, cls_raw,
                             box_public, cls_public,
                             &raw_hash, &box_hash, &cls_hash);
    }

    for (uint32_t c = 0; c < 4u; ++c) {
        for (uint32_t i = 0; i < YV8N_P5_SAMPLE_POINTS; ++i) {
            public_hash = hash_u32_value(
                public_hash, (uint32_t)box_public[(size_t)i * 4u + c]);
        }
    }
    for (uint32_t cls = 0; cls < YV8N_P5_CLASS_CHANNELS; ++cls) {
        for (uint32_t i = 0; i < YV8N_P5_SAMPLE_POINTS; ++i) {
            public_hash = hash_u32_value(
                public_hash,
                (uint32_t)cls_public[(size_t)i * YV8N_P5_CLASS_CHANNELS + cls]);
        }
    }

    summary->raw_hash = raw_hash;
    summary->box_hash = box_hash;
    summary->cls_hash = cls_hash;
    summary->public_hash = public_hash;

    return (yolov8n_p5_arena_high_water() <= YV8N_P5_DETECT_SCRATCH_PEAK_BYTES) ? 0 : -1;
}

static int validate_summary(const yolov8n_p5_summary_t *summary)
{
    if (summary->magic != YV8N_P5_SUMMARY_MAGIC ||
        summary->version != YV8N_P5_SUMMARY_VERSION ||
        summary->p5_input_bytes != YV8N_P5_INPUT_BYTES ||
        summary->sample_points != YV8N_P5_SAMPLE_POINTS ||
        summary->raw_channels != YV8N_P5_RAW_CHANNELS ||
        summary->public_channels != YV8N_P5_PUBLIC_CHANNELS)
        return -1;
    if (summary->input_hash != YV8N_P5_EXPECT_INPUT_HASH ||
        summary->layer_hash != YV8N_P5_EXPECT_LAYER_HASH ||
        summary->raw_hash != YV8N_P5_EXPECT_RAW_HASH ||
        summary->box_hash != YV8N_P5_EXPECT_BOX_HASH ||
        summary->cls_hash != YV8N_P5_EXPECT_CLS_HASH ||
        summary->public_hash != YV8N_P5_EXPECT_PUBLIC_HASH)
        return -1;
    for (uint32_t i = 0; i < YV8N_P5_SAMPLE_POINTS; ++i) {
        if (summary->sample_point_ids[i] != sample_points[i] ||
            summary->best_classes[i] != 0u)
            return -1;
    }
    if (summary->first_box_q8[0] != 2128 || summary->first_box_q8[1] != 6208 ||
        summary->first_box_q8[2] != 86944 || summary->first_box_q8[3] != 72832)
        return -1;
    for (uint32_t i = 0; i < (uint32_t)(sizeof(summary->reserved) /
                                        sizeof(summary->reserved[0])); ++i) {
        if (summary->reserved[i] != 0u)
            return -1;
    }
    return 0;
}

static void fill_source_edge(int8_t *out)
{
    for (uint32_t y = 0; y < YV8N_P5_H; ++y) {
        for (uint32_t x = 0; x < YV8N_P5_W; ++x) {
            for (uint32_t c = 0; c < YV8N_P5_C; ++c) {
                size_t idx = (((size_t)y * YV8N_P5_W + x) * YV8N_P5_C) + c;

                out[idx] = p5_synthetic_value(y, x, c);
            }
        }
    }
}

tpa_op_t yolov8n_p5_source_start(void)
{
    struct tpa_channel *ch = tpa_chan(0);
    int8_t *out = tpa_send_buf(ch);

    mark(YV8N_P5_TRACE_BEGIN);
    YV8N_P5_REQUIRE_MSG_STOP("YV8P5:SRCBUF\n", out != 0);
    fill_source_edge(out);
    mark(YV8N_P5_TRACE_SOURCE_SENT);
    return tpa_send(ch, out, YV8N_P5_INPUT_BYTES, yolov8n_p5_source_done);
}

tpa_op_t yolov8n_p5_source_done(void)
{
    return tpa_stop();
}

tpa_op_t yolov8n_p5_detect_start(void)
{
    yolov8n_p5_detect_ws_t *w = tpa_ws();

    return tpa_recv(tpa_chan(0), (void **)&w->p5, &w->p5_len,
                    yolov8n_p5_detect_run);
}

tpa_op_t yolov8n_p5_detect_run(void)
{
    yolov8n_p5_detect_ws_t *w = tpa_ws();
    struct tpa_channel *out_ch = tpa_chan(1);

    YV8N_P5_REQUIRE_MSG_STOP("YV8P5:LEN\n", w->p5_len == YV8N_P5_INPUT_BYTES);
    YV8N_P5_REQUIRE_MSG_STOP("YV8P5:LAYERS\n", verify_selected_layer_shapes() == 0);
    w->summary = (yolov8n_p5_summary_t *)tpa_send_buf(out_ch);
    YV8N_P5_REQUIRE_MSG_STOP("YV8P5:SUMBUF\n", w->summary != 0);
    YV8N_P5_REQUIRE_MSG_STOP("YV8P5:RUN\n", build_summary(w->p5, w->summary) == 0);
    mark(YV8N_P5_TRACE_DETECT_DONE);
    return tpa_send(out_ch, w->summary, sizeof(*w->summary),
                    yolov8n_p5_detect_done);
}

tpa_op_t yolov8n_p5_detect_done(void)
{
    return tpa_stop();
}

tpa_op_t yolov8n_p5_checker_start(void)
{
    yolov8n_p5_checker_ws_t *w = tpa_ws();

    return tpa_recv(tpa_chan(0), (void **)&w->summary, &w->summary_len,
                    yolov8n_p5_checker_run);
}

tpa_op_t yolov8n_p5_checker_run(void)
{
    yolov8n_p5_checker_ws_t *w = tpa_ws();

    if (w->summary_len != sizeof(*w->summary) || validate_summary(w->summary)) {
        mark(YV8N_P5_TRACE_FAIL);
        TEST_FAIL;
        return tpa_stop();
    }

    mark(YV8N_P5_TRACE_CHECK_PASS);
    TEST_PASS;
    return tpa_stop();
}
