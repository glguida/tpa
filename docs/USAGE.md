# TPA HAL/Core Usage Notes

## Current structure

The runtime is organized around a narrow HAL boundary:

1. Platform headers under `tpa/hal/<platform>/include/tpa/hal/` provide
   compile-time constants and channel placement policy.
2. `tpa/hal/include/tpa/hal.h` declares callable operations for atomics,
   cache maintenance, fences, hart wake/wait, lifecycle hooks, and tracing.
3. Core modules under `tpa/lib/` use only the HAL-facing API.

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

## Limitations

The repository is still in the extraction/integration phase. There is not yet a
single finalized build invocation or complete runtime startup example. The
examples are intentionally small and are validated with strict syntax checks
against the current public headers. Full platform execution still requires the
appropriate Erbium or ET-SoC-1 runtime/toolchain integration.

The planner/mapper tooling is Python-only for now. CMake targets that build real
process objects and extract process JSON metadata have not yet been ported, so a
full demo still needs the process-build and runtime integration work.
