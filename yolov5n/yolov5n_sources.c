#include <stdint.h>
#include "tpa/tpa.h"

#include "generated/yolov5n_p4_p5_p6_case0.h"

tpa_op_t yolo_subgraph_src_p5_start(void);
tpa_op_t yolo_subgraph_src_p4_start(void);
tpa_op_t yolo_subgraph_src_p3_start(void);
tpa_op_t yolov5n_src_input_start(void);
tpa_op_t yolo_subgraph_src_done(void);

TPA_PROC_MEM_META(yolov5n_src_skip_p5_meta, 500u, 0u);
TPA_PROC_MEM_META(yolov5n_src_skip_p4_meta, 501u, 0u);
TPA_PROC_MEM_META(yolov5n_src_skip_p3_meta, 502u, 0u);
TPA_PROC_MEM_META(yolov5n_src_input_meta, 514u, 0u);

static tpa_op_t send_static(const int8_t *buf, uint32_t len)
{
    return tpa_send_const(tpa_chan(0), buf, len, yolo_subgraph_src_done);
}

tpa_op_t yolo_subgraph_src_p5_start(void)
{
    return send_static(yolov5n_p4_p5_p6_case0_p5_in,
                       sizeof(yolov5n_p4_p5_p6_case0_p5_in));
}

tpa_op_t yolo_subgraph_src_p4_start(void)
{
    return send_static(yolov5n_p4_p5_p6_case0_p4_skip,
                       sizeof(yolov5n_p4_p5_p6_case0_p4_skip));
}

tpa_op_t yolo_subgraph_src_p3_start(void)
{
    return send_static(yolov5n_p4_p5_p6_case0_p3_skip,
                       sizeof(yolov5n_p4_p5_p6_case0_p3_skip));
}

tpa_op_t yolov5n_src_input_start(void)
{
    struct tpa_chan *ch = tpa_chan(0);
    int8_t *out = tpa_send_buf(ch);
    const uint32_t len = 640u * 640u * 3u;

    if (!out)
        __builtin_trap();

    return tpa_send_const(ch, out, len, yolo_subgraph_src_done);
}

tpa_op_t yolo_subgraph_src_done(void)
{
    return tpa_stop();
}
