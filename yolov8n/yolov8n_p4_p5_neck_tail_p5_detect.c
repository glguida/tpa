#include "yolov8n_detect_downstream.h"

#include <stdint.h>

#define YV8N_P4_P5_P5_TRACE_CHECK_PASS 0xe8e5d004u
#define YV8N_P4_P5_P5_TRACE_FAIL 0xe8e5dfeeu

#define YV8N_P4_P5_P5_CHECKER_PID 914u

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

#define YV8N_P5_DENSE_C2F_IN_H 20u
#define YV8N_P5_DENSE_C2F_IN_W 20u
#define YV8N_P5_DENSE_C2F_IN_C 384u
#define YV8N_P5_DENSE_C2F_OUT_C 256u
#define YV8N_P5_DENSE_C2F_HIDDEN_C 128u
#define YV8N_P5_DENSE_C2F_POINTS (YV8N_P5_DENSE_C2F_IN_H * YV8N_P5_DENSE_C2F_IN_W)
#define YV8N_P5_DENSE_C2F_INPUT_BYTES (YV8N_P5_DENSE_C2F_POINTS * YV8N_P5_DENSE_C2F_IN_C)
#define YV8N_P5_DENSE_C2F_OUTPUT_BYTES (YV8N_P5_DENSE_C2F_POINTS * YV8N_P5_DENSE_C2F_OUT_C)
#define YV8N_P5_DENSE_C2F_SCRATCH_PEAK_BYTES 262144u
#define YV8N_P5_DENSE_C2F_DETECT_SAMPLE_POINTS 2u
#define YV8N_P5_DENSE_C2F_SUMMARY_MAGIC UINT32_C(0x59384435) /* "Y8D5" */
#define YV8N_P5_DENSE_C2F_SUMMARY_VERSION 1u

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

#define YV8N_P5_NECK_C2F_EXPECT_INPUT_HASH YV8N_P4_P5_CONCAT_EXPECT_OUTPUT_HASH
#define YV8N_P5_NECK_C2F_EXPECT_LAYER_HASH UINT64_C(0x243583d1486cabda)
#define YV8N_P5_NECK_C2F_EXPECT_OUTPUT_HASH UINT64_C(0x56d5f2d010c0394d)
#define YV8N_P5_NECK_C2F_EXPECT_SAMPLED_HASH UINT64_C(0x56d5f2d010c0394d)
#define YV8N_P5_NECK_C2F_EXPECT_POINT_HASH UINT64_C(0x3b8ec8550d64d7b5)
#define YV8N_P5_NECK_C2F_EXPECT_COMPUTED_POINTS 400u
#define YV8N_P5_NECK_C2F_EXPECT_FIRST0 -4
#define YV8N_P5_NECK_C2F_EXPECT_FIRST1 2
#define YV8N_P5_NECK_C2F_EXPECT_FIRST2 98
#define YV8N_P5_NECK_C2F_EXPECT_FIRST3 -7

#define YV8N_P5_NECK_DETECT_EXPECT_LAYER_HASH UINT64_C(0x7fee3de2b008ded2)
#define YV8N_P5_NECK_DETECT_EXPECT_RAW_HASH UINT64_C(0x9756de2dee2ccc87)
#define YV8N_P5_NECK_DETECT_EXPECT_BOX_HASH UINT64_C(0xe2ed5966d222cc3d)
#define YV8N_P5_NECK_DETECT_EXPECT_CLS_HASH UINT64_C(0xd5548314c55c5e65)
#define YV8N_P5_NECK_DETECT_EXPECT_PUBLIC_HASH UINT64_C(0x219fa8f9a19848b5)
#define YV8N_P5_NECK_DETECT_EXPECT_BEST0 0u
#define YV8N_P5_NECK_DETECT_EXPECT_BEST1 0u
#define YV8N_P5_NECK_DETECT_EXPECT_FIRST0 3472
#define YV8N_P5_NECK_DETECT_EXPECT_FIRST1 16912
#define YV8N_P5_NECK_DETECT_EXPECT_FIRST2 80032
#define YV8N_P5_NECK_DETECT_EXPECT_FIRST3 98976

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
    const yolov8n_p4_dense_c2f_summary_t *p4_summary;
    const yolov8n_p4_p5_model19_summary_t *model19_summary;
    const yolov8n_p4_p5_concat_summary_t *concat_summary;
    const yolov8n_p5_dense_c2f_summary_t *p5_summary;
    const yolov8n_detect_summary_t *detect_summary;
    uint32_t p4_len;
    uint32_t model19_len;
    uint32_t concat_len;
    uint32_t p5_len;
    uint32_t detect_len;
} neck_p5_checker_ws_t;

