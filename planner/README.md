# TPA Planner

This package contains the offline Python planner/mapper path for TPA programs.
It was ported from the original TPA repository and currently provides:

- `tpa-extract-process-json` — extract planner process metadata from built ELF
  object files.
- `tpa-map-program` — map a `.tpp` program plus process metadata onto a machine
  topology.
- `tpa-plan-program` — produce program planning reports from process metadata.
- Machine topology inputs in the repository-level `machines/` directory.

The planner remains a Python package, but it is now used by structured CMake
integration for the ported ET demo paths. The `kernels/` simple demos build
through `add_tpa_process()` / `add_tpa_program()`, and the YOLOv5n downstream
path has CMake targets that extract process metadata, generate planner reports,
map the program, and build a generated device ELF.

## Setup

From the structured repository root:

```sh
python3 -m venv .venv-planner
. .venv-planner/bin/activate
python -m pip install -e planner
```

## Smoke tests

```sh
python -m unittest discover -s planner/tests

tpa-map-program --help
tpa-plan-program --help
tpa-extract-process-json --help
```

The tests exercise the checked-in machine topologies and a synthetic tensor
matmul mapping workload. They do not require the original TPA build tree.

## CMake-integrated planner targets

With an ET platform root and the planner package installed in the selected
Python environment, the top-level ET superbuild forwards the currently ported
planner/demo targets:

```sh
cmake -S . -B build-et-erbium -DET_ROOT=/opt/et -DTPA_PLATFORM=erbium -DPYTHON=$(command -v python)
cmake --build build-et-erbium --target tpa_pipe_demo.elf
cmake --build build-et-erbium --target tpa_empty.elf
cmake --build build-et-erbium --target tpa_yolov5n_downstream_plan_planner_json
cmake --build build-et-erbium --target tpa_yolov5n_downstream_map_mapped_program
cmake --build build-et-erbium --target tpa_yolov5n_downstream.elf
```

The YOLO downstream planner/map targets use the structured `machines/` JSONs and
write generated planner JSON, mapped-program JSON, placement, scratch config,
and edge-buffer config artifacts under the device build tree. The generated
`kernels/` and `yolov5n/` ELFs can be loaded with `erbium_emu` as documented in
the repository-level `README.md` and `docs/USAGE.md`.

These ET builds are platform validation. Host smoke-test-double builds are only
local syntax/unit smoke tests and must not be treated as Erbium or ET-SoC-1
validation.

## Example standalone mapper invocation

After installing the package, provide a program file, process metadata JSON,
optional compute-cost hints, and one of the checked-in machine topology files:

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
  --scratch-header-out /tmp/tpa_scratch_config.h
```

## Current model and limitations

The mapper uses a simplified but current memory model:

- process metadata includes mutable/static object sections, but known embedded
  edge-payload symbols may be reclassified out of process data;
- edge buffers are first-class in the current mapped-program memory model
  (`edge-planned-v0`) and generated edge-buffer configuration header path;
- mapping remains acyclic and starts with a HEFT-like performance-first list
  scheduler using explicit process execution-cost hints;
- communication costs are machine-described by `machines/single-minion.json`,
  `machines/erbium.json`, and `machines/etsoc1.json`;
- if a memory budget is provided, greedy repair collapses contexts only until
  the mapping fits.

Generated mapper outputs can include:

- a mapper report JSON;
- a mapped-program artifact containing mapped instances, mapped connections,
  scratch domains, and memory summary;
- a generated `.place` file;
- a generated scratch header containing one arena-size macro per minion;
- a generated edge-buffer configuration header for mapped TPA channels.

Remaining follow-up work is broader integration, not absence of all integration:
YOLO full/demo host launcher support, broader metadata extraction coverage,
ltfarm experiments, and the full cooperative runtime scheduler are still tracked
as follow-up work in `docs/USAGE.md`. YOLO tools/models and representative block
tests are documented in `docs/yolo-demo.md`. Message/queue/negative test ELFs
are ported, while their full behavioral validation depends on the full
cooperative scheduler follow-up.
