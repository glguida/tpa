#include <stddef.h>
#include <stdint.h>

#include <etsoc/isa/cacheops.h>
#include <etsoc/isa/tensors.h>

#include "test.h"
#include "tpa/tpa.h"

#define TA_DIM 16u
#define TA_ROW_BYTES (TA_DIM * (uint32_t)sizeof(float))
#define TA_MATRIX_BYTES (TA_DIM * TA_ROW_BYTES)
#define TA_PACKET_HEADER_BYTES 64u
#define TA_PACKET_ALIGN 64u

#define TA_TENSOR_A_START 0u
#define TA_TENSOR_B_START 16u
#define TA_TENSOR_ROWS (TA_DIM - 1u)
#define TA_TENSOR_COLS (TA_DIM - 1u)
#define TA_TENSOR_BCOLS ((TA_DIM / 4u) - 1u)
#define TA_TENSOR_LOAD_NORMAL 0u
#define TA_TENSOR_FP32 0u

#define TA_A_VALUE 1.0f
#define TA_B_VALUE 2.0f
#define TA_EXPECT_VALUE (TA_A_VALUE * TA_B_VALUE * (float)TA_DIM)

#define TA_PACKET_MAGIC 0x54414c4e554c4c45ull
#define TA_EXPECTED_L1SCPDIS_ERROR (1ul << TENSOR_ERROR_SCP_DISABLED)

#define TA_TRACE_BEGIN         0x7a100000u
#define TA_TRACE_ALIGNED_BEGIN 0x7a100001u
#define TA_TRACE_ALIGNED_END   0x7a100002u
#define TA_TRACE_NEG_BEGIN     0x7a100003u
#define TA_TRACE_NEG_ERROR     0x7a100400u
#define TA_TRACE_NEG_END       0x7a100005u
#define TA_TRACE_PASS          0x7a1000ffu
#define TA_TRACE_FAIL          0x7a1000eeu

struct ta_tensor_packet {
    uint64_t magic;
    uint8_t header_pad[TA_PACKET_HEADER_BYTES - sizeof(uint64_t)];
    float a[TA_DIM][TA_DIM];
    float b[TA_DIM][TA_DIM];
} __attribute__((aligned(TA_PACKET_ALIGN)));

struct ta_check_ws {
    struct ta_tensor_packet *packet;
    uint32_t packet_len;
};

TPA_STATIC_ASSERT(TA_ROW_BYTES == 64u,
                  "Tensor alignment row must be one 64-byte FP32 row");
TPA_STATIC_ASSERT(sizeof(((struct ta_tensor_packet *)0)->header_pad) == 56u,
                  "Tensor alignment packet header pad must reserve 64 bytes");
TPA_STATIC_ASSERT(offsetof(struct ta_tensor_packet, a) == TA_PACKET_HEADER_BYTES,
                  "Tensor alignment A matrix must start after a 64-byte header");
TPA_STATIC_ASSERT(offsetof(struct ta_tensor_packet, b) ==
                      TA_PACKET_HEADER_BYTES + TA_MATRIX_BYTES,
                  "Tensor alignment B matrix must be 64-byte aligned");
TPA_STATIC_ASSERT(sizeof(struct ta_tensor_packet) ==
                      TA_PACKET_HEADER_BYTES + (2u * TA_MATRIX_BYTES),
                  "Tensor alignment packet size must match edge capacity");
TPA_STATIC_ASSERT(_Alignof(struct ta_tensor_packet) == TA_PACKET_ALIGN,
                  "Tensor alignment packet must be 64-byte aligned");
TPA_STATIC_ASSERT(sizeof(struct ta_check_ws) <= 16u,
                  "Tensor alignment checker manifest workspace too small");

TPA_PROC_MEM_META(tensor_alignment_source_meta, 711u, 0u);
TPA_PROC_MEM_META(tensor_alignment_check_meta, 712u, TA_MATRIX_BYTES);

tpa_op_t tensor_alignment_source_start(void);
tpa_op_t tensor_alignment_source_done(void);
tpa_op_t tensor_alignment_check_recv(void);
tpa_op_t tensor_alignment_check_done(void);

static void ta_mark(uint32_t tag)
{
    arch_trace(tag);
}

static void ta_fail(void)
{
    ta_mark(TA_TRACE_FAIL);
    TEST_FAIL;
}

static int ta_is_aligned(const void *ptr)
{
    return (((uintptr_t)ptr) & (uintptr_t)(TA_PACKET_ALIGN - 1u)) == 0u;
}

static void ta_clear_tensor_error(void)
{
    asm volatile("csrwi tensor_error, 0" : : : "memory");
}

