#include <stdint.h>

#include <etsoc/isa/cacheops.h>
#include <etsoc/isa/tensors.h>

#include "tpa/tpa.h"

#ifndef TPA_TENSOR_MATMUL_ROWS
#define TPA_TENSOR_MATMUL_ROWS 8u
#endif

#ifndef TPA_TENSOR_MATMUL_COLS
#define TPA_TENSOR_MATMUL_COLS 8u
#endif

#ifndef TPA_TENSOR_MATMUL_ROUNDS
#define TPA_TENSOR_MATMUL_ROUNDS 4u
#endif

#ifndef TPA_TENSOR_MATMUL_TILE_N
#define TPA_TENSOR_MATMUL_TILE_N 64u
#endif

#define TM_ROWS TPA_TENSOR_MATMUL_ROWS
#define TM_COLS TPA_TENSOR_MATMUL_COLS
#define TM_ROUNDS TPA_TENSOR_MATMUL_ROUNDS
#define TM_TILE_N TPA_TENSOR_MATMUL_TILE_N
#define TM_TENSOR_N 16u
#define TM_TENSOR_BLOCKS (TM_TILE_N / TM_TENSOR_N)
#define TM_TILE_ELEMS (TM_TILE_N * TM_TILE_N)
#define TM_TILE_BYTES (TM_TILE_ELEMS * sizeof(float))
#define TM_TILE_ROW_BYTES (TM_TILE_N * sizeof(float))
#define TM_NR_TILES (TM_ROWS * TM_COLS)
#define TM_TENSOR_TL0_START 0u
#define TM_TENSOR_TL1_START 32u
#define TM_TENSOR_ROWS (TM_TENSOR_N - 1u)
#define TM_TENSOR_COLS (TM_TENSOR_N - 1u)
#define TM_TENSOR_BCOLS ((TM_TENSOR_N / 4u) - 1u)
#define TM_TENSOR_FP32 0u

#define TM_A_VALUE 1.0f
#define TM_B_VALUE 2.0f
#define TM_EXPECT_VALUE ((float)TM_ROUNDS * (float)TM_TILE_N * TM_A_VALUE * TM_B_VALUE)

#define TM_MARK_BEGIN 0xd3610000ull
#define TM_MARK_PASS  0xd36100ffull
#define TM_MARK_FAIL  0xd36100eeull
#define TM_TEST_PASS  0x1feed000ull
#define TM_TEST_FAIL  0x50bad000ull

struct tm_feed_ws {
    float tile[TM_ROUNDS][TM_TILE_ELEMS] __attribute__((aligned(64)));
    uint32_t round;
};

struct tm_cell_ws {
    float *a;
    float *b;
    uint32_t a_len;
    uint32_t b_len;
    float acc[TM_TILE_ELEMS] __attribute__((aligned(64)));
    uint32_t round;
};

struct tm_check_ws {
    float *tile;
    uint32_t tile_len;
};

TPA_STATIC_ASSERT(sizeof(struct tm_feed_ws) <= 65664u,
                  "tensor matmul feed manifest workspace too small");
TPA_STATIC_ASSERT(sizeof(struct tm_cell_ws) <= 16576u,
                  "tensor matmul cell manifest workspace too small");
TPA_STATIC_ASSERT(sizeof(struct tm_check_ws) <= 64u,
                  "tensor matmul check manifest workspace too small");

TPA_PROC_MEM_META(tensor_matmul_a_feed_meta, 601u, 0u);
TPA_PROC_MEM_META(tensor_matmul_b_feed_meta, 602u, 0u);
TPA_PROC_MEM_META(tensor_matmul_cell_meta, 603u, TM_TILE_BYTES);
TPA_PROC_MEM_META(tensor_matmul_check_meta, 604u, 0u);

static volatile uint32_t tm_started __attribute__((aligned(64)));
static volatile uint32_t tm_failed __attribute__((aligned(64)));
static volatile uint32_t tm_done __attribute__((aligned(64)));
static volatile uint32_t tm_tensor_ready[ARCH_NR_HARTS] __attribute__((aligned(64)));

static inline void tm_mark(uint64_t v)
{
    arch_trace((uint32_t)v);
}

static void tm_fail(void)
{
    tpa_hal_atomic_or_u32(&tm_failed, 1u);
    tm_mark(TM_MARK_FAIL);
    tm_mark(TM_TEST_FAIL);
    tpa_hal_test_fail();
}

