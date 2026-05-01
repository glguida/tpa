#include <stdint.h>
#include <string.h>

#include "test.h"
#include "tpa/tpa.h"
#include "yolov5n_common.h"
#include "yolov5n_demo.h"

typedef struct {
    const int8_t *box;
    uint32_t box_len;
    const int8_t *cls;
    uint32_t cls_len;
} yv5n_demo_head_ws_t;

typedef struct {
    yv5n_demo_head_ws_t p6;
    yv5n_demo_head_ws_t p8;
    yv5n_demo_head_ws_t p10;
} yv5n_demo_sink_ws_t;

volatile yv5n_demo_result_t yolov5n_demo_result __attribute__((aligned(64)));

tpa_op_t yolov5n_demo_sink_start(void);
tpa_op_t yolov5n_demo_sink_recv_p10_cls(void);
tpa_op_t yolov5n_demo_sink_recv_p8_box(void);
tpa_op_t yolov5n_demo_sink_recv_p8_cls(void);
tpa_op_t yolov5n_demo_sink_recv_p6_box(void);
tpa_op_t yolov5n_demo_sink_recv_p6_cls(void);
tpa_op_t yolov5n_demo_sink_done(void);

TPA_PROC_MEM_META(yolov5n_demo_sink_meta, 519u, 0u);

static yv5n_demo_tensor_ref_t make_ref(const void *ptr, uint32_t len,
                                       uint16_t h, uint16_t w, uint16_t c,
                                       uint16_t stride, float scale)
{
    yv5n_demo_tensor_ref_t ref = {
        .addr = (uint64_t)(uintptr_t)ptr,
        .len = len,
        .h = h,
        .w = w,
        .c = c,
        .stride = stride,
        .scale = scale,
        ._pad0 = 0u,
    };

    return ref;
}

tpa_op_t yolov5n_demo_sink_start(void)
{
    yv5n_demo_sink_ws_t *w = tpa_ws();

    memset((void *)&yolov5n_demo_result, 0, sizeof(yolov5n_demo_result));

    if (!w)
        YV5N_FAIL_MSG_STOP("SINK:WS\n");

    return tpa_recv(tpa_chan(4), (void **)&w->p10.box, &w->p10.box_len,
                    yolov5n_demo_sink_recv_p10_cls);
}

tpa_op_t yolov5n_demo_sink_recv_p10_cls(void)
{
    yv5n_demo_sink_ws_t *w = tpa_ws();

    if (!w)
        YV5N_FAIL_MSG_STOP("SINK:W10C\n");

    return tpa_recv(tpa_chan(5), (void **)&w->p10.cls, &w->p10.cls_len,
                    yolov5n_demo_sink_recv_p8_box);
}

tpa_op_t yolov5n_demo_sink_recv_p8_box(void)
{
    yv5n_demo_sink_ws_t *w = tpa_ws();

    if (!w)
        YV5N_FAIL_MSG_STOP("SINK:W8B\n");

    return tpa_recv(tpa_chan(2), (void **)&w->p8.box, &w->p8.box_len,
                    yolov5n_demo_sink_recv_p8_cls);
}

tpa_op_t yolov5n_demo_sink_recv_p8_cls(void)
{
    yv5n_demo_sink_ws_t *w = tpa_ws();

    if (!w)
        YV5N_FAIL_MSG_STOP("SINK:W8C\n");

    return tpa_recv(tpa_chan(3), (void **)&w->p8.cls, &w->p8.cls_len,
                    yolov5n_demo_sink_recv_p6_box);
}

tpa_op_t yolov5n_demo_sink_recv_p6_box(void)
{
    yv5n_demo_sink_ws_t *w = tpa_ws();

    if (!w)
        YV5N_FAIL_MSG_STOP("SINK:W6B\n");

    return tpa_recv(tpa_chan(0), (void **)&w->p6.box, &w->p6.box_len,
                    yolov5n_demo_sink_recv_p6_cls);
}