TPA_STATIC_ASSERT(sizeof(neck_p5_checker_ws_t) <= 128u,
                  "YOLOv8n P4-to-P5-to-P5 checker workspace exceeds .tpm declaration");

TPA_PROC_MEM_META(yolov8n_p4_p5_neck_tail_p5_detect_checker_meta,
                  YV8N_P4_P5_P5_CHECKER_PID, 0u);

tpa_op_t yolov8n_p4_p5_neck_tail_p5_detect_checker_start(void);
tpa_op_t yolov8n_p4_p5_neck_tail_p5_detect_checker_recv_model19(void);
tpa_op_t yolov8n_p4_p5_neck_tail_p5_detect_checker_recv_concat(void);
tpa_op_t yolov8n_p4_p5_neck_tail_p5_detect_checker_recv_p5(void);
tpa_op_t yolov8n_p4_p5_neck_tail_p5_detect_checker_recv_detect(void);
tpa_op_t yolov8n_p4_p5_neck_tail_p5_detect_checker_run(void);

static const uint32_t p4_sample_points[YV8N_P4_DENSE_C2F_DETECT_SAMPLE_POINTS] = {
    0u, 1u, 39u, 40u, 333u, 800u, 1511u, 1599u,
};

static const uint32_t p5_sample_points[YV8N_P5_DENSE_C2F_DETECT_SAMPLE_POINTS] = {
    0u, 21u,
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

static int validate_p5_summary(const yolov8n_p5_dense_c2f_summary_t *summary)
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
        summary->sampled_output_points != YV8N_P5_NECK_C2F_EXPECT_COMPUTED_POINTS ||
        summary->computed_point_count != YV8N_P5_NECK_C2F_EXPECT_COMPUTED_POINTS)
        return -1;
    if (summary->input_hash != YV8N_P5_NECK_C2F_EXPECT_INPUT_HASH ||
        summary->c2f_layer_hash != YV8N_P5_NECK_C2F_EXPECT_LAYER_HASH ||
        summary->output_hash != YV8N_P5_NECK_C2F_EXPECT_OUTPUT_HASH ||
        summary->sampled_output_hash != YV8N_P5_NECK_C2F_EXPECT_SAMPLED_HASH ||
        summary->computed_point_hash != YV8N_P5_NECK_C2F_EXPECT_POINT_HASH)
        return -1;
    for (uint32_t i = 0; i < YV8N_P5_DENSE_C2F_DETECT_SAMPLE_POINTS; ++i) {
        if (summary->detect_sample_point_ids[i] != p5_sample_points[i])
            return -1;
    }
    if (summary->first_output_q8[0] != YV8N_P5_NECK_C2F_EXPECT_FIRST0 ||
        summary->first_output_q8[1] != YV8N_P5_NECK_C2F_EXPECT_FIRST1 ||
        summary->first_output_q8[2] != YV8N_P5_NECK_C2F_EXPECT_FIRST2 ||
        summary->first_output_q8[3] != YV8N_P5_NECK_C2F_EXPECT_FIRST3)
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
    if (summary->input_hash != YV8N_P5_NECK_C2F_EXPECT_OUTPUT_HASH ||
        summary->layer_hash != YV8N_P5_NECK_DETECT_EXPECT_LAYER_HASH ||
        summary->raw_hash != YV8N_P5_NECK_DETECT_EXPECT_RAW_HASH ||
        summary->box_hash != YV8N_P5_NECK_DETECT_EXPECT_BOX_HASH ||
        summary->cls_hash != YV8N_P5_NECK_DETECT_EXPECT_CLS_HASH ||
        summary->public_hash != YV8N_P5_NECK_DETECT_EXPECT_PUBLIC_HASH)
        return -1;
    if (summary->sample_point_ids[0] != p5_sample_points[0] ||
        summary->sample_point_ids[1] != p5_sample_points[1] ||
        summary->best_classes[0] != YV8N_P5_NECK_DETECT_EXPECT_BEST0 ||
        summary->best_classes[1] != YV8N_P5_NECK_DETECT_EXPECT_BEST1)
        return -1;
    if (summary->first_box_q8[0] != YV8N_P5_NECK_DETECT_EXPECT_FIRST0 ||
        summary->first_box_q8[1] != YV8N_P5_NECK_DETECT_EXPECT_FIRST1 ||
        summary->first_box_q8[2] != YV8N_P5_NECK_DETECT_EXPECT_FIRST2 ||
        summary->first_box_q8[3] != YV8N_P5_NECK_DETECT_EXPECT_FIRST3)
        return -1;
    for (uint32_t i = 0; i < (uint32_t)(sizeof(summary->reserved) /
                                        sizeof(summary->reserved[0])); ++i) {
        if (summary->reserved[i] != 0u)
            return -1;
    }
    return 0;
}