static void tm_maybe_begin(void)
{
    if (!tpa_hal_atomic_exchange_u32(&tm_started, 1u))
        tm_mark(TM_MARK_BEGIN);
}

static void tm_fill(float *tile, float value)
{
    for (uint32_t i = 0; i < TM_TILE_ELEMS; i++)
        tile[i] = value;
}

static void tm_clear(float *tile)
{
    for (uint32_t i = 0; i < TM_TILE_ELEMS; i++)
        tile[i] = 0.0f;
}

static inline void tm_clear_tensor_error(void)
{
    asm volatile("csrwi tensor_error, 0" : : : "memory");
}

static void tm_enable_tensor_scratchpad(void)
{
    uint32_t hart = arch_runtime_hartid();
    uint64_t pmask = 0xffu;

    if (tm_tensor_ready[hart])
        return;

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
    tm_clear_tensor_error();
    excl_mode(0);
    tm_tensor_ready[hart] = 1u;
}

static inline float *tm_tile_block(float *tile, uint32_t brow, uint32_t bcol)
{
    return tile + brow * TM_TENSOR_N * TM_TILE_N + bcol * TM_TENSOR_N;
}

static inline const float *tm_tile_block_const(const float *tile, uint32_t brow,
                                               uint32_t bcol)
{
    return tile + brow * TM_TENSOR_N * TM_TILE_N + bcol * TM_TENSOR_N;
}

static inline void tm_load_rf_16x16(const float *base)
{
    const char *p = (const char *)base;

    asm volatile(
        "flw.ps f0, 0(%[p])\n"
        "flw.ps f1, 32(%[p])\n"
        "add %[p], %[p], %[s]\n"
        "flw.ps f2, 0(%[p])\n"
        "flw.ps f3, 32(%[p])\n"
        "add %[p], %[p], %[s]\n"
        "flw.ps f4, 0(%[p])\n"
        "flw.ps f5, 32(%[p])\n"
        "add %[p], %[p], %[s]\n"
        "flw.ps f6, 0(%[p])\n"
        "flw.ps f7, 32(%[p])\n"
        "add %[p], %[p], %[s]\n"
        "flw.ps f8, 0(%[p])\n"
        "flw.ps f9, 32(%[p])\n"
        "add %[p], %[p], %[s]\n"
        "flw.ps f10, 0(%[p])\n"
        "flw.ps f11, 32(%[p])\n"
        "add %[p], %[p], %[s]\n"
        "flw.ps f12, 0(%[p])\n"
        "flw.ps f13, 32(%[p])\n"
        "add %[p], %[p], %[s]\n"
        "flw.ps f14, 0(%[p])\n"
        "flw.ps f15, 32(%[p])\n"
        "add %[p], %[p], %[s]\n"
        "flw.ps f16, 0(%[p])\n"
        "flw.ps f17, 32(%[p])\n"
        "add %[p], %[p], %[s]\n"
        "flw.ps f18, 0(%[p])\n"
        "flw.ps f19, 32(%[p])\n"
        "add %[p], %[p], %[s]\n"
        "flw.ps f20, 0(%[p])\n"
        "flw.ps f21, 32(%[p])\n"
        "add %[p], %[p], %[s]\n"
        "flw.ps f22, 0(%[p])\n"
        "flw.ps f23, 32(%[p])\n"
        "add %[p], %[p], %[s]\n"
        "flw.ps f24, 0(%[p])\n"
        "flw.ps f25, 32(%[p])\n"
        "add %[p], %[p], %[s]\n"
        "flw.ps f26, 0(%[p])\n"
        "flw.ps f27, 32(%[p])\n"
        "add %[p], %[p], %[s]\n"
        "flw.ps f28, 0(%[p])\n"
        "flw.ps f29, 32(%[p])\n"
        "add %[p], %[p], %[s]\n"
        "flw.ps f30, 0(%[p])\n"
        "flw.ps f31, 32(%[p])\n"
        : [p] "+&r"(p)
        : [s] "r"((uint64_t)TM_TILE_ROW_BYTES)
        : "memory", "f0", "f1", "f2", "f3", "f4", "f5", "f6", "f7",
          "f8", "f9", "f10", "f11", "f12", "f13", "f14", "f15",
          "f16", "f17", "f18", "f19", "f20", "f21", "f22", "f23",
          "f24", "f25", "f26", "f27", "f28", "f29", "f30", "f31");
}

