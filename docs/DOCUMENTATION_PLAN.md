# TPA Documentation Plan

This document is the knowledge inventory and documentation architecture for the
structured TPA repository. It does not rewrite the project documentation; it
identifies the current sources of truth, assigns them to a final documentation
shape, and defines follow-up jobs.

## Audience model

### New human user

Needs a short answer to: what is TPA, what can I build today, and what is still
experimental? This audience should start with `README.md`, then
`docs/overview.md`, then `docs/build-and-run.md`.

### AI coding agent

Needs operational guardrails before editing. This audience needs a root
`AGENTS.md` with read order, current build facts, validation commands, and traps
such as not inventing a second CMake path or treating host smoke tests as ET
platform validation.

### Runtime/HAL contributor

Needs the split between platform-independent core, HAL operations, Erbium vs
ET-SoC-1 assumptions, and ET superbuild rules. This audience needs
`docs/et-architecture.md`, `docs/build-and-run.md`, and the implementation notes
in `docs/limitations.md`.

### TPA program author

Needs to go from a computation to process code, `.tpm`, `.tpp`, placement or
mapper output, CMake integration, device build, and run/validation. This
audience needs `docs/programming-model.md` and `docs/creating-programs.md`.

### Mapper/planner contributor

Needs terminology for process metadata, machine JSONs, scratch domains,
edge-buffer planning, cost models, and current approximations. This audience
needs `docs/mapper-planner.md` and `docs/memory-and-edge-buffers.md`.

## Proposed final documentation tree

The final tree should be small at the top level and detailed under `docs/`:

```text
README.md
AGENTS.md
docs/
  overview.md
  programming-model.md
  et-architecture.md
  build-and-run.md
  creating-programs.md
  mapper-planner.md
  memory-and-edge-buffers.md
  yolo-demo.md
  limitations.md
  DOCUMENTATION_PLAN.md
planner/
  README.md
```

### `README.md` — project introduction and quick starts

Purpose: a concise project entry point.

Should contain:

- one-paragraph explanation of TPA;
- repository layout;
- current validated quick starts for Erbium, ET-SoC-1, planner tests, host
  launcher, and host smoke-test doubles;
- pointers to detailed docs;
- a short honest status paragraph.

It should keep current facts: ET superbuild via `ProjectFunctions.cmake`,
`DeviceProjectNoInstall`, `HostProjectNoInstall`; `tpa_pipe_demo.elf`,
`tpa_empty.elf`, `tpa_yolov5n_downstream*` targets; `tpa_launcher`; ET-SoC-1
`tpa_core`; host smoke tests are only test doubles.

### `AGENTS.md` — coding-agent orientation and workflow rules

Purpose: prevent future agents from undoing the port by guessing.

Should contain:

- read order;
- current source of truth for build/run commands;
- exact validation command groups;
- rules for CMake integration;
- traps and anti-patterns;
- status of current limitations.

### `docs/overview.md` — what TPA is and why it exists

Purpose: teach the conceptual project in approachable language.

Should absorb:

- old `README.md` introduction;
- old `TPA.md` virtual vs physical split;
- selected background from `TRANSPUTER.md` and `HOARE.md`.

### `docs/programming-model.md` — processes, channels, continuations, artifacts

Purpose: normative terminology for process authors and reviewers.

Should explain:

- process kind vs process instance vs module;
- ports vs channels/edges;
- continuations and `tpa_op_t` operations;
- `.tpm`, `.tpp`, `.place`;
- image generation and boot sections.

Should absorb old `PROCESS_MODEL.md`, parts of `TPA.md`, and parts of
`GENERATION.md`.

### `docs/et-architecture.md` — Erbium and ET-SoC-1 topology relevant to TPA

Purpose: explain how the TPA model maps to ET hardware without exposing every ET
detail.

Should cover:

- Erbium minions/harts/MRAM;
- ET-SoC-1 shires, minions, L2, NoC, U-mode path;
- runtime hart IDs and HAL mapping;
- direct/local/fabric/external channel classes;
- `TPA_ETSOC1_NR_SHIRES` and YOLO full-card behavior.