tpa_op_t yolov8n_p4_p5_neck_tail_p5_detect_checker_start(void)
{
    neck_p5_checker_ws_t *w = tpa_ws();

    return tpa_recv(tpa_chan(0), (void **)&w->p4_summary, &w->p4_len,
                    yolov8n_p4_p5_neck_tail_p5_detect_checker_recv_model19);
}

tpa_op_t yolov8n_p4_p5_neck_tail_p5_detect_checker_recv_model19(void)
{
    neck_p5_checker_ws_t *w = tpa_ws();

    return tpa_recv(tpa_chan(1), (void **)&w->model19_summary, &w->model19_len,
                    yolov8n_p4_p5_neck_tail_p5_detect_checker_recv_concat);
}

tpa_op_t yolov8n_p4_p5_neck_tail_p5_detect_checker_recv_concat(void)
{
    neck_p5_checker_ws_t *w = tpa_ws();

    return tpa_recv(tpa_chan(2), (void **)&w->concat_summary, &w->concat_len,
                    yolov8n_p4_p5_neck_tail_p5_detect_checker_recv_p5);
}

tpa_op_t yolov8n_p4_p5_neck_tail_p5_detect_checker_recv_p5(void)
{
    neck_p5_checker_ws_t *w = tpa_ws();

    return tpa_recv(tpa_chan(3), (void **)&w->p5_summary, &w->p5_len,
                    yolov8n_p4_p5_neck_tail_p5_detect_checker_recv_detect);
}

tpa_op_t yolov8n_p4_p5_neck_tail_p5_detect_checker_recv_detect(void)
{
    neck_p5_checker_ws_t *w = tpa_ws();

    return tpa_recv(tpa_chan(4), (void **)&w->detect_summary, &w->detect_len,
                    yolov8n_p4_p5_neck_tail_p5_detect_checker_run);
}

