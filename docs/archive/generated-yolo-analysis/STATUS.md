# Archived Generated YOLO Analysis

This directory preserves historical generated YOLOv5n analysis artifacts from
the original TPA repository.

## Status

Archived/reference only. These JSON/Markdown files are not runtime inputs for
the structured repo.

## Preserved files

- `yolov5n_tpa_exec_graph.json`
- `yolov5n_tpa_first_subgraph.json`
- `yolov5n_tpa_map.json`
- `yolov5n_tpa_plan.json`
- `yolov5n_tpa_plan.md`

## Current structured replacement

The structured build generates fresh planner/map artifacts in the build tree
from checked-in process sources, manifests, program graphs, machine JSON, and
planner package:

```sh
cmake --build build-et-erbium --target tpa_yolov5n_downstream_plan_planner_json
cmake --build build-et-erbium --target tpa_yolov5n_downstream_map_mapped_program
```

Those generated build-tree artifacts are authoritative for current runtime
inputs. The archived files are useful for historical comparison, documentation,
and recovering stable tables such as the process inventory in
`yolov5n_tpa_plan.md`.

## Rationale

Keeping these files under `docs/archive/` prevents accidental use as stale
runtime inputs while preserving the planning knowledge requested by the original
parity inventory.
