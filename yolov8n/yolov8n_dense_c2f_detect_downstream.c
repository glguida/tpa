#include "yolov8n_detect_downstream.h"

#include <stdint.h>

#define YV8N_DENSE_C2F_DETECT_DOWNSTREAM_CHECKER_PID 900u

#define YV8N_DENSE_C2F_DETECT_TRACE_BEGIN 0xe8df0000u
#define YV8N_DENSE_C2F_DETECT_TRACE_CHECK_PASS 0xe8df0001u
#define YV8N_DENSE_C2F_DETECT_TRACE_FAIL 0xe8dffeeu

#define YV8N_DENSE_C2F_SUMMARY_VERSION 1u
#define YV8N_P3_DENSE_C2F_SUMMARY_MAGIC UINT32_C(0x59384433) /* "Y8D3" */
#define YV8N_P4_DENSE_C2F_SUMMARY_MAGIC UINT32_C(0x59384434) /* "Y8D4" */
#define YV8N_P5_DENSE_C2F_SUMMARY_MAGIC UINT32_C(0x59384435) /* "Y8D5" */

#define YV8N_P3_DENSE_C2F_INPUT_BYTES 1228800u
#define YV8N_P3_DENSE_C2F_OUTPUT_BYTES 409600u
#define YV8N_P4_DENSE_C2F_INPUT_BYTES 307200u
#define YV8N_P4_DENSE_C2F_OUTPUT_BYTES 204800u
#define YV8N_P5_DENSE_C2F_INPUT_BYTES 153600u
#define YV8N_P5_DENSE_C2F_OUTPUT_BYTES 102400u

#define YV8N_P3_DENSE_C2F_EXPECT_INPUT_HASH UINT64_C(0xfac4e5eb4ba224a5)
#define YV8N_P3_DENSE_C2F_EXPECT_LAYER_HASH UINT64_C(0x472b1f7ef552c2b1)
#define YV8N_P3_DENSE_C2F_EXPECT_OUTPUT_HASH UINT64_C(0x8ac0655bd69e863b)
#define YV8N_P3_DENSE_C2F_EXPECT_POINT_HASH UINT64_C(0xa55302d5f2796125)
#define YV8N_P3_DENSE_C2F_DETECT_EXPECT_LAYER_HASH UINT64_C(0x9d19e64e48a8442e)
#define YV8N_P3_DENSE_C2F_DETECT_EXPECT_RAW_HASH UINT64_C(0xc003e44e6fc47fe7)
#define YV8N_P3_DENSE_C2F_DETECT_EXPECT_BOX_HASH UINT64_C(0x1fb4069b738b06ca)
#define YV8N_P3_DENSE_C2F_DETECT_EXPECT_CLS_HASH UINT64_C(0xccf1a1f80e179625)
#define YV8N_P3_DENSE_C2F_DETECT_EXPECT_PUBLIC_HASH UINT64_C(0xcd199c9a387e5642)

#define YV8N_P4_DENSE_C2F_EXPECT_INPUT_HASH UINT64_C(0x6da53b6d0122b9a5)
#define YV8N_P4_DENSE_C2F_EXPECT_LAYER_HASH UINT64_C(0xc759e3e9b51565bc)
#define YV8N_P4_DENSE_C2F_EXPECT_OUTPUT_HASH UINT64_C(0x689e9690e25a7f2e)
#define YV8N_P4_DENSE_C2F_EXPECT_POINT_HASH UINT64_C(0x5a921301de678525)
#define YV8N_P4_DENSE_C2F_DETECT_EXPECT_LAYER_HASH UINT64_C(0x010eff3fa57a1f07)
#define YV8N_P4_DENSE_C2F_DETECT_EXPECT_RAW_HASH UINT64_C(0xffc9fb026caa3609)
#define YV8N_P4_DENSE_C2F_DETECT_EXPECT_BOX_HASH UINT64_C(0x2e4a1c023d49c224)
#define YV8N_P4_DENSE_C2F_DETECT_EXPECT_CLS_HASH UINT64_C(0xccf1a1f80e179625)
#define YV8N_P4_DENSE_C2F_DETECT_EXPECT_PUBLIC_HASH UINT64_C(0xfbc8b162a429f46c)

