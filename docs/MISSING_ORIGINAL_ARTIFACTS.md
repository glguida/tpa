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
- Simple generated TPA programs:
  - `kernels/tpa_empty.*` -> `tpa_empty.elf`;
  - `kernels/tpa_pipe_demo.*` -> `tpa_pipe_demo.elf`.
- YOLO downstream path:
  - `yolov5n/` process sources/manifests/assets;
  - CMake process metadata extraction;
  - planner JSON target;
  - mapper target producing mapped program, placement, scratch config, and edge
    config;
  - `tpa_yolov5n_downstream.elf`.
- `tests/yolo/` source/assets have been copied for representative block-test
  follow-up, but the block-test CTest/CMake integration is not complete.
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

- `tests/tpa_msg/` message/channel transport tests are missing.
- `tests/tpa_queue/` scheduler/queue tests are missing.
- `tests/tpa_negative/` expected-failure tests are missing.
- `kernels/tpa_tensor_matmul.*` and `gen_tpa_tensor_matmul.cmake` are missing.
- DNN tree/systolic demo kernels and generators are missing.
- `ltfarm/` Litecoin-farm experiment is missing.
- `models/yolov5nu.onnx` and `models/yolov5nu.pt` are missing.
- `tools/` YOLO generation/quantization/trace/planning scripts are missing.
- Old checked-in `generated/` YOLO analysis artifacts are missing; most should
  be archive/reference material, not required runtime inputs.
- Full YOLO/full-demo host pipeline and YOLO block-test CTest wiring remain
  follow-up work.
- The full cooperative runtime scheduler remains a documented limitation of the
  structured demo link harness.
- The old standalone documentation corpus is not yet fully migrated into the
  planned structured documentation tree.

## Inventory table

