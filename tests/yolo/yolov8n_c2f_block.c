#include <stddef.h>
#include <stdint.h>

#include "test.h"
#include "tpa/tpa.h"

#include "generated/yolov8n_c2f_block_case0.h"

#define YOLOV8N_C2F_BEGIN       0xe8c2f000u
#define YOLOV8N_C2F_GOOD_INPUT  0xe8c2f001u
#define YOLOV8N_C2F_GOOD_SPLIT  0xe8c2f002u
#define YOLOV8N_C2F_GOOD_OUTPUT 0xe8c2f003u
#define YOLOV8N_C2F_FAIL        0xe8c2feeu

tpa_op_t yolov8n_c2f_block_start(void);

/* Synthetic immutable test fixtures live in the generated header. These mutable
 * buffers are transient compute scratch for the block test, not graph
 * edge/channel payloads and not persistent process workspace. */
static int8_t cv1_buf[YOLOV8N_C2F_CASE0_CV1_ELEMS]
    __attribute__((aligned(64)));
static int8_t chunk0_buf[YOLOV8N_C2F_CASE0_CHUNK_ELEMS]
    __attribute__((aligned(64)));
static int8_t chunk1_buf[YOLOV8N_C2F_CASE0_CHUNK_ELEMS]
    __attribute__((aligned(64)));
static int8_t bottleneck0_buf[YOLOV8N_C2F_CASE0_CHUNK_ELEMS]
    __attribute__((aligned(64)));
static int8_t bottleneck1_buf[YOLOV8N_C2F_CASE0_CHUNK_ELEMS]
    __attribute__((aligned(64)));
static int8_t cat_buf[YOLOV8N_C2F_CASE0_CAT_ELEMS]
    __attribute__((aligned(64)));
static int8_t tmp0_buf[YOLOV8N_C2F_CASE0_CHUNK_ELEMS]
    __attribute__((aligned(64)));
static int8_t tmp1_buf[YOLOV8N_C2F_CASE0_CHUNK_ELEMS]
    __attribute__((aligned(64)));
static int8_t out_buf[YOLOV8N_C2F_CASE0_OUTPUT_ELEMS]
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

static uint64_t fnv1a64_step(uint64_t h, int8_t v)
{
    h ^= (uint8_t)v;
    h *= UINT64_C(0x100000001b3);
    return h;
}

static uint64_t hash_i8(const int8_t *buf, size_t n)
{
    uint64_t h = UINT64_C(0xcbf29ce484222325);

    for (size_t i = 0; i < n; ++i)
        h = fnv1a64_step(h, buf[i]);
    return h;
}

static int8_t clamp_i8(int32_t v)
{
    if (v < -128)
        return -128;
    if (v > 127)
        return 127;
    return (int8_t)v;
}

static int8_t synthetic_linear_one(const int8_t *src, uint32_t pix,
                                   uint32_t in_c, uint32_t oc,
                                   uint32_t seed)
{
    int32_t acc = (int32_t)(seed * 19u + (pix + 1u) * (oc + 3u)) - 37;

    for (uint32_t ic = 0; ic < in_c; ++ic) {
        int32_t w = (int32_t)((seed + 3u * (oc + 1u) +
                               5u * (ic + 1u)) % 9u) - 4;
        acc += (int32_t)src[(size_t)pix * in_c + ic] * w;
    }

    return clamp_i8(acc / 8);
}

static void run_linear(const int8_t *src, uint32_t in_c, int8_t *dst,
                       uint32_t out_c, uint32_t seed)
{
    for (uint32_t pix = 0; pix < YOLOV8N_C2F_CASE0_NR_PIX; ++pix) {
        for (uint32_t oc = 0; oc < out_c; ++oc)
            dst[(size_t)pix * out_c + oc] =
                synthetic_linear_one(src, pix, in_c, oc, seed);
    }
}

static void split_cv1(void)
{
    const uint32_t hidden_c = YOLOV8N_C2F_CASE0_HIDDEN_C;
    const uint32_t cv1_c = 2u * hidden_c;

    for (uint32_t pix = 0; pix < YOLOV8N_C2F_CASE0_NR_PIX; ++pix) {
        const size_t cv1_base = (size_t)pix * cv1_c;
        const size_t chunk_base = (size_t)pix * hidden_c;

        for (uint32_t c = 0; c < hidden_c; ++c) {
            chunk0_buf[chunk_base + c] = cv1_buf[cv1_base + c];
            chunk1_buf[chunk_base + c] = cv1_buf[cv1_base + hidden_c + c];
        }
    }
}

static void run_bottleneck(const int8_t *src, int8_t *dst, uint32_t seed)
{
    const uint32_t hidden_c = YOLOV8N_C2F_CASE0_HIDDEN_C;

    run_linear(src, hidden_c, tmp0_buf, hidden_c, seed);
    run_linear(tmp0_buf, hidden_c, tmp1_buf, hidden_c, seed + 11u);
    for (size_t i = 0; i < YOLOV8N_C2F_CASE0_CHUNK_ELEMS; ++i)
        dst[i] = clamp_i8((int32_t)tmp1_buf[i] + (int32_t)src[i]);
}