#define YV8N_P5_DENSE_C2F_EXPECT_INPUT_HASH UINT64_C(0xa32679ba0aa02025)
#define YV8N_P5_DENSE_C2F_EXPECT_LAYER_HASH UINT64_C(0x243583d1486cabda)
#define YV8N_P5_DENSE_C2F_EXPECT_OUTPUT_HASH UINT64_C(0x69c8c5f4f9827a30)
#define YV8N_P5_DENSE_C2F_EXPECT_POINT_HASH UINT64_C(0x3b8ec8550d64d7b5)
#define YV8N_P5_DENSE_C2F_DETECT_EXPECT_LAYER_HASH UINT64_C(0x7fee3de2b008ded2)
#define YV8N_P5_DENSE_C2F_DETECT_EXPECT_RAW_HASH UINT64_C(0xb9df62ac0bb5acb9)
#define YV8N_P5_DENSE_C2F_DETECT_EXPECT_BOX_HASH UINT64_C(0xffbc8daced6cb5a4)
#define YV8N_P5_DENSE_C2F_DETECT_EXPECT_CLS_HASH UINT64_C(0xd5548314c55c5e65)
#define YV8N_P5_DENSE_C2F_DETECT_EXPECT_PUBLIC_HASH UINT64_C(0xa861189329b4ad40)

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
} yolov8n_dense_c2f_summary_t;

TPA_STATIC_ASSERT(sizeof(yolov8n_dense_c2f_summary_t) == 160u,
                  "YOLOv8n dense C2f summary edge size must match .tpp");

typedef struct {
    const char *label;
    uint32_t scale_id;
    uint32_t stride;
    uint32_t c2f_magic;
    uint32_t input_bytes;
    uint32_t output_bytes;
    uint32_t h;
    uint32_t w;
    uint32_t input_c;
    uint32_t output_c;
    uint32_t hidden_c;
    uint32_t sample_count;
    uint32_t c2f_scratch_peak_bytes;
    uint32_t computed_points;
    uint64_t c2f_input_hash;
    uint64_t c2f_layer_hash;
    uint64_t c2f_output_hash;
    uint64_t c2f_point_hash;
    int32_t first_output_q8[4];
    uint64_t detect_layer_hash;
    uint64_t detect_raw_hash;
    uint64_t detect_box_hash;
    uint64_t detect_cls_hash;
    uint64_t detect_public_hash;
    int32_t first_box_q8[4];
    uint32_t sample_points[YV8N_DETECT_SAMPLE_POINTS];
} yv8n_dense_scale_expect_t;

typedef struct {
    const yolov8n_dense_c2f_summary_t *c2f[3];
    const yolov8n_detect_summary_t *detect[3];
    uint32_t c2f_len[3];
    uint32_t detect_len[3];
} yv8n_dense_combined_checker_ws_t;

