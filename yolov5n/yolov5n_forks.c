#include <stdint.h>
#include "tpa/tpa.h"

tpa_op_t yolo_full_fork_mid_p4_start(void);
tpa_op_t yolo_full_fork_mid_p4_send_dup(void);
tpa_op_t yolo_full_fork_mid_p4_done(void);

tpa_op_t yolo_full_fork_p3_start(void);
tpa_op_t yolo_full_fork_p3_send_dup(void);
tpa_op_t yolo_full_fork_p3_done(void);

tpa_op_t yolo_full_fork_p4_start(void);
tpa_op_t yolo_full_fork_p4_send_dup(void);
tpa_op_t yolo_full_fork_p4_done(void);

TPA_PROC_MEM_META(yolov5n_fork2_meta, 503u, 0u);

typedef struct {
    void *buf;
    uint32_t len;
} yolo_fork_ws_t;

static inline tpa_op_t fork_start(void **buf, uint32_t *len, tpa_cont_t cont)
{
    return tpa_recv(tpa_chan(0), buf, len, cont);
}

static inline tpa_op_t fork_send(uint32_t port, void *buf, uint32_t len,
                                 tpa_cont_t cont)
{
    return tpa_send(tpa_chan((uint16_t)port), buf, len, cont);
}

tpa_op_t yolo_full_fork_mid_p4_start(void)
{
    yolo_fork_ws_t *w = tpa_ws();

    return fork_start(&w->buf, &w->len, yolo_full_fork_mid_p4_send_dup);
}

tpa_op_t yolo_full_fork_mid_p4_send_dup(void)
{
    yolo_fork_ws_t *w = tpa_ws();

    return fork_send(1, w->buf, w->len, yolo_full_fork_mid_p4_done);
}

tpa_op_t yolo_full_fork_mid_p4_done(void)
{
    yolo_fork_ws_t *w = tpa_ws();

    return fork_send(2, w->buf, w->len, tpa_stop);
}

tpa_op_t yolo_full_fork_p3_start(void)
{
    yolo_fork_ws_t *w = tpa_ws();

    return fork_start(&w->buf, &w->len, yolo_full_fork_p3_send_dup);
}

tpa_op_t yolo_full_fork_p3_send_dup(void)
{
    yolo_fork_ws_t *w = tpa_ws();

    return fork_send(1, w->buf, w->len, yolo_full_fork_p3_done);
}

tpa_op_t yolo_full_fork_p3_done(void)
{
    yolo_fork_ws_t *w = tpa_ws();

    return fork_send(2, w->buf, w->len, tpa_stop);
}

tpa_op_t yolo_full_fork_p4_start(void)
{
    yolo_fork_ws_t *w = tpa_ws();

    return fork_start(&w->buf, &w->len, yolo_full_fork_p4_send_dup);
}

tpa_op_t yolo_full_fork_p4_send_dup(void)
{
    yolo_fork_ws_t *w = tpa_ws();

    return fork_send(1, w->buf, w->len, yolo_full_fork_p4_done);
}

tpa_op_t yolo_full_fork_p4_done(void)
{
    yolo_fork_ws_t *w = tpa_ws();

    return fork_send(2, w->buf, w->len, tpa_stop);
}
