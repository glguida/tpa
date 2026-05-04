# YOLO Demo, Tools, Models, and Block Tests

This document records the current structured YOLO status. The full downstream
YOLO planner/map/device path is ported, and this repository now also carries the
original YOLO reproduction tools, source model artifacts, and representative
block-test integration.

## Current ported pieces

- `yolov5n/` — process sources, manifests, planner/map CMake integration, and
  downstream device ELF/PASS-marker runtime path.
- `tests/yolo/` — original representative block-test sources/assets and
  structured CMake targets.
- `tools/yolo/` — original generation, quantization, tensor-weight, and
  block-case scripts.
- `models/yolov5nu.onnx` and `models/yolov5nu.pt` — source model artifacts used
  by the regeneration path.
- `models/yolov8n_artifact_manifest.json` — external-artifact manifest and
  verified architecture facts for the future YOLOv8n path; it is not a YOLOv8n
  kernel or graph integration.
- `machines/erbium.json` and `machines/etsoc1.json` — mapper topology inputs.

## Model artifact policy

The original YOLOv5n model artifacts are small enough for this repo and are
checked in directly:

| Path | SHA-256 |
|---|---|
| `models/yolov5nu.onnx` | `a25f225d4d29135249c756addc1ca388a00cf4a302790aedc739d3e771ebc915` |
| `models/yolov5nu.pt` | `9e9c1be448b0e1b8598975a9abcab9a0fd0e21182eb422cda7f044a0442d0937` |

If these are moved out of git in the future, replacement fetch instructions and
checksums must be committed at the same time.

## YOLOv8n artifact status

The full YOLOv8n model path is not integrated. The repository records
`models/yolov8n_artifact_manifest.json`, but it does not check in the YOLOv8n
`.pt` or exported `.onnx` binaries and does not provide generated YOLOv8n
weights, calibration data, model-derived fixtures, a full YOLOv8n process graph,
a downstream YOLOv8n ELF/CMake path, or full-model Erbium validation. The
representative synthetic C2f and Detect/DFL block tests under `tests/yolo/` are
the only YOLOv8n process-kernel, `.tpp`, CMake-target, and Erbium CTest coverage
currently present; they are non-model-derived block coverage and do not
constitute full YOLOv8n graph, downstream, or model validation.

The targeted external artifact is Ultralytics `yolov8n.pt` from
`https://github.com/ultralytics/assets/releases/download/v8.3.0/yolov8n.pt`.
The manifest records the upstream AGPL-3.0 license, file size, and SHA-256
checksum. The matching ONNX architecture input is a static batch-1 640x640
opset-12 export with no NMS. These model binaries remain external until project
owners explicitly approve vendoring AGPL-3.0 artifacts.

Verified facts from that targeted artifact:

- input shape: `[1, 3, 640, 640]`;
- public ONNX output: `[1, 84, 8400]`;
- detect source layers: `model.15`, `model.18`, and `model.21`;
- detect input feature maps: P3 `[1, 64, 80, 80]`, P4 `[1, 128, 40, 40]`, and
  P5 `[1, 256, 20, 20]`;
- detect raw pre-decode tensors: `[1, 144, 80, 80]`, `[1, 144, 40, 40]`, and
  `[1, 144, 20, 20]`;
- detect layout: `reg_max=16`, 64 DFL box-distribution channels plus 80 class
  channels per scale before decode;
- ONNX operator families include Conv, SiLU represented as Sigmoid+Mul, Concat,
  Split, Add, MaxPool, Resize, Softmax, Reshape, Transpose, Slice, Div, Sub,
  Shape, and Gather.

For a first INT8 downstream milestone, the three detect-input feature maps would
be edge/channel payloads of 409600, 204800, and 102400 bytes respectively. They
are not process state. Scratch requirements and immutable generated weight data
must be determined by future YOLOv8n kernel/calibration jobs.

Compared with the current YOLOv5n downstream path, Conv+BN+SiLU, SPPF,
nearest-neighbor upsample, and concat remain recognizable building blocks, but
YOLOv8n uses C2f blocks instead of the current C3/Bottleneck assumptions and an
anchor-free DFL Detect head instead of the YOLOv5n detect-head assumptions. The
repository includes representative synthetic C2f and Detect/DFL block tests to
exercise those semantics before a full YOLOv8n TPA graph is implemented.

## Reproduction tools

