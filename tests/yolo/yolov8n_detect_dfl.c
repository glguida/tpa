#include <stddef.h>
#include <stdint.h>

#include "test.h"
#include "tpa/tpa.h"

#include "generated/yolov8n_detect_dfl_case0.h"

#define YOLOV8N_DFL_BEGIN       0xe8df1000u
#define YOLOV8N_DFL_GOOD_RAW    0xe8df1001u
#define YOLOV8N_DFL_GOOD_BOX    0xe8df1002u
#define YOLOV8N_DFL_GOOD_CLS    0xe8df1003u
#define YOLOV8N_DFL_GOOD_PUBLIC 0xe8df1004u
#define YOLOV8N_DFL_FAIL        0xe8df1eeu

#define POINT_STRIDE_FIELDS 4u
#define POINT_SCALE_IDX     0u
#define POINT_STRIDE        1u
#define POINT_Y             2u
#define POINT_X             3u

tpa_op_t yolov8n_detect_dfl_start(void);

/* The raw fixture is an immutable synthetic stand-in for Detect head edge
 * payloads. The decoded buffers are transient scratch local to this block test;
 * a future YOLOv8n graph must model real feature tensors as edge/channel data.
 * public_out_buf models the reduced public [1, 84, N] output with contiguous
 * channel-major layout: public[channel * NR_POINTS + point]. */
static int32_t box_q8_buf[YOLOV8N_DETECT_CASE0_BOX_ELEMS]
    __attribute__((aligned(64)));
static uint16_t cls_q15_buf[YOLOV8N_DETECT_CASE0_CLS_ELEMS]
    __attribute__((aligned(64)));
static int32_t public_out_buf[YOLOV8N_DETECT_CASE0_PUBLIC_ELEMS]
    __attribute__((aligned(64)));

static void mark(uint32_t v)
{
    arch_trace(v);
}

static inline void diag_putc(char c)
{
    uint64_t v = (1ull << 56) | (uint8_t)c;

    asm volatile("csrw validation1, %0" :: "r"(v) : "memory");
}

static void diag_puts(const char *s)
{
    while (*s)
        diag_putc(*s++);
}

static void diag_putu64(uint64_t v)
{
    char buf[32];
    uint32_t n = 0;

    if (!v) {
        diag_putc('0');
        return;
    }

    while (v) {
        buf[n++] = (char)('0' + (v % 10ull));
        v /= 10ull;
    }
    while (n)
        diag_putc(buf[--n]);
}

static void diag_line(const char *name, uint64_t v)
{
    diag_puts(name);
    diag_putc('=');
    diag_putu64(v);
    diag_putc('\n');
}

static uint64_t fnv1a64_step(uint64_t h, uint8_t v)
{
    h ^= v;
    h *= UINT64_C(0x100000001b3);
    return h;
}

static uint64_t hash_i8(const int8_t *buf, size_t n)
{
    uint64_t h = UINT64_C(0xcbf29ce484222325);

    for (size_t i = 0; i < n; ++i)
        h = fnv1a64_step(h, (uint8_t)buf[i]);
    return h;
}

static uint64_t hash_u32_value(uint64_t h, uint32_t value)
{
    h = fnv1a64_step(h, (uint8_t)(value & 0xffu));
    h = fnv1a64_step(h, (uint8_t)((value >> 8) & 0xffu));
    h = fnv1a64_step(h, (uint8_t)((value >> 16) & 0xffu));
    h = fnv1a64_step(h, (uint8_t)((value >> 24) & 0xffu));
    return h;
}

static uint64_t hash_i32(const int32_t *buf, size_t n)
{
    uint64_t h = UINT64_C(0xcbf29ce484222325);

    for (size_t i = 0; i < n; ++i)
        h = hash_u32_value(h, (uint32_t)buf[i]);
    return h;
}

static uint64_t hash_u16_as_i32(const uint16_t *buf, size_t n)
{
    uint64_t h = UINT64_C(0xcbf29ce484222325);

    for (size_t i = 0; i < n; ++i)
        h = hash_u32_value(h, (uint32_t)buf[i]);
    return h;
}

