# TPA HAL/Core Usage Notes

## Current structure

The runtime is organized around a narrow HAL boundary and ET platform build:

1. Platform headers under `tpa/hal/<platform>/include/tpa/hal/` provide
   compile-time constants and channel placement policy.
2. `tpa/hal/include/tpa/hal.h` declares callable operations for atomics,
   cache maintenance, fences, hart wake/wait, lifecycle hooks, and tracing.
3. Core modules under `tpa/lib/` use only the HAL-facing API.
4. `tpa-device/` configures with the ET RISC-V toolchain and links the real
   selected HAL against `et-common-libs::cm-umode`.
5. `kernels/` contains ported original TPA demo assets (`.c`, `.tpm`, `.tpp`, `.place`).
6. `cmake/tpa-kernel.cmake` provides structured `add_tpa_process()` and `add_tpa_program()` helpers that generate image metadata with `gen_tpa_image.cmake`.
7. `yolov5n/` ports the original YOLOv5n process sources/manifests and CMake planner/mapper targets for the downstream graph.
8. `tpa-host/` builds the structured `tpa_launcher` executable against
   et-platform host/runtime packages.
9. `planner/` provides Python-only offline process metadata extraction,
   mapping, and planning commands; `machines/` provides mapper topology inputs.

## Selecting a platform

For Erbium:

```c
#include <tpa/hal/erbium.h>
```

For ET-SoC-1:

```c
#include <tpa/hal/etsoc1.h>
```

Include the selected platform header before public core headers. The platform
header defines `TPA_HAL_NR_HARTS`, `TPA_HAL_CACHELINE_BYTES`, and
`TPA_HAL_CH_KIND()`, then includes the common `tpa/hal.h` declarations.

## Build modes

### ET device/host build (primary)

```sh
cmake -S . -B build-et-erbium -DET_ROOT=/path/to/et-platform -DTPA_PLATFORM=erbium
cmake --build build-et-erbium --target tpa_host_tools
cmake --build build-et-erbium --target tpa_pipe_demo.elf
cmake --build build-et-erbium --target tpa_empty.elf
cmake --build build-et-erbium --target tpa_yolov5n_downstream_plan_planner_json
cmake --build build-et-erbium --target tpa_yolov5n_downstream_map_mapped_program
cmake --build build-et-erbium --target tpa_yolov5n_downstream.elf
/opt/et/bin/erbium_emu -elf_load build-et-erbium/tpa-device-prefix/src/tpa-device-build/kernels/tpa_pipe_demo.elf -max_cycles 10000
/opt/et/bin/erbium_emu -elf_load build-et-erbium/tpa-device-prefix/src/tpa-device-build/yolov5n/tpa_yolov5n_downstream.elf -max_cycles 10000

cmake -S . -B build-et-etsoc1 -DET_ROOT=/path/to/et-platform -DTPA_PLATFORM=etsoc1
cmake --build build-et-etsoc1 --target tpa_core
```

The top-level CMake discovers `ProjectFunctions.cmake`, calls
`DeviceProjectNoInstall(tpa-device ...)`, and calls
`HostProjectNoInstall(tpa-host ...)`. `tpa-device` fails during configure if the
ET RISC-V toolchain or required ET CMake packages are unavailable. The `tpa_host_tools` target builds `tpa_launcher` in the host subproject. The `tpa_pipe_demo.elf`, `tpa_empty.elf`, and `tpa_yolov5n_downstream.elf` targets are generated through the TPA process/program flow, not as handcrafted standalone executables.

### Host launcher

```sh
build-et-erbium/tpa-host-prefix/src/tpa-host-build/tpa_launcher \
  --kernel build-et-erbium/tpa-device-prefix/src/tpa-device-build/kernels/tpa_pipe_demo.elf \
  --mode sysemu \
  --timeout 300
```

`tpa_launcher` supports `--mode sysemu`, `--mode pcie`, and `--mode fake`, plus
sysemu firmware/log/options arguments. Run `tpa_launcher --help` for the full
option list.

YOLO targets currently port the downstream planner/map/device path. On ET-SoC-1, `BUILD_TPA_YOLOV5N` defaults OFF unless `TPA_ETSOC1_NR_SHIRES=32` because the original YOLO mapping uses the full-card machine description.