static const yv8n_dense_scale_expect_t scale_expect[3] = {
    {
        .label = "P3",
        .scale_id = 0u,
        .stride = 8u,
        .c2f_magic = YV8N_P3_DENSE_C2F_SUMMARY_MAGIC,
        .input_bytes = YV8N_P3_DENSE_C2F_INPUT_BYTES,
        .output_bytes = YV8N_P3_DENSE_C2F_OUTPUT_BYTES,
        .h = 80u,
        .w = 80u,
        .input_c = 192u,
        .output_c = 64u,
        .hidden_c = 32u,
        .sample_count = 8u,
        .c2f_scratch_peak_bytes = 1048576u,
        .computed_points = 6400u,
        .c2f_input_hash = YV8N_P3_DENSE_C2F_EXPECT_INPUT_HASH,
        .c2f_layer_hash = YV8N_P3_DENSE_C2F_EXPECT_LAYER_HASH,
        .c2f_output_hash = YV8N_P3_DENSE_C2F_EXPECT_OUTPUT_HASH,
        .c2f_point_hash = YV8N_P3_DENSE_C2F_EXPECT_POINT_HASH,
        .first_output_q8 = {88, 121, 121, -9},
        .detect_layer_hash = YV8N_P3_DENSE_C2F_DETECT_EXPECT_LAYER_HASH,
        .detect_raw_hash = YV8N_P3_DENSE_C2F_DETECT_EXPECT_RAW_HASH,
        .detect_box_hash = YV8N_P3_DENSE_C2F_DETECT_EXPECT_BOX_HASH,
        .detect_cls_hash = YV8N_P3_DENSE_C2F_DETECT_EXPECT_CLS_HASH,
        .detect_public_hash = YV8N_P3_DENSE_C2F_DETECT_EXPECT_PUBLIC_HASH,
        .first_box_q8 = {-416, 700, 12768, 14472},
        .sample_points = {0u, 1u, 79u, 80u, 1234u, 3200u, 6321u, 6399u},
    },
    {
        .label = "P4",
        .scale_id = 1u,
        .stride = 16u,
        .c2f_magic = YV8N_P4_DENSE_C2F_SUMMARY_MAGIC,
        .input_bytes = YV8N_P4_DENSE_C2F_INPUT_BYTES,
        .output_bytes = YV8N_P4_DENSE_C2F_OUTPUT_BYTES,
        .h = 40u,
        .w = 40u,
        .input_c = 192u,
        .output_c = 128u,
        .hidden_c = 64u,
        .sample_count = 8u,
        .c2f_scratch_peak_bytes = 524288u,
        .computed_points = 1600u,
        .c2f_input_hash = YV8N_P4_DENSE_C2F_EXPECT_INPUT_HASH,
        .c2f_layer_hash = YV8N_P4_DENSE_C2F_EXPECT_LAYER_HASH,
        .c2f_output_hash = YV8N_P4_DENSE_C2F_EXPECT_OUTPUT_HASH,
        .c2f_point_hash = YV8N_P4_DENSE_C2F_EXPECT_POINT_HASH,
        .first_output_q8 = {-11, -8, -10, -11},
        .detect_layer_hash = YV8N_P4_DENSE_C2F_DETECT_EXPECT_LAYER_HASH,
        .detect_raw_hash = YV8N_P4_DENSE_C2F_DETECT_EXPECT_RAW_HASH,
        .detect_box_hash = YV8N_P4_DENSE_C2F_DETECT_EXPECT_BOX_HASH,
        .detect_cls_hash = YV8N_P4_DENSE_C2F_DETECT_EXPECT_CLS_HASH,
        .detect_public_hash = YV8N_P4_DENSE_C2F_DETECT_EXPECT_PUBLIC_HASH,
        .first_box_q8 = {1992, 3264, 30992, 37088},
        .sample_points = {0u, 1u, 39u, 40u, 333u, 800u, 1511u, 1599u},
    },
    {
        .label = "P5",
        .scale_id = 2u,
        .stride = 32u,
        .c2f_magic = YV8N_P5_DENSE_C2F_SUMMARY_MAGIC,
        .input_bytes = YV8N_P5_DENSE_C2F_INPUT_BYTES,
        .output_bytes = YV8N_P5_DENSE_C2F_OUTPUT_BYTES,
        .h = 20u,
        .w = 20u,
        .input_c = 384u,
        .output_c = 256u,
        .hidden_c = 128u,
        .sample_count = 2u,
        .c2f_scratch_peak_bytes = 262144u,
        .computed_points = 400u,
        .c2f_input_hash = YV8N_P5_DENSE_C2F_EXPECT_INPUT_HASH,
        .c2f_layer_hash = YV8N_P5_DENSE_C2F_EXPECT_LAYER_HASH,
        .c2f_output_hash = YV8N_P5_DENSE_C2F_EXPECT_OUTPUT_HASH,
        .c2f_point_hash = YV8N_P5_DENSE_C2F_EXPECT_POINT_HASH,
        .first_output_q8 = {-4, -6, 68, -4},
        .detect_layer_hash = YV8N_P5_DENSE_C2F_DETECT_EXPECT_LAYER_HASH,
        .detect_raw_hash = YV8N_P5_DENSE_C2F_DETECT_EXPECT_RAW_HASH,
        .detect_box_hash = YV8N_P5_DENSE_C2F_DETECT_EXPECT_BOX_HASH,
        .detect_cls_hash = YV8N_P5_DENSE_C2F_DETECT_EXPECT_CLS_HASH,
        .detect_public_hash = YV8N_P5_DENSE_C2F_DETECT_EXPECT_PUBLIC_HASH,
        .first_box_q8 = {1056, 11952, 69184, 88544},
        .sample_points = {0u, 21u},
    },
};

TPA_STATIC_ASSERT(sizeof(yv8n_dense_combined_checker_ws_t) <= 96u,
                  "YOLOv8n dense combined checker workspace exceeds .tpm declaration");

