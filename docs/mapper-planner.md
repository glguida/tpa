# Mapper and Planner Guide

The TPA planner/mapper is a compiler-like subsystem. It takes a TPA program
graph, process metadata, and a machine topology description, then produces
reports and generated artifacts that let the build turn a logical graph into a
mapped device program.

The package lives in `planner/`. This document is the deeper conceptual guide;
`planner/README.md` remains the package quick reference.

## Program author quick start

Author process code, `.tpm` manifests, and `.tpp` graphs as hardware-independent
dataflow first. Keep runtime hart ids and channel transport classes out of those
files.

Then choose one of the current placement paths:

- For tiny worked examples, deterministic smoke tests, or mapper/debug
  inspection, write a small `.place` file and optionally inspect it with
  `tpa-plan-program`.
- For non-trivial, production, or topology-sensitive graphs, use
  `tpa-map-program` to generate placement and mapped artifacts from the `.tpp`,
  process metadata JSON, and a machine JSON.

The generated or hand `.place` is then consumed by `add_tpa_program(PROGRAM ...
PLACEMENT ...)` through `cmake/gen_tpa_image.cmake`. Do not introduce alternate
CMake paths or stale CLI names.

## What the planner and mapper do

A `.tpp` program says which process instances exist and how their ports are
connected. It does not know how expensive a process is, how much scratch a
process may need, which ET contexts exist, or where channel storage should live.

The planner/mapper fills in those missing facts:

- extract process metadata from built process objects;
- parse a `.tpp` graph and optional `.place` file;
- read machine JSON topology;
- choose runtime hart placement for each instance;
- classify each channel as `direct`, `local`, `fabric`, or `external`;
- estimate schedule and memory usage;
- generate placement, scratch, edge-buffer, and mapped-program artifacts.

This is compiler-like because the inputs are high-level program descriptions and
compiled object metadata, and the outputs are lower-level build artifacts used by
image generation and runtime code.

## Current tools

Install the package from the repository root:

```sh
python3 -m venv .venv-planner
. .venv-planner/bin/activate
python -m pip install -e planner
```

Smoke-test the tools:

```sh
python -m unittest discover -s planner/tests
tpa-extract-process-json --help
tpa-plan-program --help
tpa-map-program --help
```

The command-line entry points are:

- `tpa-extract-process-json` — extracts process metadata JSON from built object
  files and the `.tpm` manifest;
- `tpa-plan-program` — produces a planning/report JSON from a `.tpp`, a
  placement, and process metadata;
- `tpa-map-program` — maps a `.tpp` plus process metadata onto a machine JSON
  and writes mapped artifacts.

## Inputs

### Program graph: `.tpp`

The mapper parses `inst` and `conn` records from the `.tpp` file:

```text
inst <inst_id> <pdef_name>
conn <src_inst> <src_port> <dst_inst> <dst_port> <bytes>
```

The current simplified mapper requires the graph to be acyclic.

### Optional placement: `.place`

`tpa-plan-program` uses an existing placement to report topology, per-hart
scratch, and edge lifetimes. A `.place` maps instance ids to runtime hart ids and
may include generated `chan` lines:

```text
201 0
202 2
chan 201 0 202 0 local
```

`tpa-map-program` can generate this file.

### Process metadata JSON

Process metadata is produced from built object files plus the `.tpm` manifest.
The extractor records:

- process definition names, ids, start symbols, ports, and declared workspace;
- `scratch_peak_bytes` from `.tpa.proc.meta` records when present;
- mutable workspace bytes from `.data`, `.sdata`, `.bss`, and `.sbss`;
- immutable model/blob bytes from `.mram_data`;
- other static bytes from relevant read-only/TPA metadata sections;
- embedded edge-payload symbols that should be reclassified out of process data.

The extractor uses RISC-V binutils tools by default, with prefix
`riscv64-unknown-elf-`.

### Compute costs

`tpa-map-program` accepts optional compute-cost hints:

```sh
--compute-costs-json path/to/compute_costs.json
```

The file maps process definition names to numeric costs. If no hint exists, the
current implementation uses a simple fallback: tiny costs for source/fork-like
names and a uniform default cost for other process kinds.

### Machine JSON

A machine JSON defines mapper-visible execution contexts and communication cost
classes. Checked-in topologies are:

- `machines/single-minion.json` — one test minion/context for degenerate mapper
  tests;