Should absorb old `TOPOLOGY.md`, ET parts of `TPA.md`, and build-relevant facts
from current CMake.

### `docs/build-and-run.md` — ET superbuild, device/host, emulator, smoke tests

Purpose: operational runbook for current structured repo.

Should cover:

- `ET_ROOT`/`ET_PLATFORM_PATH` discovery;
- Erbium configure/build/run commands;
- ET-SoC-1 configure/build commands;
- `tpa_launcher` usage for `sysemu`, `pcie`, and `fake` modes;
- planner venv setup when YOLO CMake targets need Python imports;
- host smoke-test-double mode and its non-validation status.

Should absorb old `RUNNING.md` while adapting paths and target names to the
structured repo.

### `docs/creating-programs.md` — from computational problem to device program

Purpose: a practical authoring guide.

Should walk through the full pipeline from problem decomposition to validated
ELF. It should use `kernels/tpa_pipe_demo.*` as the small example and then point
to YOLO as the larger example.

### `docs/mapper-planner.md` — mapper theory, machine JSONs, outputs, limits

Purpose: explain the planner package and CMake-integrated mapper path.

Should cover:

- process metadata extraction;
- `tpa-plan-program`;
- `tpa-map-program`;
- `machines/*.json`;
- performance-first mapping under hard memory budget;
- output artifacts: plan JSON, map JSON, mapped-program JSON, `.place`, scratch
  header, edge config header;
- current simplified model and limitations.

Should absorb old `planner/README.md`, old `MAPPING.md`, and current
`planner/README.md`.

### `docs/memory-and-edge-buffers.md` — process state, scratch, edge memory, reuse

Purpose: normative memory model.

Should explain:

- immutable model data;
- persistent process data;
- scratch;
- channel/edge data;
- edge ownership and lifetime;
- scratch domains and edge-buffer planning;
- zero-copy cases and current conservative approximations.

Should absorb old `MEMORY.md`, `EDGE_BUFFER.md`, and `STATUS.md` memory sections.

### `docs/yolo-demo.md` — YOLO path as an end-to-end example

Purpose: document the largest currently ported example.

Should cover:

- why YOLO is split at graph boundaries rather than every primitive op;
- process inventory and channel inventory;
- downstream vs full/demo status;
- CMake planner/map targets;
- build/run commands for `tpa_yolov5n_downstream.elf`;
- ET-SoC-1 full-card shire caveat;
- remaining follow-up for full/demo host launcher integration and block-test
  CTest wiring.

Should absorb old `YOLO_TPA_PLAN.md`, `generated/yolov5n_tpa_plan.md`, current
`yolov5n/CMakeLists.txt`, and current validation logs.

### `docs/limitations.md` — honest status/follow-up work

Purpose: keep status from being scattered through every doc.

Should record:

- current validated targets;
- host launcher status;
- simple demo and YOLO downstream status;
- full cooperative runtime scheduler limitation;
- broader metadata extraction coverage still needed;
- YOLO full/demo host launcher and block-test CTest follow-up;
- message tests and ltfarm follow-up;
- planner/model limitations.

## Source-to-target migration map

