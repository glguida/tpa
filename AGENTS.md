# AGENTS Guide for TPA Structured Runtime

This file orients coding agents working in this repository. It is a runbook and
navigation aid, not a replacement for the detailed docs.

## Read order

Read these before making changes:

1. `README.md`
2. `docs/overview.md`
3. `docs/programming-model.md`
4. `docs/et-architecture.md`
5. `docs/creating-programs.md`
6. `docs/mapper-planner.md`
7. `docs/memory-and-edge-buffers.md`
8. `docs/USAGE.md`
9. `planner/README.md` when touching planner/mapper code
10. `docs/yolo-demo.md` when touching YOLO paths

## Core rules

- **do not invent alternate CMake paths.** The primary product is the ET
  superbuild from the repository root.
- Use top-level `CMakeLists.txt`, `ET_ROOT`/`ET_PLATFORM_PATH`,
  `DeviceProjectNoInstall(tpa-device ...)`, and
  `HostProjectNoInstall(tpa-host ...)`.
- Device programs must use the `.c + .tpm + .tpp + .place` path or
  mapper-generated equivalents through `add_tpa_process()` and
  `add_tpa_program()`.
- Do not bypass `cmake/gen_tpa_image.cmake` for graph programs.
- Host smoke tests are not ET platform validation. They are only local
  syntax/unit smoke-test doubles.
- Do not hide missing ET toolchain/HAL behavior behind host fallbacks.
- Do not claim missing original tests, demos, tools, or model artifacts are
  complete. Check current status docs such as `docs/USAGE.md` and
  `docs/yolo-demo.md`.
- Keep process state, scratch, immutable model data, and edge/channel data
  distinct in code and documentation.
- Warnings are errors for current process/device targets; do not introduce new
  warnings.

## Creating or changing TPA programs

Follow `docs/creating-programs.md`.

The intended program authoring pipeline is:

1. Decompose the computation into process kinds and ports.
2. Write continuation-style C process code returning `tpa_op_t` operations.
3. Declare process kinds and ports in `.tpm` manifests.
4. Instantiate process kinds and connect ports in a `.tpp` program graph.
5. Use a hand `.place` for small examples or mapper output for larger graphs.
6. Integrate with `add_tpa_process()` and `add_tpa_program()`.
7. Build through the ET superbuild.
8. Run with `erbium_emu` or `tpa_launcher` as required.
9. Validate application PASS/FAIL behavior and generated artifacts.

Use `kernels/tpa_pipe_demo.*` as the small current example,
`kernels/tpa_tensor_matmul.*` as the intermediate generated-graph example, and
`yolov5n/` as the larger mapper-integrated example.

## Mapper/planner workflow

Follow `docs/mapper-planner.md` and `docs/memory-and-edge-buffers.md`.

Important points:

- Process metadata comes from built objects plus `.tpm` manifests.
- `tpa-plan-program` reports on an existing `.tpp` + `.place` + metadata set.
- `tpa-map-program` maps `.tpp` + metadata onto a machine JSON and can emit a
  map report, mapped-program JSON, generated `.place`, scratch header, and edge
  config header.
- Checked-in machine JSONs live in `machines/`: `single-minion.json`,
  `erbium.json`, and `etsoc1.json`.
- Communication class is a mapped edge property: `direct`, `local`, `fabric`, or
  `external`.
- The current mapper is performance-first under a hard memory budget, with
  optional greedy context-collapse repair.

Install the planner package before using planner CLI entry points directly and
before running planner package tests:

```sh
python3 -m venv .venv-planner
. .venv-planner/bin/activate
python -m pip install -e planner
python -m unittest discover -s planner/tests
```

CMake-integrated YOLO planner/map targets prepend this repository's
`planner/src` to `PYTHONPATH` so stale globally installed planner packages are
not used. Configure with `-DPYTHON=$(command -v python)` when using a selected
Python environment.

## Validation

Run the smallest sufficient set for your change, but do not report smoke tests
as platform validation.

### Docs-only changes

```sh
git diff --check
python3 -m unittest discover -s planner/tests
```

Also run any job-specific documentation sanity check.

### Planner/mapper changes

```sh
python3 -m venv .venv-planner
. .venv-planner/bin/activate
python -m pip install -e planner
python -m unittest discover -s planner/tests

tpa-map-program --help
tpa-plan-program --help
tpa-extract-process-json --help
```

### Host smoke-test double

```sh
cmake -S . -B build-smoke -DTPA_HOST_SMOKE_TEST_DOUBLE=ON
cmake --build build-smoke
ctest --test-dir build-smoke --output-on-failure
```

