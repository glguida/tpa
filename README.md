# TPA Structured Runtime

TPA is a transputer-style process/channel runtime and build/mapping flow for ET.
It describes programs as graphs of continuation-style processes, maps those
graphs onto ET topologies, generates image metadata, and builds ET device ELFs
for Erbium or ET-SoC-1.

## Programming model at a glance

TPA keeps program dataflow separate from hardware realization:

```text
Program author writes:
  process C + .tpm manifests + .tpp graph
  (hardware-independent process behavior and logical dataflow)

Mapping/build supplies:
  mapper-generated or small/debug hand .place
  + channel classes + generated image C + ET device ELF
```

Process code and `.tpp` graphs must not name harts, minions, shires, or channel
transport classes. Mapper-generated placement is the normal scalable path for
large or topology-sensitive programs, while hand `.place` files remain valid for
small worked examples, deterministic smoke tests, and mapper/debug inspection.
See `docs/hardware-agnostic-programming.md` for the detailed guide.

The primary build is the ET superbuild path. The top-level project discovers
et-platform's `ProjectFunctions.cmake` and configures structured `tpa-device`
and `tpa-host` subprojects through `DeviceProjectNoInstall()` and
`HostProjectNoInstall()`. Host-only builds are retained only as explicitly named
smoke-test doubles; they are not ET platform validation.

## Repository layout

- `CMakeLists.txt` — ET superbuild entry point.
- `tpa/` — platform-independent core modules and HAL-facing headers.
- `tpa/hal/` — Erbium and ET-SoC-1 HAL implementations.
- `tpa-device/` — ET RISC-V device project and runtime link harness.
- `tpa-host/` — ET host project with `tpa_launcher`.
- `cmake/tpa-kernel.cmake` — `add_tpa_process()` / `add_tpa_program()` helpers.
- `cmake/gen_tpa_image.cmake` — image generation from `.tpm`, `.tpp`, and
  `.place` or mapper output.
- `kernels/` — current simple generated programs: `tpa_empty` and
  `tpa_pipe_demo`, and `tpa_tensor_matmul`.
- `attention/` — fixed-size structured fast-attention demo with parallel and
  serial Erbium placements.
- `depth/` — no-weights stereo SAD depth demo with source, four worker stripes,
  checker, and hand Erbium placement.
- `yolov5n/` — current YOLOv5n downstream process sources and planner/map/device
  targets.
- `yolov8n/` — explicit external-header YOLOv8n downstream milestones for
  P5-only Detect/DFL, sampled P3/P4/P5 Detect/DFL branch plumbing, sampled
  per-scale P3 `model.15`, P4 `model.18`, and P5 `model.21` C2f
  source-modules feeding Detect/DFL, a sampled combined P3/P4/P5 C2f+Detect
  downstream graph, dense P3 `model.15`, P4 `model.18`, and P5 `model.21` C2f
  feature-map validation feeding sampled Detect/DFL, a dense combined
  P3/P4/P5 C2f+Detect downstream graph, and a P4-to-P5 `model.19` neck-tail
  Conv+Concat graph, gated by `BUILD_TPA_YOLOV8N=ON` and external generated
  weight header/manifest cache variables.
- `tests/tpa_msg/`, `tests/tpa_queue/`, `tests/tpa_negative/` — ported
  original message/channel, scheduler/queue, and expected-failure runtime test
  assets integrated through the structured TPA process/program build path.
- `tests/yolo/` — YOLO block-test sources/assets with representative structured
  Erbium block-test targets.
- `tools/yolo/` and `models/` — YOLO regeneration/quantization tools, checked-in
  YOLOv5nu source model artifacts, and the external-artifact YOLOv8n manifest.
- `planner/` — Python metadata extraction, planning, and mapping package.
- `machines/` — mapper machine topology JSON inputs.
- `tools/trace/` — Erbium emulator trace splitting and symbol-attribution tools.
- `docs/archive/` — preserved original-reference material that is not an active
  runtime input, including DNN demos, LTFarm, and historical YOLO analysis.
- `docs/` — detailed project documentation.

## Read next

- `docs/overview.md` — conceptual overview and current validated paths.
- `docs/theory/HOARE.md` — CSP theoretical foundation.
- `docs/theory/TRANSPUTER.md` — transputer/occam runtime model.
- `docs/theory/TPA.md` — core TPA architecture and ET mapping principles.
- `docs/theory/PROCESS_MODEL.md` — process, process-kind, and program-model
  terminology.
