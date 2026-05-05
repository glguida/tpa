#include <stdint.h>

#ifndef TPA_YOLOV8N_EXTERNAL_WEIGHTS_SOURCE_HEADER
#error "Configure with -DTPA_YOLOV8N_EXTERNAL_WEIGHTS_SOURCE_HEADER=/path/to/yolov8n_external_detect_c2f_weights.h"
#endif
#include TPA_YOLOV8N_EXTERNAL_WEIGHTS_SOURCE_HEADER

const yolov8n_external_detect_c2f_layer_t * const
    yolov8n_external_detect_c2f_layers_exported =
        yolov8n_external_detect_c2f_layers;
