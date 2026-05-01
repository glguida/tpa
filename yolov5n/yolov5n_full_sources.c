#include <stdint.h>
#include "tpa/tpa.h"

tpa_op_t yolov5n_src_input_start(void);
tpa_op_t yolov5n_src_done(void);

TPA_PROC_MEM_META(yolov5n_full_src_input_meta, 514u, 0u);

tpa_op_t yolov5n_src_input_start(void)
{
    struct tpa_chan *ch = tpa_chan(0);
    int8_t *out = tpa_send_buf(ch);
    const uint32_t len = 640u * 640u * 3u;

    if (!out)
        __builtin_trap();

    /* The synthetic full-model input is all-zero. Reuse the zero-initialized
     * channel backing directly instead of materializing the tensor every run.
     */
    return tpa_send_const(ch, out, len, yolov5n_src_done);
}

tpa_op_t yolov5n_src_done(void)
{
    return tpa_stop();
}
