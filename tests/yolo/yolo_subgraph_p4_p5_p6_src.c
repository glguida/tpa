#include <stdint.h>

#include "test.h"
#include "tpa/tpa.h"

#include "generated/yolov5n_p4_p5_p6_case0.h"

tpa_op_t yolo_subgraph_src_p5_start(void);
tpa_op_t yolo_subgraph_src_p4_start(void);
tpa_op_t yolo_subgraph_src_p3_start(void);
tpa_op_t yolo_subgraph_src_done(void);

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

static tpa_op_t send_static(struct tpa_chan *ch, const int8_t *buf, uint32_t len)
{
    if (!ch) {
        diag_puts("FAIL_SRC_CH\n");
        TEST_FAIL;
        return tpa_stop();
    }

    return tpa_send(ch, (void *)buf, len, yolo_subgraph_src_done);
}

tpa_op_t yolo_subgraph_src_p5_start(void)
{
    diag_puts("SRC_P5\n");
    return send_static(tpa_chan(0),
                       yolov5n_p4_p5_p6_case0_p5_in,
                       sizeof(yolov5n_p4_p5_p6_case0_p5_in));
}

tpa_op_t yolo_subgraph_src_p4_start(void)
{
    diag_puts("SRC_P4\n");
    return send_static(tpa_chan(0),
                       yolov5n_p4_p5_p6_case0_p4_skip,
                       sizeof(yolov5n_p4_p5_p6_case0_p4_skip));
}

tpa_op_t yolo_subgraph_src_p3_start(void)
{
    diag_puts("SRC_P3\n");
    return send_static(tpa_chan(0),
                       yolov5n_p4_p5_p6_case0_p3_skip,
                       sizeof(yolov5n_p4_p5_p6_case0_p3_skip));
}

tpa_op_t yolo_subgraph_src_done(void)
{
    diag_puts("SRC_DONE\n");
    return tpa_stop();
}