static void concat_c2f(void)
{
    const uint32_t hidden_c = YOLOV8N_C2F_CASE0_HIDDEN_C;
    const uint32_t cat_c = YOLOV8N_C2F_CASE0_CAT_C;

    for (uint32_t pix = 0; pix < YOLOV8N_C2F_CASE0_NR_PIX; ++pix) {
        const size_t chunk_base = (size_t)pix * hidden_c;
        const size_t cat_base = (size_t)pix * cat_c;

        for (uint32_t c = 0; c < hidden_c; ++c) {
            cat_buf[cat_base + c] = chunk0_buf[chunk_base + c];
            cat_buf[cat_base + hidden_c + c] = chunk1_buf[chunk_base + c];
            cat_buf[cat_base + 2u * hidden_c + c] =
                bottleneck0_buf[chunk_base + c];
            cat_buf[cat_base + 3u * hidden_c + c] =
                bottleneck1_buf[chunk_base + c];
        }
    }
}

static int check_hash(const char *name, const int8_t *buf, size_t n,
                      uint64_t expected)
{
    uint64_t got = hash_i8(buf, n);

    if (got == expected)
        return 0;

    diag_puts("HASH_");
    diag_puts(name);
    diag_putc('\n');
    diag_line("got", got);
    diag_line("exp", expected);
    return -1;
}

static int compare_output(void)
{
    for (size_t i = 0; i < YOLOV8N_C2F_CASE0_OUTPUT_ELEMS; ++i) {
        if (out_buf[i] != yolov8n_c2f_case0_out[i]) {
            diag_puts("DIFF_OUT\n");
            diag_line("idx", i);
            diag_line("got", (uint8_t)out_buf[i]);
            diag_line("exp", (uint8_t)yolov8n_c2f_case0_out[i]);
            return -1;
        }
    }
    return 0;
}

tpa_op_t yolov8n_c2f_block_start(void)
{
    mark(YOLOV8N_C2F_BEGIN);

    if (YOLOV8N_C2F_CASE0_HIDDEN_C != 4u ||
        YOLOV8N_C2F_CASE0_BLOCKS != 2u ||
        YOLOV8N_C2F_CASE0_CAT_C !=
            (2u + YOLOV8N_C2F_CASE0_BLOCKS) * YOLOV8N_C2F_CASE0_HIDDEN_C ||
        YOLOV8N_C2F_CASE0_INPUT_ELEMS !=
            YOLOV8N_C2F_CASE0_NR_PIX * YOLOV8N_C2F_CASE0_IN_C ||
        YOLOV8N_C2F_CASE0_OUTPUT_ELEMS !=
            YOLOV8N_C2F_CASE0_NR_PIX * YOLOV8N_C2F_CASE0_OUT_C) {
        mark(YOLOV8N_C2F_FAIL);
        mark(0x10u);
        TEST_FAIL;
        return tpa_stop();
    }

    if (check_hash("INPUT", yolov8n_c2f_case0_in,
                   YOLOV8N_C2F_CASE0_INPUT_ELEMS,
                   YOLOV8N_C2F_CASE0_INPUT_HASH)) {
        mark(YOLOV8N_C2F_FAIL);
        mark(0x11u);
        TEST_FAIL;
        return tpa_stop();
    }
    mark(YOLOV8N_C2F_GOOD_INPUT);

    run_linear(yolov8n_c2f_case0_in, YOLOV8N_C2F_CASE0_IN_C, cv1_buf,
               2u * YOLOV8N_C2F_CASE0_HIDDEN_C, 3u);
    split_cv1();
    run_bottleneck(chunk1_buf, bottleneck0_buf, 17u);
    run_bottleneck(bottleneck0_buf, bottleneck1_buf, 29u);
    concat_c2f();
    run_linear(cat_buf, YOLOV8N_C2F_CASE0_CAT_C, out_buf,
               YOLOV8N_C2F_CASE0_OUT_C, 43u);

    if (check_hash("CHUNK0", chunk0_buf, YOLOV8N_C2F_CASE0_CHUNK_ELEMS,
                   YOLOV8N_C2F_CASE0_CHUNK0_HASH) ||
        check_hash("CHUNK1", chunk1_buf, YOLOV8N_C2F_CASE0_CHUNK_ELEMS,
                   YOLOV8N_C2F_CASE0_CHUNK1_HASH) ||
        check_hash("B0", bottleneck0_buf, YOLOV8N_C2F_CASE0_CHUNK_ELEMS,
                   YOLOV8N_C2F_CASE0_B0_HASH) ||
        check_hash("B1", bottleneck1_buf, YOLOV8N_C2F_CASE0_CHUNK_ELEMS,
                   YOLOV8N_C2F_CASE0_B1_HASH) ||
        check_hash("CAT", cat_buf, YOLOV8N_C2F_CASE0_CAT_ELEMS,
                   YOLOV8N_C2F_CASE0_CAT_HASH)) {
        mark(YOLOV8N_C2F_FAIL);
        mark(0x20u);
        TEST_FAIL;
        return tpa_stop();
    }
    mark(YOLOV8N_C2F_GOOD_SPLIT);

    if (compare_output() ||
        check_hash("OUTPUT", out_buf, YOLOV8N_C2F_CASE0_OUTPUT_ELEMS,
                   YOLOV8N_C2F_CASE0_OUTPUT_HASH)) {
        mark(YOLOV8N_C2F_FAIL);
        mark(0x30u);
        TEST_FAIL;
        return tpa_stop();
    }

    mark(YOLOV8N_C2F_GOOD_OUTPUT);
    TEST_PASS;
    return tpa_stop();
}