The tools live under `tools/yolo/`. They are ported as source/reference tools;
running all of them may require heavyweight third-party Python packages such as
PyTorch, ONNX, NumPy, and Ultralytics. Normal CI-style validation does not
install those packages or regenerate every checked-in header.

See `tools/yolo/README.md` for the tool list and checksum policy. Use
`tools/yolo/inspect_yolo_artifact.py` to regenerate or audit the YOLOv8n
architecture manifest; its `--help` path works without importing Ultralytics,
PyTorch, ONNX, NumPy, or OpenCV.

## Downstream YOLO planner/map build

From a planner-enabled Python environment:

```sh
python3 -m venv .venv-planner
. .venv-planner/bin/activate
python -m pip install -e planner
cmake -S . -B build-et-erbium -DET_ROOT=/opt/et -DTPA_PLATFORM=erbium -DPYTHON=$(command -v python)
cmake --build build-et-erbium --target tpa_yolov5n_downstream_plan_planner_json
cmake --build build-et-erbium --target tpa_yolov5n_downstream_map_mapped_program
```

The downstream planner/map artifact path is integrated. CMake prepends this
repository's `planner/src` to `PYTHONPATH` for these planner commands so a stale
globally installed `tpa_planner` package cannot generate mismatched headers. The
downstream device ELF links on Erbium and reports a PASS marker under
`erbium_emu`:

```sh
cmake --build build-et-erbium --target tpa_yolov5n_downstream.elf
/opt/et/bin/erbium_emu \
  -minions 0x1f \
  -elf_load build-et-erbium/tpa-device-prefix/src/tpa-device-build/yolov5n/tpa_yolov5n_downstream.elf \
  -max_cycles 100000000
```

## Representative block tests

Structured CMake now includes YOLO block-test targets when building for Erbium.
Forwarded top-level representative targets are:

```sh
cmake --build build-et-erbium --target tpa_yolo_cbs_l40.elf
cmake --build build-et-erbium --target tpa_yolo_sppf_l31_32.elf
cmake --build build-et-erbium --target tpa_yolov8n_c2f_block.elf
cmake --build build-et-erbium --target tpa_yolov8n_detect_dfl.elf
```

The YOLOv8n block tests use deterministic synthetic fixtures generated by
`tools/yolo/gen_yolov8n_synthetic_cases.py`; they do not vendor AGPL-3.0
YOLOv8n model binaries, generated weights, calibration data, model activations,
or expected tensors derived from the external artifact. `tpa_yolov8n_c2f_block`
checks reduced C2f split/concat/two-bottleneck residual flow and final
projection. `tpa_yolov8n_detect_dfl` checks the 4x16 DFL channel layout,
fixed-point softmax/bin projection, three stride classes, 80 class channels, and
explicit reduced public `[1, 84, N]` output ordering using
`public[channel * NR_POINTS + point]` with box channels 0-3 and class channels
4-83. These tests are representative block coverage only, not a full YOLOv8n
graph or downstream Erbium validation.

Run the representative ELFs directly:

```sh
/opt/et/bin/erbium_emu \
  -elf_load build-et-erbium/tpa-device-prefix/src/tpa-device-build/tests/yolo/tpa_yolo_cbs_l40.elf \
  -max_cycles 200000000
/opt/et/bin/erbium_emu \
  -elf_load build-et-erbium/tpa-device-prefix/src/tpa-device-build/tests/yolo/tpa_yolo_sppf_l31_32.elf \
  -max_cycles 300000000
```

Or run the device-subbuild CTest entries:

```sh
ctest --test-dir build-et-erbium/tpa-device-prefix/src/tpa-device-build \
  -R 'tpa_yolo_(cbs_l40|sppf_l31_32)|tpa_yolov8n_(c2f_block|detect_dfl)' \
  --output-on-failure
```

Additional block-target definitions remain in `tests/yolo/CMakeLists.txt` for
future expansion, but the representative validated targets above are the current
reviewed coverage.

## ET-SoC-1 caveat

The downstream YOLO mapper path uses the full-card ET-SoC-1 machine JSON. The
ET-SoC-1 default one-shire configuration validates `tpa_core`; YOLO requires
`TPA_ETSOC1_NR_SHIRES=32` before enabling the full-card path.

## Remaining follow-up

The full original YOLO end-user host pipeline is still separate follow-up work.
The current validated paths are the downstream planner/map/device ELF path, its
Erbium PASS-marker runtime validation, and the representative Erbium block-test
ELFs documented above. The YOLOv8n block tests are representative synthetic
operator coverage and do not imply a full YOLOv8n model path.
