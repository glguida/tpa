#include <cstdlib>
#include <cstring>

#include <etsoc/isa/cacheops.h>
#include <etsoc/isa/tensors.h>

#include "tpa/tpa.h"

namespace {

constexpr uint32_t nr_rows = 8;
constexpr uint32_t nr_cols = 8;
constexpr uint32_t nr_rounds = 4;
constexpr uint32_t tile_n = 128;
constexpr uint32_t tensor_n = 16;
constexpr uint32_t tensor_blocks = tile_n / tensor_n;
constexpr uint32_t tile_elems = tile_n * tile_n;
constexpr uint32_t tile_bytes = tile_elems * sizeof(float);
constexpr uint32_t tile_row_bytes = tile_n * sizeof(float);
constexpr uint32_t nr_tiles = nr_rows * nr_cols;
constexpr uint32_t nr_minions = ARCH_NR_MINIONS;
constexpr uint32_t nr_harts = ARCH_NR_HARTS;
static_assert((nr_rows % nr_minions) == 0,
              "row feeder placement must be uniform");
static_assert((nr_cols % nr_minions) == 0,
              "column feeder placement must be uniform");
static_assert((nr_tiles % nr_minions) == 0,
              "cell placement must be uniform");

constexpr uint32_t north_feeders_per_h0 = nr_cols / nr_minions;
constexpr uint32_t east_feeders_per_h0 = nr_rows / nr_minions;
constexpr uint32_t cells_per_h0 = nr_tiles / nr_minions;
constexpr uint32_t procs_per_h0 =
    north_feeders_per_h0 + east_feeders_per_h0 + 2 * cells_per_h0;
constexpr uint32_t evt_cap_per_hart = 2048;
constexpr uint64_t diag_putchar = 1ull << 56;
constexpr uint64_t diag_cycle = 7ull << 56;

constexpr float north_value = 1.0f;
constexpr float east_value = 2.0f;
constexpr float expect_value = (float)nr_rounds * (float)tile_n *
                               north_value * east_value;

constexpr uint64_t mark_begin = 0xd3510000ull;
constexpr uint64_t mark_sample = 0xd3510001ull;
constexpr uint64_t mark_pass = 0xd35100ffull;
constexpr uint64_t mark_fail = 0xd35100eeull;
constexpr uint64_t mark_test_pass = 0x1feed000ull;
constexpr uint64_t mark_test_fail = 0x50bad000ull;
constexpr uint64_t tensor_tl0_start = 0;
constexpr uint64_t tensor_tl1_start = 32;
constexpr uint64_t tensor_rows = tensor_n - 1;
constexpr uint64_t tensor_cols = tensor_n - 1;
constexpr uint64_t tensor_bcols = (tensor_n / 4) - 1;
constexpr uint64_t tensor_fp32 = 0;

enum {
    evt_runtime = 1,
    evt_compute = 2,
};

struct feed_ws {
    alignas(64) float tile[nr_rounds][tile_elems];
    uint32_t round;
};

struct cell_ws {
    float *north;
    float *east;
    uint32_t north_len;
    uint32_t east_len;
    alignas(64) float acc[tile_elems];
    uint32_t round;
};

struct check_ws {
    float *tile;
    uint32_t tile_len;
};

static_assert(sizeof(feed_ws) <= 262208, "feed_ws manifest too small");
static_assert(sizeof(cell_ws) <= 65664, "cell_ws manifest too small");
static_assert(sizeof(check_ws) <= 64, "check_ws manifest too small");

struct demo_evt {
    uint32_t beg;
    uint32_t end;
    uint16_t kind;
    uint16_t pad;
};

struct demo_iv {
    uint32_t beg;
    uint32_t end;
};

static volatile uint32_t demo_started __attribute__((aligned(64)));
static volatile uint32_t demo_sampled __attribute__((aligned(64)));
static volatile uint32_t demo_failed __attribute__((aligned(64)));
static volatile uint32_t demo_done __attribute__((aligned(64)));
static volatile uint32_t hart_done[nr_harts] __attribute__((aligned(64)));
static volatile uint32_t tensor_ready[nr_harts] __attribute__((aligned(64)));
static uint32_t rt_beg[nr_harts] __attribute__((aligned(64)));
static uint32_t compute_beg[nr_harts] __attribute__((aligned(64)));
static uint32_t evt_n[nr_harts] __attribute__((aligned(64)));
static demo_evt evtv[nr_harts][evt_cap_per_hart] __attribute__((aligned(64)));
static uint64_t start_cycle __attribute__((aligned(64)));
static uint64_t end_cycle __attribute__((aligned(64)));
static demo_iv compute_iv[nr_harts * evt_cap_per_hart] __attribute__((aligned(64)));
static demo_iv runtime_iv[nr_harts * evt_cap_per_hart] __attribute__((aligned(64)));

static inline void mark(uint64_t v)
{
    arch_trace((uint32_t)v);
}

static inline void diag_putc(char c)
{
    uint64_t v = diag_putchar | (uint8_t)c;

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
    uint32_t i = 0;

    if (!v) {
        diag_putc('0');
        return;
    }

    while (v) {
        buf[i++] = '0' + (char)(v % 10ull);
        v /= 10ull;
    }

    while (i)
        diag_putc(buf[--i]);
}

static void diag_line(const char *name, uint64_t v)
{
    diag_puts(name);
    diag_putc('=');
    diag_putu64(v);
    diag_putc('\n');
}

static inline uint64_t emu_cycle(void)
{
    uint64_t v;

    asm volatile(
        "csrw validation1, %1\n\t"
        "csrr %0, validation1\n"
        : "=r"(v)
        : "r"(diag_cycle)
        : "memory");

    return v;
}

static inline uint32_t self_minion(void)
{
    return et_hart_minion(arch_runtime_hartid());
}

static inline uint32_t raw_bits(float v)
{
    union {
        float f;
        uint32_t u;
    } bits = { .f = v };

    return bits.u;
}

static void fill_tile(float *tile, float value)
{
    for (uint32_t i = 0; i < tile_elems; i++)
        tile[i] = value;
}

static void clear_tile(float *tile)
{
    for (uint32_t i = 0; i < tile_elems; i++)
        tile[i] = 0.0f;
}

static inline void clear_tensor_error(void)
{
    asm volatile("csrwi tensor_error, 0" : : : "memory");
}

static inline float *tile_block(float *tile, uint32_t brow, uint32_t bcol)
{
    return tile + brow * tensor_n * tile_n + bcol * tensor_n;
}

static inline const float *tile_block(const float *tile, uint32_t brow,
                                      uint32_t bcol)
{
    return tile + brow * tensor_n * tile_n + bcol * tensor_n;
}

static void enable_tensor_scratchpad(void)
{
    uint32_t hart = arch_runtime_hartid();
    uint64_t pmask = 0xff;

    if (tensor_ready[hart])
        return;

    excl_mode(1);
    et_cache_evict_l1d_to_l2();
    asm volatile("fence rw, rw");
    mcache_control(1, 0, 0, 0);
    mcache_control(1, 1, 0, 0);
    asm volatile("csrwi tensor_mask, 0\n"
                 "csrwi tensor_coop, 0\n"
                 "mova.m.x %0\n"
                 :
                 : "r"(pmask)
                 : "memory");
    clear_tensor_error();
    excl_mode(0);
    tensor_ready[hart] = 1;
}

static inline void load_rf_16x16(const float *base)
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
        : [s] "r"((uint64_t)tile_row_bytes)
        : "memory", "f0", "f1", "f2", "f3", "f4", "f5", "f6", "f7",
          "f8", "f9", "f10", "f11", "f12", "f13", "f14", "f15",
          "f16", "f17", "f18", "f19", "f20", "f21", "f22", "f23",
          "f24", "f25", "f26", "f27", "f28", "f29", "f30", "f31");
}

