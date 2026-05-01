# YOLOv5n TPA Step-Zero Inventory

This file is generated from the local `YOLOv5n` checkpoint and the
current coarse TPA process plan.

## Process Inventory

| Proc | Kind | Layers | Inputs | Outputs | Weight Bytes | Scratch Est |
|---|---|---|---|---|---:|---:|
| P0 | `yolo_stage_stem` | `0, 1, 2, 3, 4, 5, 6` | `input` | `t2` | 19328 | 2048000 |
| P1 | `yolo_stage_down` | `7, 8, 9, 10, 11, 12, 13, 14` | `t2` | `skip_p3` | 57856 | 1024000 |
| P2 | `yolo_stage_down` | `15, 16, 17, 18, 19, 20, 21, 22, 23, 24` | `skip_p3` | `skip_p4` | 238080 | 512000 |
| P3 | `yolo_stage_top` | `25, 26, 27, 28, 29, 30, 31, 32, 33` | `skip_p4` | `skip_p5` | 801024 | 409600 |
| P4 | `yolo_neck_up` | `34, 35, 36, 37, 38, 39` | `skip_p5, skip_p4` | `mid_p4` | 103424 | 512000 |
| P5 | `yolo_neck_up` | `40, 41, 42, 43, 44` | `mid_p4, skip_p3` | `p3` | 27392 | 1024000 |
| P6 | `yolo_detect_scale` | `57, 58, 69, 63, 64, 72` | `p3` | `p3_box, p3_cls` | 200064 | 512000 |
| P7 | `yolo_neck_down` | `45, 46, 47, 48, 49, 50` | `p3, mid_p4` | `p4` | 115712 | 512000 |
| P8 | `yolo_detect_scale` | `59, 60, 70, 65, 66, 73` | `p4` | `p4_box, p4_cls` | 283008 | 128000 |
| P9 | `yolo_neck_down` | `51, 52, 53, 54, 55, 56` | `p4, skip_p5` | `p5` | 451072 | 256000 |
| P10 | `yolo_detect_scale` | `61, 62, 71, 67, 68, 74` | `p5` | `p5_box, p5_cls` | 448896 | 32000 |

## Channel Inventory

| Channel | Producer | Consumers | Shape | Bytes | Role |
|---|---|---|---|---:|---|
| `t2` | P0 | P1 | `32x160x160` | 819200 | sequential stage handoff |
| `skip_p3` | P1 | P5 | `64x80x80` | 409600 | backbone skip |
| `skip_p4` | P2 | P4 | `128x40x40` | 204800 | backbone skip |
| `skip_p5` | P3 | P9 | `128x20x20` | 51200 | top feature |
| `mid_p4` | P4 | P7 | `64x40x40` | 102400 | neck intermediate |
| `p3` | P5 | P6, P7 | `64x80x80` | 409600 | detect scale input |
| `p4` | P7 | P8, P9 | `128x40x40` | 204800 | detect scale input |
| `p5` | P9 | P10 | `256x20x20` | 102400 | detect scale input |
| `p3_box` | P6 | - | `64x80x80` | 409600 | terminal output |
| `p3_cls` | P6 | - | `80x80x80` | 512000 | terminal output |
| `p4_box` | P8 | - | `64x40x40` | 102400 | terminal output |
| `p4_cls` | P8 | - | `80x40x40` | 128000 | terminal output |
| `p5_box` | P10 | - | `64x20x20` | 25600 | terminal output |
| `p5_cls` | P10 | - | `80x20x20` | 32000 | terminal output |

## Representative Block Tests

| Block | Layers | Reason |
|---|---|---|
| `CBS` | `40` | representative mid-neck 1x1 CBS |
| `Bottleneck` | `27, 28` | first non-trivial residual block |
| `C3_1` | `40, 41, 42, 43, 44` | smallest CSP-style fused block |
| `C3_3` | `16, 17, 18, 19, 20, 21, 22, 23, 24` | heavier backbone C3 block |
| `SPPF` | `31, 32` | special pooling/concat topology |
| `neck_up` | `34, 35, 36, 37, 38, 39` | upsample+concat+C3 fusion |
| `neck_down` | `45, 46, 47, 48, 49, 50` | downsample+concat+C3 fusion |
| `detect_p3` | `57, 58, 69, 63, 64, 72` | terminal branch pair at finest scale |

## First Subgraph

`P3 -> P4 -> P5 -> P6`

first meaningful TPA subgraph with skip reuse, upsample, concat, and one detect branch
