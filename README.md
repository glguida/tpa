# TPA Structured Runtime

This repository is being split into a platform-independent TPA core,
platform-specific HAL implementations, and a CMake build that produces one
static TPA library per supported HAL platform.

## Layout

- `tpa/hal/include/tpa/hal.h` — core-facing HAL API used by `tpa/lib`.
- `tpa/hal/erbium/` — Erbium compile-time configuration and HAL operations.
- `tpa/hal/etsoc1/` — ET-SoC-1 compile-time configuration and HAL operations.
- `tpa/lib/include/tpa/` — public core headers for scheduler, process, and
  channel modules.
- `tpa/lib/src/` — platform-independent core module implementations.
- `examples/` — syntax-checkable snippets showing the intended include and
  usage pattern.
- `planner/` — Python offline process metadata extraction, mapping, and
  planning tools.
- `machines/` — machine topology JSON inputs consumed by the planner/mapper.

## Configure, build, and test

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

The default configuration builds all currently supported platform variants:

- `tpa_erbium`
- `tpa_etsoc1`

## Selecting platforms

Use the CMake options below at configure time to enable or disable individual
platform variants:

```sh
cmake -S . -B build -DTPA_BUILD_ERBIUM=ON -DTPA_BUILD_ETSOC1=ON
cmake -S . -B build -DTPA_BUILD_ERBIUM=OFF
cmake -S . -B build -DTPA_BUILD_ETSOC1=OFF
```

Code that needs public TPA core types must select exactly one platform before
including core headers that depend on layout constants:

```c
#include <tpa/hal/erbium.h>   /* or <tpa/hal/etsoc1.h> */
#include <tpa/scheduler.h>
#include <tpa/process.h>
#include <tpa/channel.h>
```

Platform selection supplies compile-time constants such as
`TPA_HAL_NR_HARTS`, `TPA_HAL_CACHELINE_BYTES`, and `TPA_HAL_CH_KIND()`. Each
platform library publishes the common TPA headers and the selected platform's
own HAL include directory. Platform configuration comes from the per-platform
headers under `tpa/hal/<platform>/include`, for example `<tpa/hal/erbium.h>`
and `<tpa/hal/etsoc1.h>`.

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
See `docs/USAGE.md` for runtime details and current integration limitations.
