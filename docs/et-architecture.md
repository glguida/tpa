# ET Architecture for TPA

This document explains the ET hardware concepts that matter to TPA. It is not a
complete ET architecture manual. It focuses on what program authors, runtime
contributors, and mapper contributors need to know.

## TPA's hardware-facing split

TPA keeps two concerns separate:

- the **virtual program model**: processes, ports, channels/edges, program
  graphs, placement, and generated images;
- the **physical ET realization**: harts, minions, memory domains, fabric,
  cache maintenance, wakeups, toolchains, and host/device launch mechanisms.

The bridge is the HAL plus the mapper:

- platform headers define compile-time constants such as `TPA_HAL_NR_HARTS`,
  `TPA_HAL_CACHELINE_BYTES`, and channel-kind policy;
- HAL implementations provide atomics, cache maintenance, wake/wait, lifecycle,
  tracing, and diagnostics;
- machine JSONs describe mapper-visible execution contexts and communication
  domains;
- generated images consume placement and channel-kind decisions.

## Runtime hart IDs

TPA placement uses **runtime hart IDs**. A `.place` file or mapper output assigns
process instances to these ids.

The selected platform/HAL interprets those ids in terms of actual hardware:

- which minion or shire a hart belongs to;
- which harts share local resources;
- how to wake a remote hart;
- what cache maintenance is required;
- what channel locality class applies when no explicit override is present.

Process code should not decode ET topology directly. It should use TPA APIs and
let placement, image generation, and HAL code handle topology.

## Erbium topology relevant to TPA

For the current structured Erbium path, the useful model is:

- 8 minions;
- 2 harts per minion (`H0` and `H1`);
- a shared MRAM-style memory domain used by the raw Erbium ELF path;
- `erbium_emu` as the currently validated emulator path.

TPA treats runtime harts as placement targets. Conceptually, a minion is the
natural transputer-like node, with `H0` and `H1` corresponding to low/high
priority lanes in the transputer inspiration. The current structured docs should
not overclaim full priority/preemption semantics beyond what the current runtime
and HAL validate.

Erbium generated ELFs use the structured platform/startup assets under
`platform/` and the selected Erbium HAL implementation under `tpa/hal/erbium/`.
Current validated Erbium targets include:

- `tpa_pipe_demo.elf`;
- `tpa_empty.elf`;
- `tpa_yolov5n_downstream.elf`.

`erbium_emu` validation is ET platform validation. Host smoke-test-double builds
are not.

## ET-SoC-1 topology relevant to TPA

ET-SoC-1 is modeled as a larger machine with:

- multiple shires;
- minions within each shire;
- harts within minions;
- shire-local memory/scratchpad resources;
- a same-device fabric/NoC connecting non-local shire resources.

For TPA, the important distinction is not every low-level topology label. The
important mapping questions are:

- what execution contexts are legal for this workload;
- what memory homes exist;
- which contexts share scratch or compute resources;
- whether an edge is direct, local, fabric, or external;
- what communication cost and memory cost a mapped edge implies.

The structured ET-SoC-1 path currently validates `tpa_core` in the default
one-shire configuration. YOLO mapping uses the full-card machine description;
therefore `BUILD_TPA_YOLOV5N` defaults off for ET-SoC-1 unless
`TPA_ETSOC1_NR_SHIRES=32` is provided.

`TPA_ETSOC1_NR_SHIRES` is the number of shires for which TPA runtime queues are
compiled, starting at shire 0. It is not the same as a host launch mask. A host
or sysemu launch may choose which harts to run, but harts outside the compiled
TPA queue range cannot participate in TPA work.

## Channel locality classes

TPA's process-facing send/recv contract should stay uniform. The mapper and
image generator classify each connection into a transport/memory class.

Current classes are:

### `direct`

Use when the producer and consumer can hand off data in the most local context.
This is the cheapest class and should not imply off-context storage.

### `local`

Use when producer and consumer share a local memory domain. On Erbium this can
mean the shared local machine domain. On ET-SoC-1 this usually means a shire or
other local domain described by the machine model.

### `fabric`

Use for same-device, non-local communication. On ET-SoC-1 this corresponds to
communication across the device fabric/NoC between non-local domains. The
channel remains a TPA edge; the physical access path changes.

### `external`

Reserved for off-device communication, such as future peer-to-peer or
host/device network paths. It is part of the model but not a currently validated
runtime path.

A port is never inherently direct/local/fabric/external. The mapped edge between
two ports receives the class.

## Machine JSON relationship to architecture

The planner consumes machine descriptions under `machines/`:

- `machines/single-minion.json`;
- `machines/erbium.json`;
- `machines/etsoc1.json`.

These files are executable approximations used by the mapper, not full ET
architecture specifications. They describe mapper-visible contexts and
communication domains such as:

- `direct_domain`;
- `local_domain`;
- `device_domain`;
- communication costs;
- repeated context grids.

The mapper derives channel kind roughly as:

- different device domain -> `external`;
- same direct domain -> `direct`;
- same local domain -> `local`;
- same device domain -> `fabric`.

Future richer machine descriptions should model shared compute engines, scratch
domains, memory homes, startup/per-byte costs, and capacity constraints more
explicitly. Until then, docs should present the JSONs as the current mapper
input format, not as a complete hardware model.

## ET platform CMake model

The structured repo's primary build is the ET superbuild:

- top-level `CMakeLists.txt` discovers `ProjectFunctions.cmake` from
  `ET_ROOT`, `ET_PLATFORM_PATH`, `CMAKE_MODULE_PATH`, or `/opt/et`;
- `DeviceProjectNoInstall(tpa-device ...)` configures the device project;
- `HostProjectNoInstall(tpa-host ...)` configures the host project;
- `tpa-device` requires the ET RISC-V toolchain and `et-common-libs::cm-umode`;
- `tpa-host` builds `tpa_launcher` against ET host/runtime/device packages.

Do not describe old CMake presets as the current structured workflow. Use the
explicit top-level `cmake -S . -B build-et-* ...` flow until a new preset policy
is intentionally added.

## Host launcher and emulator roles

`erbium_emu` is currently used directly to validate Erbium generated ELFs.
`tpa_launcher` is built in the host subproject and loads generated ELFs through
ET runtime device layers. It supports modes such as:

- `sysemu`;
- `pcie`;
- `fake`.

The `fake` mode and host smoke-test-double builds are useful smoke checks, but
they are not substitutes for Erbium or ET-SoC-1 device validation.

## What remains follow-up

Architecture docs should not imply that all original ET/TPA artifacts are fully
validated. The message/channel, queue/scheduler, and negative expected-failure test
assets and ELF build targets are ported under `tests/tpa_msg/`,
`tests/tpa_queue/`, and `tests/tpa_negative/`. Their full behavioral validation,
including expected-failure runtime semantics, remains tied to the full
cooperative runtime scheduler follow-up.

Other missing or partial areas include:

- YOLO block-test CTest wiring;
- active DNN demo and LTFarm build targets (sources are archived under
  `docs/archive/`);
- YOLO model regeneration tools and model artifact policy;
- broader metadata extraction coverage;
- full cooperative runtime scheduler completion.

Those follow-up items are not reasons to use an alternate build path.