| Source | Target doc(s) | Valuable material to migrate |
|---|---|---|
| old `AGENTS.md` | `AGENTS.md`, `docs/build-and-run.md`, `docs/creating-programs.md` | Read order, no alternate build path rule, process/state/scratch/edge distinctions, validation command style. Update old paths and stale status. |
| old `README.md` | `README.md`, `docs/overview.md`, `docs/programming-model.md`, `docs/build-and-run.md` | Intro to TPA, `.tpm/.tpp/.place` explanation, example graphs, basic build/run structure. Replace old presets with structured ET superbuild commands. |
| old `TPA.md` | `docs/overview.md`, `docs/programming-model.md`, `docs/et-architecture.md` | Virtual/physical layers, process/channel/scheduler model, Erbium/ET-SoC-1 mapping, channel locality classes. |
| old `TPA_IMPL.md` | `docs/programming-model.md`, `docs/limitations.md`, `AGENTS.md` | Practical implementation order, process/image/boot model, non-goals. Mark historical implementation checklist vs current structured state. |
| old `ARCHITECTURE.md` | `docs/overview.md`, `docs/programming-model.md`, `docs/mapper-planner.md`, `docs/limitations.md` | Best high-level architecture: process code, graph, mapping, runtime/image execution; working rules; extension points. Adapt `arch_*` names to current HAL surface where needed. |
| old `PROCESS_MODEL.md` | `docs/programming-model.md`, `docs/creating-programs.md` | Normative terms: process kind, instance, port, program, mapped program, image, module. |
| old `TOPOLOGY.md` | `docs/et-architecture.md`, `docs/mapper-planner.md` | Topology entities, execution contexts, machine JSON concept, communication domain classification. |
| old `MEMORY.md` | `docs/memory-and-edge-buffers.md`, `docs/mapper-planner.md` | Four memory classes, ET-SoC-1 memory-home policy, process contract, required analyses. |
| old `MAPPING.md` | `docs/mapper-planner.md`, `docs/memory-and-edge-buffers.md` | Performance-first under hard memory budget, mapper inputs/outputs, staged strategy, fit definition. |
| old `EDGE_BUFFER.md` | `docs/memory-and-edge-buffers.md`, `docs/mapper-planner.md` | Edge ownership, fanout, lifetime, physical placement, reuse/coloring, relationship to mapper. |
| old `GENERATION.md` | `docs/programming-model.md`, `docs/creating-programs.md`, `AGENTS.md` | Existing generation contract, meaning of `.tpm/.tpp/.place`, `add_tpa_process`, `add_tpa_program`, `gen_tpa_image.cmake`, what not to bypass. |
| old `RUNNING.md` | `docs/build-and-run.md`, `docs/yolo-demo.md` | Operational runbook style, generated artifact locations, host launcher concepts. Must be rewritten for current `build-et-*` paths and current target status. |
| old `STATUS.md` | `docs/limitations.md`, `docs/memory-and-edge-buffers.md`, `docs/mapper-planner.md`, `docs/yolo-demo.md` | Current-direction framing, memory/planner/YOLO status categories. Use only after reconciling with current structured repo state. |
| old `TRANSPUTER.md` | `docs/overview.md`, `docs/programming-model.md` | Background on transputer process scheduling, rendezvous channels, two priorities, ALT/timers. Keep concise; do not make it the main user path. |
| old `HOARE.md` | `docs/overview.md` | CSP motivation: synchronous communication, processes as interaction patterns, buffering via processes. Keep as conceptual background. |
| old `planner/README.md` | `docs/mapper-planner.md`, current `planner/README.md` | Planner package split, process metadata fields, command examples, generated outputs, Python selection. Update stale limitations. |
| old `YOLO_TPA_PLAN.md` | `docs/yolo-demo.md`, `docs/creating-programs.md` | Process boundary rule, YOLO process graph, channel inventory, representative tests, non-goals. |
| old `generated/yolov5n_tpa_plan.md` | `docs/yolo-demo.md` | Step-zero process/channel inventory tables and representative block tests. |
| current `README.md` | `README.md`, `docs/build-and-run.md`, `docs/limitations.md` | Current ET commands, host launcher, targets, smoke-test warning. |
| current `docs/USAGE.md` | `docs/build-and-run.md`, `docs/programming-model.md`, `docs/limitations.md` | Current structure and known limitations. Eventually split into focused docs. |
| current `planner/README.md` | `docs/mapper-planner.md` and keep as package quick reference | Current CMake-integrated planner target examples and standalone mapper invocation. |
| current CMake files | `docs/build-and-run.md`, `docs/creating-programs.md`, `docs/yolo-demo.md`, `AGENTS.md` | Actual target names/options: `BUILD_TPA_YOLOV5N`, `TPA_HOST_SMOKE_TEST_DOUBLE`, `TPA_PLATFORM`, forwarded targets, Python selection. |
| current `planner/src/tpa_planner/` and `machines/` | `docs/mapper-planner.md` | CLI behavior, machine JSON schema as implemented, edge planning currently available. |
| job specs/logs | `docs/limitations.md`, `docs/build-and-run.md`, `docs/yolo-demo.md` | Validated commands and status transitions from ET integration, planner, demo runtime, YOLO, host launcher, final audit. |

