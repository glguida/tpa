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
- `regen_yolov8n_external_weights.py` — verify external YOLOv8n `.pt`/`.onnx`
  checksums, validate the YOLOv8n layer-selection config, run PTQ generation
  into an external/untracked output directory, and write a generated-output
  checksum/provenance manifest. Its `--help` and `--validate-config-only` paths
  avoid heavyweight optional imports.
- `yolov8n_external_layer_selection.json` — stable YOLOv8n layer/module
  selection for the first external INT8 downstream milestone. It records only
  manifest-derived names, shapes, module roles, and output policy; it contains
  no weights, activations, calibration data, or model-derived tensor values.
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

## YOLOv8n external weight/header workflow

Keep YOLOv8n model binaries, generated weights, calibration images, model
activations, and generated checksum manifests outside the repository unless
project owners explicitly approve vendoring AGPL-3.0/model-derived artifacts.
The claim/provenance checklist is
`docs/yolo-demo.md#yolov8n-calibration-data-and-generated-artifact-policy`.
The default generator output directory is `/tmp/tpa-yolov8n/generated-weights`,
and the wrapper refuses repository-local output unless
`--allow-repo-out-dir` is supplied deliberately.

Acquire the external artifacts using the policy in `models/README.md`, then run
lightweight config validation from the repository root:

```sh
python3 tools/yolo/regen_yolov8n_external_weights.py --validate-config-only
```

Generate headers in a heavyweight throwaway environment that has Ultralytics,
PyTorch, NumPy, and any calibration-image dependencies available:

```sh
python3 tools/yolo/regen_yolov8n_external_weights.py \
  --external-root /tmp/tpa-yolov8n \
  --out-dir /tmp/tpa-yolov8n/generated-weights \
  --calib-dir /path/to/representative/calibration/images
```

For tool-plumbing smoke only, omit `--calib-dir`; the underlying PTQ exporter
uses deterministic synthetic random inputs. That synthetic mode is not
production quantization evidence. Before generation, the wrapper verifies
`external/yolov8n.pt` and `external/yolov8n.onnx` against
`models/yolov8n_artifact_manifest.json`. After generation it writes
`<stem>_generated_manifest.json` beside the header with source artifact,
selection-config, calibration, command, output checksum, and policy metadata so
future process/kernel jobs can cite exact external outputs without committing
them. Pass the resulting paths through CMake cache variables or equivalent
explicit inputs; committed code must not name AgentWS workspace paths.

The selected modules come from `yolov8n_external_layer_selection.json`, which is
validated against the checked-in artifact manifest. The selection covers Detect
source C2f modules `model.15`, `model.18`, and `model.21`, the P4-to-P5
neck-down `model.19` Conv module, plus the Detect/DFL branches under `model.22`
needed to produce 64 box-distribution and 80 class channels per scale. Detect
input tensors, neck tensors, and concat tensors remain graph edge/channel
payloads; generated weights and quantization tables are immutable model data,
not process scratch or persistent process state.

## Normal validation

Normal project validation does not require rerunning the heavyweight model
regeneration scripts. The default validation for this port is:

- `python3 tools/yolo/inspect_yolo_artifact.py --help` for the lightweight
  YOLO artifact inspector;
- `python3 tools/yolo/gen_yolov8n_synthetic_cases.py --help` for the
  non-model-derived YOLOv8n fixture generator;
- `python3 tools/yolo/regen_yolov8n_external_weights.py --help` and
  `python3 tools/yolo/regen_yolov8n_external_weights.py --validate-config-only`
  for the external YOLOv8n weight/header workflow without heavyweight imports;
- planner unit tests;
- ET Erbium YOLO downstream planner/map/device targets;
- representative `tests/yolo` block ELFs under `erbium_emu`;
- ET-SoC-1 `tpa_core`;
- host smoke-test-double build/CTest.
