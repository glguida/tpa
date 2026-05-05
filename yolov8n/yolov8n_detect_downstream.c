#include "yolov8n_detect_downstream.h"

#include <stdint.h>

#ifndef TPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER
#error "Configure with -DTPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER=/path/to/yolov8n_external_detect_c2f_weights.h"
#endif
#include TPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER

#define YV8N_DETECT_TRACE_BEGIN 0xe8d70000u
#define YV8N_DETECT_TRACE_SOURCE_SENT 0xe8d70001u
#define YV8N_DETECT_TRACE_SCALE_DONE 0xe8d70002u
#define YV8N_DETECT_TRACE_CHECK_PASS 0xe8d70003u
#define YV8N_DETECT_TRACE_FAIL 0xe8d70eeu

#define YV8N_DFL_ID 30u
#define FNV1A64_INIT UINT64_C(0xcbf29ce484222325)
#define FNV1A64_PRIME UINT64_C(0x100000001b3)

typedef struct {
    uint32_t scale_id;
    uint32_t input_h;
    uint32_t input_w;
    uint32_t input_c;
    uint32_t stride;
    uint32_t input_bytes;
    uint32_t seed;
    uint32_t box_ids[3];
    uint32_t cls_ids[3];
    uint32_t sample_points[YV8N_DETECT_SAMPLE_POINTS];
    uint64_t expect_input_hash;
    uint64_t expect_layer_hash;
    uint64_t expect_raw_hash;
    uint64_t expect_box_hash;
    uint64_t expect_cls_hash;
    uint64_t expect_public_hash;
    int32_t expect_first_box_q8[4];
} detect_scale_config_t;

typedef struct {
    const int8_t *input;
    uint32_t input_len;
    uint32_t scale_id;
    yolov8n_detect_summary_t *summary;
} detect_ws_t;

typedef struct {
    const yolov8n_detect_summary_t *summary[3];
    uint32_t len[3];
} checker_ws_t;

tpa_op_t yolov8n_p3_source_start(void);
tpa_op_t yolov8n_p4_source_start(void);
tpa_op_t yolov8n_detect_p5_source_start(void);
tpa_op_t yolov8n_detect_source_done(void);
tpa_op_t yolov8n_p3_detect_start(void);
tpa_op_t yolov8n_p4_detect_start(void);
tpa_op_t yolov8n_detect_p5_start(void);
tpa_op_t yolov8n_detect_run(void);
tpa_op_t yolov8n_detect_done(void);
tpa_op_t yolov8n_detect_checker_start(void);
tpa_op_t yolov8n_detect_checker_recv_p4(void);
tpa_op_t yolov8n_detect_checker_recv_p5(void);
tpa_op_t yolov8n_detect_checker_run(void);

TPA_PROC_MEM_META(yolov8n_p3_source_meta, 820u, 0u);
TPA_PROC_MEM_META(yolov8n_p4_source_meta, 821u, 0u);
TPA_PROC_MEM_META(yolov8n_detect_p5_source_meta, 822u, 0u);
TPA_PROC_MEM_META(yolov8n_p3_detect_meta, 823u,
                  YV8N_DETECT_SCRATCH_PEAK_BYTES);
TPA_PROC_MEM_META(yolov8n_p4_detect_meta, 824u,
                  YV8N_DETECT_SCRATCH_PEAK_BYTES);
TPA_PROC_MEM_META(yolov8n_detect_p5_meta, 825u,
                  YV8N_DETECT_SCRATCH_PEAK_BYTES);
TPA_PROC_MEM_META(yolov8n_detect_checker_meta, 826u, 0u);