## Agent orientation design

Root `AGENTS.md` should be short and prescriptive.

### Read order

1. `README.md`
2. `docs/overview.md`
3. `docs/build-and-run.md`
4. `docs/programming-model.md`
5. `docs/creating-programs.md`
6. `docs/mapper-planner.md`
7. `docs/memory-and-edge-buffers.md`
8. `docs/limitations.md`
9. `docs/yolo-demo.md` when touching YOLO

### Non-negotiable build rules

- Do not invent alternate CMake paths.
- The primary product is the ET superbuild, not local host static libraries.
- Use top-level `CMakeLists.txt` with `ET_ROOT`/`ET_PLATFORM_PATH`.
- Device projects go through `DeviceProjectNoInstall(tpa-device ...)`.
- Host tools go through `HostProjectNoInstall(tpa-host ...)`.
- Host smoke-test-double mode is only syntax/unit smoke and is not platform
  validation.
- Do not hide missing ET integration behind host fallback HAL behavior.

### Program authoring rules

- Process implementation is C plus a `.tpm` process manifest.
- Program graph is `.tpp`.
- Mapping is hand `.place` or mapper-generated `.place` plus generated scratch
  and edge config.
- Integrate with `add_tpa_process()` and `add_tpa_program()`.
- Do not handcraft a standalone ELF when a program should be a TPA graph.

### Planner/mapper rules

- Install/use the local `planner/` package.
- Use structured `machines/*.json`.
- Keep performance-first-under-memory-budget framing.
- Treat edge memory, scratch, and process state as separate concepts.

### Validation commands

`AGENTS.md` should list tiers:

- Docs-only: `python -m unittest discover -s planner/tests` when planner docs or
  examples changed; run the documentation sanity checks requested by jobs.
- Host smoke: `cmake -S . -B build-smoke -DTPA_HOST_SMOKE_TEST_DOUBLE=ON`, build,
  and `ctest`.
- ET Erbium: configure with `/opt/et`, build `tpa_host_tools`, `tpa_pipe_demo.elf`,
  `tpa_empty.elf`, YOLO downstream planner/map/ELF targets, run emulator where
  relevant.
- ET-SoC-1: configure `TPA_PLATFORM=etsoc1`, build `tpa_core`; only enable YOLO
  with `TPA_ETSOC1_NR_SHIRES=32`.

### Common traps

- Confusing host smoke tests with ET validation.
- Confusing process kind with process instance.
- Treating ports as transport classes; transport belongs to mapped edges.
- Counting output payloads as process-owned memory at the program-model level.
- Optimizing mapper output for low memory before checking whether the
  performance-first mapping fits.
- Using stale old-repo paths or presets in structured docs.
- Forgetting to pass/select a Python environment with the planner package for
  YOLO CMake planner targets.

## Program authoring pipeline outline

1. **Identify the computational graph.** Find natural process boundaries at
   fan-out, fan-in, named tensor lifetime boundaries, or independently placeable
   coarse work. Do not split straight-line compute just to look concurrent.
2. **Choose process kinds and ports.** For each reusable behavior, define input
   and output ports and decide whether it is `user` or `sys`.
3. **Write continuation-style process code.** Implement start and resume
   continuations that return `tpa_op_t` values such as `tpa_send`, `tpa_recv`,
   `tpa_yield`, or `tpa_stop`. Keep scratch transient and persistent process
   state explicit.