static inline void store_rf_16x16(float *base)
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
        : [s] "r"((uint64_t)tile_row_bytes)
        : "memory");
}

static int tensor_matmul_acc(float *dst, const float *lhs, const float *rhs)
{
    clear_tensor_error();

    for (uint32_t brow = 0; brow < tensor_blocks; brow++) {
        for (uint32_t bcol = 0; bcol < tensor_blocks; bcol++) {
            load_rf_16x16(tile_block(dst, brow, bcol));

            for (uint32_t bk = 0; bk < tensor_blocks; bk++) {
                const float *ab = tile_block(lhs, brow, bk);
                const float *bb = tile_block(rhs, bk, bcol);

                tensor_load(0, 0, tensor_tl0_start, 0, 0, (uint64_t)ab, 0,
                            tensor_rows, tile_row_bytes, 0);
                tensor_load(0, 0, tensor_tl1_start, 0, 1, (uint64_t)bb, 0,
                            tensor_rows, tile_row_bytes, 1);
                tensor_wait(TENSOR_LOAD_WAIT_0);
                tensor_fma(0, tensor_bcols, tensor_rows, tensor_cols, 0, 0, 0,
                           0, 1, tensor_tl1_start, tensor_tl0_start,
                           tensor_fp32, 0);
                tensor_wait(TENSOR_FMA_WAIT);
            }

            store_rf_16x16(tile_block(dst, brow, bcol));
        }
    }

    return get_tensor_error() == 0 ? 0 : -1;
}