| Original path(s) | Structured path/status | Category | Priority | Affects | Recommended destination |
|---|---|---:|---:|---|---|
| `tests/tpa_msg/` | Missing. No structured message/channel test targets beyond simple demos. | tests / runtime validation | high | runtime validation, agents | Port to `tests/tpa_msg/`; document in `docs/build-and-run.md` and `docs/limitations.md`. |
| `tests/tpa_queue/` | Missing. Scheduler/queue behavior lacks original regression coverage. | tests / runtime validation | high | runtime/HAL validation, agents | Port to `tests/tpa_queue/`; include target list in `docs/build-and-run.md`. |
| `tests/tpa_negative/` | Missing. Expected-failure path not covered. | tests / runtime validation | medium | agents, runtime validation | Port to `tests/tpa_negative/`; document expected failure semantics in `docs/build-and-run.md`. |
| `tests/yolo/` | Sources/assets copied, including generated weight headers, but CMake/CTest integration is not yet wired in structured device build. | tests / YOLO validation | high | mapper validation, runtime validation | Keep under `tests/yolo/`; integrate in `docs/yolo-demo.md` and `docs/limitations.md`. |
| `kernels/tpa_tensor_matmul.c`, `.tpm`, generated `.tpp/.place`, `gen_tpa_tensor_matmul.cmake` | Missing. Structured `kernels/` has only empty and pipe demo. | demo / mapper validation | medium | users, agents, mapper validation | Port to `kernels/`; use as `docs/creating-programs.md` intermediate example after pipe demo. |
| `kernels/tpa_dnn_tree_demo.*` | Missing. | demo / research | medium | users, program authors | Port if DNN demo remains valuable; document in `docs/creating-programs.md` or archive in limitations. |
| `kernels/tpa_dnn_systolic_demo.*`, `gen_tpa_dnn_systolic_demo.cmake` | Missing. | demo / research | low-medium | research, mapper stress | Port after tensor matmul/DNN tree; document as advanced example or archive-only if obsolete. |
| `ltfarm/` | Missing. | demo / research experiment | low | research only unless revived | Archive or port to `ltfarm/`; document status in `docs/limitations.md`. |
| `models/yolov5nu.onnx`, `models/yolov5nu.pt` | Missing. YOLO checked-in weights/headers exist under `tests/yolo/generated`, but source model files are not in structured repo. | model artifact | medium | YOLO regeneration, users | Decide artifact policy. If large files are not committed, document acquisition/regeneration in `docs/yolo-demo.md`. |
| `tools/regen_yolov5n_weights.py`, `ptq_yolov5.py`, `gen_yolo_*`, `gen_yolo_tensor_weights.py`, `yolov5n_legacy_layer_map.json` | Missing. Ported YOLO uses checked-in generated headers/assets but not regeneration tools. | tool / model generation | high for reproducibility | mapper validation, YOLO contributors | Port to `tools/`; document in `docs/yolo-demo.md` and `docs/mapper-planner.md`. |
| `tools/analyze_trace_by_hart.*`, `split*_trace_by_hart.sh` | Missing. | tool / debugging | medium | agents, runtime contributors | Port to `tools/trace/` or `tools/`; document in `docs/build-and-run.md` debugging section. |
| `tools/plan_yolov5n.py` | Missing; functionality overlaps with ported planner package and YOLO CMake targets. | tool / generated analysis | archive or selective port | mapper contributors | Compare with `planner/`; migrate unique logic to planner docs/tests or archive under `docs/yolo-demo.md`. |
| `generated/yolov5n_tpa_exec_graph.json`, `yolov5n_tpa_first_subgraph.json`, `yolov5n_tpa_map.json`, `yolov5n_tpa_plan.json`, `yolov5n_tpa_plan.md` | Missing. `generated/yolov5n_tpa_plan.md` is source material for docs; current CMake generates fresh planner/map outputs in build tree. | generated artifact / docs source | archive only / docs | mapper contributors, docs | Do not make runtime depend on old generated files. Extract useful tables into `docs/yolo-demo.md`. |
| Old root docs: `TPA.md`, `TPA_IMPL.md`, `ARCHITECTURE.md`, `PROCESS_MODEL.md`, `TOPOLOGY.md`, `MEMORY.md`, `MAPPING.md`, `EDGE_BUFFER.md`, `GENERATION.md`, `RUNNING.md`, `STATUS.md`, `TRANSPUTER.md`, `HOARE.md`, `YOLO_TPA_PLAN.md` | Not directly ported. `docs/DOCUMENTATION_PLAN.md` maps them into new docs. | docs | high | users, agents | Implement through docs follow-up jobs; do not copy wholesale. |
| `CMakePresets.json` | Missing. Structured repo uses explicit `cmake -S/-B` commands instead of old presets. | build config | low | agents | Do not port unless a new preset policy is chosen; document explicit commands in `docs/build-and-run.md`. |
| `cmake/run_erbium_test.cmake`, `run_erbium_test_fast.cmake` | Missing. Current validation runs `erbium_emu` manually; no CTest wrapper for ET device targets. | test harness | medium | runtime validation, agents | Port/adapt if adding device CTest targets; document in `docs/build-and-run.md`. |
| `platform/erbium.ld` | Missing; structured uses `platform/erbium_mram.ld`. | platform/linker | archive only unless needed | runtime contributors | Document current linker script; archive old variant if useful. |
| `platform/etsoc1/*`, `platform/tpa_entry.c`, `platform/tpa_runtime.c` | Mostly missing in old form; structured HAL/core and runtime harness differ. | runtime feature | medium-high | runtime contributors | Document divergence in `docs/limitations.md`; port only if full cooperative runtime scheduler work needs it. |
| `libtpa/src/tpa.c`, old `libtpa/include/tpa/runtime.h`, old `arch.h` surface | Not ported wholesale. Structured has `tpa/lib`, `tpa/hal`, compatibility `tpa/tpa.h`, and demo runtime harness. | runtime feature | high for future scheduler | runtime contributors | Track full cooperative runtime scheduler as a runtime implementation project; explain current harness in `docs/limitations.md`. |
| YOLO full/demo targets (`tpa_yolov5n_full.elf`, `tpa_yolov5n_demo.elf`, `yolo_demo_host`) | Original CMake has them; structured currently validates downstream path and host launcher but not full/demo host YOLO pipeline. | demo / tool | high | users, mapper validation | Future `port-yolo-full-demo-host` or similar; document in `docs/yolo-demo.md`. |
| Message/queue/failure forwarded targets in old top-level CMake | Missing because underlying tests are missing. | build/test integration | high | agents, runtime validation | Reintroduce when tests are ported. |
| Old `.codex`, egg-info, `__pycache__` | Not ported and should not be. | generated/local artifact | archive never | none | Ignore. |

## Missing test coverage plan

### `tests/tpa_msg`