- `machines/erbium.json` — Erbium model with 8 useful minions, `h0` contexts,
  one local MRAM-style domain, and one edge pool;
- `machines/etsoc1.json` — ET-SoC-1 full-card model with 32 shires, 32 minions
  per shire, `h0` contexts, shire-local L2 scratchpad pools, and same-device
  fabric communication.

Machine JSONs can use `context_grid` to expand contexts. Important fields are:

- `devices`;
- `local_domains_per_device`;
- `minions_per_local_domain`;
- `harts`;
- `hart_stride`;
- `label_format`;
- `direct_domain_format`;
- `local_domain_format`;
- `device_domain_format`;
- `edge_pool_format`;
- `comm_cost_local`, `comm_cost_fabric`, `comm_cost_external`.

The mapper turns these into normalized contexts with `label`, `hartid`,
`minion`, `hart`, `direct_domain`, `local_domain`, and `device_domain`.

## Communication classes

For every mapped connection, the mapper compares source and destination
contexts:

- different `device_domain` -> `external`;
- same `direct_domain` -> `direct`;
- same `local_domain` -> `local`;
- otherwise same device -> `fabric`.

These classes are edge properties, not port properties. The generated `.place`
can include `chan` lines for the selected class.

## `tpa-plan-program`

`tpa-plan-program` is a report tool for an already placed program. It reads:

```sh
tpa-plan-program \
  --program path/to/program.tpp \
  --placement path/to/program.place \
  --process-jsons path/to/process.json \
  --output /tmp/plan.json
```

The report includes:

- topological order and generations;
- process objects and instance counts;
- per-instance pdef, object label, placement, workspace, scratch, and edge
  summaries;
- per-hart instance lists, scratch peak, and declared workspace totals;
- warnings, such as multiple instances sharing mutable static storage.

Use it when a hand `.place` exists and you want to inspect memory/topology
implications without asking the mapper to choose placement.

## `tpa-map-program`

`tpa-map-program` chooses placement and writes generated artifacts:

```sh
tpa-map-program \
  --program path/to/program.tpp \
  --process-jsons path/to/process.json \
  --compute-costs-json path/to/compute_costs.json \
  --machine-json machines/erbium.json \
  --num-minions 8 \
  --output /tmp/tpa_map.json \
  --mapped-program-out /tmp/tpa_mapped_program.json \
  --placement-out /tmp/tpa_mapped.place \
  --scratch-header-out /tmp/tpa_scratch_config.h \
  --edge-config-header-out /tmp/tpa_edge_config.h
```

`--compute-costs-json` and memory budget arguments are optional. `--machine-json`
is recommended for current structured usage so the mapper uses the checked-in ET
models instead of a fallback single-cluster shape.

## Current algorithm

The current mapper uses a HEFT-like, performance-first list scheduler:

1. Parse the graph and require it to be acyclic.
2. Compute per-instance costs from hints or fallback heuristics.
3. Compute upward ranks using graph successors and default communication cost.
4. Maintain a ready set of instances whose predecessors are scheduled.
5. For each ready instance, evaluate each context and choose the earliest finish
   candidate, using communication costs when predecessors are on different
   contexts.
6. Record placement, schedule, per-context scratch peak, context load, and
   makespan proxy.
7. Evaluate memory: fixed process/model bytes, scratch totals, and planned edge
   buffers.
8. If a hard `--memory-budget-bytes` is provided and the mapping does not fit,
   run greedy memory repair by removing active contexts when doing so reduces
   memory; choose repairs by fit, makespan penalty, and bytes saved.

The objective is performance-first under a hard memory budget. It is not a
low-memory-first mapper.

## Outputs

`tpa-map-program` can write several output files.

### Map report JSON

The main `--output` JSON records:

- assumptions;
- initial candidate summary;
- repair trace;
- selected candidate;
- placement lines;
- channel placement lines;
- mapped instances;
- embedded mapped-program data;
- warnings.

### Mapped-program JSON

`--mapped-program-out` writes `kind: tpa_mapped_program_v0`. It contains:

- source program path;
- selected machine summary;
- memory model (`edge-planned-v0`);
- scratch domains;
- edge memory plan;
- mapped instances;
- mapped connections;
- memory summary.

This is the canonical structured artifact for reviewing mapper output.

### Generated `.place`

`--placement-out` writes instance-to-hart placement plus generated channel class
lines. It can be consumed by `add_tpa_program()` through image generation.

### Scratch header

