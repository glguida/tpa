# TPA Structured Runtime

TPA is a transputer-style process/channel runtime and build/mapping flow for ET.
It describes programs as graphs of continuation-style processes, maps those
graphs onto ET topologies, generates image metadata, and builds ET device ELFs
for Erbium or ET-SoC-1.

The primary build is the ET superbuild path. The top-level project discovers
et-platform's `ProjectFunctions.cmake` and configures structured `tpa-device`
and `tpa-host` subprojects through `DeviceProjectNoInstall()` and
`HostProjectNoInstall()`. Host-only builds are retained only as explicitly named
smoke-test doubles; they are not ET platform validation.

## Repository layout

- `CMakeLists.txt` — ET superbuild entry point.
- `tpa/` — platform-independent core modules and HAL-facing headers.
- `tpa/hal/` — Erbium and ET-SoC-1 HAL implementations.
- `tpa-device/` — ET RISC-V device project and runtime link harness.
- `tpa-host/` — ET host project with `tpa_launcher`.
- `cmake/tpa-kernel.cmake` — `add_tpa_process()` / `add_tpa_program()` helpers.
- `cmake/gen_tpa_image.cmake` — image generation from `.tpm`, `.tpp`, and
  `.place` or mapper output.
- `kernels/` — current simple generated programs: `tpa_empty` and
  `tpa_pipe_demo`.
- `yolov5n/` — current YOLO downstream process sources and planner/map/device
  targets.
- `tests/yolo/` — YOLO block-test sources/assets with representative structured
  Erbium block-test targets.
- `tools/yolo/` and `models/` — YOLO regeneration/quantization tools and source
  model artifacts.
- `planner/` — Python metadata extraction, planning, and mapping package.
- `machines/` — mapper machine topology JSON inputs.
- `docs/` — detailed project documentation.

## Read next

- `docs/overview.md` — conceptual overview and current validated paths.
- `docs/programming-model.md` — process, channel, image, and artifact terms.
- `docs/et-architecture.md` — Erbium/ET-SoC-1 topology relevant to TPA.
- `docs/creating-programs.md` — practical guide for creating TPA programs.
- `docs/mapper-planner.md` — planner/mapper inputs, algorithms, outputs, and
  commands.
- `docs/memory-and-edge-buffers.md` — memory taxonomy and edge-buffer planning.
- `docs/yolo-demo.md` — YOLO downstream, tools/models policy, and block tests.
- `docs/MISSING_ORIGINAL_ARTIFACTS.md` — ported vs missing original artifacts.
- `docs/USAGE.md` — current runtime usage notes and caveats.
- `planner/README.md` — quick reference for the Python planner package.

Coding agents should also read `AGENTS.md` before editing.

## Quick start: planner package

```sh
python3 -m venv .venv-planner
. .venv-planner/bin/activate
python -m pip install -e planner
python -m unittest discover -s planner/tests

tpa-map-program --help
tpa-plan-program --help
tpa-extract-process-json --help
```

## Quick start: Erbium ET build and run

Set `ET_ROOT` or `ET_PLATFORM_PATH` to an et-platform install/root containing
`ProjectFunctions.cmake`, the ET RISC-V toolchain file, and ET CMake packages.
Examples below use `/opt/et`.

```sh
cmake -S . -B build-et-erbium -DET_ROOT=/opt/et -DTPA_PLATFORM=erbium
cmake --build build-et-erbium --target tpa_pipe_demo.elf
/opt/et/bin/erbium_emu \
  -elf_load build-et-erbium/tpa-device-prefix/src/tpa-device-build/kernels/tpa_pipe_demo.elf \
  -max_cycles 10000
```

YOLO downstream planner/map/device path:

```sh
python3 -m venv .venv-planner
. .venv-planner/bin/activate
python -m pip install -e planner
cmake -S . -B build-et-erbium -DET_ROOT=/opt/et -DTPA_PLATFORM=erbium -DPYTHON=$(command -v python)
cmake --build build-et-erbium --target tpa_yolov5n_downstream_plan_planner_json
cmake --build build-et-erbium --target tpa_yolov5n_downstream_map_mapped_program
cmake --build build-et-erbium --target tpa_yolov5n_downstream.elf
/opt/et/bin/erbium_emu \
  -elf_load build-et-erbium/tpa-device-prefix/src/tpa-device-build/yolov5n/tpa_yolov5n_downstream.elf \
  -max_cycles 10000
```

## Quick start: host launcher

Build the host launcher through the ET host subproject:

```sh
cmake --build build-et-erbium --target tpa_host_tools
```

Run a generated ELF through the ET runtime sysemu device layer:

```sh
build-et-erbium/tpa-host-prefix/src/tpa-host-build/tpa_launcher \
  --kernel build-et-erbium/tpa-device-prefix/src/tpa-device-build/kernels/tpa_pipe_demo.elf \
  --mode sysemu \
  --timeout 300
```

`tpa_launcher` also supports `--mode pcie` for silicon and `--mode fake` for
host runtime API smoke checks. Use `--help` for all options.

## Quick start: ET-SoC-1

The default ET-SoC-1 path validates the structured runtime archive in a one-shire
configuration:

```sh
cmake -S . -B build-et-etsoc1 -DET_ROOT=/opt/et -DTPA_PLATFORM=etsoc1
cmake --build build-et-etsoc1 --target tpa_core
```

YOLO mapping uses the full-card `machines/etsoc1.json` model. The device project
therefore keeps YOLO off by default for ET-SoC-1 unless configured with
`TPA_ETSOC1_NR_SHIRES=32`.

## Host smoke-test double

For local syntax/unit smoke without et-platform:

```sh
cmake -S . -B build-smoke -DTPA_HOST_SMOKE_TEST_DOUBLE=ON
cmake --build build-smoke
ctest --test-dir build-smoke --output-on-failure
```

This mode defines `TPA_HOST_SMOKE_TEST_DOUBLE=1` and builds host test doubles
only. It must not be used as evidence that Erbium or ET-SoC-1 device platform
integration works.

## Current status

Ported and validated today:

- ET superbuild integration for device and host subprojects.
- Erbium `tpa_empty.elf`, `tpa_pipe_demo.elf`, and
  `tpa_yolov5n_downstream.elf` build paths.
- Erbium emulator validation for `tpa_pipe_demo.elf` and YOLO downstream.
- ET-SoC-1 default one-shire `tpa_core` build.
- `tpa_launcher` host tool target.
- Python planner package, checked-in machine JSONs, and planner tests.
- Host smoke-test-double mode for non-platform syntax/unit smoke.

Important missing or partial areas remain: original message/queue/negative test
suites, tensor matmul and DNN demos, ltfarm, full YOLO end-user host pipeline,
and the full cooperative runtime scheduler. YOLO tools/models and representative
block tests are now ported; see `docs/yolo-demo.md` and
`docs/MISSING_ORIGINAL_ARTIFACTS.md` for the detailed inventory.
