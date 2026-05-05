# TPA Overview

TPA is a transputer-style process/channel runtime and build/mapping flow for ET.
It combines a small continuation runtime, static program-image generation,
offline planner/mapper tooling, and ET-platform CMake integration so a program
can be described as a graph of communicating processes and built as a mapped ET
device ELF.

TPA is not only a library. The useful unit is the whole flow:

```text
hardware-independent process code + process manifests
        + hardware-independent program graph
        + mapper-generated or small/debug hand placement
        -> generated image C
        -> ET device ELF
        -> emulator / launcher / hardware validation
```

## Why TPA exists

ET machines expose many hardware contexts, memory domains, and communication
paths. Large static workloads such as model subgraphs need more than a thread
pool: they need a way to state the graph, name the communication edges, choose
placement, reason about scratch and edge memory, and generate a device image
that the runtime can execute.

TPA provides that structure:

- **processes** express reusable pieces of computation;
- **ports** describe each process kind's local communication interface;
- **program graphs** instantiate process kinds and connect ports;
- **channels/edges** represent the dataflow between process instances;
- **placement or mapping** chooses where instances and channel storage live;
- **image generation** materializes concrete process objects, channel objects,
  port bindings, boot data, and generated backing storage;
- **ET platform builds** compile and link the result for Erbium or ET-SoC-1.

## The conceptual background

TPA borrows the useful operational ideas from the transputer and CSP/occam line
without trying to reproduce old hardware literally.

The ideas that matter are:

- processes communicate through explicit channels;
- communication is a synchronization point rather than a hidden shared-memory
  side effect;
- buffering, fanout, and placement should be explicit program/mapping choices;
- blocked processes should not consume execution resources;
- the same logical program should be mappable onto different physical ET
  topologies.

Hoare's CSP is useful as motivation: communication is an interaction between
processes, not a mailbox implementation detail. The transputer is useful as an
implementation reference: a machine can make process scheduling, priority, and
channel rendezvous cheap when those concepts are first-class. TPA adapts those
ideas to ET's memory-backed communication model.

## Virtual model vs physical model

TPA has two layers.

### Virtual layer

The virtual layer is the program model:

- process kinds;
- process instances;
- ports;
- channel/edge connections;
- continuations;
- program graphs;
- mapped-program artifacts.

This layer should stay independent of runtime hart ids, minions, shires, and
whether a connection is direct, local, fabric-backed, or eventually external.
A `.tpp` graph expresses dataflow between process instances; it is not a
placement file.

### Physical layer

The physical layer is the ET realization:

- Erbium minions, harts, MRAM, and emulator startup;
- ET-SoC-1 shires, minions, harts, shire-local memory, and NoC/fabric;
- ET RISC-V device toolchain and platform-specific device libraries;
- host runtime/device-layer packages;
- `erbium_emu`, `tpa_launcher`, sysemu, pcie, and fake host modes.

The mapper and HAL bridge the two layers. Process code should not need to know
which exact ET transport is chosen for every edge, and ports should not be used
as transport-class labels.

## Main repository pieces

Current structured pieces are:

- `CMakeLists.txt` — ET superbuild entry point. It discovers
  `ProjectFunctions.cmake` and creates `tpa-device` and `tpa-host` subprojects.
- `tpa/` — platform-independent core modules and HAL-facing public headers.
- `tpa/hal/` — Erbium and ET-SoC-1 platform selection and HAL implementations.
- `cmake/tpa-kernel.cmake` — `add_tpa_process()` and `add_tpa_program()`.
- `cmake/gen_tpa_image.cmake` — parses `.tpm`, `.tpp`, and `.place` inputs and
  generates image C.
- `kernels/` — currently ported TPA programs: `tpa_empty`,
  `tpa_pipe_demo`, and the generated tensor matmul demo.
- `attention/` — fixed-size structured fast-attention demo with parallel and
  serial Erbium placements.
- `depth/` — no-weights stereo SAD depth demo for deterministic 96x64
  synthetic grayscale input, using a source/four-worker/checker graph and hand
  Erbium placement.
- `yolov5n/` — currently ported YOLOv5n downstream process sources and CMake
  planner/map/device targets.
- `yolov8n/` — opt-in external-header YOLOv8n downstream milestones for P5-only
  Detect/DFL, sampled P3/P4/P5 Detect/DFL branch plumbing, sampled P3
  `model.15`, P4 `model.18`, and P5 `model.21` C2f source-modules feeding
  Detect/DFL, a sampled combined P3/P4/P5 C2f+Detect downstream graph, and
  dense P5 `model.21` C2f feature-map validation feeding Detect/DFL, with
  mapper-generated placement/edge config.
- `tests/yolo/` — YOLO block-test sources/assets with representative Erbium
  CMake/CTest coverage.
- `planner/` — Python metadata extraction, planning, and mapping package.
- `machines/` — machine JSON topology inputs for the planner/mapper.
- `tpa-host/` — host project containing `tpa_launcher`.