TPA_PROC_MEM_META(yolov8n_dense_c2f_detect_downstream_checker_meta,
                  YV8N_DENSE_C2F_DETECT_DOWNSTREAM_CHECKER_PID, 0u);

tpa_op_t yolov8n_dense_c2f_detect_downstream_checker_start(void);
tpa_op_t yolov8n_dense_c2f_detect_downstream_checker_recv_p3_detect(void);
tpa_op_t yolov8n_dense_c2f_detect_downstream_checker_recv_p4_c2f(void);
tpa_op_t yolov8n_dense_c2f_detect_downstream_checker_recv_p4_detect(void);
tpa_op_t yolov8n_dense_c2f_detect_downstream_checker_recv_p5_c2f(void);
tpa_op_t yolov8n_dense_c2f_detect_downstream_checker_recv_p5_detect(void);
tpa_op_t yolov8n_dense_c2f_detect_downstream_checker_run(void);

static void mark(uint32_t v)
{
    arch_trace(v);
}

static void diag_scale(const yv8n_dense_scale_expect_t *expect, const char *msg)
{
    yolov8n_detect_diag_puts("Y8DENSE:");
    yolov8n_detect_diag_puts(expect->label);
    yolov8n_detect_diag_putc(':');
    yolov8n_detect_diag_puts(msg);
    yolov8n_detect_diag_putc('\n');
}

static int validate_c2f_summary(const yolov8n_dense_c2f_summary_t *summary,
                                const yv8n_dense_scale_expect_t *expect)
{
    if (summary->magic != expect->c2f_magic ||
        summary->version != YV8N_DENSE_C2F_SUMMARY_VERSION ||
        summary->input_bytes != expect->input_bytes ||
        summary->output_bytes != expect->output_bytes ||
        summary->input_h != expect->h ||
        summary->input_w != expect->w ||
        summary->input_c != expect->input_c ||
        summary->output_c != expect->output_c ||
        summary->hidden_c != expect->hidden_c ||
        summary->detect_sample_points != expect->sample_count ||
        summary->scratch_peak_bytes != expect->c2f_scratch_peak_bytes ||
        summary->sampled_output_points != expect->computed_points ||
        summary->computed_point_count != expect->computed_points)
        return -1;

    if (summary->input_hash != expect->c2f_input_hash ||
        summary->c2f_layer_hash != expect->c2f_layer_hash ||
        summary->output_hash != expect->c2f_output_hash ||
        summary->sampled_output_hash != expect->c2f_output_hash ||
        summary->computed_point_hash != expect->c2f_point_hash)
        return -1;

    for (uint32_t i = 0; i < expect->sample_count; ++i) {
        if (summary->detect_sample_point_ids[i] != expect->sample_points[i])
            return -1;
    }
    for (uint32_t i = 0; i < 4u; ++i) {
        if (summary->first_output_q8[i] != expect->first_output_q8[i])
            return -1;
    }
    for (uint32_t i = 0; i < (uint32_t)(sizeof(summary->reserved) /
                                        sizeof(summary->reserved[0])); ++i) {
        if (summary->reserved[i] != 0u)
            return -1;
    }

    return 0;
}

static int validate_detect_summary(const yolov8n_detect_summary_t *summary,
                                   const yv8n_dense_scale_expect_t *expect)
{
    if (summary->magic != YV8N_DETECT_SUMMARY_MAGIC ||
        summary->version != YV8N_DETECT_SUMMARY_VERSION ||
        summary->scale_id != expect->scale_id ||
        summary->input_bytes != expect->output_bytes ||
        summary->input_h != expect->h ||
        summary->input_w != expect->w ||
        summary->input_c != expect->output_c ||
        summary->stride != expect->stride ||
        summary->raw_channels != YV8N_DETECT_RAW_CHANNELS ||
        summary->public_channels != YV8N_DETECT_PUBLIC_CHANNELS ||
        summary->sample_points != expect->sample_count ||
        summary->scratch_peak_bytes != YV8N_DETECT_SCRATCH_PEAK_BYTES)
        return -1;

    if (summary->input_hash != expect->c2f_output_hash ||
        summary->layer_hash != expect->detect_layer_hash ||
        summary->raw_hash != expect->detect_raw_hash ||
        summary->box_hash != expect->detect_box_hash ||
        summary->cls_hash != expect->detect_cls_hash ||
        summary->public_hash != expect->detect_public_hash)
        return -1;

    for (uint32_t i = 0; i < expect->sample_count; ++i) {
        if (summary->sample_point_ids[i] != expect->sample_points[i] ||
            summary->best_classes[i] != 0u)
            return -1;
    }
    for (uint32_t i = 0; i < 4u; ++i) {
        if (summary->first_box_q8[i] != expect->first_box_q8[i])
            return -1;
    }
    for (uint32_t i = 0; i < (uint32_t)(sizeof(summary->reserved) /
                                        sizeof(summary->reserved[0])); ++i) {
        if (summary->reserved[i] != 0u)
            return -1;
    }

    return 0;
}