static const int8_t *raw_point(uint32_t point)
{
    return yolov8n_detect_case0_raw +
           (size_t)point * YOLOV8N_DETECT_CASE0_RAW_CHANNELS;
}

static uint32_t point_field(uint32_t point, uint32_t field)
{
    return yolov8n_detect_case0_points[(size_t)point * POINT_STRIDE_FIELDS + field];
}

static uint32_t dfl_project_q8(uint32_t point, uint32_t coord)
{
    const int8_t *raw = raw_point(point);
    const uint32_t base = coord * YOLOV8N_DETECT_CASE0_REG_MAX;
    uint32_t total = 0;
    uint32_t weighted = 0;

    for (uint32_t bin = 0; bin < YOLOV8N_DETECT_CASE0_REG_MAX; ++bin) {
        int32_t lut_idx = (int32_t)raw[base + bin] + 8;
        uint32_t weight;

        if (lut_idx < 0 || lut_idx > 16)
            return 0xffffffffu;
        weight = yolov8n_detect_case0_exp_lut_q12[(uint32_t)lut_idx];
        total += weight;
        weighted += bin * weight;
    }

    if (!total)
        return 0xffffffffu;
    return (weighted * 256u + total / 2u) / total;
}

static uint16_t sigmoid_q15(int8_t logit)
{
    int32_t lut_idx = (int32_t)logit + 8;

    if (lut_idx < 0 || lut_idx > 16)
        return UINT16_MAX;
    return yolov8n_detect_case0_sigmoid_lut_q15[(uint32_t)lut_idx];
}

static int decode_point(uint32_t point)
{
    const uint32_t stride = point_field(point, POINT_STRIDE);
    const uint32_t y = point_field(point, POINT_Y);
    const uint32_t x = point_field(point, POINT_X);
    const uint32_t left = dfl_project_q8(point, 0u);
    const uint32_t top = dfl_project_q8(point, 1u);
    const uint32_t right = dfl_project_q8(point, 2u);
    const uint32_t bottom = dfl_project_q8(point, 3u);
    const int32_t cx_grid_q8 = (int32_t)(x * 256u + 128u);
    const int32_t cy_grid_q8 = (int32_t)(y * 256u + 128u);
    const int32_t x1 = (cx_grid_q8 - (int32_t)left) * (int32_t)stride;
    const int32_t y1 = (cy_grid_q8 - (int32_t)top) * (int32_t)stride;
    const int32_t x2 = (cx_grid_q8 + (int32_t)right) * (int32_t)stride;
    const int32_t y2 = (cy_grid_q8 + (int32_t)bottom) * (int32_t)stride;
    int32_t *box = box_q8_buf + (size_t)point * 4u;
    const int8_t *raw = raw_point(point);
    const size_t cls_out = (size_t)point * YOLOV8N_DETECT_CASE0_CLASS_CHANNELS;

    if (left == 0xffffffffu || top == 0xffffffffu ||
        right == 0xffffffffu || bottom == 0xffffffffu)
        return -1;

    box[0] = (x1 + x2) / 2;
    box[1] = (y1 + y2) / 2;
    box[2] = x2 - x1;
    box[3] = y2 - y1;

    for (uint32_t cls = 0; cls < YOLOV8N_DETECT_CASE0_CLASS_CHANNELS; ++cls) {
        const size_t raw_idx = YOLOV8N_DETECT_CASE0_DFL_CHANNELS + cls;
        uint16_t score = sigmoid_q15(raw[raw_idx]);

        if (score == UINT16_MAX)
            return -1;
        cls_q15_buf[cls_out + cls] = score;
    }

    return 0;
}

