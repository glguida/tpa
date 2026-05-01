#include <array>

#include <dnn_lib/Operators.h>
#include <dnn_lib/ElementBinaryInst.h>
#include <dnn_lib/LibTensor.h>

#include "tpa/tpa.h"

using dnn_lib::ElemKind;
using dnn_lib::LibTensor;
using dnn_lib::dim_t;

namespace {

constexpr uint32_t tile_h = 64;
constexpr uint32_t tile_w = 64;
constexpr uint32_t tile_elems = tile_h * tile_w;
constexpr uint64_t tile_bytes = tile_elems * sizeof(float);
constexpr uint64_t dnn_flags = 31;

constexpr uint64_t mark_begin = 0xd3310000ull;
constexpr uint64_t mark_value = 0xd3310001ull;
constexpr uint64_t mark_pass = 0xd33100ffull;
constexpr uint64_t mark_fail = 0xd33100eeull;

struct src_ws {
    float out[tile_elems];
};

struct add_ws {
    float *lhs;
    float *rhs;
    uint32_t lhs_len;
    uint32_t rhs_len;
    float out[tile_elems];
};

struct sink_ws {
    float *in;
    uint32_t in_len;
};

static std::array<dim_t, 2> tile_dims = {tile_h, tile_w};
static std::array<dim_t, 2> tile_strides = {tile_w, 1};

static inline void mark(uint64_t v)
{
    arch_trace((uint32_t)v);
}

static inline LibTensor make_tensor(float *buf)
{
    return LibTensor(ElemKind::FloatTy, buf, tile_dims, tile_strides, false);
}

static inline uint32_t self_minion(void)
{
    return et_hart_minion(arch_runtime_hartid());
}

static inline float expect_elem(uint32_t idx)
{
    return 6000.0f + 4.0f * (float)idx;
}

static inline uint32_t raw_bits(float v)
{
    union {
        float f;
        uint32_t u;
    } bits = { .f = v };

    return bits.u;
}

} /* namespace */

extern "C" tpa_op_t dnn_tree_src_start(void);
extern "C" tpa_op_t dnn_tree_src_done(void);
extern "C" tpa_op_t dnn_tree_add_recv_lhs(void);
extern "C" tpa_op_t dnn_tree_add_recv_rhs(void);
extern "C" tpa_op_t dnn_tree_add_send(void);
extern "C" tpa_op_t dnn_tree_add_done(void);
extern "C" tpa_op_t dnn_tree_sink_start(void);
extern "C" tpa_op_t dnn_tree_sink_done(void);

extern "C" tpa_op_t dnn_tree_src_start(void)
{
    auto *w = static_cast<src_ws *>(tpa_ws());
    struct tpa_chan *ch = tpa_chan(0);
    uint32_t src_id = self_minion();

    if (!w || !ch || src_id > 3) {
        mark(mark_fail);
        return tpa_stop();
    }

    if (src_id == 0)
        mark(mark_begin);

    for (uint32_t i = 0; i < tile_elems; i++)
        w->out[i] = 1000.0f * (float)src_id + (float)i;

    return tpa_send(ch, w->out, tile_bytes, dnn_tree_src_done);
}

extern "C" tpa_op_t dnn_tree_src_done(void)
{
    return tpa_stop();
}

extern "C" tpa_op_t dnn_tree_add_recv_lhs(void)
{
    auto *w = static_cast<add_ws *>(tpa_ws());
    struct tpa_chan *ch = tpa_chan(0);

    if (!w || !ch) {
        mark(mark_fail);
        return tpa_stop();
    }

    return tpa_recv(ch, (void **)&w->lhs, &w->lhs_len,
                    dnn_tree_add_recv_rhs);
}

extern "C" tpa_op_t dnn_tree_add_recv_rhs(void)
{
    auto *w = static_cast<add_ws *>(tpa_ws());
    struct tpa_chan *ch = tpa_chan(1);

    if (!w || !ch) {
        mark(mark_fail);
        return tpa_stop();
    }

    return tpa_recv(ch, (void **)&w->rhs, &w->rhs_len, dnn_tree_add_send);
}

extern "C" tpa_op_t dnn_tree_add_send(void)
{
    auto *w = static_cast<add_ws *>(tpa_ws());
    struct tpa_chan *ch = tpa_chan(2);
    uint32_t minion = self_minion();

    if (!w || !ch) {
        mark(mark_fail);
        return tpa_stop();
    }

    if (!w->lhs || !w->rhs || w->lhs_len != tile_bytes ||
        w->rhs_len != tile_bytes) {
        mark(mark_fail);
        return tpa_stop();
    }

    LibTensor out = make_tensor(w->out);
    LibTensor lhs = make_tensor(w->lhs);
    LibTensor rhs = make_tensor(w->rhs);

    dnn_lib::inlining::fwdLibElementAddInst<ElemKind::FloatTy>(
        &out, &lhs, &rhs, dnn_flags, minion, 1);

    return tpa_send(ch, w->out, tile_bytes, dnn_tree_add_done);
}

extern "C" tpa_op_t dnn_tree_add_done(void)
{
    return tpa_stop();
}

extern "C" tpa_op_t dnn_tree_sink_start(void)
{
    auto *w = static_cast<sink_ws *>(tpa_ws());
    struct tpa_chan *ch = tpa_chan(0);

    if (!w || !ch) {
        mark(mark_fail);
        return tpa_stop();
    }

    return tpa_recv(ch, (void **)&w->in, &w->in_len, dnn_tree_sink_done);
}

extern "C" tpa_op_t dnn_tree_sink_done(void)
{
    auto *w = static_cast<sink_ws *>(tpa_ws());

    if (!w) {
        mark(mark_fail);
        return tpa_stop();
    }

    if (!w->in || w->in_len != tile_bytes) {
        mark(mark_fail);
        return tpa_stop();
    }

    for (uint32_t i = 0; i < tile_elems; i++) {
        if (w->in[i] != expect_elem(i)) {
            mark(mark_fail);
            mark(i);
            return tpa_stop();
        }
    }

    mark(mark_value);
    mark(raw_bits(w->in[0]));
    mark(raw_bits(w->in[tile_elems - 1]));
    mark(mark_pass);
    return tpa_stop();
}
