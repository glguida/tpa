# Missing Original Artifacts Inventory

This inventory compares the original TPA repository at
`/home/glguida/think/etsw/tpa` with the structured repository. It records what
is ported, what is partial, what is intentionally deferred, and where future
documentation or implementation work should land.

## Executive summary

### Ported and validated today

The structured repository currently has these important pieces ported and
validated:

- ET superbuild: top-level `ProjectFunctions.cmake` discovery plus
  `DeviceProjectNoInstall(tpa-device ...)` and `HostProjectNoInstall(tpa-host ...)`.
- Device HAL/core build for Erbium and ET-SoC-1.
- Host smoke-test-double mode, clearly marked as non-platform validation.
- Generated TPA programs:
  - `kernels/tpa_empty.*` -> `tpa_empty.elf`;
  - `kernels/tpa_pipe_demo.*` -> `tpa_pipe_demo.elf`;
  - `kernels/tpa_tensor_matmul.*` plus generated `.tpp/.place` ->
    `tpa_tensor_matmul.elf`.
- YOLO downstream path:
  - `yolov5n/` process sources/manifests/assets;
  - CMake process metadata extraction;
  - planner JSON target;
  - mapper target producing mapped program, placement, scratch config, and edge
    config;
  - `tpa_yolov5n_downstream.elf`.
- `tests/tpa_msg/`, `tests/tpa_queue/`, and `tests/tpa_negative/` original
  runtime regression assets are ported and build as structured ET program ELFs.
  Representative positive tests load/pass under the current Erbium demo harness;
  negative expected-failure runtime semantics are documented for the full
  cooperative scheduler follow-up.
- `tests/yolo/` source/assets are integrated for representative Erbium block-test
  targets (`tpa_yolo_cbs_l40.elf`, `tpa_yolo_sppf_l31_32.elf`) with device-subbuild CTest entries.
- Trace analysis tools have been ported to `tools/trace/`.
- Original DNN demos and LTFarm are preserved as archived/reference material
  with dependency/status notes under `docs/archive/`.
- Historical generated YOLO analysis artifacts are preserved under
  `docs/archive/generated-yolo-analysis/` and are not runtime inputs.
- Python planner package, machine JSONs, unit tests, and CLI entry points.
- `tpa-host/src/tpa_launcher.cpp` and the `tpa_host_tools` target.
- Erbium emulator validation for `tpa_pipe_demo.elf` and
  `tpa_yolov5n_downstream.elf`.
- ET-SoC-1 default one-shire `tpa_core` build. YOLO defaults off unless
  `TPA_ETSOC1_NR_SHIRES=32` because the YOLO mapping uses the full-card machine
  description.

### Remaining unported, partially ported, or intentionally deferred

The structured repo does **not** yet contain or integrate all original tests,
demos, tools, generated reports, or model artifacts. Important gaps include:

- DNN tree/systolic demo kernels and generators are archived/reference only,
  not active build targets.
- `ltfarm/` Litecoin-farm experiment is archived/reference only, not an active
  build target.
- `models/yolov5nu.onnx` and `models/yolov5nu.pt` are checked in under `models/` with checksums documented in `docs/yolo-demo.md`.
- YOLO generation/quantization/case tools are ported under `tools/yolo/`; trace analysis scripts are also ported under `tools/trace/`.
- Old checked-in `generated/` YOLO analysis artifacts are archived as reference
  material, not required runtime inputs.
- Full YOLO/full-demo host pipeline remains follow-up work; representative block-test CMake/CTest coverage is ported.
- The full cooperative runtime scheduler remains a documented limitation of the
  structured demo link harness.
- The old standalone documentation corpus is not yet fully migrated into the
  planned structured documentation tree.

## Inventory table