static void maybe_mark_begin(void)
{
    if (!arch_atomic_exchange_u32(&demo_started, 1)) {
        start_cycle = emu_cycle();
        mark(mark_begin);
    }
}

static void maybe_mark_sample(float first, float last)
{
    if (arch_atomic_exchange_u32(&demo_sampled, 1))
        return;

    mark(mark_sample);
    mark(raw_bits(first));
    mark(raw_bits(last));
}

static tpa_op_t fail_stop(uint32_t idx, float got);

static void trace_append(uint32_t kind, uint32_t beg, uint32_t end)
{
    uint32_t hart = arch_runtime_hartid();
    uint32_t idx = evt_n[hart];

    if (idx >= evt_cap_per_hart) {
        fail_stop(0xff00u + hart, (float)idx);
        return;
    }

    evtv[hart][idx].beg = beg;
    evtv[hart][idx].end = end;
    evtv[hart][idx].kind = (uint16_t)kind;
    evt_n[hart] = idx + 1;
}

static void trace_runtime_beg(void)
{
    rt_beg[arch_runtime_hartid()] = (uint32_t)emu_cycle();
}

static void trace_runtime_end(void)
{
    uint32_t hart = arch_runtime_hartid();
    uint32_t beg = rt_beg[hart];
    uint32_t end = (uint32_t)emu_cycle();

    if (beg)
        trace_append(evt_runtime, beg, end);
    rt_beg[hart] = 0;
}

static void trace_compute_beg(void)
{
    compute_beg[arch_runtime_hartid()] = (uint32_t)emu_cycle();
}

static void trace_compute_end(void)
{
    uint32_t hart = arch_runtime_hartid();
    uint32_t beg = compute_beg[hart];
    uint32_t end = (uint32_t)emu_cycle();

    if (beg)
        trace_append(evt_compute, beg, end);
    compute_beg[hart] = 0;
}

extern "C" void tpa_trace(uint32_t tag)
{
    switch (tag) {
    case TPA_TRACE_RT_SEND_BEG:
    case TPA_TRACE_RT_RECV_BEG:
    case TPA_TRACE_RT_SEND_RESUME_BEG:
    case TPA_TRACE_RT_RECV_RESUME_BEG:
        trace_runtime_beg();
        break;
    case TPA_TRACE_RT_SEND_END:
    case TPA_TRACE_RT_RECV_END:
    case TPA_TRACE_RT_SEND_RESUME_END:
    case TPA_TRACE_RT_RECV_RESUME_END:
        trace_runtime_end();
        break;
    default:
        break;
    }
}

static int iv_cmp(const void *a, const void *b)
{
    const demo_iv *ia = static_cast<const demo_iv *>(a);
    const demo_iv *ib = static_cast<const demo_iv *>(b);

    if (ia->beg < ib->beg)
        return -1;
    if (ia->beg > ib->beg)
        return 1;
    if (ia->end < ib->end)
        return -1;
    if (ia->end > ib->end)
        return 1;
    return 0;
}

static uint32_t merge_iv(demo_iv *iv, uint32_t n, uint64_t *cover)
{
    uint32_t out = 0;
    uint64_t sum = 0;

    if (!n) {
        *cover = 0;
        return 0;
    }

    std::qsort(iv, n, sizeof(iv[0]), iv_cmp);
    iv[out++] = iv[0];

    for (uint32_t i = 1; i < n; i++) {
        if (iv[i].beg <= iv[out - 1].end) {
            if (iv[i].end > iv[out - 1].end)
                iv[out - 1].end = iv[i].end;
            continue;
        }

        iv[out++] = iv[i];
    }

    for (uint32_t i = 0; i < out; i++)
        sum += (uint64_t)(iv[i].end - iv[i].beg);

    *cover = sum;
    return out;
}

