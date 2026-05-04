#ifndef TPA_YOLOV8N_P5_COMMON_H
#define TPA_YOLOV8N_P5_COMMON_H

#include <stddef.h>
#include <stdint.h>

#include "test.h"
#include "tpa/tpa.h"

#define YV8N_P5_H 20u
#define YV8N_P5_W 20u
#define YV8N_P5_C 256u
#define YV8N_P5_POINTS (YV8N_P5_H * YV8N_P5_W)
#define YV8N_P5_INPUT_BYTES (YV8N_P5_POINTS * YV8N_P5_C)
#define YV8N_P5_REG_MAX 16u
#define YV8N_P5_DFL_CHANNELS (4u * YV8N_P5_REG_MAX)
#define YV8N_P5_CLASS_CHANNELS 80u
#define YV8N_P5_RAW_CHANNELS (YV8N_P5_DFL_CHANNELS + YV8N_P5_CLASS_CHANNELS)
#define YV8N_P5_PUBLIC_CHANNELS (4u + YV8N_P5_CLASS_CHANNELS)
#define YV8N_P5_SAMPLE_POINTS 8u
#define YV8N_P5_DETECT_SCRATCH_PEAK_BYTES 4096u

#define YV8N_P5_SUMMARY_MAGIC UINT32_C(0x59385035) /* "Y8P5" */
#define YV8N_P5_SUMMARY_VERSION 1u

#define YV8N_P5_EXPECT_INPUT_HASH UINT64_C(0xd96b5a8240c15925)
#define YV8N_P5_EXPECT_LAYER_HASH UINT64_C(0x7fee3de2b008ded2)
#define YV8N_P5_EXPECT_RAW_HASH UINT64_C(0x3745115a813ef580)
#define YV8N_P5_EXPECT_BOX_HASH UINT64_C(0x20ae08b5c9a56429)
#define YV8N_P5_EXPECT_CLS_HASH UINT64_C(0xccf1a1f80e179625)
#define YV8N_P5_EXPECT_PUBLIC_HASH UINT64_C(0xb4d949f3c3807a75)

#define YV8N_P5_EXTERNAL_HEADER_SHA256 \
    "162b5e7bed60aea49a09febcd5492e3651dbfd0d9165c7a94da415fd1ae86f01"
#define YV8N_P5_EXTERNAL_MANIFEST_SHA256 \
    "0cf54bbce34b9a16ea25675109ecd69a22fde1eb1e68139b6f0ebad4e054ec3e"

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t p5_input_bytes;
    uint32_t sample_points;
    uint32_t raw_channels;
    uint32_t public_channels;
    uint64_t input_hash;
    uint64_t layer_hash;
    uint64_t raw_hash;
    uint64_t box_hash;
    uint64_t cls_hash;
    uint64_t public_hash;
    uint32_t sample_point_ids[YV8N_P5_SAMPLE_POINTS];
    uint16_t best_classes[YV8N_P5_SAMPLE_POINTS];
    int32_t first_box_q8[4];
    uint32_t reserved[6];
} yolov8n_p5_summary_t;

TPA_STATIC_ASSERT(sizeof(yolov8n_p5_summary_t) == 160u,
                  "YOLOv8n P5 summary edge size must match .tpp");

static inline void yolov8n_p5_diag_putc(char c)
{
    arch_diag_putc(c);
}

static inline void yolov8n_p5_diag_puts(const char *s)
{
    while (*s)
        yolov8n_p5_diag_putc(*s++);
}

#define YV8N_P5_FAIL_STOP() do { \
    TEST_FAIL; \
    return tpa_stop(); \
} while (0)

#define YV8N_P5_FAIL_MSG_STOP(msg) do { \
    yolov8n_p5_diag_puts(msg); \
    TEST_FAIL; \
    return tpa_stop(); \
} while (0)

#define YV8N_P5_REQUIRE_MSG_STOP(msg, cond) do { \
    if (!(cond)) \
        YV8N_P5_FAIL_MSG_STOP(msg); \
} while (0)

void yolov8n_p5_arena_begin(uint32_t declared_peak_bytes);
void *yolov8n_p5_arena_alloc(size_t bytes, size_t align);
uint32_t yolov8n_p5_arena_high_water(void);

#endif /* TPA_YOLOV8N_P5_COMMON_H */