static const detect_scale_config_t scale_configs[3] = {
    {
        .scale_id = 0u,
        .input_h = 80u,
        .input_w = 80u,
        .input_c = 64u,
        .stride = 8u,
        .input_bytes = 409600u,
        .seed = 7u,
        .box_ids = { 12u, 13u, 14u },
        .cls_ids = { 21u, 22u, 23u },
        .sample_points = { 0u, 1u, 79u, 80u, 1234u, 3200u, 6321u, 6399u },
        .expect_input_hash = UINT64_C(0x4b30c1d8cde2ed25),
        .expect_layer_hash = UINT64_C(0x9d19e64e48a8442e),
        .expect_raw_hash = UINT64_C(0xa700f1a7e5080657),
        .expect_box_hash = UINT64_C(0x60b19de0282a5796),
        .expect_cls_hash = UINT64_C(0xccf1a1f80e179625),
        .expect_public_hash = UINT64_C(0xe527ab1041a1ae06),
        .expect_first_box_q8 = { -108, 5972, 10120, 26264 },
    },
    {
        .scale_id = 1u,
        .input_h = 40u,
        .input_w = 40u,
        .input_c = 128u,
        .stride = 16u,
        .input_bytes = 204800u,
        .seed = 3u,
        .box_ids = { 15u, 16u, 17u },
        .cls_ids = { 24u, 25u, 26u },
        .sample_points = { 0u, 1u, 39u, 40u, 333u, 800u, 1511u, 1599u },
        .expect_input_hash = UINT64_C(0xf0daec814873f825),
        .expect_layer_hash = UINT64_C(0x10eff3fa57a1f07),
        .expect_raw_hash = UINT64_C(0x2ed96213fc3d5689),
        .expect_box_hash = UINT64_C(0xc4be6a8c404fd834),
        .expect_cls_hash = UINT64_C(0xccf1a1f80e179625),
        .expect_public_hash = UINT64_C(0xf3bae7a94b5a4f7c),
        .expect_first_box_q8 = { 1488, 4960, 37504, 65184 },
    },
    {
        .scale_id = 2u,
        .input_h = 20u,
        .input_w = 20u,
        .input_c = 256u,
        .stride = 32u,
        .input_bytes = 102400u,
        .seed = 0u,
        .box_ids = { 18u, 19u, 20u },
        .cls_ids = { 27u, 28u, 29u },
        .sample_points = { 0u, 1u, 19u, 20u, 123u, 210u, 381u, 399u },
        .expect_input_hash = UINT64_C(0xd96b5a8240c15925),
        .expect_layer_hash = UINT64_C(0x7fee3de2b008ded2),
        .expect_raw_hash = UINT64_C(0x3745115a813ef580),
        .expect_box_hash = UINT64_C(0x20ae08b5c9a56429),
        .expect_cls_hash = UINT64_C(0xccf1a1f80e179625),
        .expect_public_hash = UINT64_C(0xb4d949f3c3807a75),
        .expect_first_box_q8 = { 2128, 6208, 86944, 72832 },
    },
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

static int8_t scale_synthetic_value(const detect_scale_config_t *cfg,
                                    uint32_t y,
                                    uint32_t x,
                                    uint32_t c)
{
    uint32_t v = (y * 37u + x * 17u + c * 13u +
                  (y + 1u) * (x + 3u) + ((c & 7u) * 11u) +
                  cfg->seed * 29u) & 0xffu;

    return (int8_t)((int32_t)v - 128);
}

static const detect_scale_config_t *scale_config(uint32_t scale_id)
{
    if (scale_id >= (uint32_t)(sizeof(scale_configs) / sizeof(scale_configs[0])))
        return 0;
    return &scale_configs[scale_id];
}

static uint64_t hash_full_input(const int8_t *input, uint32_t bytes)
{
    uint64_t h = FNV1A64_INIT;

    for (uint32_t i = 0; i < bytes; ++i)
        h = fnv1a64_step(h, (uint8_t)input[i]);

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

static void conv3x3_input_at(const detect_scale_config_t *cfg,
                             const yolov8n_external_detect_c2f_layer_t *layer,
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

                if (iy < 0 || iy >= (int32_t)cfg->input_h ||
                    ix < 0 || ix >= (int32_t)cfg->input_w)
                    continue;

                for (uint32_t ic = 0; ic < layer->C_in; ++ic) {
                    size_t in_idx = (((size_t)(uint32_t)iy * cfg->input_w +
                                      (uint32_t)ix) * cfg->input_c) + ic;

                    acc += (int32_t)input[in_idx] * (int32_t)layer->w[w_base + ic];
                }
            }
        }
        out[oc] = requant_layer_value(layer, oc, acc);
    }
}