tpa_op_t yolov8n_p4_p5_neck_tail_p5_detect_checker_run(void)
{
    neck_p5_checker_ws_t *w = tpa_ws();
    int failed = 0;

    if (w->p4_len != sizeof(*w->p4_summary)) {
        yolov8n_detect_diag_puts("Y8P4P5P5:P4LEN\n");
        failed = 1;
    } else if (validate_p4_summary(w->p4_summary)) {
        yolov8n_detect_diag_puts("Y8P4P5P5:P4SUM\n");
        diag_hex64("P4IN=", w->p4_summary->input_hash);
        diag_hex64("P4OUT=", w->p4_summary->output_hash);
        diag_hex64("P4POINT=", w->p4_summary->computed_point_hash);
        failed = 1;
    }
    if (w->model19_len != sizeof(*w->model19_summary)) {
        yolov8n_detect_diag_puts("Y8P4P5P5:M19LEN\n");
        failed = 1;
    } else if (validate_model19_summary(w->model19_summary)) {
        yolov8n_detect_diag_puts("Y8P4P5P5:M19SUM\n");
        diag_hex64("M19IN=", w->model19_summary->input_hash);
        diag_hex64("M19LAYER=", w->model19_summary->layer_hash);
        diag_hex64("M19OUT=", w->model19_summary->output_hash);
        diag_hex64("M19POINT=", w->model19_summary->point_hash);
        failed = 1;
    }
    if (w->concat_len != sizeof(*w->concat_summary)) {
        yolov8n_detect_diag_puts("Y8P4P5P5:CTSUMLEN\n");
        failed = 1;
    } else if (validate_concat_summary(w->concat_summary)) {
        yolov8n_detect_diag_puts("Y8P4P5P5:CTSUM\n");
        diag_hex64("CTM19=", w->concat_summary->model19_hash);
        diag_hex64("CTM9=", w->concat_summary->model9_hash);
        diag_hex64("CTOUT=", w->concat_summary->output_hash);
        diag_hex64("CTLAY=", w->concat_summary->layout_hash);
        failed = 1;
    }
    if (w->p5_len != sizeof(*w->p5_summary)) {
        yolov8n_detect_diag_puts("Y8P4P5P5:P5LEN\n");
        failed = 1;
    } else {
        if (w->concat_len == sizeof(*w->concat_summary) &&
            w->p5_summary->input_hash != w->concat_summary->output_hash) {
            yolov8n_detect_diag_puts("Y8P4P5P5:P5CTHASH\n");
            diag_hex64("P5IN=", w->p5_summary->input_hash);
            diag_hex64("CTOUT=", w->concat_summary->output_hash);
            failed = 1;
        }
        if (validate_p5_summary(w->p5_summary)) {
            yolov8n_detect_diag_puts("Y8P4P5P5:P5SUM\n");
            diag_hex64("P5IN=", w->p5_summary->input_hash);
            diag_hex64("P5LAYER=", w->p5_summary->c2f_layer_hash);
            diag_hex64("P5OUT=", w->p5_summary->output_hash);
            diag_hex64("P5SAMP=", w->p5_summary->sampled_output_hash);
            diag_hex64("P5POINT=", w->p5_summary->computed_point_hash);
            diag_hex64("P5COUNT=", w->p5_summary->computed_point_count);
            diag_hex64("P5F0=", (uint32_t)w->p5_summary->first_output_q8[0]);
            diag_hex64("P5F1=", (uint32_t)w->p5_summary->first_output_q8[1]);
            diag_hex64("P5F2=", (uint32_t)w->p5_summary->first_output_q8[2]);
            diag_hex64("P5F3=", (uint32_t)w->p5_summary->first_output_q8[3]);
            failed = 1;
        }
    }
    if (w->detect_len != sizeof(*w->detect_summary)) {
        yolov8n_detect_diag_puts("Y8P4P5P5:DETLEN\n");
        failed = 1;
    } else {
        if (w->p5_len == sizeof(*w->p5_summary) &&
            w->detect_summary->input_hash != w->p5_summary->output_hash) {
            yolov8n_detect_diag_puts("Y8P4P5P5:DETP5HASH\n");
            diag_hex64("DIN=", w->detect_summary->input_hash);
            diag_hex64("P5OUT=", w->p5_summary->output_hash);
            failed = 1;
        }
        if (validate_detect_summary(w->detect_summary)) {
            yolov8n_detect_diag_puts("Y8P4P5P5:DETSUM\n");
            diag_hex64("DIN=", w->detect_summary->input_hash);
            diag_hex64("DLAYER=", w->detect_summary->layer_hash);
            diag_hex64("DRAW=", w->detect_summary->raw_hash);
            diag_hex64("DBOX=", w->detect_summary->box_hash);
            diag_hex64("DCLS=", w->detect_summary->cls_hash);
            diag_hex64("DPUB=", w->detect_summary->public_hash);
            diag_hex64("DBEST0=", w->detect_summary->best_classes[0]);
            diag_hex64("DBEST1=", w->detect_summary->best_classes[1]);
            diag_hex64("DF0=", (uint32_t)w->detect_summary->first_box_q8[0]);
            diag_hex64("DF1=", (uint32_t)w->detect_summary->first_box_q8[1]);
            diag_hex64("DF2=", (uint32_t)w->detect_summary->first_box_q8[2]);
            diag_hex64("DF3=", (uint32_t)w->detect_summary->first_box_q8[3]);
            failed = 1;
        }
    }

    if (failed) {
        mark(YV8N_P4_P5_P5_TRACE_FAIL);
        TEST_FAIL;
        return tpa_stop();
    }

    mark(YV8N_P4_P5_P5_TRACE_CHECK_PASS);
    TEST_PASS;
    return tpa_stop();
}
