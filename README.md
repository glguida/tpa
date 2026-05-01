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
- `tpa-device/` — structured ET RISC-V device project.
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
```

ET-SoC-1:

```sh
cmake -S . -B build-et-etsoc1 -DET_ROOT=/path/to/et-platform -DTPA_PLATFORM=etsoc1
cmake --build build-et-etsoc1 --target tpa_core
```

Forwarded top-level targets currently include:

- `tpa_core` — selected-platform structured runtime archive in the device
  subproject.
- `tpa_pipe_demo.elf` — minimal real ET RISC-V executable linked against the
  selected HAL/core while full legacy mapper/demo integration is ported.
- `tpa_host_tools` — host subproject package-discovery target documenting that
  full host launcher tooling remains follow-up work.

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
See `docs/USAGE.md` for runtime details, smoke-test caveats, and current
integration limitations.