static void ta_enable_tensor_scratchpad(void)
{
    uint64_t pmask = 0xffu;

    excl_mode(1);
    et_cache_evict_l1d_to_l2();
    asm volatile("fence rw, rw" : : : "memory");
    mcache_control(1, 0, 0, 0);
    mcache_control(1, 1, 0, 0);
    asm volatile("csrwi tensor_mask, 0\n"
                 "csrwi tensor_coop, 0\n"
                 "mova.m.x %0\n"
                 :
                 : "r"(pmask)
                 : "memory");
    ta_clear_tensor_error();
    excl_mode(0);
}

static void ta_disable_tensor_scratchpad(void)
{
    excl_mode(1);
    et_cache_evict_l1d_to_l2();
    asm volatile("fence rw, rw" : : : "memory");
    mcache_control(0, 0, 0, 0);
    asm volatile("fence rw, rw" : : : "memory");
    ta_clear_tensor_error();
    excl_mode(0);
}

static void ta_store_rf_16x16(float dst[TA_DIM][TA_DIM])
{
    char *p = (char *)dst;

    asm volatile(
        "fsw.ps f0, 0(%[p])\n"
        "fsw.ps f1, 32(%[p])\n"
        "add %[p], %[p], %[s]\n"
        "fsw.ps f2, 0(%[p])\n"
        "fsw.ps f3, 32(%[p])\n"
        "add %[p], %[p], %[s]\n"
        "fsw.ps f4, 0(%[p])\n"
        "fsw.ps f5, 32(%[p])\n"
        "add %[p], %[p], %[s]\n"
        "fsw.ps f6, 0(%[p])\n"
        "fsw.ps f7, 32(%[p])\n"
        "add %[p], %[p], %[s]\n"
        "fsw.ps f8, 0(%[p])\n"
        "fsw.ps f9, 32(%[p])\n"
        "add %[p], %[p], %[s]\n"
        "fsw.ps f10, 0(%[p])\n"
        "fsw.ps f11, 32(%[p])\n"
        "add %[p], %[p], %[s]\n"
        "fsw.ps f12, 0(%[p])\n"
        "fsw.ps f13, 32(%[p])\n"
        "add %[p], %[p], %[s]\n"
        "fsw.ps f14, 0(%[p])\n"
        "fsw.ps f15, 32(%[p])\n"
        "add %[p], %[p], %[s]\n"
        "fsw.ps f16, 0(%[p])\n"
        "fsw.ps f17, 32(%[p])\n"
        "add %[p], %[p], %[s]\n"
        "fsw.ps f18, 0(%[p])\n"
        "fsw.ps f19, 32(%[p])\n"
        "add %[p], %[p], %[s]\n"
        "fsw.ps f20, 0(%[p])\n"
        "fsw.ps f21, 32(%[p])\n"
        "add %[p], %[p], %[s]\n"
        "fsw.ps f22, 0(%[p])\n"
        "fsw.ps f23, 32(%[p])\n"
        "add %[p], %[p], %[s]\n"
        "fsw.ps f24, 0(%[p])\n"
        "fsw.ps f25, 32(%[p])\n"
        "add %[p], %[p], %[s]\n"
        "fsw.ps f26, 0(%[p])\n"
        "fsw.ps f27, 32(%[p])\n"
        "add %[p], %[p], %[s]\n"
        "fsw.ps f28, 0(%[p])\n"
        "fsw.ps f29, 32(%[p])\n"
        "add %[p], %[p], %[s]\n"
        "fsw.ps f30, 0(%[p])\n"
        "fsw.ps f31, 32(%[p])\n"
        : [p] "+&r"(p)
        : [s] "r"((uint64_t)TA_ROW_BYTES)
        : "memory", "f0", "f1", "f2", "f3", "f4", "f5", "f6", "f7",
          "f8", "f9", "f10", "f11", "f12", "f13", "f14", "f15",
          "f16", "f17", "f18", "f19", "f20", "f21", "f22", "f23",
          "f24", "f25", "f26", "f27", "f28", "f29", "f30", "f31");
}

static void ta_fill_packet(struct ta_tensor_packet *packet)
{
    packet->magic = TA_PACKET_MAGIC;
    for (uint32_t i = 0; i < sizeof(packet->header_pad); i++)
        packet->header_pad[i] = (uint8_t)i;

    for (uint32_t row = 0; row < TA_DIM; row++) {
        for (uint32_t col = 0; col < TA_DIM; col++) {
            packet->a[row][col] = TA_A_VALUE;
            packet->b[row][col] = TA_B_VALUE;
        }
    }
}

static int ta_packet_is_valid(const struct ta_tensor_packet *packet,
                              uint32_t packet_len)
{
    return packet && packet_len == sizeof(*packet) &&
           packet->magic == TA_PACKET_MAGIC && ta_is_aligned(packet) &&
           ta_is_aligned(packet->a) && ta_is_aligned(packet->b);
}