- `docs/design/TPA_IMPL.md` — implementation decisions and rationale.
- `docs/design/MEMORY.md` — memory model and hierarchy.
- `docs/design/MAPPING.md` — mapping algorithms and strategy.
- `docs/design/EDGE_BUFFER.md` — edge-buffer design and management.
- `docs/design/TOPOLOGY.md` — topology considerations and machine JSONs.
- `docs/design/GENERATION.md` — code/image generation approach.
- `docs/design/EDGE_BUFFER_PLAN.md` — edge-buffer planning strategy.
- `docs/programming-model.md` — process, channel, image, and artifact terms.
- `docs/et-architecture.md` — Erbium/ET-SoC-1 topology relevant to TPA.
- `docs/et-simd-tensor-kernel-notes.md` — practical ET packed-single SIMD and
  Tensor guidance for TPA kernels such as tensor matmul and attention.
- `docs/et-vector-tensor-hackers-guide.md` — longer practical guide to ET
  packed-single SIMD and Tensor programming inside TPA process kernels.
- `docs/creating-programs.md` — practical guide for creating TPA programs.
- `docs/hardware-agnostic-programming.md` — how to keep process code and `.tpp`
  graphs hardware-independent while using placement/mapping artifacts.
- `docs/mapper-planner.md` — planner/mapper inputs, algorithms, outputs, and
  commands.
- `docs/memory-and-edge-buffers.md` — memory taxonomy and edge-buffer planning.
- `docs/yolo-demo.md` — YOLO downstream, tools/models policy, and block tests.
- `docs/USAGE.md` — current runtime usage notes and caveats.
- `planner/README.md` — quick reference for the Python planner package.

Coding agents should also read `AGENTS.md` before editing.

## Quick start: planner package

```sh
python3 -m venv .venv-planner
. .venv-planner/bin/activate
python -m pip install -e planner
python -m unittest discover -s planner/tests

tpa-map-program --help
tpa-plan-program --help
tpa-extract-process-json --help
```

## Quick start: Erbium ET build and run

Set `ET_ROOT` or `ET_PLATFORM_PATH` to an et-platform install/root containing
`ProjectFunctions.cmake`, the ET RISC-V toolchain file, and ET CMake packages.
Examples below use `/opt/et`.

```sh
cmake -S . -B build-et-erbium -DET_ROOT=/opt/et -DTPA_PLATFORM=erbium
cmake --build build-et-erbium --target tpa_pipe_demo.elf
cmake --build build-et-erbium --target tpa_tensor_matmul.elf
cmake --build build-et-erbium --target tpa_fast_attention_map_mapped_program
cmake --build build-et-erbium --target tpa_fast_attention.elf
cmake --build build-et-erbium --target tpa_fast_attention_serial.elf
cmake --build build-et-erbium --target tpa_stereo_sad_map_mapped_program
cmake --build build-et-erbium --target tpa_stereo_sad.elf
cmake --build build-et-erbium --target tpa_stereo_sad_mapped.elf
/opt/et/bin/erbium_emu \
  -elf_load build-et-erbium/tpa-device-prefix/src/tpa-device-build/kernels/tpa_pipe_demo.elf \
  -max_cycles 10000
/opt/et/bin/erbium_emu \
  -minions 0x1f \
  -elf_load build-et-erbium/tpa-device-prefix/src/tpa-device-build/attention/tpa_fast_attention.elf \
  -max_cycles 5000000
/opt/et/bin/erbium_emu \
  -minions 0x1f \
  -elf_load build-et-erbium/tpa-device-prefix/src/tpa-device-build/depth/tpa_stereo_sad.elf \
  -max_cycles 100000000
/opt/et/bin/erbium_emu \
  -minions 0xff \
  -elf_load build-et-erbium/tpa-device-prefix/src/tpa-device-build/depth/tpa_stereo_sad_mapped.elf \
  -max_cycles 100000000
```

Generated graph tests signal application success or failure with emulator log
markers such as `Signal end test with PASS` or `Signal end test with FAIL`.
Some direct `erbium_emu` runs can print a PASS marker and then exit non-zero
when the emulator reports waiting/sleeping harts. Prefer the registered CTests or
`cmake/run_erbium_test_fast.cmake` for validation: they reject explicit FAIL or
missing-PASS runs, and they accept a PASS marker before considering the raw
emulator return code.

YOLO downstream planner/map/device path:

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

YOLOv8n external-header milestones require the external generated
header/manifest and use synthetic-calibration hashes only; they are not full
YOLOv8n model or accuracy validation. See
`docs/yolo-demo.md#yolov8n-calibration-data-and-generated-artifact-policy` for
the artifact, calibration, and claim checklist:

```sh
cmake -S . -B build-et-erbium-yolov8n \
  -DET_ROOT=/opt/et \
  -DTPA_PLATFORM=erbium \
  -DPYTHON=$(command -v python) \
  -DBUILD_TPA_YOLOV8N=ON \
  -DTPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER=/path/to/yolov8n_external_detect_c2f_weights.h \
  -DTPA_YOLOV8N_EXTERNAL_WEIGHTS_MANIFEST=/path/to/yolov8n_external_detect_c2f_generated_manifest.json
cmake --build build-et-erbium-yolov8n --target tpa_yolov8n_p5_detect_map_mapped_program
cmake --build build-et-erbium-yolov8n --target tpa_yolov8n_p5_detect.elf
cmake --build build-et-erbium-yolov8n --target tpa_yolov8n_p5_c2f_detect_map_mapped_program
cmake --build build-et-erbium-yolov8n --target tpa_yolov8n_p5_c2f_detect.elf
cmake --build build-et-erbium-yolov8n --target tpa_yolov8n_p5_dense_c2f_detect_map_mapped_program
cmake --build build-et-erbium-yolov8n --target tpa_yolov8n_p5_dense_c2f_detect.elf
cmake --build build-et-erbium-yolov8n --target tpa_yolov8n_p3_c2f_detect_map_mapped_program
cmake --build build-et-erbium-yolov8n --target tpa_yolov8n_p3_c2f_detect.elf
cmake --build build-et-erbium-yolov8n --target tpa_yolov8n_p3_dense_c2f_detect_map_mapped_program
cmake --build build-et-erbium-yolov8n --target tpa_yolov8n_p3_dense_c2f_detect.elf
cmake --build build-et-erbium-yolov8n --target tpa_yolov8n_p4_c2f_detect_map_mapped_program
cmake --build build-et-erbium-yolov8n --target tpa_yolov8n_p4_c2f_detect.elf
cmake --build build-et-erbium-yolov8n --target tpa_yolov8n_p4_dense_c2f_detect_map_mapped_program
cmake --build build-et-erbium-yolov8n --target tpa_yolov8n_p4_dense_c2f_detect.elf
cmake --build build-et-erbium-yolov8n --target tpa_yolov8n_p4_p5_neck_tail_map_mapped_program
cmake --build build-et-erbium-yolov8n --target tpa_yolov8n_p4_p5_neck_tail.elf
cmake --build build-et-erbium-yolov8n --target tpa_yolov8n_dense_c2f_detect_downstream_map_mapped_program
cmake --build build-et-erbium-yolov8n --target tpa_yolov8n_dense_c2f_detect_downstream.elf
cmake --build build-et-erbium-yolov8n --target tpa_yolov8n_c2f_detect_downstream_map_mapped_program
cmake --build build-et-erbium-yolov8n --target tpa_yolov8n_c2f_detect_downstream.elf
cmake --build build-et-erbium-yolov8n --target tpa_yolov8n_detect_downstream_map_mapped_program
cmake --build build-et-erbium-yolov8n --target tpa_yolov8n_detect_downstream.elf
/opt/et/bin/erbium_emu \
  -minions 0x7 \
  -elf_load build-et-erbium-yolov8n/tpa-device-prefix/src/tpa-device-build/yolov8n/tpa_yolov8n_detect_downstream.elf \
  -max_cycles 800000000
```

## Quick start: runtime regression test ELFs

Build representative original message, queue, and negative regression tests
through the Erbium ET superbuild:

```sh
cmake -S . -B build-et-erbium -DET_ROOT=/opt/et -DTPA_PLATFORM=erbium
cmake --build build-et-erbium --target \
  tpa_queue_basic.elf tpa_queue_yield.elf tpa_queue_many.elf tpa_queue_wake.elf
cmake --build build-et-erbium --target \
  tpa_msg_same_send_first.elf tpa_msg_cross_send_first.elf tpa_msg_fabric_send_first.elf
cmake --build build-et-erbium --target tpa_negative_expected_fail.elf
```

Representative ELFs can be loaded with `erbium_emu` in the same way as the demo
ELFs. The negative target intentionally calls `TEST_FAIL` when the cooperative runtime
scheduler executes process continuations. Expected-failure runtime semantics and
broader message/queue/negative behavioral coverage remain scheduler-hardening
follow-up work.

## Quick start: host launcher

Build the host launcher through the ET host subproject:

```sh
cmake --build build-et-erbium --target tpa_host_tools
```

Run a generated ELF through the ET runtime sysemu device layer:

```sh
build-et-erbium/tpa-host-prefix/src/tpa-host-build/tpa_launcher \
  --kernel build-et-erbium/tpa-device-prefix/src/tpa-device-build/kernels/tpa_pipe_demo.elf \
  --mode sysemu \
  --timeout 300
```