`--scratch-header-out` writes a generated arena header. Today the YOLO path uses
`YV5N_ARENA_M<minion>_BYTES`, `YV5N_ARENA_STORAGE_BYTES`, and
`YV5N_ARENA_STATE_INITIALIZERS` macros. The header is generated from per-context
scratch peaks and rounded to 64-byte alignment.

### Edge config header

`--edge-config-header-out` writes static edge-pool arrays and per-channel buffer
macros such as `TPA_EDGE_CH_<idx>_BUF0`. This connects the edge-buffer plan to
generated programs that use mapped channel storage.

## CMake-integrated stereo SAD mapper path

The stereo SAD demo exposes CMake mapper/report targets plus a distinct mapped
runtime ELF while preserving its validated hand-placed runtime ELF:

```sh
cmake -S . -B build-et-erbium -DET_ROOT=/opt/et -DTPA_PLATFORM=erbium \
  -DPYTHON=$(command -v python)
cmake --build build-et-erbium --target tpa_stereo_sad_hand_plan_planner_json
cmake --build build-et-erbium --target tpa_stereo_sad_map_mapped_program
cmake --build build-et-erbium --target tpa_stereo_sad.elf
cmake --build build-et-erbium --target tpa_stereo_sad_mapped.elf
```

The map target writes a map report, mapped-program JSON, mapper-generated
placement, scratch header, and edge-buffer config header under the device build
tree. The default `tpa_stereo_sad.elf` target still consumes
`depth/stereo_sad.place`; `tpa_stereo_sad_mapped.elf` consumes the generated
placement and edge-buffer config header from the mapper output.

## CMake-integrated YOLO downstream path

The current structured repo has integrated YOLO downstream planner/map artifact
paths, downstream device ELF link, and Erbium PASS-marker runtime validation.
With a planner-enabled Python environment:

```sh
python3 -m venv .venv-planner
. .venv-planner/bin/activate
python -m pip install -e planner
cmake -S . -B build-et-erbium -DET_ROOT=/opt/et -DTPA_PLATFORM=erbium -DPYTHON=$(command -v python)
cmake --build build-et-erbium --target tpa_yolov5n_downstream_plan_planner_json
cmake --build build-et-erbium --target tpa_yolov5n_downstream_map_mapped_program
cmake --build build-et-erbium --target tpa_yolov5n_downstream.elf
/opt/et/bin/erbium_emu \
  -minions 0x1f \
  -elf_load build-et-erbium/tpa-device-prefix/src/tpa-device-build/yolov5n/tpa_yolov5n_downstream.elf \
  -max_cycles 100000000
```

`yolov5n/CMakeLists.txt` wires these steps by:

- selecting `machines/erbium.json` or `machines/etsoc1.json`;
- prepending this repository's `planner/src` to `PYTHONPATH` for CMake planner
  commands so stale globally installed `tpa_planner` packages are not used;
- checking that the selected Python can import `tpa_planner`;
- extracting process metadata for each YOLO process target;
- producing planner reports with `tpa_planner.plan_program`;
- producing mapped program, placement, scratch header, and edge header with
  `tpa_planner.map_program`;
- passing generated placement and edge config into `add_tpa_program()`.

For ET-SoC-1, the YOLO mapping uses the full-card `machines/etsoc1.json` model.
The device project therefore requires `TPA_ETSOC1_NR_SHIRES=32` for YOLO; the
default one-shire ET-SoC-1 configuration validates `tpa_core` but does not build
YOLO by default.

## Limitations and follow-up

Current limitations are explicit and should not be hidden:

- the mapper model is simplified and acyclic;
- compute costs require hints for realistic scheduling;
- machine JSONs are mapper-visible approximations, not full ET architecture
  specifications;
- broader process metadata extraction coverage remains follow-up;
- YOLO downstream planner/map/device ELF and Erbium PASS-marker runtime
  validation are complete; full/demo host launcher integration remains
  follow-up;
- YOLO model artifacts and regeneration tools are ported, but heavyweight
  regeneration dependencies are not part of normal planner validation;
- representative message/channel and queue test ELFs report PASS under Erbium,
  and the negative expected-failure ELF reports the intended FAIL marker, but
  broader scheduler coverage remains hardening follow-up;
- DNN demos and LTFarm are archived/reference material, not active mapper inputs;
- tensor matmul and YOLO downstream now have Erbium PASS-marker validation, but
  broader runtime hardening remains separate from mapper documentation.
