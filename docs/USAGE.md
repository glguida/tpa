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
5. `tpa-host/` configures against et-platform host packages. The full launcher
   and demo tooling are explicit follow-up work.

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
cmake --build build-et-erbium --target tpa_pipe_demo.elf

cmake -S . -B build-et-etsoc1 -DET_ROOT=/path/to/et-platform -DTPA_PLATFORM=etsoc1
cmake --build build-et-etsoc1 --target tpa_core
```

The top-level CMake discovers `ProjectFunctions.cmake`, calls
`DeviceProjectNoInstall(tpa-device ...)`, and calls
`HostProjectNoInstall(tpa-host ...)`. `tpa-device` fails during configure if the
ET RISC-V toolchain or required ET CMake packages are unavailable.

### Host smoke-test doubles (not platform validation)

```sh
cmake -S . -B build-smoke -DTPA_HOST_SMOKE_TEST_DOUBLE=ON
cmake --build build-smoke
ctest --test-dir build-smoke --output-on-failure
```

This mode is for local syntax and unit smoke only. It is intentionally named as
a test double and defines `TPA_HOST_SMOKE_TEST_DOUBLE=1`; real device builds do
not silently use host atomic/cache/fence fallbacks.

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
- `tpa-device/examples/tpa_pipe_demo.c` is a minimal ET RISC-V target linked
  with the selected real HAL while the complete legacy TPA mapper/demo stack is
  ported.

## Current limitations / follow-up

- The structured host project validates ET host package discovery but does not
  yet port the original `tpa_launcher` implementation.
- The full legacy TPA program generator, placement mapper, JSON planner targets,
  YOLO demos, message tests, and ltfarm experiments still need ordered porting
  into the structured tree.
- `tpa_pipe_demo.elf` is currently a minimal real device-link target, not the
  complete original pipe demo workload.
