# TPA Structured Runtime

This repository is being split into a platform-independent TPA core and
platform-specific HAL implementations.

## Layout

- `tpa/hal/include/tpa/hal.h` — core-facing HAL API used by `tpa/lib`.
- `tpa/hal/erbium/` — Erbium compile-time configuration and HAL operations.
- `tpa/hal/etsoc1/` — ET-SoC-1 compile-time configuration and HAL operations.
- `tpa/lib/include/tpa/` — public core headers for scheduler, process, and
  channel modules.
- `tpa/lib/src/` — platform-independent core module implementations.
- `examples/` — syntax-checkable snippets showing the intended include and
  usage pattern.

## Platform selection

Code that needs public TPA core types must select exactly one platform before
including core headers that depend on layout constants:

```c
#include <tpa/hal/erbium.h>   /* or <tpa/hal/etsoc1.h> */
#include <tpa/scheduler.h>
#include <tpa/process.h>
#include <tpa/channel.h>
```

Platform selection supplies compile-time constants such as
`TPA_HAL_NR_HARTS`, `TPA_HAL_CACHELINE_BYTES`, and `TPA_HAL_CH_KIND()`. Core
code should include `tpa/hal.h` or the core headers only; it must not include
Erbium or ET-SoC-1 private implementation files.

See `docs/USAGE.md` for details and current integration limitations.
