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

## Reproduction tools

The tools live under `tools/yolo/`. They are ported as source/reference tools;
running all of them may require heavyweight third-party Python packages such as
PyTorch, ONNX, NumPy, and Ultralytics. Normal CI-style validation does not
install those packages or regenerate every checked-in header.

See `tools/yolo/README.md` for the tool list and checksum policy.

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

The downstream planner/map artifact path is integrated. The downstream device
ELF links on Erbium and reports a PASS marker under `erbium_emu`:

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
```

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
  -R 'tpa_yolo_(cbs_l40|sppf_l31_32)' \
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
ELFs documented above.