This is not Erbium or ET-SoC-1 validation.

### Erbium ET validation

```sh
cmake -S . -B build-et-erbium -DET_ROOT=/opt/et -DTPA_PLATFORM=erbium
cmake --build build-et-erbium --target tpa_host_tools
cmake --build build-et-erbium --target tpa_pipe_demo.elf
cmake --build build-et-erbium --target tpa_empty.elf
cmake --build build-et-erbium --target tpa_tensor_matmul.elf
/opt/et/bin/erbium_emu \
  -elf_load build-et-erbium/tpa-device-prefix/src/tpa-device-build/kernels/tpa_pipe_demo.elf \
  -max_cycles 10000
```

For YOLO downstream planner/map/device validation:

```sh
python3 -m venv .venv-planner
. .venv-planner/bin/activate
python -m pip install -e planner
cmake -S . -B build-et-erbium -DET_ROOT=/opt/et -DTPA_PLATFORM=erbium -DPYTHON=$(command -v python)
cmake --build build-et-erbium --target tpa_yolov5n_downstream_plan_planner_json
cmake --build build-et-erbium --target tpa_yolov5n_downstream_map_mapped_program
cmake --build build-et-erbium --target tpa_yolov5n_downstream.elf
/opt/et/bin/erbium_emu \
  -minions 0x1f \
  -elf_load build-et-erbium/tpa-device-prefix/src/tpa-device-build/yolov5n/tpa_yolov5n_downstream.elf \
  -max_cycles 100000000
```

### Host launcher validation

```sh
cmake --build build-et-erbium --target tpa_host_tools
build-et-erbium/tpa-host-prefix/src/tpa-host-build/tpa_launcher --help
build-et-erbium/tpa-host-prefix/src/tpa-host-build/tpa_launcher \
  --kernel build-et-erbium/tpa-device-prefix/src/tpa-device-build/kernels/tpa_pipe_demo.elf \
  --mode sysemu \
  --timeout 300
```

Use `--mode pcie` for silicon and `--mode fake` only for host runtime API smoke.

### ET-SoC-1 validation

```sh
cmake -S . -B build-et-etsoc1 -DET_ROOT=/opt/et -DTPA_PLATFORM=etsoc1
cmake --build build-et-etsoc1 --target tpa_core
```

Only enable YOLO for ET-SoC-1 when configured with `TPA_ETSOC1_NR_SHIRES=32`,
because the current YOLO mapping uses the full-card machine JSON.

## Common traps

- Confusing host smoke-test-double success with ET device validation.
- Confusing process kind with process instance.
- Treating ports as transport classes; transport belongs to mapped edges.
- Counting edge/channel payloads as persistent process state.
- Treating scratch as persistent state or as an output channel.
- Optimizing mapper output for low memory before checking whether the
  performance-first mapping fits.
- Using stale old-repo CMake presets or old build directories as current
  instructions.
- Forgetting to install/select the planner Python environment for YOLO CMake
  planner/map targets.
- Claiming YOLO block CTests or model regeneration tools are integrated before
  they are actually ported. YOLO tools/models and representative block tests are
  ported; check `docs/yolo-demo.md` for current scope.
- Claiming ported message/queue/negative test ELFs prove full scheduler runtime
  behavior before the cooperative runtime scheduler executes process
  continuations.
- Claiming archived DNN demos or LTFarm are active build targets; they are
  preserved under `docs/archive/` until their dependencies/harnesses are
  revived.
- Claiming ported message/queue/negative test ELFs prove full scheduler runtime
  behavior before the cooperative runtime scheduler executes process
  continuations.

## Current status notes

Validated current paths include Erbium simple demo ELFs, the packed-single row,
Tensor alignment/error, and PMU counter sanity micro-examples, the tensor matmul
demo ELF, the attention packed-single softmax subtract-max experiment, YOLO
downstream planner/map/device ELF PASS-marker runtime path,
representative message/channel and queue runtime regression ELFs, the negative
expected-failure ELF's intentional FAIL marker, ET-SoC-1 default `tpa_core`, the
host launcher target, planner tests, and host smoke-test doubles.

Important follow-up areas remain: broader scheduler hardening beyond the
representative runtime regressions and full YOLO host/demo integration. YOLO
tools/models and representative block tests are documented in
`docs/yolo-demo.md`. DNN demos, LTFarm, trace tools, and historical generated
YOLO reports have explicit port/archive status.
