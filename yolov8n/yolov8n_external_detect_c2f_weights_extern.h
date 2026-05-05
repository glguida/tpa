#ifndef TPA_YOLOV8N_EXTERNAL_DETECT_C2F_WEIGHTS_EXTERN_H
#define TPA_YOLOV8N_EXTERNAL_DETECT_C2F_WEIGHTS_EXTERN_H

#include <stdint.h>

typedef struct {
    const int8_t   *w;
    const int32_t  *b;
    const float    *s;
    const uint8_t  *lut;
    uint32_t        K_out;
    uint32_t        C_in;
    uint32_t        kH, kW;
    uint32_t        stride_h, stride_w;
    uint32_t        pad_h, pad_w;
    uint32_t        K_inner;
    uint32_t        w_stride;
    uint32_t        K_out_pad;
    float           act_in_scale;
    float           act_out_scale;
} yolov8n_external_detect_c2f_layer_t;

#define YOLOV8N_EXTERNAL_DETECT_C2F_N_LAYERS 32

extern const yolov8n_external_detect_c2f_layer_t * const
    yolov8n_external_detect_c2f_layers_exported;

#define yolov8n_external_detect_c2f_layers \
    yolov8n_external_detect_c2f_layers_exported

#endif /* TPA_YOLOV8N_EXTERNAL_DETECT_C2F_WEIGHTS_EXTERN_H */