static uint64_t iv_overlap(const demo_iv *a, uint32_t na,
                           const demo_iv *b, uint32_t nb)
{
    uint32_t ia = 0;
    uint32_t ib = 0;
    uint64_t sum = 0;

    while (ia < na && ib < nb) {
        uint32_t beg = a[ia].beg > b[ib].beg ? a[ia].beg : b[ib].beg;
        uint32_t end = a[ia].end < b[ib].end ? a[ia].end : b[ib].end;

        if (beg < end)
            sum += (uint64_t)(end - beg);

        if (a[ia].end <= b[ib].end)
            ia++;
        else
            ib++;
    }

    return sum;
}

static void flush_hart_trace(void)
{
    uint32_t hart = arch_runtime_hartid();
    uint32_t lines =
        (uint32_t)(((evt_n[hart] * sizeof(demo_evt)) + 63u) >> 6);

    for (uint32_t i = 0; i < lines; i++)
        et_cache_flush_line((uint64_t)&evtv[hart][0] + (uint64_t)i * 64ull);

    et_cache_flush_line((uint64_t)&evt_n[hart]);
    et_cache_flush_line((uint64_t)&hart_done[hart]);
    asm volatile("fence rw, rw");
}

static void demo_proc_done(void)
{
    uint32_t hart = arch_runtime_hartid();

    if (et_hart_hi(hart) ||
        arch_atomic_add_u32(&hart_done[hart], 1) + 1 != procs_per_h0)
        return;

    flush_hart_trace();
}

static void print_summary(void)
{
    uint32_t compute_n = 0;
    uint32_t runtime_n = 0;
    uint64_t compute_work = 0;
    uint64_t runtime_work = 0;
    uint64_t compute_cover;
    uint64_t runtime_cover;
    uint64_t overlap;
    uint64_t total;
    uint64_t compute_only;
    uint64_t runtime_only;
    uint64_t other;

    for (uint32_t hart = 0; hart < nr_harts; hart++) {
        uint32_t n;
        uint32_t lines;

        et_cache_evict_line((uint64_t)&evt_n[hart]);
        n = evt_n[hart];
        lines = (uint32_t)(((n * sizeof(demo_evt)) + 63u) >> 6);

        for (uint32_t i = 0; i < lines; i++)
            et_cache_evict_line((uint64_t)&evtv[hart][0] + (uint64_t)i * 64ull);

        for (uint32_t i = 0; i < n; i++) {
            const demo_evt *e = &evtv[hart][i];
            uint64_t dt = (uint64_t)(e->end - e->beg);

            if (e->kind == evt_compute) {
                compute_iv[compute_n++] = { e->beg, e->end };
                compute_work += dt;
            } else if (e->kind == evt_runtime) {
                runtime_iv[runtime_n++] = { e->beg, e->end };
                runtime_work += dt;
            }
        }
    }

    compute_n = merge_iv(compute_iv, compute_n, &compute_cover);
    runtime_n = merge_iv(runtime_iv, runtime_n, &runtime_cover);
    overlap = iv_overlap(compute_iv, compute_n, runtime_iv, runtime_n);
    total = end_cycle - start_cycle;
    compute_only = compute_cover - overlap;
    runtime_only = runtime_cover - overlap;
    other = total - compute_only - runtime_only - overlap;

    diag_line("TOTAL", total);
    diag_line("COMPUTE_WORK", compute_work);
    diag_line("RUNTIME_WORK", runtime_work);
    diag_line("COMPUTE_COVER", compute_cover);
    diag_line("RUNTIME_COVER", runtime_cover);
    diag_line("OVERLAP", overlap);
    diag_line("COMPUTE_ONLY", compute_only);
    diag_line("RUNTIME_ONLY", runtime_only);
    diag_line("OTHER", other);

    for (uint32_t hart = 0; hart < nr_harts; hart += 2) {
        uint32_t n = evt_n[hart];
        uint32_t hc_n = 0;
        uint32_t hr_n = 0;
        uint64_t hc_work = 0;
        uint64_t hr_work = 0;
        uint64_t hc_cover;
        uint64_t hr_cover;
        uint64_t hov;
        uint64_t hco;
        uint64_t hro;
        uint64_t hother;
        uint32_t minion = et_hart_minion(hart);

        for (uint32_t i = 0; i < n; i++) {
            const demo_evt *e = &evtv[hart][i];
            uint64_t dt = (uint64_t)(e->end - e->beg);

            if (e->kind == evt_compute) {
                compute_iv[hc_n++] = { e->beg, e->end };
                hc_work += dt;
            } else if (e->kind == evt_runtime) {
                runtime_iv[hr_n++] = { e->beg, e->end };
                hr_work += dt;
            }
        }

        hc_n = merge_iv(compute_iv, hc_n, &hc_cover);
        hr_n = merge_iv(runtime_iv, hr_n, &hr_cover);
        hov = iv_overlap(compute_iv, hc_n, runtime_iv, hr_n);
        hco = hc_cover - hov;
        hro = hr_cover - hov;
        hother = total - hco - hro - hov;

        diag_putc('M');
        diag_putu64(minion);
        diag_puts("_COMPUTE_WORK=");
        diag_putu64(hc_work);
        diag_putc('\n');

        diag_putc('M');
        diag_putu64(minion);
        diag_puts("_RUNTIME_WORK=");
        diag_putu64(hr_work);
        diag_putc('\n');

        diag_putc('M');
        diag_putu64(minion);
        diag_puts("_COMPUTE_COVER=");
        diag_putu64(hc_cover);
        diag_putc('\n');

        diag_putc('M');
        diag_putu64(minion);
        diag_puts("_RUNTIME_COVER=");
        diag_putu64(hr_cover);
        diag_putc('\n');

        diag_putc('M');
        diag_putu64(minion);
        diag_puts("_OVERLAP=");
        diag_putu64(hov);
        diag_putc('\n');

        diag_putc('M');
        diag_putu64(minion);
        diag_puts("_COMPUTE_ONLY=");
        diag_putu64(hco);
        diag_putc('\n');

        diag_putc('M');
        diag_putu64(minion);
        diag_puts("_RUNTIME_ONLY=");
        diag_putu64(hro);
        diag_putc('\n');

        diag_putc('M');
        diag_putu64(minion);
        diag_puts("_OTHER=");
        diag_putu64(hother);
        diag_putc('\n');
    }
}