static tpa_op_t fail_scale(const yv8n_dense_scale_expect_t *expect, const char *msg)
{
    diag_scale(expect, msg);
    mark(YV8N_DENSE_C2F_DETECT_TRACE_FAIL);
    TEST_FAIL;
    return tpa_stop();
}

tpa_op_t yolov8n_dense_c2f_detect_downstream_checker_start(void)
{
    yv8n_dense_combined_checker_ws_t *w = tpa_ws();

    mark(YV8N_DENSE_C2F_DETECT_TRACE_BEGIN);
    return tpa_recv(tpa_chan(0), (void **)&w->c2f[0], &w->c2f_len[0],
                    yolov8n_dense_c2f_detect_downstream_checker_recv_p3_detect);
}

tpa_op_t yolov8n_dense_c2f_detect_downstream_checker_recv_p3_detect(void)
{
    yv8n_dense_combined_checker_ws_t *w = tpa_ws();

    return tpa_recv(tpa_chan(1), (void **)&w->detect[0], &w->detect_len[0],
                    yolov8n_dense_c2f_detect_downstream_checker_recv_p4_c2f);
}

tpa_op_t yolov8n_dense_c2f_detect_downstream_checker_recv_p4_c2f(void)
{
    yv8n_dense_combined_checker_ws_t *w = tpa_ws();

    return tpa_recv(tpa_chan(2), (void **)&w->c2f[1], &w->c2f_len[1],
                    yolov8n_dense_c2f_detect_downstream_checker_recv_p4_detect);
}

tpa_op_t yolov8n_dense_c2f_detect_downstream_checker_recv_p4_detect(void)
{
    yv8n_dense_combined_checker_ws_t *w = tpa_ws();

    return tpa_recv(tpa_chan(3), (void **)&w->detect[1], &w->detect_len[1],
                    yolov8n_dense_c2f_detect_downstream_checker_recv_p5_c2f);
}

tpa_op_t yolov8n_dense_c2f_detect_downstream_checker_recv_p5_c2f(void)
{
    yv8n_dense_combined_checker_ws_t *w = tpa_ws();

    return tpa_recv(tpa_chan(4), (void **)&w->c2f[2], &w->c2f_len[2],
                    yolov8n_dense_c2f_detect_downstream_checker_recv_p5_detect);
}

tpa_op_t yolov8n_dense_c2f_detect_downstream_checker_recv_p5_detect(void)
{
    yv8n_dense_combined_checker_ws_t *w = tpa_ws();

    return tpa_recv(tpa_chan(5), (void **)&w->detect[2], &w->detect_len[2],
                    yolov8n_dense_c2f_detect_downstream_checker_run);
}

tpa_op_t yolov8n_dense_c2f_detect_downstream_checker_run(void)
{
    yv8n_dense_combined_checker_ws_t *w = tpa_ws();

    for (uint32_t scale = 0; scale < 3u; ++scale) {
        const yv8n_dense_scale_expect_t *expect = &scale_expect[scale];

        if (w->c2f_len[scale] != sizeof(*w->c2f[scale]))
            return fail_scale(expect, "C2FLEN");
        if (w->detect_len[scale] != sizeof(*w->detect[scale]))
            return fail_scale(expect, "DETLEN");
        if (validate_c2f_summary(w->c2f[scale], expect))
            return fail_scale(expect, "C2FSUM");
        if (validate_detect_summary(w->detect[scale], expect))
            return fail_scale(expect, "DETSUM");
    }

    mark(YV8N_DENSE_C2F_DETECT_TRACE_CHECK_PASS);
    TEST_PASS;
    return tpa_stop();
}
