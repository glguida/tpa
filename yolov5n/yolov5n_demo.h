#ifndef YOLOV5N_DEMO_H
#define YOLOV5N_DEMO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define YV5N_DEMO_RESULT_MAGIC UINT32_C(0x59444d4f)
#define YV5N_DEMO_INPUT_BYTES  (640u * 640u * 3u)

#define YV5N_DEMO_P6_H 80u
#define YV5N_DEMO_P6_W 80u
#define YV5N_DEMO_P8_H 40u
#define YV5N_DEMO_P8_W 40u
#define YV5N_DEMO_P10_H 20u
#define YV5N_DEMO_P10_W 20u

#define YV5N_DEMO_BOX_C 64u
#define YV5N_DEMO_CLS_C 80u

#define YV5N_DEMO_P6_STRIDE 8u
#define YV5N_DEMO_P8_STRIDE 16u
#define YV5N_DEMO_P10_STRIDE 32u

#define YV5N_DEMO_P6_BOX_SCALE  9.94826565e-02f
#define YV5N_DEMO_P6_CLS_SCALE  1.47422490e-01f
#define YV5N_DEMO_P8_BOX_SCALE  7.34780830e-02f
#define YV5N_DEMO_P8_CLS_SCALE  1.36029221e-01f
#define YV5N_DEMO_P10_BOX_SCALE 6.26516455e-02f
#define YV5N_DEMO_P10_CLS_SCALE 1.44189159e-01f

#define YV5N_DEMO_P6_BOX_BYTES  (YV5N_DEMO_P6_H * YV5N_DEMO_P6_W * YV5N_DEMO_BOX_C)
#define YV5N_DEMO_P6_CLS_BYTES  (YV5N_DEMO_P6_H * YV5N_DEMO_P6_W * YV5N_DEMO_CLS_C)
#define YV5N_DEMO_P8_BOX_BYTES  (YV5N_DEMO_P8_H * YV5N_DEMO_P8_W * YV5N_DEMO_BOX_C)
#define YV5N_DEMO_P8_CLS_BYTES  (YV5N_DEMO_P8_H * YV5N_DEMO_P8_W * YV5N_DEMO_CLS_C)
#define YV5N_DEMO_P10_BOX_BYTES (YV5N_DEMO_P10_H * YV5N_DEMO_P10_W * YV5N_DEMO_BOX_C)
#define YV5N_DEMO_P10_CLS_BYTES (YV5N_DEMO_P10_H * YV5N_DEMO_P10_W * YV5N_DEMO_CLS_C)

typedef struct {
    uint64_t addr;
    uint32_t len;
    uint16_t h;
    uint16_t w;
    uint16_t c;
    uint16_t stride;
    float scale;
    uint32_t _pad0;
} yv5n_demo_tensor_ref_t;

typedef struct {
    uint32_t magic;
    uint32_t ready;
    yv5n_demo_tensor_ref_t p6_box;
    yv5n_demo_tensor_ref_t p6_cls;
    yv5n_demo_tensor_ref_t p8_box;
    yv5n_demo_tensor_ref_t p8_cls;
    yv5n_demo_tensor_ref_t p10_box;
    yv5n_demo_tensor_ref_t p10_cls;
} yv5n_demo_result_t;

#ifdef __cplusplus
}
#endif

#endif /* YOLOV5N_DEMO_H */