static int verify_layout(void)
{
    if (YOLOV8N_DETECT_CASE0_REG_MAX != 16u ||
        YOLOV8N_DETECT_CASE0_DFL_CHANNELS != 4u * YOLOV8N_DETECT_CASE0_REG_MAX ||
        YOLOV8N_DETECT_CASE0_CLASS_CHANNELS != 80u ||
        YOLOV8N_DETECT_CASE0_RAW_CHANNELS !=
            YOLOV8N_DETECT_CASE0_DFL_CHANNELS + YOLOV8N_DETECT_CASE0_CLASS_CHANNELS ||
        YOLOV8N_DETECT_CASE0_PUBLIC_CHANNELS != 4u + YOLOV8N_DETECT_CASE0_CLASS_CHANNELS)
        return -1;

    if (YOLOV8N_DETECT_CASE0_RAW_ELEMS !=
        YOLOV8N_DETECT_CASE0_NR_POINTS * YOLOV8N_DETECT_CASE0_RAW_CHANNELS)
        return -1;
    if (YOLOV8N_DETECT_CASE0_BOX_ELEMS != YOLOV8N_DETECT_CASE0_NR_POINTS * 4u)
        return -1;
    if (YOLOV8N_DETECT_CASE0_CLS_ELEMS !=
        YOLOV8N_DETECT_CASE0_NR_POINTS * YOLOV8N_DETECT_CASE0_CLASS_CHANNELS)
        return -1;
    if (YOLOV8N_DETECT_CASE0_PUBLIC_ELEMS !=
        YOLOV8N_DETECT_CASE0_NR_POINTS * YOLOV8N_DETECT_CASE0_PUBLIC_CHANNELS)
        return -1;

    for (uint32_t point = 0; point < YOLOV8N_DETECT_CASE0_NR_POINTS; ++point) {
        const uint32_t scale_idx = point_field(point, POINT_SCALE_IDX);
        const uint32_t stride = point_field(point, POINT_STRIDE);

        if (scale_idx >= YOLOV8N_DETECT_CASE0_NR_SCALES)
            return -1;
        if (!(stride == 8u || stride == 16u || stride == 32u))
            return -1;
    }

    return 0;
}

static int compare_box(void)
{
    for (size_t i = 0; i < YOLOV8N_DETECT_CASE0_BOX_ELEMS; ++i) {
        if (box_q8_buf[i] != yolov8n_detect_case0_box_q8[i]) {
            diag_puts("DIFF_BOX\n");
            diag_line("idx", i);
            diag_line("got", (uint32_t)box_q8_buf[i]);
            diag_line("exp", (uint32_t)yolov8n_detect_case0_box_q8[i]);
            return -1;
        }
    }
    return 0;
}

static int compare_cls(void)
{
    for (size_t i = 0; i < YOLOV8N_DETECT_CASE0_CLS_ELEMS; ++i) {
        if (cls_q15_buf[i] != yolov8n_detect_case0_cls_q15[i]) {
            diag_puts("DIFF_CLS\n");
            diag_line("idx", i);
            diag_line("got", cls_q15_buf[i]);
            diag_line("exp", yolov8n_detect_case0_cls_q15[i]);
            return -1;
        }
    }
    return 0;
}

static void build_public_output(void)
{
    for (uint32_t point = 0; point < YOLOV8N_DETECT_CASE0_NR_POINTS; ++point) {
        const size_t box_base = (size_t)point * 4u;
        const size_t cls_base = (size_t)point * YOLOV8N_DETECT_CASE0_CLASS_CHANNELS;

        for (uint32_t channel = 0; channel < 4u; ++channel) {
            const size_t out_idx =
                (size_t)channel * YOLOV8N_DETECT_CASE0_NR_POINTS + point;

            public_out_buf[out_idx] = box_q8_buf[box_base + channel];
        }
        for (uint32_t cls = 0; cls < YOLOV8N_DETECT_CASE0_CLASS_CHANNELS; ++cls) {
            const size_t out_idx =
                (size_t)(4u + cls) * YOLOV8N_DETECT_CASE0_NR_POINTS + point;

            public_out_buf[out_idx] = (int32_t)cls_q15_buf[cls_base + cls];
        }
    }
}

