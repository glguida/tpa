#include "yolov8n_p5_common.h"

#ifdef YV8N_P5_SCRATCH_CONFIG_HEADER
#include YV8N_P5_SCRATCH_CONFIG_HEADER
#endif

#ifndef YV5N_ARENA_STORAGE_BYTES
#define YV5N_ARENA_STORAGE_BYTES (8u * 64u)
#define YV5N_ARENA_STATE_INITIALIZERS                                      \
    [0] = { .base = yolov5n_arena_storage + 0u, .cap = 64u },              \
    [2] = { .base = yolov5n_arena_storage + 64u, .cap = 64u },             \
    [4] = { .base = yolov5n_arena_storage + 128u, .cap = 64u },            \
    [6] = { .base = yolov5n_arena_storage + 192u, .cap = 64u },            \
    [8] = { .base = yolov5n_arena_storage + 256u, .cap = 64u },            \
    [10] = { .base = yolov5n_arena_storage + 320u, .cap = 64u },           \
    [12] = { .base = yolov5n_arena_storage + 384u, .cap = 64u },           \
    [14] = { .base = yolov5n_arena_storage + 448u, .cap = 64u },
#endif

struct yolov8n_p5_arena_state {
    uint8_t *base;
    uint32_t cap;
    uint32_t brk;
    uint32_t hi;
    uint32_t declared_cap;
};

/* The current mapper emits YV5N_ARENA_* initializers that name this storage
 * symbol. Keep that compatibility localized here while the YOLOv8n process code
 * uses the yolov8n_p5_arena_* API above. */
static uint8_t yolov5n_arena_storage[YV5N_ARENA_STORAGE_BYTES]
    __attribute__((aligned(64)));

static struct yolov8n_p5_arena_state yolov8n_p5_arena[ARCH_NR_HARTS] = {
    YV5N_ARENA_STATE_INITIALIZERS
};

static struct yolov8n_p5_arena_state *arena_self(void)
{
    uint32_t hartid = arch_runtime_hartid();

    if (hartid >= ARCH_NR_HARTS)
        return 0;

    return &yolov8n_p5_arena[hartid];
}

static void arena_fail(void)
{
    __builtin_trap();
}

void yolov8n_p5_arena_begin(uint32_t declared_peak_bytes)
{
    struct yolov8n_p5_arena_state *st = arena_self();

    if (!st || !st->base || !st->cap)
        arena_fail();

    st->brk = 0;
    st->hi = 0;
    st->declared_cap = declared_peak_bytes ? declared_peak_bytes : st->cap;

    if (st->declared_cap > st->cap)
        arena_fail();
}

void *yolov8n_p5_arena_alloc(size_t bytes, size_t align)
{
    struct yolov8n_p5_arena_state *st = arena_self();
    uint32_t limit;
    uint32_t off;
    uint32_t end;
    uintptr_t ptr;

    if (!st || !st->base || !st->cap)
        arena_fail();
    if (align == 0u)
        align = 1u;

    limit = st->declared_cap ? st->declared_cap : st->cap;
    off = (st->brk + (uint32_t)align - 1u) & ~((uint32_t)align - 1u);
    end = off + (uint32_t)bytes;
    if (end < off || end > limit || end > st->cap)
        arena_fail();

    ptr = (uintptr_t)st->base + off;
    st->brk = end;
    if (end > st->hi)
        st->hi = end;

    return (void *)ptr;
}

uint32_t yolov8n_p5_arena_high_water(void)
{
    struct yolov8n_p5_arena_state *st = arena_self();

    if (!st)
        return 0u;

    return st->hi;
}