Original coverage includes same-hart, cross-hart, fabric, send-first,
recv-first, race, benchmark, and explicit edge-storage variants. This is the
highest-value missing runtime test suite because it validates the channel
transport classes and rendezvous ordering that TPA depends on.

Recommended plan:

1. Port sources/manifests/placements to `tests/tpa_msg/`.
2. Add a structured `tests/CMakeLists.txt` and `tests/tpa_msg/CMakeLists.txt`
   using current `add_tpa_process()` / `add_tpa_program()`.
3. Forward important targets at top level only after they build cleanly.
4. Add emulator/CTest wrappers or document manual `erbium_emu` commands.
5. Include direct/local/fabric edge-storage coverage in the first pass.

Future job: `port-message-queue-tests` may include both message and queue tests,
or split message tests into `port-message-channel-tests` if scope is too large.

### `tests/tpa_queue`

Original coverage includes `basic`, `yield`, `many`, and `wake`. These validate
scheduler queues, yielding, many runnable processes, and wake behavior.

Recommended plan:

1. Port `tests/tpa_queue/` assets.
2. Build through the same generated image path as current `kernels/` demos.
3. Run under Erbium emulator.
4. If the full cooperative runtime scheduler is still limited, mark any blocked
   cases explicitly rather than weakening the tests.

Future job: `port-message-queue-tests`.

### `tests/tpa_negative`

Original `expected_fail` validates failure signaling and expected-failure test
harness behavior. This is useful for making CI honest: not all emulator FAIL
signals should mean a broken test harness.

Recommended plan:

1. Port `tests/tpa_negative/expected_fail.*`.
2. Port/adapt `run_erbium_test.cmake` expected-failure handling if needed.
3. Document how expected-failure tests are named and run.

Future job: `port-negative-runtime-tests` or include in
`port-message-queue-tests` if small.

### YOLO block tests

The structured repo has copied `tests/yolo/` sources and generated headers, but
these are not yet integrated into the structured device build/CTest flow. These
tests are important because they validate individual fused YOLO blocks before
full/downstream graph assembly.

Recommended plan:

1. Adapt `tests/yolo/CMakeLists.txt` to structured `tpa-kernel.cmake`.
2. Start with one representative block target such as `tpa_yolo_cbs_l40.elf`.
3. Add remaining block targets after compile/runtime issues are resolved.
4. Use emulator max-cycle values from the old CMake as references.
5. Document which block tests are expected to pass in `docs/yolo-demo.md`.

Future job: `port-yolo-block-tests`.

## Missing demo/tool/model plan

### Tensor matmul demo

Original files:

- `kernels/tpa_tensor_matmul.c`
- `kernels/tpa_tensor_matmul.tpm`
- `kernels/gen_tpa_tensor_matmul.cmake`

Status: missing.

Plan: port after message/queue tests or alongside program-authoring docs. It is
a good intermediate example between `tpa_pipe_demo` and YOLO because it has
generated `.tpp/.place` but is smaller than YOLO.

Future job: `port-tensor-matmul-demo`.

### DNN demos

Original files include:

- `kernels/tpa_dnn_tree_demo.*`
- `kernels/tpa_dnn_systolic_demo.*`
- `kernels/gen_tpa_dnn_systolic_demo.cmake`

Status: missing.

Plan: treat as advanced examples. First decide whether dependencies such as
`dnnLibrary` are available and still relevant. If not, archive the design in
docs rather than porting dead code.

Future job: `port-dnn-demo-kernels`.

### `ltfarm`

Original directory: `ltfarm/`.

Status: missing.

Plan: classify as research/experiment until a current user need exists. Preserve
`PLAN.md` knowledge in `docs/limitations.md` or an archive note if not ported.

Future job: `port-ltfarm-experiment` only if revived; otherwise archive-only.

### YOLO tools/model artifacts

Original assets include:

- `models/yolov5nu.onnx`
- `models/yolov5nu.pt`
- `tools/regen_yolov5n_weights.py`
- `tools/ptq_yolov5.py`
- `tools/gen_yolo_*_case.py`
- `tools/gen_yolo_tensor_weights.py`
- `tools/yolov5n_legacy_layer_map.json`

Status: missing, while generated weight headers and YOLO process sources are
present.