static tpa_op_t fail_stop(uint32_t idx, float got)
{
    if (!arch_atomic_exchange_u32(&demo_failed, 1)) {
        mark(mark_fail);
        mark(idx);
        mark(raw_bits(got));
        mark(mark_test_fail);
    }

    return tpa_stop();
}

} /* namespace */

extern "C" tpa_op_t systolic_north_feed_start(void);
extern "C" tpa_op_t systolic_north_feed_done(void);
extern "C" tpa_op_t systolic_east_feed_start(void);
extern "C" tpa_op_t systolic_east_feed_done(void);
extern "C" tpa_op_t systolic_cell_init(void);
extern "C" tpa_op_t systolic_cell_recv_north(void);
extern "C" tpa_op_t systolic_cell_recv_east(void);
extern "C" tpa_op_t systolic_cell_compute(void);
extern "C" tpa_op_t systolic_cell_send_west(void);
extern "C" tpa_op_t systolic_cell_after_forward(void);
extern "C" tpa_op_t systolic_cell_done(void);
extern "C" tpa_op_t systolic_check_start(void);
extern "C" tpa_op_t systolic_check_done(void);

extern "C" tpa_op_t systolic_north_feed_start(void)
{
    auto *w = static_cast<feed_ws *>(tpa_ws());
    struct tpa_chan *ch = tpa_chan(0);

    if (!w || !ch)
        return fail_stop(0, -1.0f);

    fill_tile(w->tile[w->round], north_value);

    maybe_mark_begin();
    return tpa_send(ch, w->tile[w->round], tile_bytes,
                    systolic_north_feed_done);
}

extern "C" tpa_op_t systolic_north_feed_done(void)
{
    auto *w = static_cast<feed_ws *>(tpa_ws());

    if (!w)
        return fail_stop(1, -1.0f);

    w->round++;
    if (w->round >= nr_rounds) {
        demo_proc_done();
        return tpa_stop();
    }

    return systolic_north_feed_start();
}

extern "C" tpa_op_t systolic_east_feed_start(void)
{
    auto *w = static_cast<feed_ws *>(tpa_ws());
    struct tpa_chan *ch = tpa_chan(0);

    if (!w || !ch)
        return fail_stop(2, -1.0f);

    fill_tile(w->tile[w->round], east_value);

    maybe_mark_begin();
    return tpa_send(ch, w->tile[w->round], tile_bytes,
                    systolic_east_feed_done);
}