static inline void tm_store_rf_16x16(float *base)
{
    char *p = (char *)base;

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
        : [s] "r"((uint64_t)TM_TILE_ROW_BYTES)
        : "memory");
}

static int tm_tensor_matmul_acc(float *dst, const float *lhs, const float *rhs)
{
    tm_clear_tensor_error();

    for (uint32_t brow = 0; brow < TM_TENSOR_BLOCKS; brow++) {
        for (uint32_t bcol = 0; bcol < TM_TENSOR_BLOCKS; bcol++) {
            tm_load_rf_16x16(tm_tile_block_const(dst, brow, bcol));

            for (uint32_t bk = 0; bk < TM_TENSOR_BLOCKS; bk++) {
                const float *ab = tm_tile_block_const(lhs, brow, bk);
                const float *bb = tm_tile_block_const(rhs, bk, bcol);

                tensor_load(0, 0, TM_TENSOR_TL0_START, 0, 0, (uint64_t)ab, 0,
                            TM_TENSOR_ROWS, TM_TILE_ROW_BYTES, 0);
                tensor_load(0, 0, TM_TENSOR_TL1_START, 0, 1, (uint64_t)bb, 0,
                            TM_TENSOR_ROWS, TM_TILE_ROW_BYTES, 1);
                tensor_wait(TENSOR_LOAD_WAIT_0);
                tensor_fma(0, TM_TENSOR_BCOLS, TM_TENSOR_ROWS, TM_TENSOR_COLS,
                           0, 0, 0, 0, 1, TM_TENSOR_TL1_START,
                           TM_TENSOR_TL0_START, TM_TENSOR_FP32, 0);
                tensor_wait(TENSOR_FMA_WAIT);
            }

            tm_store_rf_16x16(tm_tile_block(dst, brow, bcol));
        }
    }

    return get_tensor_error() == 0 ? 0 : -1;
}

tpa_op_t tensor_matmul_a_feed_start(void);
tpa_op_t tensor_matmul_a_feed_done(void);
tpa_op_t tensor_matmul_b_feed_start(void);
tpa_op_t tensor_matmul_b_feed_done(void);
tpa_op_t tensor_matmul_cell_init(void);
tpa_op_t tensor_matmul_cell_recv_a(void);
tpa_op_t tensor_matmul_cell_recv_b(void);
tpa_op_t tensor_matmul_cell_compute(void);
tpa_op_t tensor_matmul_cell_send_south(void);
tpa_op_t tensor_matmul_cell_send_west(void);
tpa_op_t tensor_matmul_cell_after_forward(void);
tpa_op_t tensor_matmul_cell_done(void);
tpa_op_t tensor_matmul_check_start(void);
tpa_op_t tensor_matmul_check_done(void);

static tpa_op_t tm_feed_start(float value, tpa_cont_t done)
{
    struct tm_feed_ws *w = tpa_ws();
    struct tpa_chan *ch = tpa_chan(0);

    tm_maybe_begin();
    if (!w || !ch) {
        tm_fail();
        return tpa_stop();
    }

    tm_fill(w->tile[w->round], value);
    return tpa_send(ch, w->tile[w->round], TM_TILE_BYTES, done);
}

static tpa_op_t tm_feed_done(tpa_cont_t restart)
{
    struct tm_feed_ws *w = tpa_ws();

    if (!w) {
        tm_fail();
        return tpa_stop();
    }

    w->round++;
    if (w->round < TM_ROUNDS)
        return restart();

    return tpa_stop();
}

tpa_op_t tensor_matmul_a_feed_start(void)
{
    return tm_feed_start(TM_A_VALUE, tensor_matmul_a_feed_done);
}

tpa_op_t tensor_matmul_a_feed_done(void)
{
    return tm_feed_done(tensor_matmul_a_feed_start);
}

tpa_op_t tensor_matmul_b_feed_start(void)
{
    return tm_feed_start(TM_B_VALUE, tensor_matmul_b_feed_done);
}

tpa_op_t tensor_matmul_b_feed_done(void)
{
    return tm_feed_done(tensor_matmul_b_feed_start);
}