`tpa_launcher` also supports `--mode pcie` for silicon and `--mode fake` for
host runtime API smoke checks. Use `--help` for all options.

## Quick start: ET-SoC-1

The default ET-SoC-1 path validates the structured runtime archive in a one-shire
configuration:

```sh
cmake -S . -B build-et-etsoc1 -DET_ROOT=/opt/et -DTPA_PLATFORM=etsoc1
cmake --build build-et-etsoc1 --target tpa_core
```

YOLO mapping uses the full-card `machines/etsoc1.json` model. The device project
therefore keeps YOLO off by default for ET-SoC-1 unless configured with
`TPA_ETSOC1_NR_SHIRES=32`.

## Quick start: trace tools

Trace tools operate on `erbium_emu` logs and built ELFs:

```sh
tools/trace/split_trace_by_hart.sh /tmp/erbium.log /tmp/tpa-trace-by-hart
tools/trace/analyze_trace_by_hart.sh \
  build-et-erbium/tpa-device-prefix/src/tpa-device-build/kernels/tpa_pipe_demo.elf \
  /tmp/tpa-trace-by-hart/m0_h0.inst.log \
  --top
```

See `tools/trace/README.md` for gzip and cycle-window options.

## Host smoke-test double

For local syntax/unit smoke without et-platform:

```sh
cmake -S . -B build-smoke -DTPA_HOST_SMOKE_TEST_DOUBLE=ON
cmake --build build-smoke
ctest --test-dir build-smoke --output-on-failure
```

This mode defines `TPA_HOST_SMOKE_TEST_DOUBLE=1` and builds host test doubles
only. It must not be used as evidence that Erbium or ET-SoC-1 device platform
integration works.

## Current status

Ported and validated today:

- ET superbuild integration for device and host subprojects.
- Erbium `tpa_empty.elf`, `tpa_pipe_demo.elf`, `tpa_tensor_matmul.elf`,
  `tpa_fast_attention.elf`, `tpa_fast_attention_serial.elf`,
  `tpa_stereo_sad.elf`, `tpa_stereo_sad_mapped.elf`, and representative
  message/queue/negative regression ELF build paths.
- Cooperative runtime scheduler execution for generated graph programs, with
  Erbium emulator PASS validation for `tpa_empty.elf`, `tpa_pipe_demo.elf`,
  `tpa_tensor_matmul.elf`, `tpa_fast_attention.elf`,
  `tpa_fast_attention_serial.elf`, `tpa_stereo_sad.elf`,
  `tpa_stereo_sad_mapped.elf`, representative message/channel tests, and
  representative queue tests.
- Negative expected-failure execution reports the intended Erbium FAIL marker.
- YOLOv5n downstream planner/map artifact generation, downstream device ELF
  link, and Erbium emulator PASS-marker runtime validation.
- YOLOv8n external-header mapper/device milestones are integrated behind
  `BUILD_TPA_YOLOV8N=ON`; they consume external generated weights and validate
  deterministic synthetic-calibration hashes for sampled P5 Detect/DFL,
  sampled P3/P4/P5 Detect/DFL branch points, sampled P3 `model.15`, P4
  `model.18`, and P5 `model.21` C2f source modules feeding Detect/DFL, a
  sampled combined P3/P4/P5 C2f+Detect downstream graph, dense P3
  `model.15`, P4 `model.18`, and P5 `model.21` C2f feature-map validation
  feeding sampled Detect/DFL, a dense combined P3/P4/P5 C2f+Detect graph that
  validates all three dense C2f summaries together while Detect remains sampled,
  and a P4-to-P5 `model.19` neck-tail Conv+Concat graph that consumes dense P4
  C2f output and a deterministic synthetic SPPF-side edge.
- ET-SoC-1 default one-shire `tpa_core` build.
- `tpa_launcher` host tool target.
- Python planner package, checked-in machine JSONs, and planner tests.
- Ported message/channel, queue, and negative runtime regression test build
  targets.
- Trace analysis tools under `tools/trace/`.
- Archived/reference DNN demos, LTFarm experiment, and historical generated YOLO
  analysis under `docs/archive/`.
- Host smoke-test-double mode for non-platform syntax/unit smoke.

Important missing or partial areas remain: the full YOLO end-user host
pipeline and broader scheduler coverage beyond the representative runtime
regressions. YOLO tools/models and representative block tests are now ported;
DNN and LTFarm sources are archived/reference material. See `docs/yolo-demo.md`
for the current YOLO scope.