tpa_op_t yolov5n_demo_sink_recv_p6_cls(void)
{
    yv5n_demo_sink_ws_t *w = tpa_ws();

    if (!w)
        YV5N_FAIL_MSG_STOP("SINK:W6C\n");

    return tpa_recv(tpa_chan(1), (void **)&w->p6.cls, &w->p6.cls_len,
                    yolov5n_demo_sink_done);
}

tpa_op_t yolov5n_demo_sink_done(void)
{
    yv5n_demo_sink_ws_t *w = tpa_ws();

    if (!w)
        YV5N_FAIL_MSG_STOP("SINK:WD\n");
    if (w->p6.box_len != YV5N_DEMO_P6_BOX_BYTES)
        YV5N_FAIL_MSG_STOP("SINK:P6B\n");
    if (w->p6.cls_len != YV5N_DEMO_P6_CLS_BYTES)
        YV5N_FAIL_MSG_STOP("SINK:P6C\n");
    if (w->p8.box_len != YV5N_DEMO_P8_BOX_BYTES)
        YV5N_FAIL_MSG_STOP("SINK:P8B\n");
    if (w->p8.cls_len != YV5N_DEMO_P8_CLS_BYTES)
        YV5N_FAIL_MSG_STOP("SINK:P8C\n");
    if (w->p10.box_len != YV5N_DEMO_P10_BOX_BYTES)
        YV5N_FAIL_MSG_STOP("SINK:P10B\n");
    if (w->p10.cls_len != YV5N_DEMO_P10_CLS_BYTES)
        YV5N_FAIL_MSG_STOP("SINK:P10C\n");

    yolov5n_demo_result.p6_box = make_ref(
        w->p6.box, w->p6.box_len, YV5N_DEMO_P6_H, YV5N_DEMO_P6_W,
        YV5N_DEMO_BOX_C, YV5N_DEMO_P6_STRIDE, YV5N_DEMO_P6_BOX_SCALE);
    yolov5n_demo_result.p6_cls = make_ref(
        w->p6.cls, w->p6.cls_len, YV5N_DEMO_P6_H, YV5N_DEMO_P6_W,
        YV5N_DEMO_CLS_C, YV5N_DEMO_P6_STRIDE, YV5N_DEMO_P6_CLS_SCALE);
    yolov5n_demo_result.p8_box = make_ref(
        w->p8.box, w->p8.box_len, YV5N_DEMO_P8_H, YV5N_DEMO_P8_W,
        YV5N_DEMO_BOX_C, YV5N_DEMO_P8_STRIDE, YV5N_DEMO_P8_BOX_SCALE);
    yolov5n_demo_result.p8_cls = make_ref(
        w->p8.cls, w->p8.cls_len, YV5N_DEMO_P8_H, YV5N_DEMO_P8_W,
        YV5N_DEMO_CLS_C, YV5N_DEMO_P8_STRIDE, YV5N_DEMO_P8_CLS_SCALE);
    yolov5n_demo_result.p10_box = make_ref(
        w->p10.box, w->p10.box_len, YV5N_DEMO_P10_H, YV5N_DEMO_P10_W,
        YV5N_DEMO_BOX_C, YV5N_DEMO_P10_STRIDE, YV5N_DEMO_P10_BOX_SCALE);
    yolov5n_demo_result.p10_cls = make_ref(
        w->p10.cls, w->p10.cls_len, YV5N_DEMO_P10_H, YV5N_DEMO_P10_W,
        YV5N_DEMO_CLS_C, YV5N_DEMO_P10_STRIDE, YV5N_DEMO_P10_CLS_SCALE);
    yolov5n_demo_result.magic = YV5N_DEMO_RESULT_MAGIC;
    yolov5n_demo_result.ready = 1u;
    TEST_PASS;
    return tpa_stop();
}
