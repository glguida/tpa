# YOLO Reproduction Tools

This directory contains the YOLO generation, quantization, and case-generation
helpers ported from the original TPA repository.

## Ported tools

- `regen_yolov5n_weights.py` — regenerate checked-in YOLO weight headers.
- `inspect_yolo_artifact.py` — inspect an external YOLO `.pt`/`.onnx` artifact
  and emit a JSON architecture/provenance manifest. Its `--help` path avoids
  importing heavyweight optional packages.
- `gen_yolov8n_synthetic_cases.py` — generate deterministic, non-model-derived
  fixture headers for the representative YOLOv8n C2f and Detect/DFL block
  tests, including the reduced Detect/DFL public `[1, 84, N]` channel-major
  ordering fixture.
- `ptq_yolov5.py` — post-training quantization helper for the YOLO model path.
- `gen_yolo_tensor_weights.py` — generate tensor-weight headers.
- `gen_yolo_block_case.py` — generate CBS/block test cases.
- `gen_yolo_bottleneck_case.py` — generate bottleneck test cases.
- `gen_yolo_c3_case.py` — generate C3 test cases.
- `gen_yolo_detect_case.py` — generate detect-head test cases.
- `gen_yolo_neck_down_case.py` — generate neck-down test cases.
- `gen_yolo_neck_up_case.py` — generate neck-up test cases.
- `gen_yolo_sppf_case.py` — generate SPPF test cases.
- `gen_yolo_subgraph_p4_p5_p6_case.py` — generate subgraph test cases.
- `yolov5n_legacy_layer_map.json` — legacy layer-name mapping used by the
  regeneration path.

These scripts are preserved to make the checked-in generated headers and block
cases reproducible. Some scripts require third-party Python packages such as
PyTorch, ONNX, NumPy, or Ultralytics; those heavyweight dependencies are not
installed by the normal planner test environment. New inspection helpers should
keep heavyweight imports inside the command path that needs them so lightweight
`--help` checks continue to work.

## Model artifact policy

The source YOLOv5n model artifacts are small enough for this repository and are
checked in under `models/`:

| Path | SHA-256 |
|---|---|
| `models/yolov5nu.onnx` | `a25f225d4d29135249c756addc1ca388a00cf4a302790aedc739d3e771ebc915` |
| `models/yolov5nu.pt` | `9e9c1be448b0e1b8598975a9abcab9a0fd0e21182eb422cda7f044a0442d0937` |

If these files ever become too large or move to external storage, keep this
policy file updated with acquisition instructions and checksums before removing
them.

`models/yolov8n_artifact_manifest.json` records a YOLOv8n target without
checking in the AGPL-3.0 Ultralytics model binaries. It names the exact
Ultralytics v8.3.0 `.pt` URL, the static 640x640 opset-12 ONNX export policy,
SHA-256 checksums, and architecture facts for future YOLOv8n planning.

## Normal validation

Normal project validation does not require rerunning the heavyweight model
regeneration scripts. The default validation for this port is:

- `python3 tools/yolo/inspect_yolo_artifact.py --help` for the lightweight
  YOLO artifact inspector;
- `python3 tools/yolo/gen_yolov8n_synthetic_cases.py --help` for the
  non-model-derived YOLOv8n fixture generator;
- planner unit tests;
- ET Erbium YOLO downstream planner/map/device targets;
- representative `tests/yolo` block ELFs under `erbium_emu`;
- ET-SoC-1 `tpa_core`;
- host smoke-test-double build/CTest.
