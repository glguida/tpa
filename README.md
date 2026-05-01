# TPA Structured Runtime

This repository is being split into a platform-independent TPA core,
platform-specific HAL implementations, ET platform CMake projects, and offline
planner/mapper tooling. The primary runtime build is the ET superbuild path: the
top-level project discovers et-platform's `ProjectFunctions.cmake` and
configures structured `tpa-device` and `tpa-host` subprojects through
`DeviceProjectNoInstall()` and `HostProjectNoInstall()`.

Host-only builds are retained only as explicitly named smoke-test doubles; they
are not platform validation.

## Layout

- `CMakeLists.txt` — ET superbuild entry point.
- `tpa-device/` — structured ET RISC-V device project and runtime link harness.
- `kernels/` — ported original TPA process/program demo manifests and sources.
- `yolov5n/` — ported YOLOv5n process sources, manifests, planner reports, mapper targets, and downstream demo target.
- `tests/yolo/` — representative original YOLO block test sources/assets retained for follow-up block-test integration.
- `cmake/tpa-kernel.cmake` — structured `add_tpa_process()` / `add_tpa_program()` helpers.
- `platform/` — Erbium startup/linker assets used by generated demo ELFs.
- `tpa-host/` — structured ET host integration project. Full launcher/demo
  tooling is still being ported.
- `tpa/hal/include/tpa/hal.h` — core-facing HAL API used by `tpa/lib`.
- `tpa/hal/erbium/` — Erbium compile-time configuration and HAL operations.
- `tpa/hal/etsoc1/` — ET-SoC-1 compile-time configuration and HAL operations.
- `tpa/lib/include/tpa/` — public core headers for scheduler, process, and
  channel modules.
- `tpa/lib/src/` — platform-independent core module implementations.
- `examples/` — syntax-checkable snippets showing include and usage patterns.
- `planner/` — Python offline process metadata extraction, mapping, and
  planning tools.
- `machines/` — machine topology JSON inputs consumed by the planner/mapper.

## ET platform configure and build

Set either `ET_ROOT` or `ET_PLATFORM_PATH` to an et-platform install/root that
contains `ProjectFunctions.cmake`, `riscv64-ec-toolchain.cmake`, and the ET
CMake packages.

Erbium:

```sh
cmake -S . -B build-et-erbium -DET_ROOT=/path/to/et-platform -DTPA_PLATFORM=erbium
cmake --build build-et-erbium --target tpa_pipe_demo.elf
cmake --build build-et-erbium --target tpa_empty.elf
cmake --build build-et-erbium --target tpa_yolov5n_downstream_plan_planner_json
cmake --build build-et-erbium --target tpa_yolov5n_downstream_map_mapped_program
cmake --build build-et-erbium --target tpa_yolov5n_downstream.elf
/opt/et/bin/erbium_emu -elf_load build-et-erbium/tpa-device-prefix/src/tpa-device-build/kernels/tpa_pipe_demo.elf -max_cycles 10000
/opt/et/bin/erbium_emu -elf_load build-et-erbium/tpa-device-prefix/src/tpa-device-build/yolov5n/tpa_yolov5n_downstream.elf -max_cycles 10000
```

ET-SoC-1:

```sh
cmake -S . -B build-et-etsoc1 -DET_ROOT=/path/to/et-platform -DTPA_PLATFORM=etsoc1
cmake --build build-et-etsoc1 --target tpa_core
```

Forwarded top-level targets currently include:

- `tpa_core` — selected-platform structured runtime archive in the device
  subproject.
- `tpa_empty.elf` — generated from the original `tpa_empty.c/.tpm/.tpp/.place` path.
- `tpa_pipe_demo.elf` — generated from the original pipe demo process/program manifests through structured `add_tpa_process()` / `add_tpa_program()` helpers, linked with Erbium startup and the selected HAL/core.
- `tpa_yolov5n_downstream_plan_planner_json` — extracts process metadata and writes the planner JSON report.
- `tpa_yolov5n_downstream_map_mapped_program` — writes mapped-program JSON, generated placement, scratch config, and edge-buffer config.
- `tpa_yolov5n_downstream.elf` — generated YOLO downstream device ELF linked with the structured runtime harness.
- `tpa_host_tools` — host subproject package-discovery target documenting that
  full host launcher tooling remains follow-up work.

For ET-SoC-1, YOLO mapping uses the full-card machine JSON. The device project therefore defaults `BUILD_TPA_YOLOV5N=OFF` unless `TPA_ETSOC1_NR_SHIRES=32` is provided; `tpa_core` still builds in the default one-shire ET-SoC-1 configuration.

## Local smoke-test doubles

For machines without et-platform, a syntax/build smoke mode is available:

```sh
cmake -S . -B build-smoke -DTPA_HOST_SMOKE_TEST_DOUBLE=ON
cmake --build build-smoke
ctest --test-dir build-smoke --output-on-failure
```

This mode defines `TPA_HOST_SMOKE_TEST_DOUBLE=1` and builds host test doubles
only. It must not be used as evidence that Erbium or ET-SoC-1 device platform
integration works.

## Platform selection in code

Code that needs public TPA core types must select exactly one platform before
including core headers that depend on layout constants:

```c
#include <tpa/hal/erbium.h>   /* or <tpa/hal/etsoc1.h> */
#include <tpa/scheduler.h>
#include <tpa/process.h>
#include <tpa/channel.h>
```

Core code should include `tpa/hal.h` or the core headers only; it must not
include Erbium or ET-SoC-1 private implementation files.

## Planner/mapper tools

The offline planner package can be installed in editable mode from the
repository root:

```sh
python3 -m venv .venv-planner
. .venv-planner/bin/activate
python -m pip install -e planner
python -m unittest discover -s planner/tests

tpa-map-program --help
tpa-plan-program --help
tpa-extract-process-json --help
```

See `planner/README.md` for mapper command examples and current limitations.
See `docs/USAGE.md` for runtime demo details, smoke-test caveats, and current
integration limitations.