Plan: decide whether source model artifacts belong in git, external storage, or
an acquisition script. Port the regeneration and quantization tools required to
reproduce `tests/yolo/generated/` headers. Document exact toolchain/version
requirements.

Future job: `port-yolo-tools-models`.

### Trace analysis tools

Original tools include:

- `tools/analyze_trace_by_hart.awk`
- `tools/analyze_trace_by_hart.sh`
- `tools/split_inst_trace_by_hart.sh`
- `tools/split_trace_by_hart.sh`

Status: missing.

Plan: port to `tools/trace/` or `tools/`, then document under a debugging
section in `docs/build-and-run.md`. These are useful for agents and runtime
contributors when emulator logs are large.

Future job: `port-trace-analysis-tools`.

### Generated YOLO analysis reports

Original `generated/` reports document a historical YOLO planning snapshot.
Current structured CMake generates fresh planner/map outputs in the build tree.

Plan: do not make old generated JSON files runtime inputs. Extract stable tables
and explanations into `docs/yolo-demo.md`. Keep old generated reports as
archive/reference only if needed.

Future job: part of `docs-04-mapper-planner-guide` or
`docs-integrate-missing-artifacts-status`.

## Documentation integration plan

This inventory should feed the documentation tree from `docs/DOCUMENTATION_PLAN.md`:

- `docs/limitations.md`
  - Centralize missing/partial status for message tests, queue tests, negative
    tests, YOLO block tests, tensor matmul, DNN demos, ltfarm, tools, models,
    full runtime scheduler, and broader metadata coverage.
- `docs/build-and-run.md`
  - Add only currently validated commands as primary commands.
  - Add missing tests/demos as future target lists, not runnable instructions.
  - Add trace-tool section after tools are ported.
- `docs/yolo-demo.md`
  - Explain downstream is ported/validated.
  - Explain full/demo/block-test/model-regeneration gaps.
  - Include model artifact policy once decided.
- `docs/mapper-planner.md`
  - Describe current CMake planner/map targets and note broader metadata and old
    generated-report migration status.
- `docs/creating-programs.md`
  - Use `tpa_pipe_demo` first, `tpa_tensor_matmul` later if ported, and YOLO as
    an advanced mapped example.
- `AGENTS.md`
  - Include common trap: do not assume all old repo tests/demos/tools are
    present just because ET acceptance passes.
  - Include validation tiers and missing-test follow-up references.
- `README.md`
  - Keep quick starts focused on validated targets; link to limitations rather
    than listing every missing original artifact.

## Future implementation job recommendations

### `port-message-queue-tests`

Scope: port `tests/tpa_msg/`, `tests/tpa_queue/`, and possibly
`tests/tpa_negative/` if small enough. Add structured CMake and emulator/CTest
validation for representative targets.

Priority: high.

### `port-tensor-matmul-demo`

Scope: port `tpa_tensor_matmul.c/.tpm` and `gen_tpa_tensor_matmul.cmake`, add
structured CMake target, forward `tpa_tensor_matmul.elf`, and document as an
intermediate program-authoring example.

Priority: medium.

### `port-yolo-block-tests`

Scope: integrate copied `tests/yolo/` block tests into structured device CMake
and CTest/emulator validation, starting with one representative block and then
expanding.

Priority: high.

### `port-yolo-tools-models`

Scope: port YOLO model-regeneration/quantization/case-generation tools, decide
model artifact policy for `models/`, and document reproducibility commands.

Priority: high for reproducibility, medium for runtime users.

### `port-ltfarm-experiment`

Scope: decide whether `ltfarm/` is still active. If yes, port CMake/sources and
host helper. If no, write an archive note and exclude from active build docs.

Priority: low.

### `port-dnn-demo-kernels`

Scope: port DNN tree and systolic demos if dependencies are available; otherwise
archive their design and generated graph ideas.

Priority: medium-low.

### `port-trace-analysis-tools`

Scope: port trace splitting and analysis scripts, add examples using
`tpa_launcher`/`erbium_emu` logs.

Priority: medium.

### `docs-integrate-missing-artifacts-status`

Scope: after this inventory lands, ensure `docs/limitations.md`,
`docs/build-and-run.md`, `docs/yolo-demo.md`, `docs/mapper-planner.md`,
`docs/creating-programs.md`, and `AGENTS.md` incorporate the missing-artifact
status without claiming unported targets are available.

Priority: high for documentation correctness.