static void fill_first_layer_taps(
    const detect_scale_config_t *cfg,
    const yolov8n_external_detect_c2f_layer_t *layer,
    const int8_t *input,
    uint32_t point,
    int8_t *taps,
    uint32_t tap_stride,
    uint8_t *valid)
{
    int32_t y = (int32_t)(point / cfg->input_w);
    int32_t x = (int32_t)(point % cfg->input_w);

    for (uint32_t tap = 0; tap < 9u; ++tap) {
        int32_t iy = y + (int32_t)(tap / 3u) - 1;
        int32_t ix = x + (int32_t)(tap % 3u) - 1;

        valid[tap] = (iy >= 0 && iy < (int32_t)cfg->input_h &&
                      ix >= 0 && ix < (int32_t)cfg->input_w) ? 1u : 0u;
        if (valid[tap]) {
            conv3x3_input_at(cfg, layer, input, iy, ix,
                             taps + (size_t)tap * tap_stride);
        }
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

static uint64_t hash_selected_layers(const detect_scale_config_t *cfg)
{
    uint64_t h = FNV1A64_INIT;

    for (uint32_t i = 0; i < 7u; ++i) {
        uint32_t id = (i < 3u) ? cfg->box_ids[i] :
                      ((i < 6u) ? cfg->cls_ids[i - 3u] : YV8N_DFL_ID);
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

static int verify_scale_layers(const detect_scale_config_t *cfg)
{
    const yolov8n_external_detect_c2f_layer_t *box0 =
        &yolov8n_external_detect_c2f_layers[cfg->box_ids[0]];
    const yolov8n_external_detect_c2f_layer_t *box1 =
        &yolov8n_external_detect_c2f_layers[cfg->box_ids[1]];
    const yolov8n_external_detect_c2f_layer_t *box2 =
        &yolov8n_external_detect_c2f_layers[cfg->box_ids[2]];
    const yolov8n_external_detect_c2f_layer_t *cls0 =
        &yolov8n_external_detect_c2f_layers[cfg->cls_ids[0]];
    const yolov8n_external_detect_c2f_layer_t *cls1 =
        &yolov8n_external_detect_c2f_layers[cfg->cls_ids[1]];
    const yolov8n_external_detect_c2f_layer_t *cls2 =
        &yolov8n_external_detect_c2f_layers[cfg->cls_ids[2]];
    const yolov8n_external_detect_c2f_layer_t *dfl =
        &yolov8n_external_detect_c2f_layers[YV8N_DFL_ID];

    if (YOLOV8N_EXTERNAL_DETECT_C2F_N_LAYERS != 32u)
        return -1;
    if (box0->K_out != 64u || box0->C_in != cfg->input_c || box0->kH != 3u ||
        box0->kW != 3u || box0->pad_h != 1u || box0->pad_w != 1u)
        return -1;
    if (box1->K_out != 64u || box1->C_in != 64u || box1->kH != 3u ||
        box1->kW != 3u || box1->pad_h != 1u || box1->pad_w != 1u)
        return -1;
    if (box2->K_out != 64u || box2->C_in != 64u || box2->kH != 1u ||
        box2->kW != 1u)
        return -1;
    if (cls0->K_out != 80u || cls0->C_in != cfg->input_c || cls0->kH != 3u ||
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

static void update_point_outputs(const detect_scale_config_t *cfg,
                                 yolov8n_detect_summary_t *summary,
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
    uint32_t y = point / cfg->input_w;
    uint32_t x = point % cfg->input_w;
    uint32_t left = dfl_project_q8(box_raw, 0u);
    uint32_t top = dfl_project_q8(box_raw, 1u);
    uint32_t right = dfl_project_q8(box_raw, 2u);
    uint32_t bottom = dfl_project_q8(box_raw, 3u);
    int32_t cx_grid_q8 = (int32_t)(x * 256u + 128u);
    int32_t cy_grid_q8 = (int32_t)(y * 256u + 128u);
    int32_t x1 = (cx_grid_q8 - (int32_t)left) * (int32_t)cfg->stride;
    int32_t y1 = (cy_grid_q8 - (int32_t)top) * (int32_t)cfg->stride;
    int32_t x2 = (cx_grid_q8 + (int32_t)right) * (int32_t)cfg->stride;
    int32_t y2 = (cy_grid_q8 + (int32_t)bottom) * (int32_t)cfg->stride;

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

static int build_summary(const detect_scale_config_t *cfg,
                         const int8_t *input,
                         yolov8n_detect_summary_t *summary)
{
    const yolov8n_external_detect_c2f_layer_t *box0 =
        &yolov8n_external_detect_c2f_layers[cfg->box_ids[0]];
    const yolov8n_external_detect_c2f_layer_t *box1 =
        &yolov8n_external_detect_c2f_layers[cfg->box_ids[1]];
    const yolov8n_external_detect_c2f_layer_t *box2 =
        &yolov8n_external_detect_c2f_layers[cfg->box_ids[2]];
    const yolov8n_external_detect_c2f_layer_t *cls0 =
        &yolov8n_external_detect_c2f_layers[cfg->cls_ids[0]];
    const yolov8n_external_detect_c2f_layer_t *cls1 =
        &yolov8n_external_detect_c2f_layers[cfg->cls_ids[1]];
    const yolov8n_external_detect_c2f_layer_t *cls2 =
        &yolov8n_external_detect_c2f_layers[cfg->cls_ids[2]];
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
        YV8N_DETECT_SAMPLE_POINTS * 4u * sizeof(*box_public), 64u);
    cls_public = yolov8n_p5_arena_alloc(
        YV8N_DETECT_SAMPLE_POINTS * YV8N_DETECT_CLASS_CHANNELS *
        sizeof(*cls_public), 64u);
    if (!box_taps || !box1_out || !box_raw || !cls_taps || !cls1_out ||
        !cls_raw || !box_public || !cls_public)
        return -1;

    summary->magic = YV8N_DETECT_SUMMARY_MAGIC;
    summary->version = YV8N_DETECT_SUMMARY_VERSION;
    summary->scale_id = cfg->scale_id;
    summary->input_bytes = cfg->input_bytes;
    summary->input_h = cfg->input_h;
    summary->input_w = cfg->input_w;
    summary->input_c = cfg->input_c;
    summary->stride = cfg->stride;
    summary->raw_channels = YV8N_DETECT_RAW_CHANNELS;
    summary->public_channels = YV8N_DETECT_PUBLIC_CHANNELS;
    summary->sample_points = YV8N_DETECT_SAMPLE_POINTS;
    summary->scratch_peak_bytes = YV8N_DETECT_SCRATCH_PEAK_BYTES;
    summary->input_hash = hash_full_input(input, cfg->input_bytes);
    summary->layer_hash = hash_selected_layers(cfg);
    for (uint32_t i = 0; i < (uint32_t)(sizeof(summary->reserved) /
                                        sizeof(summary->reserved[0])); ++i)
        summary->reserved[i] = 0u;

    for (uint32_t i = 0; i < YV8N_DETECT_SAMPLE_POINTS; ++i) {
        uint32_t point = cfg->sample_points[i];

        fill_first_layer_taps(cfg, box0, input, point, box_taps, 64u, tap_valid);
        conv3x3_taps_at(box1, box_taps, 64u, tap_valid, box1_out);
        conv1x1_vec(box2, box1_out, box_raw);

        fill_first_layer_taps(cfg, cls0, input, point, cls_taps, 80u, tap_valid);
        conv3x3_taps_at(cls1, cls_taps, 80u, tap_valid, cls1_out);
        conv1x1_vec(cls2, cls1_out, cls_raw);

        update_point_outputs(cfg, summary, i, point, box_raw, cls_raw,
                             box_public, cls_public,
                             &raw_hash, &box_hash, &cls_hash);
    }

    for (uint32_t c = 0; c < 4u; ++c) {
        for (uint32_t i = 0; i < YV8N_DETECT_SAMPLE_POINTS; ++i) {
            public_hash = hash_u32_value(
                public_hash, (uint32_t)box_public[(size_t)i * 4u + c]);
        }
    }
    for (uint32_t cls = 0; cls < YV8N_DETECT_CLASS_CHANNELS; ++cls) {
        for (uint32_t i = 0; i < YV8N_DETECT_SAMPLE_POINTS; ++i) {
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

static int validate_summary(const detect_scale_config_t *cfg,
                            const yolov8n_detect_summary_t *summary)
{
    if (summary->magic != YV8N_DETECT_SUMMARY_MAGIC ||
        summary->version != YV8N_DETECT_SUMMARY_VERSION ||
        summary->scale_id != cfg->scale_id ||
        summary->input_bytes != cfg->input_bytes ||
        summary->input_h != cfg->input_h ||
        summary->input_w != cfg->input_w ||
        summary->input_c != cfg->input_c ||
        summary->stride != cfg->stride ||
        summary->raw_channels != YV8N_DETECT_RAW_CHANNELS ||
        summary->public_channels != YV8N_DETECT_PUBLIC_CHANNELS ||
        summary->sample_points != YV8N_DETECT_SAMPLE_POINTS ||
        summary->scratch_peak_bytes != YV8N_DETECT_SCRATCH_PEAK_BYTES)
        return -1;
    if (summary->input_hash != cfg->expect_input_hash ||
        summary->layer_hash != cfg->expect_layer_hash ||
        summary->raw_hash != cfg->expect_raw_hash ||
        summary->box_hash != cfg->expect_box_hash ||
        summary->cls_hash != cfg->expect_cls_hash ||
        summary->public_hash != cfg->expect_public_hash)
        return -1;
    for (uint32_t i = 0; i < YV8N_DETECT_SAMPLE_POINTS; ++i) {
        if (summary->sample_point_ids[i] != cfg->sample_points[i] ||
            summary->best_classes[i] != 0u)
            return -1;
    }
    for (uint32_t i = 0; i < 4u; ++i) {
        if (summary->first_box_q8[i] != cfg->expect_first_box_q8[i])
            return -1;
    }
    for (uint32_t i = 0; i < (uint32_t)(sizeof(summary->reserved) /
                                        sizeof(summary->reserved[0])); ++i) {
        if (summary->reserved[i] != 0u)
            return -1;
    }
    return 0;
}

static void fill_source_edge(const detect_scale_config_t *cfg, int8_t *out)
{
    for (uint32_t y = 0; y < cfg->input_h; ++y) {
        for (uint32_t x = 0; x < cfg->input_w; ++x) {
            for (uint32_t c = 0; c < cfg->input_c; ++c) {
                size_t idx = (((size_t)y * cfg->input_w + x) * cfg->input_c) + c;

                out[idx] = scale_synthetic_value(cfg, y, x, c);
            }
        }
    }
}

static tpa_op_t source_start(uint32_t scale_id)
{
    const detect_scale_config_t *cfg = scale_config(scale_id);
    struct tpa_channel *ch = tpa_chan(0);
    int8_t *out = tpa_send_buf(ch);

    mark(YV8N_DETECT_TRACE_BEGIN | scale_id);
    YV8N_DETECT_REQUIRE_MSG_STOP("YV8DET:CFG\n", cfg != 0);
    YV8N_DETECT_REQUIRE_MSG_STOP("YV8DET:SRCBUF\n", out != 0);
    fill_source_edge(cfg, out);
    mark(YV8N_DETECT_TRACE_SOURCE_SENT | scale_id);
    return tpa_send(ch, out, cfg->input_bytes, yolov8n_detect_source_done);
}

tpa_op_t yolov8n_p3_source_start(void)
{
    return source_start(0u);
}

tpa_op_t yolov8n_p4_source_start(void)
{
    return source_start(1u);
}

tpa_op_t yolov8n_detect_p5_source_start(void)
{
    return source_start(2u);
}

tpa_op_t yolov8n_detect_source_done(void)
{
    return tpa_stop();
}

static tpa_op_t detect_start(uint32_t scale_id)
{
    detect_ws_t *w = tpa_ws();

    w->scale_id = scale_id;
    return tpa_recv(tpa_chan(0), (void **)&w->input, &w->input_len,
                    yolov8n_detect_run);
}

tpa_op_t yolov8n_p3_detect_start(void)
{
    return detect_start(0u);
}

tpa_op_t yolov8n_p4_detect_start(void)
{
    return detect_start(1u);
}

tpa_op_t yolov8n_detect_p5_start(void)
{
    return detect_start(2u);
}

tpa_op_t yolov8n_detect_run(void)
{
    detect_ws_t *w = tpa_ws();
    const detect_scale_config_t *cfg = scale_config(w->scale_id);
    struct tpa_channel *out_ch = tpa_chan(1);

    YV8N_DETECT_REQUIRE_MSG_STOP("YV8DET:CFG\n", cfg != 0);
    YV8N_DETECT_REQUIRE_MSG_STOP("YV8DET:LEN\n", w->input_len == cfg->input_bytes);
    YV8N_DETECT_REQUIRE_MSG_STOP("YV8DET:LAYERS\n", verify_scale_layers(cfg) == 0);
    w->summary = (yolov8n_detect_summary_t *)tpa_send_buf(out_ch);
    YV8N_DETECT_REQUIRE_MSG_STOP("YV8DET:SUMBUF\n", w->summary != 0);
    YV8N_DETECT_REQUIRE_MSG_STOP("YV8DET:RUN\n", build_summary(cfg, w->input, w->summary) == 0);
    mark(YV8N_DETECT_TRACE_SCALE_DONE | w->scale_id);
    return tpa_send(out_ch, w->summary, sizeof(*w->summary),
                    yolov8n_detect_done);
}

tpa_op_t yolov8n_detect_done(void)
{
    return tpa_stop();
}

tpa_op_t yolov8n_detect_checker_start(void)
{
    checker_ws_t *w = tpa_ws();

    return tpa_recv(tpa_chan(0), (void **)&w->summary[0], &w->len[0],
                    yolov8n_detect_checker_recv_p4);
}

tpa_op_t yolov8n_detect_checker_recv_p4(void)
{
    checker_ws_t *w = tpa_ws();

    return tpa_recv(tpa_chan(1), (void **)&w->summary[1], &w->len[1],
                    yolov8n_detect_checker_recv_p5);
}

tpa_op_t yolov8n_detect_checker_recv_p5(void)
{
    checker_ws_t *w = tpa_ws();

    return tpa_recv(tpa_chan(2), (void **)&w->summary[2], &w->len[2],
                    yolov8n_detect_checker_run);
}

tpa_op_t yolov8n_detect_checker_run(void)
{
    checker_ws_t *w = tpa_ws();

    for (uint32_t i = 0; i < 3u; ++i) {
        if (w->len[i] != sizeof(*w->summary[i]) ||
            validate_summary(&scale_configs[i], w->summary[i])) {
            mark(YV8N_DETECT_TRACE_FAIL | i);
            TEST_FAIL;
            return tpa_stop();
        }
    }

    mark(YV8N_DETECT_TRACE_CHECK_PASS);
    TEST_PASS;
    return tpa_stop();
}