4. **Declare process manifests (`.tpm`).** For each process kind, declare
   `pdef <name> <user|sys> <pid> <start> <ws_sz>` and `port` lines.
5. **Describe the program graph (`.tpp`).** Instantiate process kinds with
   `inst` lines and connect output ports to input ports with `conn` lines and
   byte capacities.
6. **Choose placement.** For small programs, write a hand `.place`. For mapped
   programs, run the planner/mapper to generate placement, scratch config, edge
   config, mapped-program JSON, and reports.
7. **Integrate with CMake.** Use `add_tpa_process()` for each process target and
   `add_tpa_program()` for the ELF target. Do not bypass `gen_tpa_image.cmake`.
8. **Extract metadata and map if needed.** For planner-backed paths, ensure the
   process targets have metadata JSON targets and the CMake target invokes
   `tpa_planner.extract_process_metadata`, `plan_program`, and/or `map_program`.
9. **Build the ET target.** Configure the top-level ET superbuild with
   `ET_ROOT=/opt/et` and `TPA_PLATFORM=erbium` or `etsoc1`, then build the
   forwarded target.
10. **Run under available execution path.** Use `erbium_emu` for Erbium ELFs and
    `tpa_launcher` for host runtime sysemu/pcie/fake modes as appropriate.
11. **Validate outputs.** Check emulator PASS markers, launcher exit status,
    generated planner/map artifacts, and any application-level hashes or output
    checks.

## Follow-up job breakdown

### `docs-02-theory-architecture`

Scope:

- Create `docs/overview.md`, `docs/programming-model.md`, and
  `docs/et-architecture.md`.
- Migrate/adapt conceptual material from old `TPA.md`, `ARCHITECTURE.md`,
  `PROCESS_MODEL.md`, `TOPOLOGY.md`, `TRANSPUTER.md`, and `HOARE.md`.
- Keep current structured HAL and ET superbuild naming, not old `arch_*`-only
  assumptions.

Acceptance:

- New docs explain what TPA is, the core process/channel model, and how Erbium
  and ET-SoC-1 topology matter.
- No stale old-repo command paths as primary instructions.

### `docs-03-program-authoring-guide`

Scope:

- Create `docs/creating-programs.md` and expand `docs/build-and-run.md`.
- Use `kernels/tpa_pipe_demo.*` as the small worked example.
- Explain `.c` + `.tpm` + `.tpp` + `.place`, CMake integration, generated image
  C, ET build, emulator run, and host launcher usage.

Acceptance:

- A new program author can create and build a small TPA program without reading
  old repo docs.
- Host smoke tests are clearly separated from ET validation.

### `docs-04-mapper-planner-guide`

Scope:

- Create `docs/mapper-planner.md` and `docs/memory-and-edge-buffers.md`.
- Migrate/adapt old `MAPPING.md`, `MEMORY.md`, `EDGE_BUFFER.md`, old/current
  planner READMEs, machine JSON facts, and current YOLO downstream CMake target
  flow.

Acceptance:

- Docs explain planner inputs, outputs, machine JSONs, process metadata,
  scratch, edge buffers, mapper objective, and limitations.
- Commands for planner tests/help and YOLO downstream planner/map targets are
  current.

### `docs-05-readme-agents-polish`

Scope:

- Rewrite/polish top-level `README.md`.
- Add root `AGENTS.md` using the orientation design above.
- Ensure `planner/README.md` remains a package quick reference and links to the
  deeper docs.

Acceptance:

- New human users and coding agents get the correct read order and build/run
  commands.
- Agent guardrails explicitly prevent alternate CMake paths and host-smoke
  validation confusion.

### `docs-06-final-docs-review`

Scope:

- Review all docs for consistency, stale claims, broken links, command drift,
  unsupported feature claims, and duplicated/conflicting terminology.
- Run documentation sanity checks and lightweight validation commands.

Acceptance:

- `README.md`, `AGENTS.md`, `docs/*.md`, and `planner/README.md` agree on
  current status and limitations.
- Follow-up implementation limitations are centralized in `docs/limitations.md`.