| Original path(s) | Structured path/status | Category | Priority | Affects | Recommended destination |
|---|---|---:|---:|---|---|
| `tests/tpa_msg/` | Ported. Structured message/channel test ELF targets cover same/cross/fabric send/recv ordering, race/bench, and edge-storage variants. | tests / runtime validation | high | runtime validation, agents | Keep under `tests/tpa_msg/`; expand runtime assertions when full scheduler executes continuations. |
| `tests/tpa_queue/` | Ported. Structured queue ELF targets cover basic/yield/many/wake. | tests / runtime validation | high | runtime/HAL validation, agents | Keep under `tests/tpa_queue/`; validate behavior with full cooperative scheduler follow-up. |
| `tests/tpa_negative/` | Ported. `tpa_negative_expected_fail.elf` builds and expected-failure wrapper is available; current demo harness does not yet execute the failing continuation. | tests / runtime validation | medium | agents, runtime validation | Keep under `tests/tpa_negative/`; enable expected-failure CTest when scheduler runtime runs process continuations. |
| `tests/yolo/` | Representative Erbium block-test targets are wired through structured device CMake/CTest (`tpa_yolo_cbs_l40.elf`, `tpa_yolo_sppf_l31_32.elf`). Additional targets remain available in CMake for future expansion. | tests / YOLO validation | partially done | mapper validation, runtime validation | Keep status in `docs/yolo-demo.md`; expand coverage as needed. |
| `kernels/tpa_tensor_matmul.c`, `.tpm`, generated `.tpp/.place`, `gen_tpa_tensor_matmul.cmake` | Ported. Structured CMake generates `.tpp/.place` and builds `tpa_tensor_matmul.elf`. Runtime/emulator status should be checked with the port job logs. | demo / mapper validation | done | users, agents, mapper validation | Document as the intermediate example between `tpa_pipe_demo` and YOLO. |
| `kernels/tpa_dnn_tree_demo.*` | Archived under `docs/archive/original-dnn-demos/`; not an active build target due DNN library/dependency blockers. | demo / research | archived | users, program authors | See `docs/archive/original-dnn-demos/STATUS.md` for future port steps. |
| `kernels/tpa_dnn_systolic_demo.*`, `gen_tpa_dnn_systolic_demo.cmake` | Archived under `docs/archive/original-dnn-demos/`; not an active build target due tensor/DNN dependency blockers. | demo / research | archived | research, mapper stress | See `docs/archive/original-dnn-demos/STATUS.md` for future port steps. |
| `ltfarm/` | Archived under `docs/archive/ltfarm/`; not an active build target because the required Litecoin-vector host oracle/sysemu harness is not ported. | demo / research experiment | archived | research | See `docs/archive/ltfarm/STATUS.md` for preserved hard rules and future port steps. |
| `models/yolov5nu.onnx`, `models/yolov5nu.pt` | Ported under `models/` with SHA-256 checksums documented in `docs/yolo-demo.md` and `tools/yolo/README.md`. | model artifact | done | YOLO regeneration, users | Keep checksums/policy current if artifacts move to external storage. |
| `tools/regen_yolov5n_weights.py`, `ptq_yolov5.py`, `gen_yolo_*`, `gen_yolo_tensor_weights.py`, `yolov5n_legacy_layer_map.json` | Ported under `tools/yolo/`. Heavyweight Python dependencies are documented but not part of normal planner tests. | tool / model generation | done | mapper validation, YOLO contributors | Keep `tools/yolo/README.md` and `docs/yolo-demo.md` current. |
| `tools/analyze_trace_by_hart.*`, `split*_trace_by_hart.sh` | Ported to `tools/trace/` with usage docs. | tool / debugging | done | agents, runtime contributors | Keep `tools/trace/README.md` current with emulator/log formats. |
| `tools/plan_yolov5n.py` | Missing; functionality overlaps with ported planner package and YOLO CMake targets. | tool / generated analysis | archive or selective port | mapper contributors | Compare with `planner/`; migrate unique logic to planner docs/tests or archive under `docs/yolo-demo.md`. |
| `generated/yolov5n_tpa_exec_graph.json`, `yolov5n_tpa_first_subgraph.json`, `yolov5n_tpa_map.json`, `yolov5n_tpa_plan.json`, `yolov5n_tpa_plan.md` | Archived under `docs/archive/generated-yolo-analysis/`; current CMake generates fresh planner/map outputs in build tree. | generated artifact / docs source | archived | mapper contributors, docs | Do not make runtime depend on archived generated files. Use them only for historical comparison/docs. |
| Old root docs: `TPA.md`, `TPA_IMPL.md`, `ARCHITECTURE.md`, `PROCESS_MODEL.md`, `TOPOLOGY.md`, `MEMORY.md`, `MAPPING.md`, `EDGE_BUFFER.md`, `GENERATION.md`, `RUNNING.md`, `STATUS.md`, `TRANSPUTER.md`, `HOARE.md`, `YOLO_TPA_PLAN.md` | Not directly ported. `docs/DOCUMENTATION_PLAN.md` maps them into new docs. | docs | high | users, agents | Implement through docs follow-up jobs; do not copy wholesale. |
| `CMakePresets.json` | Missing. Structured repo uses explicit `cmake -S/-B` commands instead of old presets. | build config | low | agents | Do not port unless a new preset policy is chosen; document explicit commands in `docs/build-and-run.md`. |
| `cmake/run_erbium_test.cmake`, `run_erbium_test_fast.cmake` | Partially ported. `cmake/run_erbium_test_fast.cmake` is available for PASS-based emulator CTest entries, and `cmake/run_erbium_expected_fail.cmake` is available for future expected-failure semantics. A broader/full ET device CTest harness remains follow-up. | test harness | partially done | runtime validation, agents | Keep wrapper usage documented in `docs/USAGE.md`; expand CTest coverage as runtime semantics mature. |
| `platform/erbium.ld` | Missing; structured uses `platform/erbium_mram.ld`. | platform/linker | archive only unless needed | runtime contributors | Document current linker script; archive old variant if useful. |
| `platform/etsoc1/*`, `platform/tpa_entry.c`, `platform/tpa_runtime.c` | Mostly missing in old form; structured HAL/core and runtime harness differ. | runtime feature | medium-high | runtime contributors | Document divergence in `docs/limitations.md`; port only if full cooperative runtime scheduler work needs it. |
| `libtpa/src/tpa.c`, old `libtpa/include/tpa/runtime.h`, old `arch.h` surface | Not ported wholesale. Structured has `tpa/lib`, `tpa/hal`, compatibility `tpa/tpa.h`, and demo runtime harness. | runtime feature | high for future scheduler | runtime contributors | Track full cooperative runtime scheduler as a runtime implementation project; explain current harness in `docs/limitations.md`. |
| YOLO full/demo targets (`tpa_yolov5n_full.elf`, `tpa_yolov5n_demo.elf`, `yolo_demo_host`) | Original CMake has them; structured currently validates downstream path and host launcher but not full/demo host YOLO pipeline. | demo / tool | high | users, mapper validation | Future `port-yolo-full-demo-host` or similar; document in `docs/yolo-demo.md`. |
| Message/queue/failure forwarded targets in old top-level CMake | Ported for structured Erbium test ELFs through top-level forwarded targets. | build/test integration | high | agents, runtime validation | Keep target list in sync with `tests/tpa_msg`, `tests/tpa_queue`, and `tests/tpa_negative`. |
| Old `.codex`, egg-info, `__pycache__` | Not ported and should not be. | generated/local artifact | archive never | none | Ignore. |