### Host smoke-test doubles (not platform validation)

```sh
cmake -S . -B build-smoke -DTPA_HOST_SMOKE_TEST_DOUBLE=ON
cmake --build build-smoke
ctest --test-dir build-smoke --output-on-failure
```

This mode is for local syntax and unit smoke only. It is intentionally named as
a test double and defines `TPA_HOST_SMOKE_TEST_DOUBLE=1`; real device builds do
not silently use host atomic/cache/fence fallbacks.

## Planner/mapper workflow

The offline Python planner/mapper package lives in `planner/`, with topology
inputs in `machines/`. Install and smoke-test it from the repository root:

```sh
python3 -m venv .venv-planner
. .venv-planner/bin/activate
python -m pip install -e planner
python -m unittest discover -s planner/tests

tpa-map-program --help
tpa-plan-program --help
tpa-extract-process-json --help
```

The package currently ports the original extract/map/plan Python entry points
and the machine topology inputs used by the mapper. See `planner/README.md` for
example mapper commands.

## Trace analysis tools

Erbium emulator traces can be split and summarized with the ported original
trace scripts:

```sh
tools/trace/split_trace_by_hart.sh /tmp/erbium.log /tmp/tpa-trace-by-hart
tools/trace/split_inst_trace_by_hart.sh /tmp/erbium.log /tmp/tpa-inst-by-hart
tools/trace/analyze_trace_by_hart.sh \
  build-et-erbium/tpa-device-prefix/src/tpa-device-build/kernels/tpa_pipe_demo.elf \
  /tmp/tpa-trace-by-hart/m0_h0.inst.log \
  --top
```

`analyze_trace_by_hart.sh` also supports `--from <cycle>` and `--to <cycle>`.
See `tools/trace/README.md`.

## Archived original-reference material

Original DNN demos, the LTFarm experiment, and historical generated YOLO
analysis files are preserved under `docs/archive/` as reference material, not
active runtime inputs:

- `docs/archive/original-dnn-demos/STATUS.md`
- `docs/archive/ltfarm/STATUS.md`
- `docs/archive/generated-yolo-analysis/STATUS.md`

The structured build should use current generated artifacts from CMake/planner
targets, not archived generated JSON.

## Core concepts

- Scheduler: `tpa/scheduler.h` owns per-hart run queues and remote-ready bell
  state. Wake paths mark a slot ready and ring the destination hart only if it
  is armed.
- Process: `tpa/process.h` owns process layout, slot registration, mailbox
  publish/complete helpers, and process wait-state transitions.
- Channel: `tpa/channel.h` owns channel state transitions. It receives runtime
  callbacks for process wake/run/wait actions so channel code does not depend
  on scheduler internals.

## Examples

- `examples/core_concepts.c` uses fixed host constants to demonstrate the core
  public headers and data flow without selecting a real platform.
- `examples/platform_erbium.c` demonstrates Erbium platform selection and
  compile-time channel policy.
- `examples/platform_etsoc1.c` demonstrates ET-SoC-1 platform selection and
  compile-time channel policy.
- `kernels/tpa_empty.*` is the original empty TPA process/program demo path.
- `kernels/tpa_pipe_demo.*` is the original pipe demo process/program path, built via `add_tpa_process()` / `add_tpa_program()` into `tpa_pipe_demo.elf`.
- `yolov5n/` contains the original YOLOv5n process sources/assets plus downstream planner/map targets and `tpa_yolov5n_downstream.elf`.

## Current limitations / follow-up

- The structured host project ports the original `tpa_launcher` implementation
  for loading generated ELFs through ET runtime device layers.
- The structured demo link harness currently proves generated process/image metadata compile, link, and load/pass in `erbium_emu`; it does not yet implement the complete cooperative runtime scheduler that executes every generated process continuation.
- YOLO full/demo host launcher integration and message tests still need ordered porting into the structured tree. Representative YOLO block-test CMake/CTest coverage is now available under `tests/yolo/`.
- DNN demos and LTFarm are preserved as archived/reference material with
  dependency/status notes rather than active build targets.
- Python mapper/planner commands are ported and the YOLO downstream CMake planner/map targets use them; broader CMake metadata extraction coverage remains follow-up work.