static int compare_public(void)
{
    for (size_t i = 0; i < YOLOV8N_DETECT_CASE0_PUBLIC_ELEMS; ++i) {
        if (public_out_buf[i] != yolov8n_detect_case0_public[i]) {
            diag_puts("DIFF_PUBLIC\n");
            diag_line("idx", i);
            diag_line("got", (uint32_t)public_out_buf[i]);
            diag_line("exp", (uint32_t)yolov8n_detect_case0_public[i]);
            return -1;
        }
    }
    return 0;
}

tpa_op_t yolov8n_detect_dfl_start(void)
{
    uint64_t raw_hash;
    uint64_t box_hash;
    uint64_t cls_hash;
    uint64_t public_hash;

    mark(YOLOV8N_DFL_BEGIN);
    if (verify_layout()) {
        mark(YOLOV8N_DFL_FAIL);
        mark(0x10u);
        TEST_FAIL;
        return tpa_stop();
    }

    raw_hash = hash_i8(yolov8n_detect_case0_raw,
                       YOLOV8N_DETECT_CASE0_RAW_ELEMS);
    if (raw_hash != YOLOV8N_DETECT_CASE0_RAW_HASH) {
        diag_puts("HASH_RAW\n");
        diag_line("got", raw_hash);
        diag_line("exp", YOLOV8N_DETECT_CASE0_RAW_HASH);
        mark(YOLOV8N_DFL_FAIL);
        mark(0x11u);
        TEST_FAIL;
        return tpa_stop();
    }
    mark(YOLOV8N_DFL_GOOD_RAW);

    for (uint32_t point = 0; point < YOLOV8N_DETECT_CASE0_NR_POINTS; ++point) {
        if (decode_point(point)) {
            mark(YOLOV8N_DFL_FAIL);
            mark(0x20u + point);
            TEST_FAIL;
            return tpa_stop();
        }
    }

    if (compare_box()) {
        mark(YOLOV8N_DFL_FAIL);
        mark(0x30u);
        TEST_FAIL;
        return tpa_stop();
    }
    box_hash = hash_i32(box_q8_buf, YOLOV8N_DETECT_CASE0_BOX_ELEMS);
    if (box_hash != YOLOV8N_DETECT_CASE0_BOX_HASH) {
        diag_puts("HASH_BOX\n");
        diag_line("got", box_hash);
        diag_line("exp", YOLOV8N_DETECT_CASE0_BOX_HASH);
        mark(YOLOV8N_DFL_FAIL);
        mark(0x31u);
        TEST_FAIL;
        return tpa_stop();
    }
    mark(YOLOV8N_DFL_GOOD_BOX);

    if (compare_cls()) {
        mark(YOLOV8N_DFL_FAIL);
        mark(0x40u);
        TEST_FAIL;
        return tpa_stop();
    }
    cls_hash = hash_u16_as_i32(cls_q15_buf, YOLOV8N_DETECT_CASE0_CLS_ELEMS);
    if (cls_hash != YOLOV8N_DETECT_CASE0_CLS_HASH) {
        diag_puts("HASH_CLS\n");
        diag_line("got", cls_hash);
        diag_line("exp", YOLOV8N_DETECT_CASE0_CLS_HASH);
        mark(YOLOV8N_DFL_FAIL);
        mark(0x41u);
        TEST_FAIL;
        return tpa_stop();
    }
    mark(YOLOV8N_DFL_GOOD_CLS);

    build_public_output();
    if (compare_public()) {
        mark(YOLOV8N_DFL_FAIL);
        mark(0x50u);
        TEST_FAIL;
        return tpa_stop();
    }
    public_hash = hash_i32(public_out_buf, YOLOV8N_DETECT_CASE0_PUBLIC_ELEMS);
    if (public_hash != YOLOV8N_DETECT_CASE0_PUBLIC_HASH) {
        diag_puts("HASH_PUBLIC\n");
        diag_line("got", public_hash);
        diag_line("exp", YOLOV8N_DETECT_CASE0_PUBLIC_HASH);
        mark(YOLOV8N_DFL_FAIL);
        mark(0x51u);
        TEST_FAIL;
        return tpa_stop();
    }
    mark(YOLOV8N_DFL_GOOD_PUBLIC);

    TEST_PASS;
    return tpa_stop();
}