extern "C" tpa_op_t systolic_east_feed_done(void)
{
    auto *w = static_cast<feed_ws *>(tpa_ws());

    if (!w)
        return fail_stop(3, -1.0f);

    w->round++;
    if (w->round >= nr_rounds) {
        demo_proc_done();
        return tpa_stop();
    }

    return systolic_east_feed_start();
}

extern "C" tpa_op_t systolic_cell_init(void)
{
    auto *w = static_cast<cell_ws *>(tpa_ws());

    if (!w)
        return fail_stop(4, -1.0f);

    enable_tensor_scratchpad();
    w->round = 0;
    return systolic_cell_recv_north();
}

extern "C" tpa_op_t systolic_cell_recv_north(void)
{
    auto *w = static_cast<cell_ws *>(tpa_ws());
    struct tpa_chan *ch = tpa_chan(0);

    if (!w || !ch)
        return fail_stop(5, -1.0f);

    return tpa_recv(ch, (void **)&w->north, &w->north_len,
                    systolic_cell_recv_east);
}

extern "C" tpa_op_t systolic_cell_recv_east(void)
{
    auto *w = static_cast<cell_ws *>(tpa_ws());
    struct tpa_chan *ch = tpa_chan(1);

    if (!w || !ch)
        return fail_stop(6, -1.0f);

    return tpa_recv(ch, (void **)&w->east, &w->east_len,
                    systolic_cell_compute);
}

extern "C" tpa_op_t systolic_cell_compute(void)
{
    auto *w = static_cast<cell_ws *>(tpa_ws());
    struct tpa_chan *south = tpa_chan(2);

    if (!w)
        return fail_stop(7, -1.0f);

    if (!w->north || !w->east || w->north_len != tile_bytes ||
        w->east_len != tile_bytes) {
        return fail_stop(7, -1.0f);
    }

    trace_compute_beg();
    if (tensor_matmul_acc(w->acc, w->north, w->east))
        return fail_stop(7, (float)get_tensor_error());
    trace_compute_end();

    if (south)
        return tpa_send(south, w->north, tile_bytes, systolic_cell_send_west);

    return systolic_cell_send_west();
}

extern "C" tpa_op_t systolic_cell_send_west(void)
{
    auto *w = static_cast<cell_ws *>(tpa_ws());
    struct tpa_chan *west = tpa_chan(3);

    if (!w)
        return fail_stop(8, -1.0f);

    if (west)
        return tpa_send(west, w->east, tile_bytes, systolic_cell_after_forward);

    return systolic_cell_after_forward();
}

extern "C" tpa_op_t systolic_cell_after_forward(void)
{
    auto *w = static_cast<cell_ws *>(tpa_ws());
    struct tpa_chan *check = tpa_chan(4);

    if (!w)
        return fail_stop(9, -1.0f);

    w->round++;
    if (w->round < nr_rounds)
        return systolic_cell_recv_north();

    if (!check)
        return fail_stop(10, -1.0f);

    return tpa_send(check, w->acc, tile_bytes, systolic_cell_done);
}

extern "C" tpa_op_t systolic_cell_done(void)
{
    demo_proc_done();
    return tpa_stop();
}

extern "C" tpa_op_t systolic_check_start(void)
{
    auto *w = static_cast<check_ws *>(tpa_ws());
    struct tpa_chan *ch = tpa_chan(0);

    if (!w || !ch)
        return fail_stop(11, -1.0f);

    return tpa_recv(ch, (void **)&w->tile, &w->tile_len,
                    systolic_check_done);
}

extern "C" tpa_op_t systolic_check_done(void)
{
    auto *w = static_cast<check_ws *>(tpa_ws());

    if (!w)
        return fail_stop(12, -1.0f);

    if (!w->tile || w->tile_len != tile_bytes)
        return fail_stop(12, -1.0f);

    for (uint32_t i = 0; i < tile_elems; i++) {
        if (w->tile[i] != expect_value)
            return fail_stop(0x1000u + i, w->tile[i]);
    }

    maybe_mark_sample(w->tile[0], w->tile[tile_elems - 1]);

    demo_proc_done();

    if (arch_atomic_add_u32(&demo_done, 1) + 1 == nr_tiles &&
        !(arch_atomic_or_u32(&demo_failed, 0) & 1)) {
        end_cycle = emu_cycle();
        print_summary();
        mark(mark_pass);
        mark(mark_test_pass);
    }

    return tpa_stop();
}
