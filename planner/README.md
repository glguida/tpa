# TPA Planner

This package contains the offline Python planner/mapper path for TPA programs.
It was ported from the original TPA repository and currently provides:

- `tpa-extract-process-json` — extract planner process metadata from built ELF
  object files.
- `tpa-map-program` — map a `.tpp` program plus process metadata onto a machine
  topology.
- `tpa-plan-program` — produce program planning reports from process metadata.
- Machine topology inputs in the repository-level `machines/` directory.

The planner is intentionally Python-only in this structured runtime repository;
CMake integration for extracting real process JSON is not yet ported.

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

## Example mapper invocation

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

The mapper uses a simplified model:

- process data includes outputs;
- edge buffers are not yet first-class memory objects;
- mapping starts with a HEFT-like performance-first list scheduler using
  explicit process execution-cost hints;
- communication costs are machine-described by `machines/single-minion.json`,
  `machines/erbium.json`, and `machines/etsoc1.json`;
- if a memory budget is provided, greedy repair collapses contexts only until
  the mapping fits.

Generated mapper outputs can include:

- a mapper report JSON;
- a mapped-program artifact containing mapped instances, mapped connections,
  scratch domains, and memory summary;
- a generated `.place` file;
- a temporary scratch header containing one arena-size macro per minion.

Full end-to-end demo execution still requires porting the process builds,
metadata extraction CMake targets, and runtime/demo integration from the
original repository.