## Current validated paths

The current structured repo validates these paths:

- Erbium ET configure and build through the ET superbuild;
- `tpa_host_tools` builds `tpa_launcher`;
- `tpa_empty.elf`, `tpa_pipe_demo.elf`, `tpa_tensor_matmul.elf`,
  `tpa_fast_attention.elf`, `tpa_fast_attention_serial.elf`, and
  `tpa_stereo_sad.elf` build through the generated TPA process/program flow;
- `tpa_empty.elf`, `tpa_pipe_demo.elf`, `tpa_tensor_matmul.elf`,
  `tpa_fast_attention.elf`, `tpa_fast_attention_serial.elf`, and
  `tpa_stereo_sad.elf` run under `erbium_emu` and report PASS markers;
- representative message/channel and queue regression ELFs build and report
  PASS markers under `erbium_emu`;
- `tpa_negative_expected_fail.elf` builds and reports the expected FAIL marker
  under `erbium_emu`;
- YOLOv5n downstream planner JSON, mapped-program artifacts, downstream device
  ELF, and Erbium PASS-marker runtime path build through CMake;
- YOLOv8n external-header mapper/device targets build when explicitly
  configured with generated header/manifest paths; they validate only
  deterministic synthetic-calibration P5 Detect/DFL, sampled P3/P4/P5
  Detect/DFL, sampled per-scale P3/P4/P5 C2f-to-Detect plumbing hashes, a
  sampled combined P3/P4/P5 C2f+Detect downstream graph, and dense P5 C2f
  feature-map hashes feeding sampled P5 Detect;
- ET-SoC-1 default one-shire `tpa_core` builds;
- host smoke-test-double builds and tests pass, but those are not platform
  validation.

## How the major subsystems fit together

### Runtime and HAL

The runtime-facing core code lives under `tpa/lib`. It talks to hardware through
`tpa/hal.h` and the selected platform header (`tpa/hal/erbium.h` or
`tpa/hal/etsoc1.h`). The HAL provides topology constants, atomics, cache
maintenance, wake/wait operations, lifecycle hooks, tracing, and diagnostics.

Generated graph-program ELFs now link the cooperative runtime scheduler and
execute continuations. The current validated Erbium PASS set covers the empty,
pipe, tensor matmul, fast-attention, and no-weights stereo SAD demos plus
representative message/channel and queue regression tests. The stereo SAD demo
uses deterministic synthetic input and does not depend on external images,
datasets, model weights, or third-party stereo code. It uses hand placement;
mapper-generated placement/report work remains follow-up. YOLOv5n downstream
device-runtime validation covers the CMake planner/map/device path and Erbium
PASS marker. YOLOv8n has opt-in external-header milestones for P5-only
Detect/DFL, sampled P3/P4/P5 Detect/DFL branch plumbing, sampled per-scale
P3/P4/P5 C2f source modules feeding Detect/DFL, a sampled combined P3/P4/P5
C2f+Detect downstream graph, and dense P5 C2f feature-map validation feeding
sampled P5 Detect; dense P3/P4, dense combined/full-model validation, and the
full YOLO host/demo pipeline remain follow-up.

### Image generation

TPA programs are not assembled manually at runtime. `add_tpa_program()` invokes
`gen_tpa_image.cmake`, which reads process manifests, hardware-independent
program graphs, and separate placement files or mapper output. The generated
image C contains concrete processes, channels, port bindings, and boot entries.

### Planner/mapper

The Python planner package can extract process metadata from built objects,
produce planning reports, and map a program onto machine JSON topology. It is the
scalable path for non-trivial or topology-sensitive placement; small hand
`.place` files remain useful for worked examples, deterministic smoke tests, and
mapper/debug inspection. The YOLO downstream CMake path uses the planner to
produce metadata JSON, planner JSON,
map reports, mapped-program JSON, generated placement, scratch config, and edge
config.

### ET platform build

The primary build is the ET superbuild. The top-level CMake discovers
`ProjectFunctions.cmake`, calls `DeviceProjectNoInstall(tpa-device ...)`, and
calls `HostProjectNoInstall(tpa-host ...)`. The device subproject requires an ET
RISC-V toolchain. The host subproject builds `tpa_launcher` against ET host
packages.

## What is intentionally not implied

TPA documentation must not imply that every original repository artifact is
ported. Representative message/channel and queue runtime regression ELFs now
report PASS under Erbium, and the negative expected-failure ELF reports the
intended FAIL marker; broader scheduler coverage remains hardening work. YOLO
tools/models, representative block-test CMake/CTest coverage, YOLOv5n
downstream planner/map/device ELF plus Erbium PASS-marker runtime validation,
and the opt-in YOLOv8n external-header milestones are ported. Full YOLOv8n
graph/model validation and full YOLO host/demo integration remain follow-up. DNN
demos, `ltfarm`, and historical generated YOLO analysis are archived reference
material rather than active runtime inputs.