tpa_op_t tensor_matmul_cell_init(void)
{
    struct tm_cell_ws *w = tpa_ws();

    if (!w) {
        tm_fail();
        return tpa_stop();
    }

    tm_enable_tensor_scratchpad();
    tm_clear(w->acc);
    w->round = 0;
    return tensor_matmul_cell_recv_a();
}

tpa_op_t tensor_matmul_cell_recv_a(void)
{
    struct tm_cell_ws *w = tpa_ws();
    struct tpa_chan *ch = tpa_chan(0);

    if (!w || !ch) {
        tm_fail();
        return tpa_stop();
    }

    return tpa_recv(ch, (void **)&w->a, &w->a_len, tensor_matmul_cell_recv_b);
}

tpa_op_t tensor_matmul_cell_recv_b(void)
{
    struct tm_cell_ws *w = tpa_ws();
    struct tpa_chan *ch = tpa_chan(1);

    if (!w || !ch) {
        tm_fail();
        return tpa_stop();
    }

    return tpa_recv(ch, (void **)&w->b, &w->b_len, tensor_matmul_cell_compute);
}

tpa_op_t tensor_matmul_cell_compute(void)
{
    struct tm_cell_ws *w = tpa_ws();

    if (!w || !w->a || !w->b || w->a_len != TM_TILE_BYTES ||
        w->b_len != TM_TILE_BYTES) {
        tm_fail();
        return tpa_stop();
    }

    if (tm_tensor_matmul_acc(w->acc, w->a, w->b)) {
        tm_fail();
        return tpa_stop();
    }

    return tensor_matmul_cell_send_south();
}

tpa_op_t tensor_matmul_cell_send_south(void)
{
    struct tm_cell_ws *w = tpa_ws();
    struct tpa_chan *ch = tpa_chan(2);

    if (!w) {
        tm_fail();
        return tpa_stop();
    }

    if (ch)
        return tpa_send(ch, w->a, TM_TILE_BYTES, tensor_matmul_cell_send_west);

    return tensor_matmul_cell_send_west();
}

tpa_op_t tensor_matmul_cell_send_west(void)
{
    struct tm_cell_ws *w = tpa_ws();
    struct tpa_chan *ch = tpa_chan(3);

    if (!w) {
        tm_fail();
        return tpa_stop();
    }

    if (ch)
        return tpa_send(ch, w->b, TM_TILE_BYTES, tensor_matmul_cell_after_forward);

    return tensor_matmul_cell_after_forward();
}

tpa_op_t tensor_matmul_cell_after_forward(void)
{
    struct tm_cell_ws *w = tpa_ws();
    struct tpa_chan *ch = tpa_chan(4);

    if (!w) {
        tm_fail();
        return tpa_stop();
    }

    w->round++;
    if (w->round < TM_ROUNDS)
        return tensor_matmul_cell_recv_a();

    if (!ch) {
        tm_fail();
        return tpa_stop();
    }

    return tpa_send(ch, w->acc, TM_TILE_BYTES, tensor_matmul_cell_done);
}

tpa_op_t tensor_matmul_cell_done(void)
{
    return tpa_stop();
}

tpa_op_t tensor_matmul_check_start(void)
{
    struct tm_check_ws *w = tpa_ws();
    struct tpa_chan *ch = tpa_chan(0);

    if (!w || !ch) {
        tm_fail();
        return tpa_stop();
    }

    return tpa_recv(ch, (void **)&w->tile, &w->tile_len, tensor_matmul_check_done);
}

tpa_op_t tensor_matmul_check_done(void)
{
    struct tm_check_ws *w = tpa_ws();

    if (!w || !w->tile || w->tile_len != TM_TILE_BYTES) {
        tm_fail();
        return tpa_stop();
    }

    for (uint32_t i = 0; i < TM_TILE_ELEMS; i++) {
        if (w->tile[i] != TM_EXPECT_VALUE) {
            tm_fail();
            return tpa_stop();
        }
    }

    if (tpa_hal_atomic_add_u32(&tm_done, 1u) + 1u == TM_NR_TILES &&
        !(tpa_hal_atomic_or_u32(&tm_failed, 0u) & 1u)) {
        tm_mark(TM_MARK_PASS);
        tm_mark(TM_TEST_PASS);
    }

    return tpa_stop();
}
