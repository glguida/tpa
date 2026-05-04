#ifndef TPA_YOLOV8N_DETECT_DOWNSTREAM_H
#define TPA_YOLOV8N_DETECT_DOWNSTREAM_H

#include <stddef.h>
#include <stdint.h>

#include "test.h"
#include "tpa/tpa.h"

#define YV8N_DETECT_REG_MAX 16u
#define YV8N_DETECT_DFL_CHANNELS (4u * YV8N_DETECT_REG_MAX)
#define YV8N_DETECT_CLASS_CHANNELS 80u
#define YV8N_DETECT_RAW_CHANNELS \
    (YV8N_DETECT_DFL_CHANNELS + YV8N_DETECT_CLASS_CHANNELS)
#define YV8N_DETECT_PUBLIC_CHANNELS (4u + YV8N_DETECT_CLASS_CHANNELS)
#define YV8N_DETECT_SAMPLE_POINTS 8u
#define YV8N_DETECT_SCRATCH_PEAK_BYTES 4096u
#define YV8N_DETECT_SUMMARY_MAGIC UINT32_C(0x59384454) /* "Y8DT" */
#define YV8N_DETECT_SUMMARY_VERSION 1u

#define YV8N_DETECT_EXTERNAL_HEADER_SHA256 \
    "162b5e7bed60aea49a09febcd5492e3651dbfd0d9165c7a94da415fd1ae86f01"
#define YV8N_DETECT_EXTERNAL_MANIFEST_SHA256 \
    "0cf54bbce34b9a16ea25675109ecd69a22fde1eb1e68139b6f0ebad4e054ec3e"

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t scale_id;
    uint32_t input_bytes;
    uint32_t input_h;
    uint32_t input_w;
    uint32_t input_c;
    uint32_t stride;
    uint32_t raw_channels;
    uint32_t public_channels;
    uint32_t sample_points;
    uint32_t scratch_peak_bytes;
    uint64_t input_hash;
    uint64_t layer_hash;
    uint64_t raw_hash;
    uint64_t box_hash;
    uint64_t cls_hash;
    uint64_t public_hash;
    uint32_t sample_point_ids[YV8N_DETECT_SAMPLE_POINTS];
    uint16_t best_classes[YV8N_DETECT_SAMPLE_POINTS];
    int32_t first_box_q8[4];
    uint32_t reserved[8];
} yolov8n_detect_summary_t;

TPA_STATIC_ASSERT(sizeof(yolov8n_detect_summary_t) == 192u,
                  "YOLOv8n Detect summary edge size must match .tpp");

static inline void yolov8n_detect_diag_putc(char c)
{
    arch_diag_putc(c);
}

static inline void yolov8n_detect_diag_puts(const char *s)
{
    while (*s)
        yolov8n_detect_diag_putc(*s++);
}

#define YV8N_DETECT_FAIL_MSG_STOP(msg) do { \
    yolov8n_detect_diag_puts(msg); \
    TEST_FAIL; \
    return tpa_stop(); \
} while (0)

#define YV8N_DETECT_REQUIRE_MSG_STOP(msg, cond) do { \
    if (!(cond)) \
        YV8N_DETECT_FAIL_MSG_STOP(msg); \
} while (0)

void yolov8n_p5_arena_begin(uint32_t declared_peak_bytes);
void *yolov8n_p5_arena_alloc(size_t bytes, size_t align);
uint32_t yolov8n_p5_arena_high_water(void);

#endif /* TPA_YOLOV8N_DETECT_DOWNSTREAM_H */