static int ta_validate_result(const float result[TA_DIM][TA_DIM])
{
    for (uint32_t row = 0; row < TA_DIM; row++) {
        for (uint32_t col = 0; col < TA_DIM; col++) {
            if (result[row][col] != TA_EXPECT_VALUE)
                return -1;
        }
    }

    return 0;
}

static int ta_run_aligned_success(const struct ta_tensor_packet *packet)
{
    float result[TA_DIM][TA_DIM] __attribute__((aligned(TA_PACKET_ALIGN)));
    unsigned long err;

    if (!ta_is_aligned(result))
        return -1;

    ta_mark(TA_TRACE_ALIGNED_BEGIN);
    ta_enable_tensor_scratchpad();
    ta_clear_tensor_error();

    tensor_load(0, 0, TA_TENSOR_A_START, TA_TENSOR_LOAD_NORMAL, 0,
                (uint64_t)packet->a, 0, TA_TENSOR_ROWS, TA_ROW_BYTES, 0);
    tensor_load(0, 0, TA_TENSOR_B_START, TA_TENSOR_LOAD_NORMAL, 0,
                (uint64_t)packet->b, 0, TA_TENSOR_ROWS, TA_ROW_BYTES, 1);
    tensor_wait(TENSOR_LOAD_WAIT_0);
    tensor_wait(TENSOR_LOAD_WAIT_1);
    tensor_fma(0, TA_TENSOR_BCOLS, TA_TENSOR_ROWS, TA_TENSOR_COLS, 0, 0, 0,
               0, 0, TA_TENSOR_B_START, TA_TENSOR_A_START, TA_TENSOR_FP32,
               1);
    tensor_wait(TENSOR_FMA_WAIT);

    err = get_tensor_error();
    if (err != 0)
        return -1;

    ta_store_rf_16x16(result);
    if (ta_validate_result(result) != 0)
        return -1;

    ta_mark(TA_TRACE_ALIGNED_END);
    return 0;
}

static unsigned long ta_run_scratchpad_disabled_negative(
    const struct ta_tensor_packet *packet)
{
    unsigned long err;

    ta_mark(TA_TRACE_NEG_BEGIN);
    ta_disable_tensor_scratchpad();
    ta_clear_tensor_error();
    tensor_load(0, 0, TA_TENSOR_A_START, TA_TENSOR_LOAD_NORMAL, 0,
                (uint64_t)packet->a, 0, TA_TENSOR_ROWS, TA_ROW_BYTES, 0);
    tensor_wait(TENSOR_LOAD_WAIT_0);
    err = get_tensor_error();
    ta_mark(TA_TRACE_NEG_ERROR | ((uint32_t)err & 0xffu));

    ta_enable_tensor_scratchpad();
    ta_clear_tensor_error();
    ta_mark(TA_TRACE_NEG_END);
    return err;
}

tpa_op_t tensor_alignment_source_start(void)
{
    struct tpa_channel *ch = tpa_chan(0);
    struct ta_tensor_packet *packet;

    ta_mark(TA_TRACE_BEGIN);

    if (!ch) {
        ta_fail();
        return tpa_stop();
    }

    packet = (struct ta_tensor_packet *)tpa_send_buf(ch);
    if (!packet || !ta_is_aligned(packet)) {
        ta_fail();
        return tpa_stop();
    }

    ta_fill_packet(packet);
    return tpa_send(ch, packet, sizeof(*packet), tensor_alignment_source_done);
}

tpa_op_t tensor_alignment_source_done(void)
{
    return tpa_stop();
}

tpa_op_t tensor_alignment_check_recv(void)
{
    struct ta_check_ws *w = tpa_ws();
    struct tpa_channel *ch = tpa_chan(0);

    if (!w || !ch) {
        ta_fail();
        return tpa_stop();
    }

    return tpa_recv(ch, (void **)&w->packet, &w->packet_len,
                    tensor_alignment_check_done);
}

tpa_op_t tensor_alignment_check_done(void)
{
    struct ta_check_ws *w = tpa_ws();
    unsigned long negative_error;

    if (!w || !ta_packet_is_valid(w->packet, w->packet_len)) {
        ta_fail();
        return tpa_stop();
    }

    if (ta_run_aligned_success(w->packet) != 0) {
        ta_fail();
        return tpa_stop();
    }

    negative_error = ta_run_scratchpad_disabled_negative(w->packet);
    if (negative_error != TA_EXPECTED_L1SCPDIS_ERROR) {
        ta_fail();
        return tpa_stop();
    }

    ta_mark(TA_TRACE_PASS);
    TEST_PASS;
    return tpa_stop();
}