## Missing test coverage plan

### Message, queue, and negative regression tests

Original `tests/tpa_msg/`, `tests/tpa_queue/`, and `tests/tpa_negative/` assets
are now ported into the structured build. They compile through
`add_tpa_process()` / `add_tpa_program()`, and top-level forwarded targets cover
representative message, queue, edge-storage, and expected-failure ELFs.

Remaining work is runtime-depth, not asset/build integration: the structured
demo link harness currently proves generated process/image metadata compile,
link, and load/pass in `erbium_emu`, but it does not yet run every continuation
through the full cooperative scheduler. The negative expected-failure wrapper is
available as `cmake/run_erbium_expected_fail.cmake`; enable it in CTest once the
scheduler runtime executes the failing process continuation.

### YOLO block tests

The structured repo now wires representative Erbium block-test targets through
`tests/yolo/CMakeLists.txt` and device-subbuild CTest entries. Current reviewed
coverage includes `tpa_yolo_cbs_l40.elf` and `tpa_yolo_sppf_l31_32.elf`, both
run under `erbium_emu`. Additional block target definitions remain in CMake for
future expansion. See `docs/yolo-demo.md`.

## Missing demo/tool/model plan

### Tensor matmul demo

Original files:

- `kernels/tpa_tensor_matmul.c`
- `kernels/tpa_tensor_matmul.tpm`
- `kernels/gen_tpa_tensor_matmul.cmake`

Status: ported to structured `kernels/` with generated `.tpp/.place` CMake flow
and the forwarded `tpa_tensor_matmul.elf` target. This is the intermediate
example between `tpa_pipe_demo` and YOLO. Check the port job logs for the exact
Erbium emulator result.

### DNN demos

Original files include:

- `kernels/tpa_dnn_tree_demo.*`
- `kernels/tpa_dnn_systolic_demo.*`
- `kernels/gen_tpa_dnn_systolic_demo.cmake`

Status: archived/reference under `docs/archive/original-dnn-demos/`.

Plan: treat as advanced examples. `STATUS.md` preserves dependency blockers and
future port steps. Do not wire these into the active build until the DNN library
package/dependency policy and C++ process support are settled.

Future job: `port-dnn-demo-kernels` only if the DNN dependency stack is revived.

### `ltfarm`

Original directory: `ltfarm/`.

Status: archived/reference under `docs/archive/ltfarm/`.

Plan: classify as research/experiment until a current user need exists. The
archived `STATUS.md` preserves the original hard rule that correctness must be
host-decided against official Litecoin Core vectors, not a hand-written host
crypto path.

Future job: `port-ltfarm-experiment` only if revived; otherwise archive-only.

### YOLO tools/model artifacts

Original model artifacts and generation tools are ported:

- `models/yolov5nu.onnx`
- `models/yolov5nu.pt`
- `tools/yolo/regen_yolov5n_weights.py`
- `tools/yolo/ptq_yolov5.py`
- `tools/yolo/gen_yolo_*_case.py`
- `tools/yolo/gen_yolo_tensor_weights.py`
- `tools/yolo/yolov5n_legacy_layer_map.json`

Status: ported. The model artifacts are committed because they are small enough
for this repository; checksums and heavyweight dependency caveats are documented
in `docs/yolo-demo.md` and `tools/yolo/README.md`.

### Trace analysis tools

Original tools include:

- `tools/analyze_trace_by_hart.awk`
- `tools/analyze_trace_by_hart.sh`
- `tools/split_inst_trace_by_hart.sh`
- `tools/split_trace_by_hart.sh`

Status: ported to `tools/trace/` with `tools/trace/README.md`.

Plan: keep these scripts aligned with `erbium_emu` log formats and ET binutils
names. They are useful for agents and runtime contributors when emulator logs
are large.

Future job: completed unless log formats change.

### Generated YOLO analysis reports

Original `generated/` reports document a historical YOLO planning snapshot.
Current structured CMake generates fresh planner/map outputs in the build tree.

Status: archived/reference under `docs/archive/generated-yolo-analysis/`.

Plan: do not make old generated JSON files runtime inputs. Use archived reports
only for historical comparison or to recover stable documentation tables.

Future job: docs-only if more historical tables should be extracted.

## Documentation integration plan

This inventory should feed the documentation tree from `docs/DOCUMENTATION_PLAN.md`:

- `docs/limitations.md`
  - Centralize missing/partial status for archived DNN/LTFarm status, full YOLO
    host/demo integration, full runtime scheduler, and broader metadata
    coverage. Message/queue/negative tests are ported as build targets but need
    full scheduler behavioral validation. YOLO block tests, tools/models, tensor
    matmul, and trace tools now have explicit port status.
- `docs/build-and-run.md`
  - Add only currently validated commands as primary commands.
  - Add missing tests/demos as future target lists, not runnable instructions.
  - Keep trace-tool usage aligned with `tools/trace/README.md`.
- `docs/yolo-demo.md`
  - Explain downstream is ported/validated.
  - Explain full/demo/block-test/model-regeneration gaps.
  - Include model artifact policy once decided.
- `docs/mapper-planner.md`
  - Describe current CMake planner/map targets and note broader metadata and old
    generated-report migration status.
- `docs/creating-programs.md`
  - Use `tpa_pipe_demo` first, `tpa_tensor_matmul` as the intermediate example, and YOLO as
    an advanced mapped example.
- `AGENTS.md`
  - Include common trap: do not assume all old repo tests/demos/tools are
    present just because ET acceptance passes.
  - Include validation tiers and missing-test follow-up references.
- `README.md`
  - Keep quick starts focused on validated targets; link to limitations rather
    than listing every missing original artifact.

## Future implementation job recommendations

### `complete-runtime-scheduler-validation`

Scope: implement/enable the cooperative runtime scheduler path so the ported
message, queue, and negative ELFs execute their process continuations rather
than only loading generated images through the demo harness. Enable the negative
expected-failure CTest wrapper when this is complete.

Priority: high.

### `port-tensor-matmul-demo`

Status: completed. Keep docs and validation references current for the structured
`kernels/tpa_tensor_matmul.*` flow and forwarded `tpa_tensor_matmul.elf` target.

Priority: done.

### `port-yolo-block-tests`

Status: representative block-test CMake/CTest coverage completed by `port-yolo-tools-models-block-tests`; expand beyond `tpa_yolo_cbs_l40.elf` and `tpa_yolo_sppf_l31_32.elf` as needed.

Priority: high.

### `port-yolo-tools-models`

Status: completed by `port-yolo-tools-models-block-tests`; YOLO model artifacts
are committed under `models/`, reproduction tools live under `tools/yolo/`, and
checksums/policy are documented in `docs/yolo-demo.md`.

Priority: done.

### `port-ltfarm-experiment`

Status: archived/reference. Future active port requires a structured host
oracle/harness that checks official Litecoin Core vectors.

Priority: low unless revived.

### `port-dnn-demo-kernels`

Status: archived/reference. Future active port requires a DNN dependency policy
and C++ process support.

Priority: medium-low unless the DNN stack is revived.

### `port-trace-analysis-tools`

Status: completed. Trace splitting and symbol-attribution scripts live in
`tools/trace/` with usage docs.

Priority: done.

### `docs-integrate-missing-artifacts-status`

Scope: after this inventory lands, ensure `docs/limitations.md`,
`docs/build-and-run.md`, `docs/yolo-demo.md`, `docs/mapper-planner.md`,
`docs/creating-programs.md`, and `AGENTS.md` incorporate the missing-artifact
status without claiming unported targets are available.

Priority: high for documentation correctness.
